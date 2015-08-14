#include "ORBGPU.h"

#include <iostream>

namespace camodocal
{

cv::Ptr<ORBGPU> ORBGPU::mInstance;

ORBGPU::ORBGPU(int nFeatures, float scaleFactor,
               int nLevels, int edgeThreshold,
               int firstLevel, int WTA_K,
               int scoreType, int patchSize)
 :
 
#ifdef HAVE_OPENCV3
 mORB_GPU(ORBType::create(nFeatures, scaleFactor, nLevels, edgeThreshold,
            firstLevel, WTA_K, scoreType, patchSize)),
 mMatcher(new MatcherType())
#else // HAVE_OPENCV3
  mORB_GPU(new ORBType(nFeatures, scaleFactor, nLevels, edgeThreshold,
            firstLevel, WTA_K, scoreType, patchSize)),
  mMatcher(new MatcherType())
#endif // HAVE_OPENCV3
    
{

}

ORBGPU::~ORBGPU()
{

}

cv::Ptr<ORBGPU>
ORBGPU::instance(int nFeatures, float scaleFactor,
                 int nLevels, int edgeThreshold,
                 int firstLevel, int WTA_K,
                 int scoreType, int patchSize)
{
    if (mInstance.empty())
    {
        mInstance = cv::Ptr<ORBGPU>(new ORBGPU(nFeatures, scaleFactor,
                                               nLevels, edgeThreshold,
                                               firstLevel, WTA_K,
                                               scoreType, patchSize));
    }

    return mInstance;
}

void
ORBGPU::detect(const cv::Mat& image, std::vector<cv::KeyPoint>& keypoints,
               const cv::Mat& mask)
{
    boost::mutex::scoped_lock lock(mORBMutex);

    try
    {
#ifdef HAVE_OPENCV3
        mORB_GPU->detect(image,keypoints, mask);
    
#else // HAVE_OPENCV3

        mImageGPU.upload(image);
        if (!mask.empty())
        {
            mMaskGPU.upload(mask);
        }
        else
        {
            mMaskGPU.release();
        }
    
        (*mORB_GPU)(mImageGPU, mMaskGPU, mKptsGPU);
        (*mORB_GPU).downloadKeyPoints(mKptsGPU, keypoints);
#endif // HAVE_OPENCV3
    }
    catch (cv::Exception& exception)
    {
        std::cout << "# ERROR: ORB GPU feature detection failed: " << exception.msg << std::endl;
    }

#ifndef HAVE_OPENCV3
    mImageGPU.release();
    mMaskGPU.release();
    mKptsGPU.release();
#endif
}

void
ORBGPU::compute(const cv::Mat& image,
                 std::vector<cv::KeyPoint>& keypoints,
                 cv::Mat& descriptors)
{
    boost::mutex::scoped_lock lock(mORBMutex);

    try
    {
#ifdef HAVE_OPENCV3
       mORB_GPU->compute(image,keypoints,descriptors);
    
#else // HAVE_OPENCV3
    
        mImageGPU.upload(image);
        (*mORB_GPU)(mImageGPU, MatType(), keypoints, mDtorsGPU);
#endif // HAVE_OPENCV3
    }
    catch (cv::Exception& exception)
    {
        std::cout << "# ERROR: ORB GPU descriptor computation failed: " << exception.msg << std::endl;
    }

#ifndef HAVE_OPENCV3
    mDtorsGPU.download(descriptors);

    mImageGPU.release();
    mDtorsGPU.release();
#endif
}

void
ORBGPU::knnMatch(const cv::Mat& queryDescriptors,
                  const cv::Mat& trainDescriptors,
                  std::vector<std::vector<cv::DMatch> >& matches,
                  int k,
                  const cv::Mat& mask,
                  bool compactResult)
{
    boost::mutex::scoped_lock lock(mMatchMutex);

    if (queryDescriptors.empty() || trainDescriptors.empty())
    {
        matches.clear();
        return;
    }
#ifdef HAVE_OPENCV3
    mMatcher->knnMatch(queryDescriptors, trainDescriptors, matches, k,mask,compactResult);
#else // HAVE_OPENCV3
    matches.reserve(queryDescriptors.rows);

    mQDtorsGPU.upload(queryDescriptors);
    mTDtorsGPU.upload(trainDescriptors);

    if (!mask.empty())
    {
        mMatchMaskGPU.upload(mask);
    }
    else
    {
        mMatchMaskGPU.release();
    }

    mMatcher->knnMatch(mQDtorsGPU, mTDtorsGPU, matches, k, mMatchMaskGPU, compactResult);
#endif // HAVE_OPENCV3
}

void
ORBGPU::radiusMatch(const cv::Mat& queryDescriptors,
                     const cv::Mat& trainDescriptors,
                     std::vector<std::vector<cv::DMatch> >& matches,
                     float maxDistance,
                     const cv::Mat& mask,
                     bool compactResult)
{
    boost::mutex::scoped_lock lock(mMatchMutex);

    if (queryDescriptors.empty() || trainDescriptors.empty())
    {
        matches.clear();
        return;
    }
#ifdef HAVE_OPENCV3
    mMatcher->radiusMatch(queryDescriptors, trainDescriptors, matches, maxDistance, mask, compactResult);
#else // HAVE_OPENCV3

    matches.reserve(queryDescriptors.rows);

    mQDtorsGPU.upload(queryDescriptors);
    mTDtorsGPU.upload(trainDescriptors);

    if (!mask.empty())
    {
        mMatchMaskGPU.upload(mask);
    }
    else
    {
        mMatchMaskGPU.release();
    }

    mMatcher->radiusMatch(mQDtorsGPU, mTDtorsGPU, matches, maxDistance, mMatchMaskGPU, compactResult);
#endif // HAVE_OPENCV3
}

}
