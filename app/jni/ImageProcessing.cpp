#include "com_cabatuan_skindetector_MainActivity.h"
#include <android/log.h>
#include <android/bitmap.h>

#include "opencv2/imgproc/imgproc.hpp"

using namespace cv;

#define  LOG_TAG    "SkinDetector"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)


 
// src - input gray image
// dst - output binary image

void adaptiveThreshold(const cv::Mat& src, cv::Mat& dst, int threshold = 5) {

    int blockSize= 15; // size of the neighborhood
    dst = src.clone();

	int nl= src.rows;  
	int nc= src.cols;  
              
	// compute integral image
	cv::Mat integralImg;
	cv::integral(src, integralImg, CV_32S);

	// for each row
	int halfSize= blockSize/2;
    for (int j = halfSize; j < nl-halfSize-1; j++) { // blocksize is a regular n x n block

		  // get the address of row j
		  uchar* data= dst.ptr<uchar>(j);
		  int* idata1= integralImg.ptr<int>(j-halfSize);
		  int* idata2= integralImg.ptr<int>(j+halfSize+1);

          for (int i = halfSize; i < nc - halfSize - 1; i++) {
 
			  // compute mean
			  int mean = (idata2[i+halfSize+1] - idata2[i-halfSize] -
				        idata1[i+halfSize+1] + idata1[i-halfSize])/(blockSize * blockSize);

			  // apply adaptive threshold
			  if (data[i] < (mean - threshold))
				  data[i] = 0;
				  
			  else
				  data[i] = 255;
         }                    
    }
}



void extractVU( const Mat &image, Mat &V, Mat &U ){
   
    // Accept only char type matrices
    CV_Assert(image.depth() != sizeof(uchar));

	int nRows = image.rows;   // number of lines
    int nCols = image.cols;   // number of columns  

    if (image.isContinuous()) {
        nCols = nCols * nRows;
		nRows = 1; // converted to 1D array
	}   

    
    for (int j=0; j<nRows; j++) { // for all pixels

        // pointer to first column of line j
        uchar* data   = reinterpret_cast<uchar*>(image.data);
        uchar* colorV = reinterpret_cast<uchar*>(V.data);
        uchar* colorU = reinterpret_cast<uchar*>(U.data);

		for (int i = 0; i < nCols; i += 2) {
		
		     // process each pixel; assign to V and U alternately
            *colorV++ =  *data++;   
        	*colorU++ =  *data++;     
        }
   }
}




void detectSkin( const cv::Mat &imageU, int lowThreshU, int highThreshU, const cv::Mat &imageV, int lowThreshV, int highThreshV, cv::Mat &dst ){
  
    cv::Mat Umask, Vmask;
    cv::inRange( imageU, lowThreshU, highThreshU, Umask);
    cv::inRange( imageV, lowThreshV, highThreshV, Vmask);
    
    cv::resize(Umask & Vmask, dst, dst.size());  
    // cv::resize(Umask & Vmask & (imageV > imageU), dst, dst.size()); // 25 ms 
}




/*       Global Variables        */
cv::Mat imageU, imageV; 

int64_t t;
cv::Mat tempGray;

/*
 * Class:     com_cabatuan_skindetector_MainActivity
 * Method:    process
 * Signature: (Landroid/graphics/Bitmap;[B)V
 */
JNIEXPORT void JNICALL Java_com_cabatuan_skindetector_MainActivity_process
  (JNIEnv *pEnv, jobject clazz, jobject pTarget, jbyteArray pSource, jint lowThreshU, jint highThreshU, jint lowThreshV, jint highThreshV){

   AndroidBitmapInfo bitmapInfo;
   uint32_t* bitmapContent; // Links to Bitmap content

   if(AndroidBitmap_getInfo(pEnv, pTarget, &bitmapInfo) < 0) abort();
   
   size_t half_height = bitmapInfo.height >> 1;
   size_t half_width  = bitmapInfo.width  >> 1;
   
   if(bitmapInfo.format != ANDROID_BITMAP_FORMAT_RGBA_8888) abort();
   if(AndroidBitmap_lockPixels(pEnv, pTarget, (void**)&bitmapContent) < 0) abort();

   /// Access source array data... OK
   jbyte* source = (jbyte*)pEnv->GetPrimitiveArrayCritical(pSource, 0);
   if (source == NULL) abort();

   /// cv::Mat for YUV420sp source and output BGRA 
    Mat srcNV21(bitmapInfo.height + half_height, bitmapInfo.width, CV_8UC1, (unsigned char *)source);
    Mat mbgra(bitmapInfo.height, bitmapInfo.width, CV_8UC4, (unsigned char *)bitmapContent);
    

/***********************************************************************************************/
    /// Native Image Processing HERE... 
   if(tempGray.empty())
       tempGray = Mat(bitmapInfo.height, bitmapInfo.width, CV_8UC1);
       
   if (imageV.empty())
       imageV.create( half_height, half_width, CV_8UC1);

   if (imageU.empty())
       imageU.create( half_height, half_width, CV_8UC1);
      
    t = cv::getTickCount();     

   // Extract chroma U and Chroma V
   extractVU( srcNV21.rowRange(bitmapInfo.height, bitmapInfo.height + half_height), imageV, imageU);
     
   // Detect the skin
   detectSkin(imageU, lowThreshU, highThreshU, imageV, lowThreshV, highThreshV, tempGray); 
       
   LOGI("myskindetector() took %0.2f ms.", 1000*(cv::getTickCount() - t)/(float)cv::getTickFrequency());
    
    cvtColor(tempGray, mbgra, CV_GRAY2BGRA);
   
   // Sanity Check... OK
   //cvtColor(srcNV21.rowRange(0, bitmapInfo.height), mbgra, CV_GRAY2BGRA);
 
/************************************************************************************************/ 
   
   /// Release Java byte buffer and unlock backing bitmap
   pEnv-> ReleasePrimitiveArrayCritical(pSource,source,0);
   if (AndroidBitmap_unlockPixels(pEnv, pTarget) < 0) abort();

}
