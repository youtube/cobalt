// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/capture/mojom/video_capture_types_mojom_traits.h"

#include "ui/gfx/geometry/mojom/geometry.mojom.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

// static
media_m96::mojom::ResolutionChangePolicy
EnumTraits<media_m96::mojom::ResolutionChangePolicy,
           media_m96::ResolutionChangePolicy>::ToMojom(media_m96::ResolutionChangePolicy
                                                       input) {
  switch (input) {
    case media_m96::ResolutionChangePolicy::FIXED_RESOLUTION:
      return media_m96::mojom::ResolutionChangePolicy::FIXED_RESOLUTION;
    case media_m96::ResolutionChangePolicy::FIXED_ASPECT_RATIO:
      return media_m96::mojom::ResolutionChangePolicy::FIXED_ASPECT_RATIO;
    case media_m96::ResolutionChangePolicy::ANY_WITHIN_LIMIT:
      return media_m96::mojom::ResolutionChangePolicy::ANY_WITHIN_LIMIT;
  }
  NOTREACHED();
  return media_m96::mojom::ResolutionChangePolicy::FIXED_RESOLUTION;
}

// static
bool EnumTraits<media_m96::mojom::ResolutionChangePolicy,
                media_m96::ResolutionChangePolicy>::
    FromMojom(media_m96::mojom::ResolutionChangePolicy input,
              media_m96::ResolutionChangePolicy* output) {
  switch (input) {
    case media_m96::mojom::ResolutionChangePolicy::FIXED_RESOLUTION:
      *output = media_m96::ResolutionChangePolicy::FIXED_RESOLUTION;
      return true;
    case media_m96::mojom::ResolutionChangePolicy::FIXED_ASPECT_RATIO:
      *output = media_m96::ResolutionChangePolicy::FIXED_ASPECT_RATIO;
      return true;
    case media_m96::mojom::ResolutionChangePolicy::ANY_WITHIN_LIMIT:
      *output = media_m96::ResolutionChangePolicy::ANY_WITHIN_LIMIT;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
media_m96::mojom::PowerLineFrequency EnumTraits<
    media_m96::mojom::PowerLineFrequency,
    media_m96::PowerLineFrequency>::ToMojom(media_m96::PowerLineFrequency input) {
  switch (input) {
    case media_m96::PowerLineFrequency::FREQUENCY_DEFAULT:
      return media_m96::mojom::PowerLineFrequency::DEFAULT;
    case media_m96::PowerLineFrequency::FREQUENCY_50HZ:
      return media_m96::mojom::PowerLineFrequency::HZ_50;
    case media_m96::PowerLineFrequency::FREQUENCY_60HZ:
      return media_m96::mojom::PowerLineFrequency::HZ_60;
  }
  NOTREACHED();
  return media_m96::mojom::PowerLineFrequency::DEFAULT;
}

// static
bool EnumTraits<media_m96::mojom::PowerLineFrequency, media_m96::PowerLineFrequency>::
    FromMojom(media_m96::mojom::PowerLineFrequency input,
              media_m96::PowerLineFrequency* output) {
  switch (input) {
    case media_m96::mojom::PowerLineFrequency::DEFAULT:
      *output = media_m96::PowerLineFrequency::FREQUENCY_DEFAULT;
      return true;
    case media_m96::mojom::PowerLineFrequency::HZ_50:
      *output = media_m96::PowerLineFrequency::FREQUENCY_50HZ;
      return true;
    case media_m96::mojom::PowerLineFrequency::HZ_60:
      *output = media_m96::PowerLineFrequency::FREQUENCY_60HZ;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
media_m96::mojom::VideoCapturePixelFormat
EnumTraits<media_m96::mojom::VideoCapturePixelFormat,
           media_m96::VideoPixelFormat>::ToMojom(media_m96::VideoPixelFormat input) {
  switch (input) {
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_UNKNOWN:
      return media_m96::mojom::VideoCapturePixelFormat::UNKNOWN;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_I420:
      return media_m96::mojom::VideoCapturePixelFormat::I420;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_YV12:
      return media_m96::mojom::VideoCapturePixelFormat::YV12;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_I422:
      return media_m96::mojom::VideoCapturePixelFormat::I422;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_I420A:
      return media_m96::mojom::VideoCapturePixelFormat::I420A;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_I444:
      return media_m96::mojom::VideoCapturePixelFormat::I444;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_NV12:
      return media_m96::mojom::VideoCapturePixelFormat::NV12;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_NV21:
      return media_m96::mojom::VideoCapturePixelFormat::NV21;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_UYVY:
      return media_m96::mojom::VideoCapturePixelFormat::UYVY;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_YUY2:
      return media_m96::mojom::VideoCapturePixelFormat::YUY2;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_ARGB:
      return media_m96::mojom::VideoCapturePixelFormat::ARGB;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_BGRA:
      return media_m96::mojom::VideoCapturePixelFormat::BGRA;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_XRGB:
      return media_m96::mojom::VideoCapturePixelFormat::XRGB;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_RGB24:
      return media_m96::mojom::VideoCapturePixelFormat::RGB24;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_MJPEG:
      return media_m96::mojom::VideoCapturePixelFormat::MJPEG;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_YUV420P9:
      return media_m96::mojom::VideoCapturePixelFormat::YUV420P9;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_YUV420P10:
      return media_m96::mojom::VideoCapturePixelFormat::YUV420P10;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_YUV422P9:
      return media_m96::mojom::VideoCapturePixelFormat::YUV422P9;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_YUV422P10:
      return media_m96::mojom::VideoCapturePixelFormat::YUV422P10;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_YUV444P9:
      return media_m96::mojom::VideoCapturePixelFormat::YUV444P9;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_YUV444P10:
      return media_m96::mojom::VideoCapturePixelFormat::YUV444P10;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_YUV420P12:
      return media_m96::mojom::VideoCapturePixelFormat::YUV420P12;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_YUV422P12:
      return media_m96::mojom::VideoCapturePixelFormat::YUV422P12;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_YUV444P12:
      return media_m96::mojom::VideoCapturePixelFormat::YUV444P12;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_Y16:
      return media_m96::mojom::VideoCapturePixelFormat::Y16;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_ABGR:
      return media_m96::mojom::VideoCapturePixelFormat::ABGR;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_XBGR:
      return media_m96::mojom::VideoCapturePixelFormat::XBGR;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_P016LE:
      return media_m96::mojom::VideoCapturePixelFormat::P016LE;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_XR30:
      return media_m96::mojom::VideoCapturePixelFormat::XR30;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_XB30:
      return media_m96::mojom::VideoCapturePixelFormat::XB30;
    case media_m96::VideoPixelFormat::PIXEL_FORMAT_RGBAF16:
      return media_m96::mojom::VideoCapturePixelFormat::RGBAF16;
  }
  NOTREACHED();
  return media_m96::mojom::VideoCapturePixelFormat::I420;
}

// static
bool EnumTraits<media_m96::mojom::VideoCapturePixelFormat,
                media_m96::VideoPixelFormat>::
    FromMojom(media_m96::mojom::VideoCapturePixelFormat input,
              media_m96::VideoPixelFormat* output) {
  switch (input) {
    case media_m96::mojom::VideoCapturePixelFormat::UNKNOWN:
      *output = media_m96::PIXEL_FORMAT_UNKNOWN;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::I420:
      *output = media_m96::PIXEL_FORMAT_I420;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::YV12:
      *output = media_m96::PIXEL_FORMAT_YV12;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::I422:
      *output = media_m96::PIXEL_FORMAT_I422;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::I420A:
      *output = media_m96::PIXEL_FORMAT_I420A;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::I444:
      *output = media_m96::PIXEL_FORMAT_I444;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::NV12:
      *output = media_m96::PIXEL_FORMAT_NV12;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::NV21:
      *output = media_m96::PIXEL_FORMAT_NV21;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::UYVY:
      *output = media_m96::PIXEL_FORMAT_UYVY;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::YUY2:
      *output = media_m96::PIXEL_FORMAT_YUY2;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::ARGB:
      *output = media_m96::PIXEL_FORMAT_ARGB;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::XRGB:
      *output = media_m96::PIXEL_FORMAT_XRGB;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::RGB24:
      *output = media_m96::PIXEL_FORMAT_RGB24;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::MJPEG:
      *output = media_m96::PIXEL_FORMAT_MJPEG;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::YUV420P9:
      *output = media_m96::PIXEL_FORMAT_YUV420P9;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::YUV420P10:
      *output = media_m96::PIXEL_FORMAT_YUV420P10;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::YUV422P9:
      *output = media_m96::PIXEL_FORMAT_YUV422P9;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::YUV422P10:
      *output = media_m96::PIXEL_FORMAT_YUV422P10;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::YUV444P9:
      *output = media_m96::PIXEL_FORMAT_YUV444P9;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::YUV444P10:
      *output = media_m96::PIXEL_FORMAT_YUV444P10;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::YUV420P12:
      *output = media_m96::PIXEL_FORMAT_YUV420P12;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::YUV422P12:
      *output = media_m96::PIXEL_FORMAT_YUV422P12;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::YUV444P12:
      *output = media_m96::PIXEL_FORMAT_YUV444P12;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::Y16:
      *output = media_m96::PIXEL_FORMAT_Y16;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::ABGR:
      *output = media_m96::PIXEL_FORMAT_ABGR;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::XBGR:
      *output = media_m96::PIXEL_FORMAT_XBGR;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::P016LE:
      *output = media_m96::PIXEL_FORMAT_P016LE;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::XR30:
      *output = media_m96::PIXEL_FORMAT_XR30;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::XB30:
      *output = media_m96::PIXEL_FORMAT_XB30;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::BGRA:
      *output = media_m96::PIXEL_FORMAT_BGRA;
      return true;
    case media_m96::mojom::VideoCapturePixelFormat::RGBAF16:
      *output = media_m96::PIXEL_FORMAT_RGBAF16;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
media_m96::mojom::VideoCaptureBufferType
EnumTraits<media_m96::mojom::VideoCaptureBufferType,
           media_m96::VideoCaptureBufferType>::ToMojom(media_m96::VideoCaptureBufferType
                                                       input) {
  switch (input) {
    case media_m96::VideoCaptureBufferType::kSharedMemory:
      return media_m96::mojom::VideoCaptureBufferType::kSharedMemory;
    case media_m96::VideoCaptureBufferType::kSharedMemoryViaRawFileDescriptor:
      return media_m96::mojom::VideoCaptureBufferType::
          kSharedMemoryViaRawFileDescriptor;
    case media_m96::VideoCaptureBufferType::kMailboxHolder:
      return media_m96::mojom::VideoCaptureBufferType::kMailboxHolder;
    case media_m96::VideoCaptureBufferType::kGpuMemoryBuffer:
      return media_m96::mojom::VideoCaptureBufferType::kGpuMemoryBuffer;
  }
  NOTREACHED();
  return media_m96::mojom::VideoCaptureBufferType::kSharedMemory;
}

// static
bool EnumTraits<media_m96::mojom::VideoCaptureBufferType,
                media_m96::VideoCaptureBufferType>::
    FromMojom(media_m96::mojom::VideoCaptureBufferType input,
              media_m96::VideoCaptureBufferType* output) {
  switch (input) {
    case media_m96::mojom::VideoCaptureBufferType::kSharedMemory:
      *output = media_m96::VideoCaptureBufferType::kSharedMemory;
      return true;
    case media_m96::mojom::VideoCaptureBufferType::
        kSharedMemoryViaRawFileDescriptor:
      *output =
          media_m96::VideoCaptureBufferType::kSharedMemoryViaRawFileDescriptor;
      return true;
    case media_m96::mojom::VideoCaptureBufferType::kMailboxHolder:
      *output = media_m96::VideoCaptureBufferType::kMailboxHolder;
      return true;
    case media_m96::mojom::VideoCaptureBufferType::kGpuMemoryBuffer:
      *output = media_m96::VideoCaptureBufferType::kGpuMemoryBuffer;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
media_m96::mojom::VideoCaptureError
EnumTraits<media_m96::mojom::VideoCaptureError, media_m96::VideoCaptureError>::ToMojom(
    media_m96::VideoCaptureError input) {
  switch (input) {
    case media_m96::VideoCaptureError::kNone:
      return media_m96::mojom::VideoCaptureError::kNone;
    case media_m96::VideoCaptureError::
        kVideoCaptureControllerInvalidOrUnsupportedVideoCaptureParametersRequested:
      return media_m96::mojom::VideoCaptureError::
          kVideoCaptureControllerInvalidOrUnsupportedVideoCaptureParametersRequested;
    case media_m96::VideoCaptureError::kVideoCaptureControllerIsAlreadyInErrorState:
      return media_m96::mojom::VideoCaptureError::
          kVideoCaptureControllerIsAlreadyInErrorState;
    case media_m96::VideoCaptureError::kVideoCaptureManagerDeviceConnectionLost:
      return media_m96::mojom::VideoCaptureError::
          kVideoCaptureManagerDeviceConnectionLost;
    case media_m96::VideoCaptureError::
        kFrameSinkVideoCaptureDeviceAleradyEndedOnFatalError:
      return media_m96::mojom::VideoCaptureError::
          kFrameSinkVideoCaptureDeviceAleradyEndedOnFatalError;
    case media_m96::VideoCaptureError::
        kFrameSinkVideoCaptureDeviceEncounteredFatalError:
      return media_m96::mojom::VideoCaptureError::
          kFrameSinkVideoCaptureDeviceEncounteredFatalError;
    case media_m96::VideoCaptureError::kV4L2FailedToOpenV4L2DeviceDriverFile:
      return media_m96::mojom::VideoCaptureError::
          kV4L2FailedToOpenV4L2DeviceDriverFile;
    case media_m96::VideoCaptureError::kV4L2ThisIsNotAV4L2VideoCaptureDevice:
      return media_m96::mojom::VideoCaptureError::
          kV4L2ThisIsNotAV4L2VideoCaptureDevice;
    case media_m96::VideoCaptureError::kV4L2FailedToFindASupportedCameraFormat:
      return media_m96::mojom::VideoCaptureError::
          kV4L2FailedToFindASupportedCameraFormat;
    case media_m96::VideoCaptureError::kV4L2FailedToSetVideoCaptureFormat:
      return media_m96::mojom::VideoCaptureError::
          kV4L2FailedToSetVideoCaptureFormat;
    case media_m96::VideoCaptureError::kV4L2UnsupportedPixelFormat:
      return media_m96::mojom::VideoCaptureError::kV4L2UnsupportedPixelFormat;
    case media_m96::VideoCaptureError::kV4L2FailedToSetCameraFramerate:
      return media_m96::mojom::VideoCaptureError::kV4L2FailedToSetCameraFramerate;
    case media_m96::VideoCaptureError::kV4L2ErrorRequestingMmapBuffers:
      return media_m96::mojom::VideoCaptureError::kV4L2ErrorRequestingMmapBuffers;
    case media_m96::VideoCaptureError::kV4L2AllocateBufferFailed:
      return media_m96::mojom::VideoCaptureError::kV4L2AllocateBufferFailed;
    case media_m96::VideoCaptureError::kV4L2VidiocStreamonFailed:
      return media_m96::mojom::VideoCaptureError::kV4L2VidiocStreamonFailed;
    case media_m96::VideoCaptureError::kV4L2VidiocStreamoffFailed:
      return media_m96::mojom::VideoCaptureError::kV4L2VidiocStreamoffFailed;
    case media_m96::VideoCaptureError::kV4L2FailedToVidiocReqbufsWithCount0:
      return media_m96::mojom::VideoCaptureError::
          kV4L2FailedToVidiocReqbufsWithCount0;
    case media_m96::VideoCaptureError::kV4L2PollFailed:
      return media_m96::mojom::VideoCaptureError::kV4L2PollFailed;
    case media_m96::VideoCaptureError::
        kV4L2MultipleContinuousTimeoutsWhileReadPolling:
      return media_m96::mojom::VideoCaptureError::
          kV4L2MultipleContinuousTimeoutsWhileReadPolling;
    case media_m96::VideoCaptureError::kV4L2FailedToDequeueCaptureBuffer:
      return media_m96::mojom::VideoCaptureError::kV4L2FailedToDequeueCaptureBuffer;
    case media_m96::VideoCaptureError::kV4L2FailedToEnqueueCaptureBuffer:
      return media_m96::mojom::VideoCaptureError::kV4L2FailedToEnqueueCaptureBuffer;
    case media_m96::VideoCaptureError::
        kSingleClientVideoCaptureHostLostConnectionToDevice:
      return media_m96::mojom::VideoCaptureError::
          kSingleClientVideoCaptureHostLostConnectionToDevice;
    case media_m96::VideoCaptureError::kSingleClientVideoCaptureDeviceLaunchAborted:
      return media_m96::mojom::VideoCaptureError::
          kSingleClientVideoCaptureDeviceLaunchAborted;
    case media_m96::VideoCaptureError::
        kDesktopCaptureDeviceWebrtcDesktopCapturerHasFailed:
      return media_m96::mojom::VideoCaptureError::
          kDesktopCaptureDeviceWebrtcDesktopCapturerHasFailed;
    case media_m96::VideoCaptureError::kFileVideoCaptureDeviceCouldNotOpenVideoFile:
      return media_m96::mojom::VideoCaptureError::
          kFileVideoCaptureDeviceCouldNotOpenVideoFile;
    case media_m96::VideoCaptureError::
        kDeviceCaptureLinuxFailedToCreateVideoCaptureDelegate:
      return media_m96::mojom::VideoCaptureError::
          kDeviceCaptureLinuxFailedToCreateVideoCaptureDelegate;
    case media_m96::VideoCaptureError::
        kErrorFakeDeviceIntentionallyEmittingErrorEvent:
      return media_m96::mojom::VideoCaptureError::
          kErrorFakeDeviceIntentionallyEmittingErrorEvent;
    case media_m96::VideoCaptureError::kDeviceClientTooManyFramesDroppedY16:
      return media_m96::mojom::VideoCaptureError::
          kDeviceClientTooManyFramesDroppedY16;
    case media_m96::VideoCaptureError::
        kDeviceMediaToMojoAdapterEncounteredUnsupportedBufferType:
      return media_m96::mojom::VideoCaptureError::
          kDeviceMediaToMojoAdapterEncounteredUnsupportedBufferType;
    case media_m96::VideoCaptureError::
        kVideoCaptureManagerProcessDeviceStartQueueDeviceInfoNotFound:
      return media_m96::mojom::VideoCaptureError::
          kVideoCaptureManagerProcessDeviceStartQueueDeviceInfoNotFound;
    case media_m96::VideoCaptureError::
        kInProcessDeviceLauncherFailedToCreateDeviceInstance:
      return media_m96::mojom::VideoCaptureError::
          kInProcessDeviceLauncherFailedToCreateDeviceInstance;
    case media_m96::VideoCaptureError::
        kServiceDeviceLauncherLostConnectionToDeviceFactoryDuringDeviceStart:
      return media_m96::mojom::VideoCaptureError::
          kServiceDeviceLauncherLostConnectionToDeviceFactoryDuringDeviceStart;
    case media_m96::VideoCaptureError::
        kServiceDeviceLauncherServiceRespondedWithDeviceNotFound:
      return media_m96::mojom::VideoCaptureError::
          kServiceDeviceLauncherServiceRespondedWithDeviceNotFound;
    case media_m96::VideoCaptureError::
        kServiceDeviceLauncherConnectionLostWhileWaitingForCallback:
      return media_m96::mojom::VideoCaptureError::
          kServiceDeviceLauncherConnectionLostWhileWaitingForCallback;
    case media_m96::VideoCaptureError::kIntentionalErrorRaisedByUnitTest:
      return media_m96::mojom::VideoCaptureError::kIntentionalErrorRaisedByUnitTest;
    case media_m96::VideoCaptureError::kCrosHalV3FailedToStartDeviceThread:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3FailedToStartDeviceThread;
    case media_m96::VideoCaptureError::kCrosHalV3DeviceDelegateMojoConnectionError:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3DeviceDelegateMojoConnectionError;
    case media_m96::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToGetCameraInfo:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToGetCameraInfo;
    case media_m96::VideoCaptureError::
        kCrosHalV3DeviceDelegateMissingSensorOrientationInfo:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3DeviceDelegateMissingSensorOrientationInfo;
    case media_m96::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToOpenCameraDevice:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToOpenCameraDevice;
    case media_m96::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToInitializeCameraDevice:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToInitializeCameraDevice;
    case media_m96::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToConfigureStreams:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToConfigureStreams;
    case media_m96::VideoCaptureError::
        kCrosHalV3DeviceDelegateWrongNumberOfStreamsConfigured:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3DeviceDelegateWrongNumberOfStreamsConfigured;
    case media_m96::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToGetDefaultRequestSettings:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToGetDefaultRequestSettings;
    case media_m96::VideoCaptureError::
        kCrosHalV3BufferManagerHalRequestedTooManyBuffers:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerHalRequestedTooManyBuffers;
    case media_m96::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToCreateGpuMemoryBuffer:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToCreateGpuMemoryBuffer;
    case media_m96::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToMapGpuMemoryBuffer:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToMapGpuMemoryBuffer;
    case media_m96::VideoCaptureError::
        kCrosHalV3BufferManagerUnsupportedVideoPixelFormat:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerUnsupportedVideoPixelFormat;
    case media_m96::VideoCaptureError::kCrosHalV3BufferManagerFailedToDupFd:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToDupFd;
    case media_m96::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToWrapGpuMemoryHandle:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToWrapGpuMemoryHandle;
    case media_m96::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToRegisterBuffer:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToRegisterBuffer;
    case media_m96::VideoCaptureError::
        kCrosHalV3BufferManagerProcessCaptureRequestFailed:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerProcessCaptureRequestFailed;
    case media_m96::VideoCaptureError::
        kCrosHalV3BufferManagerInvalidPendingResultId:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerInvalidPendingResultId;
    case media_m96::VideoCaptureError::
        kCrosHalV3BufferManagerReceivedDuplicatedPartialMetadata:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerReceivedDuplicatedPartialMetadata;
    case media_m96::VideoCaptureError::
        kCrosHalV3BufferManagerIncorrectNumberOfOutputBuffersReceived:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerIncorrectNumberOfOutputBuffersReceived;
    case media_m96::VideoCaptureError::
        kCrosHalV3BufferManagerInvalidTypeOfOutputBuffersReceived:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerInvalidTypeOfOutputBuffersReceived;
    case media_m96::VideoCaptureError::
        kCrosHalV3BufferManagerReceivedMultipleResultBuffersForFrame:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerReceivedMultipleResultBuffersForFrame;
    case media_m96::VideoCaptureError::
        kCrosHalV3BufferManagerUnknownStreamInCamera3NotifyMsg:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerUnknownStreamInCamera3NotifyMsg;
    case media_m96::VideoCaptureError::
        kCrosHalV3BufferManagerReceivedInvalidShutterTime:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerReceivedInvalidShutterTime;
    case media_m96::VideoCaptureError::kCrosHalV3BufferManagerFatalDeviceError:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerFatalDeviceError;
    case media_m96::VideoCaptureError::
        kCrosHalV3BufferManagerReceivedFrameIsOutOfOrder:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerReceivedFrameIsOutOfOrder;
    case media_m96::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToUnwrapReleaseFenceFd:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToUnwrapReleaseFenceFd;
    case media_m96::VideoCaptureError::
        kCrosHalV3BufferManagerSyncWaitOnReleaseFenceTimedOut:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerSyncWaitOnReleaseFenceTimedOut;
    case media_m96::VideoCaptureError::kCrosHalV3BufferManagerInvalidJpegBlob:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerInvalidJpegBlob;
    case media_m96::VideoCaptureError::kAndroidFailedToAllocate:
      return media_m96::mojom::VideoCaptureError::kAndroidFailedToAllocate;
    case media_m96::VideoCaptureError::kAndroidFailedToStartCapture:
      return media_m96::mojom::VideoCaptureError::kAndroidFailedToStartCapture;
    case media_m96::VideoCaptureError::kAndroidFailedToStopCapture:
      return media_m96::mojom::VideoCaptureError::kAndroidFailedToStopCapture;
    case media_m96::VideoCaptureError::kAndroidApi1CameraErrorCallbackReceived:
      return media_m96::mojom::VideoCaptureError::
          kAndroidApi1CameraErrorCallbackReceived;
    case media_m96::VideoCaptureError::kAndroidApi2CameraDeviceErrorReceived:
      return media_m96::mojom::VideoCaptureError::
          kAndroidApi2CameraDeviceErrorReceived;
    case media_m96::VideoCaptureError::kAndroidApi2CaptureSessionConfigureFailed:
      return media_m96::mojom::VideoCaptureError::
          kAndroidApi2CaptureSessionConfigureFailed;
    case media_m96::VideoCaptureError::kAndroidApi2ImageReaderUnexpectedImageFormat:
      return media_m96::mojom::VideoCaptureError::
          kAndroidApi2ImageReaderUnexpectedImageFormat;
    case media_m96::VideoCaptureError::
        kAndroidApi2ImageReaderSizeDidNotMatchImageSize:
      return media_m96::mojom::VideoCaptureError::
          kAndroidApi2ImageReaderSizeDidNotMatchImageSize;
    case media_m96::VideoCaptureError::kAndroidApi2ErrorRestartingPreview:
      return media_m96::mojom::VideoCaptureError::
          kAndroidApi2ErrorRestartingPreview;
    case media_m96::VideoCaptureError::kAndroidScreenCaptureUnsupportedFormat:
      return media_m96::mojom::VideoCaptureError::
          kAndroidScreenCaptureUnsupportedFormat;
    case media_m96::VideoCaptureError::
        kAndroidScreenCaptureFailedToStartCaptureMachine:
      return media_m96::mojom::VideoCaptureError::
          kAndroidScreenCaptureFailedToStartCaptureMachine;
    case media_m96::VideoCaptureError::
        kAndroidScreenCaptureTheUserDeniedScreenCapture:
      return media_m96::mojom::VideoCaptureError::
          kAndroidScreenCaptureTheUserDeniedScreenCapture;
    case media_m96::VideoCaptureError::
        kAndroidScreenCaptureFailedToStartScreenCapture:
      return media_m96::mojom::VideoCaptureError::
          kAndroidScreenCaptureFailedToStartScreenCapture;
    case media_m96::VideoCaptureError::kWinDirectShowCantGetCaptureFormatSettings:
      return media_m96::mojom::VideoCaptureError::
          kWinDirectShowCantGetCaptureFormatSettings;
    case media_m96::VideoCaptureError::
        kWinDirectShowFailedToGetNumberOfCapabilities:
      return media_m96::mojom::VideoCaptureError::
          kWinDirectShowFailedToGetNumberOfCapabilities;
    case media_m96::VideoCaptureError::
        kWinDirectShowFailedToGetCaptureDeviceCapabilities:
      return media_m96::mojom::VideoCaptureError::
          kWinDirectShowFailedToGetCaptureDeviceCapabilities;
    case media_m96::VideoCaptureError::
        kWinDirectShowFailedToSetCaptureDeviceOutputFormat:
      return media_m96::mojom::VideoCaptureError::
          kWinDirectShowFailedToSetCaptureDeviceOutputFormat;
    case media_m96::VideoCaptureError::kWinDirectShowFailedToConnectTheCaptureGraph:
      return media_m96::mojom::VideoCaptureError::
          kWinDirectShowFailedToConnectTheCaptureGraph;
    case media_m96::VideoCaptureError::kWinDirectShowFailedToPauseTheCaptureDevice:
      return media_m96::mojom::VideoCaptureError::
          kWinDirectShowFailedToPauseTheCaptureDevice;
    case media_m96::VideoCaptureError::kWinDirectShowFailedToStartTheCaptureDevice:
      return media_m96::mojom::VideoCaptureError::
          kWinDirectShowFailedToStartTheCaptureDevice;
    case media_m96::VideoCaptureError::kWinDirectShowFailedToStopTheCaptureGraph:
      return media_m96::mojom::VideoCaptureError::
          kWinDirectShowFailedToStopTheCaptureGraph;
    case media_m96::VideoCaptureError::kWinMediaFoundationEngineIsNull:
      return media_m96::mojom::VideoCaptureError::kWinMediaFoundationEngineIsNull;
    case media_m96::VideoCaptureError::kWinMediaFoundationEngineGetSourceFailed:
      return media_m96::mojom::VideoCaptureError::
          kWinMediaFoundationEngineGetSourceFailed;
    case media_m96::VideoCaptureError::
        kWinMediaFoundationFillPhotoCapabilitiesFailed:
      return media_m96::mojom::VideoCaptureError::
          kWinMediaFoundationFillPhotoCapabilitiesFailed;
    case media_m96::VideoCaptureError::
        kWinMediaFoundationFillVideoCapabilitiesFailed:
      return media_m96::mojom::VideoCaptureError::
          kWinMediaFoundationFillVideoCapabilitiesFailed;
    case media_m96::VideoCaptureError::kWinMediaFoundationNoVideoCapabilityFound:
      return media_m96::mojom::VideoCaptureError::
          kWinMediaFoundationNoVideoCapabilityFound;
    case media_m96::VideoCaptureError::
        kWinMediaFoundationGetAvailableDeviceMediaTypeFailed:
      return media_m96::mojom::VideoCaptureError::
          kWinMediaFoundationGetAvailableDeviceMediaTypeFailed;
    case media_m96::VideoCaptureError::
        kWinMediaFoundationSetCurrentDeviceMediaTypeFailed:
      return media_m96::mojom::VideoCaptureError::
          kWinMediaFoundationSetCurrentDeviceMediaTypeFailed;
    case media_m96::VideoCaptureError::kWinMediaFoundationEngineGetSinkFailed:
      return media_m96::mojom::VideoCaptureError::
          kWinMediaFoundationEngineGetSinkFailed;
    case media_m96::VideoCaptureError::
        kWinMediaFoundationSinkQueryCapturePreviewInterfaceFailed:
      return media_m96::mojom::VideoCaptureError::
          kWinMediaFoundationSinkQueryCapturePreviewInterfaceFailed;
    case media_m96::VideoCaptureError::
        kWinMediaFoundationSinkRemoveAllStreamsFailed:
      return media_m96::mojom::VideoCaptureError::
          kWinMediaFoundationSinkRemoveAllStreamsFailed;
    case media_m96::VideoCaptureError::
        kWinMediaFoundationCreateSinkVideoMediaTypeFailed:
      return media_m96::mojom::VideoCaptureError::
          kWinMediaFoundationCreateSinkVideoMediaTypeFailed;
    case media_m96::VideoCaptureError::
        kWinMediaFoundationConvertToVideoSinkMediaTypeFailed:
      return media_m96::mojom::VideoCaptureError::
          kWinMediaFoundationConvertToVideoSinkMediaTypeFailed;
    case media_m96::VideoCaptureError::kWinMediaFoundationSinkAddStreamFailed:
      return media_m96::mojom::VideoCaptureError::
          kWinMediaFoundationSinkAddStreamFailed;
    case media_m96::VideoCaptureError::
        kWinMediaFoundationSinkSetSampleCallbackFailed:
      return media_m96::mojom::VideoCaptureError::
          kWinMediaFoundationSinkSetSampleCallbackFailed;
    case media_m96::VideoCaptureError::kWinMediaFoundationEngineStartPreviewFailed:
      return media_m96::mojom::VideoCaptureError::
          kWinMediaFoundationEngineStartPreviewFailed;
    case media_m96::VideoCaptureError::kWinMediaFoundationGetMediaEventStatusFailed:
      return media_m96::mojom::VideoCaptureError::
          kWinMediaFoundationGetMediaEventStatusFailed;
    case media_m96::VideoCaptureError::kMacSetCaptureDeviceFailed:
      return media_m96::mojom::VideoCaptureError::kMacSetCaptureDeviceFailed;
    case media_m96::VideoCaptureError::kMacCouldNotStartCaptureDevice:
      return media_m96::mojom::VideoCaptureError::kMacCouldNotStartCaptureDevice;
    case media_m96::VideoCaptureError::kMacReceivedFrameWithUnexpectedResolution:
      return media_m96::mojom::VideoCaptureError::
          kMacReceivedFrameWithUnexpectedResolution;
    case media_m96::VideoCaptureError::kMacUpdateCaptureResolutionFailed:
      return media_m96::mojom::VideoCaptureError::kMacUpdateCaptureResolutionFailed;
    case media_m96::VideoCaptureError::kMacDeckLinkDeviceIdNotFoundInTheSystem:
      return media_m96::mojom::VideoCaptureError::
          kMacDeckLinkDeviceIdNotFoundInTheSystem;
    case media_m96::VideoCaptureError::kMacDeckLinkErrorQueryingInputInterface:
      return media_m96::mojom::VideoCaptureError::
          kMacDeckLinkErrorQueryingInputInterface;
    case media_m96::VideoCaptureError::kMacDeckLinkErrorCreatingDisplayModeIterator:
      return media_m96::mojom::VideoCaptureError::
          kMacDeckLinkErrorCreatingDisplayModeIterator;
    case media_m96::VideoCaptureError::kMacDeckLinkCouldNotFindADisplayMode:
      return media_m96::mojom::VideoCaptureError::
          kMacDeckLinkCouldNotFindADisplayMode;
    case media_m96::VideoCaptureError::
        kMacDeckLinkCouldNotSelectTheVideoFormatWeLike:
      return media_m96::mojom::VideoCaptureError::
          kMacDeckLinkCouldNotSelectTheVideoFormatWeLike;
    case media_m96::VideoCaptureError::kMacDeckLinkCouldNotStartCapturing:
      return media_m96::mojom::VideoCaptureError::
          kMacDeckLinkCouldNotStartCapturing;
    case media_m96::VideoCaptureError::kMacDeckLinkUnsupportedPixelFormat:
      return media_m96::mojom::VideoCaptureError::
          kMacDeckLinkUnsupportedPixelFormat;
    case media_m96::VideoCaptureError::
        kMacAvFoundationReceivedAVCaptureSessionRuntimeErrorNotification:
      return media_m96::mojom::VideoCaptureError::
          kMacAvFoundationReceivedAVCaptureSessionRuntimeErrorNotification;
    case media_m96::VideoCaptureError::kAndroidApi2ErrorConfiguringCamera:
      return media_m96::mojom::VideoCaptureError::
          kAndroidApi2ErrorConfiguringCamera;
    case media_m96::VideoCaptureError::kCrosHalV3DeviceDelegateFailedToFlush:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToFlush;
    case media_m96::VideoCaptureError::kFuchsiaCameraDeviceDisconnected:
      return media_m96::mojom::VideoCaptureError::kFuchsiaCameraDeviceDisconnected;
    case media_m96::VideoCaptureError::kFuchsiaCameraStreamDisconnected:
      return media_m96::mojom::VideoCaptureError::kFuchsiaCameraStreamDisconnected;
    case media_m96::VideoCaptureError::kFuchsiaSysmemDidNotSetImageFormat:
      return media_m96::mojom::VideoCaptureError::
          kFuchsiaSysmemDidNotSetImageFormat;
    case media_m96::VideoCaptureError::kFuchsiaSysmemInvalidBufferIndex:
      return media_m96::mojom::VideoCaptureError::kFuchsiaSysmemInvalidBufferIndex;
    case media_m96::VideoCaptureError::kFuchsiaSysmemInvalidBufferSize:
      return media_m96::mojom::VideoCaptureError::kFuchsiaSysmemInvalidBufferSize;
    case media_m96::VideoCaptureError::kFuchsiaUnsupportedPixelFormat:
      return media_m96::mojom::VideoCaptureError::kFuchsiaUnsupportedPixelFormat;
    case media_m96::VideoCaptureError::kFuchsiaFailedToMapSysmemBuffer:
      return media_m96::mojom::VideoCaptureError::kFuchsiaFailedToMapSysmemBuffer;
    case media_m96::VideoCaptureError::kCrosHalV3DeviceContextDuplicatedClient:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3DeviceContextDuplicatedClient;
    case media_m96::VideoCaptureError::kDesktopCaptureDeviceMacFailedStreamCreate:
      return media_m96::mojom::VideoCaptureError::
          kDesktopCaptureDeviceMacFailedStreamCreate;
    case media_m96::VideoCaptureError::kDesktopCaptureDeviceMacFailedStreamStart:
      return media_m96::mojom::VideoCaptureError::
          kDesktopCaptureDeviceMacFailedStreamStart;
    case media_m96::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToReserveBuffers:
      return media_m96::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToReserveBuffers;
  }
  NOTREACHED();
  return media_m96::mojom::VideoCaptureError::kNone;
}

// static
bool EnumTraits<media_m96::mojom::VideoCaptureError, media_m96::VideoCaptureError>::
    FromMojom(media_m96::mojom::VideoCaptureError input,
              media_m96::VideoCaptureError* output) {
  switch (input) {
    case media_m96::mojom::VideoCaptureError::kNone:
      *output = media_m96::VideoCaptureError::kNone;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kVideoCaptureControllerInvalidOrUnsupportedVideoCaptureParametersRequested:
      *output = media_m96::VideoCaptureError::
          kVideoCaptureControllerInvalidOrUnsupportedVideoCaptureParametersRequested;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kVideoCaptureControllerIsAlreadyInErrorState:
      *output = media_m96::VideoCaptureError::
          kVideoCaptureControllerIsAlreadyInErrorState;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kVideoCaptureManagerDeviceConnectionLost:
      *output =
          media_m96::VideoCaptureError::kVideoCaptureManagerDeviceConnectionLost;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kFrameSinkVideoCaptureDeviceAleradyEndedOnFatalError:
      *output = media_m96::VideoCaptureError::
          kFrameSinkVideoCaptureDeviceAleradyEndedOnFatalError;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kFrameSinkVideoCaptureDeviceEncounteredFatalError:
      *output = media_m96::VideoCaptureError::
          kFrameSinkVideoCaptureDeviceEncounteredFatalError;
      return true;
    case media_m96::mojom::VideoCaptureError::kV4L2FailedToOpenV4L2DeviceDriverFile:
      *output = media_m96::VideoCaptureError::kV4L2FailedToOpenV4L2DeviceDriverFile;
      return true;
    case media_m96::mojom::VideoCaptureError::kV4L2ThisIsNotAV4L2VideoCaptureDevice:
      *output = media_m96::VideoCaptureError::kV4L2ThisIsNotAV4L2VideoCaptureDevice;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kV4L2FailedToFindASupportedCameraFormat:
      *output =
          media_m96::VideoCaptureError::kV4L2FailedToFindASupportedCameraFormat;
      return true;
    case media_m96::mojom::VideoCaptureError::kV4L2FailedToSetVideoCaptureFormat:
      *output = media_m96::VideoCaptureError::kV4L2FailedToSetVideoCaptureFormat;
      return true;
    case media_m96::mojom::VideoCaptureError::kV4L2UnsupportedPixelFormat:
      *output = media_m96::VideoCaptureError::kV4L2UnsupportedPixelFormat;
      return true;
    case media_m96::mojom::VideoCaptureError::kV4L2FailedToSetCameraFramerate:
      *output = media_m96::VideoCaptureError::kV4L2FailedToSetCameraFramerate;
      return true;
    case media_m96::mojom::VideoCaptureError::kV4L2ErrorRequestingMmapBuffers:
      *output = media_m96::VideoCaptureError::kV4L2ErrorRequestingMmapBuffers;
      return true;
    case media_m96::mojom::VideoCaptureError::kV4L2AllocateBufferFailed:
      *output = media_m96::VideoCaptureError::kV4L2AllocateBufferFailed;
      return true;
    case media_m96::mojom::VideoCaptureError::kV4L2VidiocStreamonFailed:
      *output = media_m96::VideoCaptureError::kV4L2VidiocStreamonFailed;
      return true;
    case media_m96::mojom::VideoCaptureError::kV4L2VidiocStreamoffFailed:
      *output = media_m96::VideoCaptureError::kV4L2VidiocStreamoffFailed;
      return true;
    case media_m96::mojom::VideoCaptureError::kV4L2FailedToVidiocReqbufsWithCount0:
      *output = media_m96::VideoCaptureError::kV4L2FailedToVidiocReqbufsWithCount0;
      return true;
    case media_m96::mojom::VideoCaptureError::kV4L2PollFailed:
      *output = media_m96::VideoCaptureError::kV4L2PollFailed;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kV4L2MultipleContinuousTimeoutsWhileReadPolling:
      *output = media_m96::VideoCaptureError::
          kV4L2MultipleContinuousTimeoutsWhileReadPolling;
      return true;
    case media_m96::mojom::VideoCaptureError::kV4L2FailedToDequeueCaptureBuffer:
      *output = media_m96::VideoCaptureError::kV4L2FailedToDequeueCaptureBuffer;
      return true;
    case media_m96::mojom::VideoCaptureError::kV4L2FailedToEnqueueCaptureBuffer:
      *output = media_m96::VideoCaptureError::kV4L2FailedToEnqueueCaptureBuffer;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kSingleClientVideoCaptureHostLostConnectionToDevice:
      *output = media_m96::VideoCaptureError::
          kSingleClientVideoCaptureHostLostConnectionToDevice;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kSingleClientVideoCaptureDeviceLaunchAborted:
      *output = media_m96::VideoCaptureError::
          kSingleClientVideoCaptureDeviceLaunchAborted;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kDesktopCaptureDeviceWebrtcDesktopCapturerHasFailed:
      *output = media_m96::VideoCaptureError::
          kDesktopCaptureDeviceWebrtcDesktopCapturerHasFailed;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kFileVideoCaptureDeviceCouldNotOpenVideoFile:
      *output = media_m96::VideoCaptureError::
          kFileVideoCaptureDeviceCouldNotOpenVideoFile;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kDeviceCaptureLinuxFailedToCreateVideoCaptureDelegate:
      *output = media_m96::VideoCaptureError::
          kDeviceCaptureLinuxFailedToCreateVideoCaptureDelegate;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kErrorFakeDeviceIntentionallyEmittingErrorEvent:
      *output = media_m96::VideoCaptureError::
          kErrorFakeDeviceIntentionallyEmittingErrorEvent;
      return true;
    case media_m96::mojom::VideoCaptureError::kDeviceClientTooManyFramesDroppedY16:
      *output = media_m96::VideoCaptureError::kDeviceClientTooManyFramesDroppedY16;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kDeviceMediaToMojoAdapterEncounteredUnsupportedBufferType:
      *output = media_m96::VideoCaptureError::
          kDeviceMediaToMojoAdapterEncounteredUnsupportedBufferType;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kVideoCaptureManagerProcessDeviceStartQueueDeviceInfoNotFound:
      *output = media_m96::VideoCaptureError::
          kVideoCaptureManagerProcessDeviceStartQueueDeviceInfoNotFound;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kInProcessDeviceLauncherFailedToCreateDeviceInstance:
      *output = media_m96::VideoCaptureError::
          kInProcessDeviceLauncherFailedToCreateDeviceInstance;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kServiceDeviceLauncherLostConnectionToDeviceFactoryDuringDeviceStart:
      *output = media_m96::VideoCaptureError::
          kServiceDeviceLauncherLostConnectionToDeviceFactoryDuringDeviceStart;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kServiceDeviceLauncherServiceRespondedWithDeviceNotFound:
      *output = media_m96::VideoCaptureError::
          kServiceDeviceLauncherServiceRespondedWithDeviceNotFound;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kServiceDeviceLauncherConnectionLostWhileWaitingForCallback:
      *output = media_m96::VideoCaptureError::
          kServiceDeviceLauncherConnectionLostWhileWaitingForCallback;
      return true;
    case media_m96::mojom::VideoCaptureError::kIntentionalErrorRaisedByUnitTest:
      *output = media_m96::VideoCaptureError::kIntentionalErrorRaisedByUnitTest;
      return true;
    case media_m96::mojom::VideoCaptureError::kCrosHalV3FailedToStartDeviceThread:
      *output = media_m96::VideoCaptureError::kCrosHalV3FailedToStartDeviceThread;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3DeviceDelegateMojoConnectionError:
      *output =
          media_m96::VideoCaptureError::kCrosHalV3DeviceDelegateMojoConnectionError;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToGetCameraInfo:
      *output = media_m96::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToGetCameraInfo;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3DeviceDelegateMissingSensorOrientationInfo:
      *output = media_m96::VideoCaptureError::
          kCrosHalV3DeviceDelegateMissingSensorOrientationInfo;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToOpenCameraDevice:
      *output = media_m96::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToOpenCameraDevice;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToInitializeCameraDevice:
      *output = media_m96::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToInitializeCameraDevice;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToConfigureStreams:
      *output = media_m96::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToConfigureStreams;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3DeviceDelegateWrongNumberOfStreamsConfigured:
      *output = media_m96::VideoCaptureError::
          kCrosHalV3DeviceDelegateWrongNumberOfStreamsConfigured;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToGetDefaultRequestSettings:
      *output = media_m96::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToGetDefaultRequestSettings;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerHalRequestedTooManyBuffers:
      *output = media_m96::VideoCaptureError::
          kCrosHalV3BufferManagerHalRequestedTooManyBuffers;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToCreateGpuMemoryBuffer:
      *output = media_m96::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToCreateGpuMemoryBuffer;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToMapGpuMemoryBuffer:
      *output = media_m96::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToMapGpuMemoryBuffer;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerUnsupportedVideoPixelFormat:
      *output = media_m96::VideoCaptureError::
          kCrosHalV3BufferManagerUnsupportedVideoPixelFormat;
      return true;
    case media_m96::mojom::VideoCaptureError::kCrosHalV3BufferManagerFailedToDupFd:
      *output = media_m96::VideoCaptureError::kCrosHalV3BufferManagerFailedToDupFd;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToWrapGpuMemoryHandle:
      *output = media_m96::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToWrapGpuMemoryHandle;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToRegisterBuffer:
      *output = media_m96::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToRegisterBuffer;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerProcessCaptureRequestFailed:
      *output = media_m96::VideoCaptureError::
          kCrosHalV3BufferManagerProcessCaptureRequestFailed;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerInvalidPendingResultId:
      *output = media_m96::VideoCaptureError::
          kCrosHalV3BufferManagerInvalidPendingResultId;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerReceivedDuplicatedPartialMetadata:
      *output = media_m96::VideoCaptureError::
          kCrosHalV3BufferManagerReceivedDuplicatedPartialMetadata;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerIncorrectNumberOfOutputBuffersReceived:
      *output = media_m96::VideoCaptureError::
          kCrosHalV3BufferManagerIncorrectNumberOfOutputBuffersReceived;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerInvalidTypeOfOutputBuffersReceived:
      *output = media_m96::VideoCaptureError::
          kCrosHalV3BufferManagerInvalidTypeOfOutputBuffersReceived;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerReceivedMultipleResultBuffersForFrame:
      *output = media_m96::VideoCaptureError::
          kCrosHalV3BufferManagerReceivedMultipleResultBuffersForFrame;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerUnknownStreamInCamera3NotifyMsg:
      *output = media_m96::VideoCaptureError::
          kCrosHalV3BufferManagerUnknownStreamInCamera3NotifyMsg;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerReceivedInvalidShutterTime:
      *output = media_m96::VideoCaptureError::
          kCrosHalV3BufferManagerReceivedInvalidShutterTime;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerFatalDeviceError:
      *output =
          media_m96::VideoCaptureError::kCrosHalV3BufferManagerFatalDeviceError;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerReceivedFrameIsOutOfOrder:
      *output = media_m96::VideoCaptureError::
          kCrosHalV3BufferManagerReceivedFrameIsOutOfOrder;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToUnwrapReleaseFenceFd:
      *output = media_m96::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToUnwrapReleaseFenceFd;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerSyncWaitOnReleaseFenceTimedOut:
      *output = media_m96::VideoCaptureError::
          kCrosHalV3BufferManagerSyncWaitOnReleaseFenceTimedOut;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerInvalidJpegBlob:
      *output =
          media_m96::VideoCaptureError::kCrosHalV3BufferManagerInvalidJpegBlob;
      return true;
    case media_m96::mojom::VideoCaptureError::kAndroidFailedToAllocate:
      *output = media_m96::VideoCaptureError::kAndroidFailedToAllocate;
      return true;
    case media_m96::mojom::VideoCaptureError::kAndroidFailedToStartCapture:
      *output = media_m96::VideoCaptureError::kAndroidFailedToStartCapture;
      return true;
    case media_m96::mojom::VideoCaptureError::kAndroidFailedToStopCapture:
      *output = media_m96::VideoCaptureError::kAndroidFailedToStopCapture;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kAndroidApi1CameraErrorCallbackReceived:
      *output =
          media_m96::VideoCaptureError::kAndroidApi1CameraErrorCallbackReceived;
      return true;
    case media_m96::mojom::VideoCaptureError::kAndroidApi2CameraDeviceErrorReceived:
      *output = media_m96::VideoCaptureError::kAndroidApi2CameraDeviceErrorReceived;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kAndroidApi2CaptureSessionConfigureFailed:
      *output =
          media_m96::VideoCaptureError::kAndroidApi2CaptureSessionConfigureFailed;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kAndroidApi2ImageReaderUnexpectedImageFormat:
      *output = media_m96::VideoCaptureError::
          kAndroidApi2ImageReaderUnexpectedImageFormat;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kAndroidApi2ImageReaderSizeDidNotMatchImageSize:
      *output = media_m96::VideoCaptureError::
          kAndroidApi2ImageReaderSizeDidNotMatchImageSize;
      return true;
    case media_m96::mojom::VideoCaptureError::kAndroidApi2ErrorRestartingPreview:
      *output = media_m96::VideoCaptureError::kAndroidApi2ErrorRestartingPreview;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kAndroidScreenCaptureUnsupportedFormat:
      *output =
          media_m96::VideoCaptureError::kAndroidScreenCaptureUnsupportedFormat;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kAndroidScreenCaptureFailedToStartCaptureMachine:
      *output = media_m96::VideoCaptureError::
          kAndroidScreenCaptureFailedToStartCaptureMachine;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kAndroidScreenCaptureTheUserDeniedScreenCapture:
      *output = media_m96::VideoCaptureError::
          kAndroidScreenCaptureTheUserDeniedScreenCapture;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kAndroidScreenCaptureFailedToStartScreenCapture:
      *output = media_m96::VideoCaptureError::
          kAndroidScreenCaptureFailedToStartScreenCapture;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kWinDirectShowCantGetCaptureFormatSettings:
      *output =
          media_m96::VideoCaptureError::kWinDirectShowCantGetCaptureFormatSettings;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kWinDirectShowFailedToGetNumberOfCapabilities:
      *output = media_m96::VideoCaptureError::
          kWinDirectShowFailedToGetNumberOfCapabilities;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kWinDirectShowFailedToGetCaptureDeviceCapabilities:
      *output = media_m96::VideoCaptureError::
          kWinDirectShowFailedToGetCaptureDeviceCapabilities;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kWinDirectShowFailedToSetCaptureDeviceOutputFormat:
      *output = media_m96::VideoCaptureError::
          kWinDirectShowFailedToSetCaptureDeviceOutputFormat;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kWinDirectShowFailedToConnectTheCaptureGraph:
      *output = media_m96::VideoCaptureError::
          kWinDirectShowFailedToConnectTheCaptureGraph;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kWinDirectShowFailedToPauseTheCaptureDevice:
      *output =
          media_m96::VideoCaptureError::kWinDirectShowFailedToPauseTheCaptureDevice;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kWinDirectShowFailedToStartTheCaptureDevice:
      *output =
          media_m96::VideoCaptureError::kWinDirectShowFailedToStartTheCaptureDevice;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kWinDirectShowFailedToStopTheCaptureGraph:
      *output =
          media_m96::VideoCaptureError::kWinDirectShowFailedToStopTheCaptureGraph;
      return true;
    case media_m96::mojom::VideoCaptureError::kWinMediaFoundationEngineIsNull:
      *output = media_m96::VideoCaptureError::kWinMediaFoundationEngineIsNull;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kWinMediaFoundationEngineGetSourceFailed:
      *output =
          media_m96::VideoCaptureError::kWinMediaFoundationEngineGetSourceFailed;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kWinMediaFoundationFillPhotoCapabilitiesFailed:
      *output = media_m96::VideoCaptureError::
          kWinMediaFoundationFillPhotoCapabilitiesFailed;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kWinMediaFoundationFillVideoCapabilitiesFailed:
      *output = media_m96::VideoCaptureError::
          kWinMediaFoundationFillVideoCapabilitiesFailed;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kWinMediaFoundationNoVideoCapabilityFound:
      *output =
          media_m96::VideoCaptureError::kWinMediaFoundationNoVideoCapabilityFound;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kWinMediaFoundationGetAvailableDeviceMediaTypeFailed:
      *output = media_m96::VideoCaptureError::
          kWinMediaFoundationGetAvailableDeviceMediaTypeFailed;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kWinMediaFoundationSetCurrentDeviceMediaTypeFailed:
      *output = media_m96::VideoCaptureError::
          kWinMediaFoundationSetCurrentDeviceMediaTypeFailed;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kWinMediaFoundationEngineGetSinkFailed:
      *output =
          media_m96::VideoCaptureError::kWinMediaFoundationEngineGetSinkFailed;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kWinMediaFoundationSinkQueryCapturePreviewInterfaceFailed:
      *output = media_m96::VideoCaptureError::
          kWinMediaFoundationSinkQueryCapturePreviewInterfaceFailed;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kWinMediaFoundationSinkRemoveAllStreamsFailed:
      *output = media_m96::VideoCaptureError::
          kWinMediaFoundationSinkRemoveAllStreamsFailed;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kWinMediaFoundationCreateSinkVideoMediaTypeFailed:
      *output = media_m96::VideoCaptureError::
          kWinMediaFoundationCreateSinkVideoMediaTypeFailed;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kWinMediaFoundationConvertToVideoSinkMediaTypeFailed:
      *output = media_m96::VideoCaptureError::
          kWinMediaFoundationConvertToVideoSinkMediaTypeFailed;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kWinMediaFoundationSinkAddStreamFailed:
      *output =
          media_m96::VideoCaptureError::kWinMediaFoundationSinkAddStreamFailed;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kWinMediaFoundationSinkSetSampleCallbackFailed:
      *output = media_m96::VideoCaptureError::
          kWinMediaFoundationSinkSetSampleCallbackFailed;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kWinMediaFoundationEngineStartPreviewFailed:
      *output =
          media_m96::VideoCaptureError::kWinMediaFoundationEngineStartPreviewFailed;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kWinMediaFoundationGetMediaEventStatusFailed:
      *output = media_m96::VideoCaptureError::
          kWinMediaFoundationGetMediaEventStatusFailed;
      return true;
    case media_m96::mojom::VideoCaptureError::kMacSetCaptureDeviceFailed:
      *output = media_m96::VideoCaptureError::kMacSetCaptureDeviceFailed;
      return true;
    case media_m96::mojom::VideoCaptureError::kMacCouldNotStartCaptureDevice:
      *output = media_m96::VideoCaptureError::kMacCouldNotStartCaptureDevice;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kMacReceivedFrameWithUnexpectedResolution:
      *output =
          media_m96::VideoCaptureError::kMacReceivedFrameWithUnexpectedResolution;
      return true;
    case media_m96::mojom::VideoCaptureError::kMacUpdateCaptureResolutionFailed:
      *output = media_m96::VideoCaptureError::kMacUpdateCaptureResolutionFailed;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kMacDeckLinkDeviceIdNotFoundInTheSystem:
      *output =
          media_m96::VideoCaptureError::kMacDeckLinkDeviceIdNotFoundInTheSystem;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kMacDeckLinkErrorQueryingInputInterface:
      *output =
          media_m96::VideoCaptureError::kMacDeckLinkErrorQueryingInputInterface;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kMacDeckLinkErrorCreatingDisplayModeIterator:
      *output = media_m96::VideoCaptureError::
          kMacDeckLinkErrorCreatingDisplayModeIterator;
      return true;
    case media_m96::mojom::VideoCaptureError::kMacDeckLinkCouldNotFindADisplayMode:
      *output = media_m96::VideoCaptureError::kMacDeckLinkCouldNotFindADisplayMode;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kMacDeckLinkCouldNotSelectTheVideoFormatWeLike:
      *output = media_m96::VideoCaptureError::
          kMacDeckLinkCouldNotSelectTheVideoFormatWeLike;
      return true;
    case media_m96::mojom::VideoCaptureError::kMacDeckLinkCouldNotStartCapturing:
      *output = media_m96::VideoCaptureError::kMacDeckLinkCouldNotStartCapturing;
      return true;
    case media_m96::mojom::VideoCaptureError::kMacDeckLinkUnsupportedPixelFormat:
      *output = media_m96::VideoCaptureError::kMacDeckLinkUnsupportedPixelFormat;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kMacAvFoundationReceivedAVCaptureSessionRuntimeErrorNotification:
      *output = media_m96::VideoCaptureError::
          kMacAvFoundationReceivedAVCaptureSessionRuntimeErrorNotification;
      return true;
    case media_m96::mojom::VideoCaptureError::kAndroidApi2ErrorConfiguringCamera:
      *output = media_m96::VideoCaptureError::kAndroidApi2ErrorConfiguringCamera;
      return true;
    case media_m96::mojom::VideoCaptureError::kCrosHalV3DeviceDelegateFailedToFlush:
      *output = media_m96::VideoCaptureError::kCrosHalV3DeviceDelegateFailedToFlush;
      return true;
    case media_m96::mojom::VideoCaptureError::kFuchsiaCameraDeviceDisconnected:
      *output = media_m96::VideoCaptureError::kFuchsiaCameraDeviceDisconnected;
      return true;
    case media_m96::mojom::VideoCaptureError::kFuchsiaCameraStreamDisconnected:
      *output = media_m96::VideoCaptureError::kFuchsiaCameraStreamDisconnected;
      return true;
    case media_m96::mojom::VideoCaptureError::kFuchsiaSysmemDidNotSetImageFormat:
      *output = media_m96::VideoCaptureError::kFuchsiaSysmemDidNotSetImageFormat;
      return true;
    case media_m96::mojom::VideoCaptureError::kFuchsiaSysmemInvalidBufferIndex:
      *output = media_m96::VideoCaptureError::kFuchsiaSysmemInvalidBufferIndex;
      return true;
    case media_m96::mojom::VideoCaptureError::kFuchsiaSysmemInvalidBufferSize:
      *output = media_m96::VideoCaptureError::kFuchsiaSysmemInvalidBufferSize;
      return true;
    case media_m96::mojom::VideoCaptureError::kFuchsiaUnsupportedPixelFormat:
      *output = media_m96::VideoCaptureError::kFuchsiaUnsupportedPixelFormat;
      return true;
    case media_m96::mojom::VideoCaptureError::kFuchsiaFailedToMapSysmemBuffer:
      *output = media_m96::VideoCaptureError::kFuchsiaFailedToMapSysmemBuffer;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3DeviceContextDuplicatedClient:
      *output =
          media_m96::VideoCaptureError::kCrosHalV3DeviceContextDuplicatedClient;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kDesktopCaptureDeviceMacFailedStreamCreate:
      *output =
          media_m96::VideoCaptureError::kDesktopCaptureDeviceMacFailedStreamCreate;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kDesktopCaptureDeviceMacFailedStreamStart:
      *output =
          media_m96::VideoCaptureError::kDesktopCaptureDeviceMacFailedStreamStart;
      return true;
    case media_m96::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToReserveBuffers:
      *output = media_m96::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToReserveBuffers;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
media_m96::mojom::VideoCaptureFrameDropReason
EnumTraits<media_m96::mojom::VideoCaptureFrameDropReason,
           media_m96::VideoCaptureFrameDropReason>::
    ToMojom(media_m96::VideoCaptureFrameDropReason input) {
  switch (input) {
    case media_m96::VideoCaptureFrameDropReason::kNone:
      return media_m96::mojom::VideoCaptureFrameDropReason::kNone;
    case media_m96::VideoCaptureFrameDropReason::kDeviceClientFrameHasInvalidFormat:
      return media_m96::mojom::VideoCaptureFrameDropReason::
          kDeviceClientFrameHasInvalidFormat;
    case media_m96::VideoCaptureFrameDropReason::
        kDeviceClientLibyuvConvertToI420Failed:
      return media_m96::mojom::VideoCaptureFrameDropReason::
          kDeviceClientLibyuvConvertToI420Failed;
    case media_m96::VideoCaptureFrameDropReason::kV4L2BufferErrorFlagWasSet:
      return media_m96::mojom::VideoCaptureFrameDropReason::
          kV4L2BufferErrorFlagWasSet;
    case media_m96::VideoCaptureFrameDropReason::kV4L2InvalidNumberOfBytesInBuffer:
      return media_m96::mojom::VideoCaptureFrameDropReason::
          kV4L2InvalidNumberOfBytesInBuffer;
    case media_m96::VideoCaptureFrameDropReason::kAndroidThrottling:
      return media_m96::mojom::VideoCaptureFrameDropReason::kAndroidThrottling;
    case media_m96::VideoCaptureFrameDropReason::kAndroidGetByteArrayElementsFailed:
      return media_m96::mojom::VideoCaptureFrameDropReason::
          kAndroidGetByteArrayElementsFailed;
    case media_m96::VideoCaptureFrameDropReason::kAndroidApi1UnexpectedDataLength:
      return media_m96::mojom::VideoCaptureFrameDropReason::
          kAndroidApi1UnexpectedDataLength;
    case media_m96::VideoCaptureFrameDropReason::kAndroidApi2AcquiredImageIsNull:
      return media_m96::mojom::VideoCaptureFrameDropReason::
          kAndroidApi2AcquiredImageIsNull;
    case media_m96::VideoCaptureFrameDropReason::
        kWinDirectShowUnexpectedSampleLength:
      return media_m96::mojom::VideoCaptureFrameDropReason::
          kWinDirectShowUnexpectedSampleLength;
    case media_m96::VideoCaptureFrameDropReason::
        kWinDirectShowFailedToGetMemoryPointerFromMediaSample:
      return media_m96::mojom::VideoCaptureFrameDropReason::
          kWinDirectShowFailedToGetMemoryPointerFromMediaSample;
    case media_m96::VideoCaptureFrameDropReason::
        kWinMediaFoundationReceivedSampleIsNull:
      return media_m96::mojom::VideoCaptureFrameDropReason::
          kWinMediaFoundationReceivedSampleIsNull;
    case media_m96::VideoCaptureFrameDropReason::
        kWinMediaFoundationLockingBufferDelieveredNullptr:
      return media_m96::mojom::VideoCaptureFrameDropReason::
          kWinMediaFoundationLockingBufferDelieveredNullptr;
    case media_m96::VideoCaptureFrameDropReason::
        kWinMediaFoundationGetBufferByIndexReturnedNull:
      return media_m96::mojom::VideoCaptureFrameDropReason::
          kWinMediaFoundationGetBufferByIndexReturnedNull;
    case media_m96::VideoCaptureFrameDropReason::kBufferPoolMaxBufferCountExceeded:
      return media_m96::mojom::VideoCaptureFrameDropReason::
          kBufferPoolMaxBufferCountExceeded;
    case media_m96::VideoCaptureFrameDropReason::kBufferPoolBufferAllocationFailed:
      return media_m96::mojom::VideoCaptureFrameDropReason::
          kBufferPoolBufferAllocationFailed;
    case media_m96::VideoCaptureFrameDropReason::kVideoCaptureImplNotInStartedState:
      return media_m96::mojom::VideoCaptureFrameDropReason::
          kVideoCaptureImplNotInStartedState;
    case media_m96::VideoCaptureFrameDropReason::
        kVideoCaptureImplFailedToWrapDataAsMediaVideoFrame:
      return media_m96::mojom::VideoCaptureFrameDropReason::
          kVideoCaptureImplFailedToWrapDataAsMediaVideoFrame;
    case media_m96::VideoCaptureFrameDropReason::
        kVideoTrackAdapterHasNoResolutionAdapters:
      return media_m96::mojom::VideoCaptureFrameDropReason::
          kVideoTrackAdapterHasNoResolutionAdapters;
    case media_m96::VideoCaptureFrameDropReason::kResolutionAdapterFrameIsNotValid:
      return media_m96::mojom::VideoCaptureFrameDropReason::
          kResolutionAdapterFrameIsNotValid;
    case media_m96::VideoCaptureFrameDropReason::
        kResolutionAdapterWrappingFrameForCroppingFailed:
      return media_m96::mojom::VideoCaptureFrameDropReason::
          kResolutionAdapterWrappingFrameForCroppingFailed;
    case media_m96::VideoCaptureFrameDropReason::
        kResolutionAdapterTimestampTooCloseToPrevious:
      return media_m96::mojom::VideoCaptureFrameDropReason::
          kResolutionAdapterTimestampTooCloseToPrevious;
    case media_m96::VideoCaptureFrameDropReason::
        kResolutionAdapterFrameRateIsHigherThanRequested:
      return media_m96::mojom::VideoCaptureFrameDropReason::
          kResolutionAdapterFrameRateIsHigherThanRequested;
    case media_m96::VideoCaptureFrameDropReason::kResolutionAdapterHasNoCallbacks:
      return media_m96::mojom::VideoCaptureFrameDropReason::
          kResolutionAdapterHasNoCallbacks;
    case media_m96::VideoCaptureFrameDropReason::
        kVideoTrackFrameDelivererNotEnabledReplacingWithBlackFrame:
      return media_m96::mojom::VideoCaptureFrameDropReason::
          kVideoTrackFrameDelivererNotEnabledReplacingWithBlackFrame;
    case media_m96::VideoCaptureFrameDropReason::
        kRendererSinkFrameDelivererIsNotStarted:
      return media_m96::mojom::VideoCaptureFrameDropReason::
          kRendererSinkFrameDelivererIsNotStarted;
  }
  NOTREACHED();
  return media_m96::mojom::VideoCaptureFrameDropReason::kNone;
}

// static
bool EnumTraits<media_m96::mojom::VideoCaptureFrameDropReason,
                media_m96::VideoCaptureFrameDropReason>::
    FromMojom(media_m96::mojom::VideoCaptureFrameDropReason input,
              media_m96::VideoCaptureFrameDropReason* output) {
  switch (input) {
    case media_m96::mojom::VideoCaptureFrameDropReason::kNone:
      *output = media_m96::VideoCaptureFrameDropReason::kNone;
      return true;
    case media_m96::mojom::VideoCaptureFrameDropReason::
        kDeviceClientFrameHasInvalidFormat:
      *output = media_m96::VideoCaptureFrameDropReason::
          kDeviceClientFrameHasInvalidFormat;
      return true;
    case media_m96::mojom::VideoCaptureFrameDropReason::
        kDeviceClientLibyuvConvertToI420Failed:
      *output = media_m96::VideoCaptureFrameDropReason::
          kDeviceClientLibyuvConvertToI420Failed;
      return true;
    case media_m96::mojom::VideoCaptureFrameDropReason::kV4L2BufferErrorFlagWasSet:
      *output = media_m96::VideoCaptureFrameDropReason::kV4L2BufferErrorFlagWasSet;
      return true;
    case media_m96::mojom::VideoCaptureFrameDropReason::
        kV4L2InvalidNumberOfBytesInBuffer:
      *output =
          media_m96::VideoCaptureFrameDropReason::kV4L2InvalidNumberOfBytesInBuffer;
      return true;
    case media_m96::mojom::VideoCaptureFrameDropReason::kAndroidThrottling:
      *output = media_m96::VideoCaptureFrameDropReason::kAndroidThrottling;
      return true;
    case media_m96::mojom::VideoCaptureFrameDropReason::
        kAndroidGetByteArrayElementsFailed:
      *output = media_m96::VideoCaptureFrameDropReason::
          kAndroidGetByteArrayElementsFailed;
      return true;
    case media_m96::mojom::VideoCaptureFrameDropReason::
        kAndroidApi1UnexpectedDataLength:
      *output =
          media_m96::VideoCaptureFrameDropReason::kAndroidApi1UnexpectedDataLength;
      return true;
    case media_m96::mojom::VideoCaptureFrameDropReason::
        kAndroidApi2AcquiredImageIsNull:
      *output =
          media_m96::VideoCaptureFrameDropReason::kAndroidApi2AcquiredImageIsNull;
      return true;
    case media_m96::mojom::VideoCaptureFrameDropReason::
        kWinDirectShowUnexpectedSampleLength:
      *output = media_m96::VideoCaptureFrameDropReason::
          kWinDirectShowUnexpectedSampleLength;
      return true;
    case media_m96::mojom::VideoCaptureFrameDropReason::
        kWinDirectShowFailedToGetMemoryPointerFromMediaSample:
      *output = media_m96::VideoCaptureFrameDropReason::
          kWinDirectShowFailedToGetMemoryPointerFromMediaSample;
      return true;
    case media_m96::mojom::VideoCaptureFrameDropReason::
        kWinMediaFoundationReceivedSampleIsNull:
      *output = media_m96::VideoCaptureFrameDropReason::
          kWinMediaFoundationReceivedSampleIsNull;
      return true;
    case media_m96::mojom::VideoCaptureFrameDropReason::
        kWinMediaFoundationLockingBufferDelieveredNullptr:
      *output = media_m96::VideoCaptureFrameDropReason::
          kWinMediaFoundationLockingBufferDelieveredNullptr;
      return true;
    case media_m96::mojom::VideoCaptureFrameDropReason::
        kWinMediaFoundationGetBufferByIndexReturnedNull:
      *output = media_m96::VideoCaptureFrameDropReason::
          kWinMediaFoundationGetBufferByIndexReturnedNull;
      return true;
    case media_m96::mojom::VideoCaptureFrameDropReason::
        kBufferPoolMaxBufferCountExceeded:
      *output =
          media_m96::VideoCaptureFrameDropReason::kBufferPoolMaxBufferCountExceeded;
      return true;
    case media_m96::mojom::VideoCaptureFrameDropReason::
        kBufferPoolBufferAllocationFailed:
      *output =
          media_m96::VideoCaptureFrameDropReason::kBufferPoolBufferAllocationFailed;
      return true;
    case media_m96::mojom::VideoCaptureFrameDropReason::
        kVideoCaptureImplNotInStartedState:
      *output = media_m96::VideoCaptureFrameDropReason::
          kVideoCaptureImplNotInStartedState;
      return true;
    case media_m96::mojom::VideoCaptureFrameDropReason::
        kVideoCaptureImplFailedToWrapDataAsMediaVideoFrame:
      *output = media_m96::VideoCaptureFrameDropReason::
          kVideoCaptureImplFailedToWrapDataAsMediaVideoFrame;
      return true;
    case media_m96::mojom::VideoCaptureFrameDropReason::
        kVideoTrackAdapterHasNoResolutionAdapters:
      *output = media_m96::VideoCaptureFrameDropReason::
          kVideoTrackAdapterHasNoResolutionAdapters;
      return true;
    case media_m96::mojom::VideoCaptureFrameDropReason::
        kResolutionAdapterFrameIsNotValid:
      *output =
          media_m96::VideoCaptureFrameDropReason::kResolutionAdapterFrameIsNotValid;
      return true;
    case media_m96::mojom::VideoCaptureFrameDropReason::
        kResolutionAdapterWrappingFrameForCroppingFailed:
      *output = media_m96::VideoCaptureFrameDropReason::
          kResolutionAdapterWrappingFrameForCroppingFailed;
      return true;
    case media_m96::mojom::VideoCaptureFrameDropReason::
        kResolutionAdapterTimestampTooCloseToPrevious:
      *output = media_m96::VideoCaptureFrameDropReason::
          kResolutionAdapterTimestampTooCloseToPrevious;
      return true;
    case media_m96::mojom::VideoCaptureFrameDropReason::
        kResolutionAdapterFrameRateIsHigherThanRequested:
      *output = media_m96::VideoCaptureFrameDropReason::
          kResolutionAdapterFrameRateIsHigherThanRequested;
      return true;
    case media_m96::mojom::VideoCaptureFrameDropReason::
        kResolutionAdapterHasNoCallbacks:
      *output =
          media_m96::VideoCaptureFrameDropReason::kResolutionAdapterHasNoCallbacks;
      return true;
    case media_m96::mojom::VideoCaptureFrameDropReason::
        kVideoTrackFrameDelivererNotEnabledReplacingWithBlackFrame:
      *output = media_m96::VideoCaptureFrameDropReason::
          kVideoTrackFrameDelivererNotEnabledReplacingWithBlackFrame;
      return true;
    case media_m96::mojom::VideoCaptureFrameDropReason::
        kRendererSinkFrameDelivererIsNotStarted:
      *output = media_m96::VideoCaptureFrameDropReason::
          kRendererSinkFrameDelivererIsNotStarted;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
media_m96::mojom::VideoFacingMode
EnumTraits<media_m96::mojom::VideoFacingMode, media_m96::VideoFacingMode>::ToMojom(
    media_m96::VideoFacingMode input) {
  switch (input) {
    case media_m96::VideoFacingMode::MEDIA_VIDEO_FACING_NONE:
      return media_m96::mojom::VideoFacingMode::NONE;
    case media_m96::VideoFacingMode::MEDIA_VIDEO_FACING_USER:
      return media_m96::mojom::VideoFacingMode::USER;
    case media_m96::VideoFacingMode::MEDIA_VIDEO_FACING_ENVIRONMENT:
      return media_m96::mojom::VideoFacingMode::ENVIRONMENT;
    case media_m96::VideoFacingMode::NUM_MEDIA_VIDEO_FACING_MODES:
      NOTREACHED();
      return media_m96::mojom::VideoFacingMode::NONE;
  }
  NOTREACHED();
  return media_m96::mojom::VideoFacingMode::NONE;
}

// static
bool EnumTraits<media_m96::mojom::VideoFacingMode, media_m96::VideoFacingMode>::
    FromMojom(media_m96::mojom::VideoFacingMode input,
              media_m96::VideoFacingMode* output) {
  switch (input) {
    case media_m96::mojom::VideoFacingMode::NONE:
      *output = media_m96::VideoFacingMode::MEDIA_VIDEO_FACING_NONE;
      return true;
    case media_m96::mojom::VideoFacingMode::USER:
      *output = media_m96::VideoFacingMode::MEDIA_VIDEO_FACING_USER;
      return true;
    case media_m96::mojom::VideoFacingMode::ENVIRONMENT:
      *output = media_m96::VideoFacingMode::MEDIA_VIDEO_FACING_ENVIRONMENT;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
media_m96::mojom::VideoCaptureApi
EnumTraits<media_m96::mojom::VideoCaptureApi, media_m96::VideoCaptureApi>::ToMojom(
    media_m96::VideoCaptureApi input) {
  switch (input) {
    case media_m96::VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE:
      return media_m96::mojom::VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE;
    case media_m96::VideoCaptureApi::WIN_MEDIA_FOUNDATION:
      return media_m96::mojom::VideoCaptureApi::WIN_MEDIA_FOUNDATION;
    case media_m96::VideoCaptureApi::WIN_MEDIA_FOUNDATION_SENSOR:
      return media_m96::mojom::VideoCaptureApi::WIN_MEDIA_FOUNDATION_SENSOR;
    case media_m96::VideoCaptureApi::WIN_DIRECT_SHOW:
      return media_m96::mojom::VideoCaptureApi::WIN_DIRECT_SHOW;
    case media_m96::VideoCaptureApi::MACOSX_AVFOUNDATION:
      return media_m96::mojom::VideoCaptureApi::MACOSX_AVFOUNDATION;
    case media_m96::VideoCaptureApi::MACOSX_DECKLINK:
      return media_m96::mojom::VideoCaptureApi::MACOSX_DECKLINK;
    case media_m96::VideoCaptureApi::ANDROID_API1:
      return media_m96::mojom::VideoCaptureApi::ANDROID_API1;
    case media_m96::VideoCaptureApi::ANDROID_API2_LEGACY:
      return media_m96::mojom::VideoCaptureApi::ANDROID_API2_LEGACY;
    case media_m96::VideoCaptureApi::ANDROID_API2_FULL:
      return media_m96::mojom::VideoCaptureApi::ANDROID_API2_FULL;
    case media_m96::VideoCaptureApi::ANDROID_API2_LIMITED:
      return media_m96::mojom::VideoCaptureApi::ANDROID_API2_LIMITED;
    case media_m96::VideoCaptureApi::FUCHSIA_CAMERA3:
      return media_m96::mojom::VideoCaptureApi::FUCHSIA_CAMERA3;
    case media_m96::VideoCaptureApi::VIRTUAL_DEVICE:
      return media_m96::mojom::VideoCaptureApi::VIRTUAL_DEVICE;
    case media_m96::VideoCaptureApi::UNKNOWN:
      return media_m96::mojom::VideoCaptureApi::UNKNOWN;
  }
  NOTREACHED();
  return media_m96::mojom::VideoCaptureApi::UNKNOWN;
}

// static
bool EnumTraits<media_m96::mojom::VideoCaptureApi, media_m96::VideoCaptureApi>::
    FromMojom(media_m96::mojom::VideoCaptureApi input,
              media_m96::VideoCaptureApi* output) {
  switch (input) {
    case media_m96::mojom::VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE:
      *output = media_m96::VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE;
      return true;
    case media_m96::mojom::VideoCaptureApi::WIN_MEDIA_FOUNDATION:
      *output = media_m96::VideoCaptureApi::WIN_MEDIA_FOUNDATION;
      return true;
    case media_m96::mojom::VideoCaptureApi::WIN_MEDIA_FOUNDATION_SENSOR:
      *output = media_m96::VideoCaptureApi::WIN_MEDIA_FOUNDATION_SENSOR;
      return true;
    case media_m96::mojom::VideoCaptureApi::WIN_DIRECT_SHOW:
      *output = media_m96::VideoCaptureApi::WIN_DIRECT_SHOW;
      return true;
    case media_m96::mojom::VideoCaptureApi::MACOSX_AVFOUNDATION:
      *output = media_m96::VideoCaptureApi::MACOSX_AVFOUNDATION;
      return true;
    case media_m96::mojom::VideoCaptureApi::MACOSX_DECKLINK:
      *output = media_m96::VideoCaptureApi::MACOSX_DECKLINK;
      return true;
    case media_m96::mojom::VideoCaptureApi::ANDROID_API1:
      *output = media_m96::VideoCaptureApi::ANDROID_API1;
      return true;
    case media_m96::mojom::VideoCaptureApi::ANDROID_API2_LEGACY:
      *output = media_m96::VideoCaptureApi::ANDROID_API2_LEGACY;
      return true;
    case media_m96::mojom::VideoCaptureApi::ANDROID_API2_FULL:
      *output = media_m96::VideoCaptureApi::ANDROID_API2_FULL;
      return true;
    case media_m96::mojom::VideoCaptureApi::ANDROID_API2_LIMITED:
      *output = media_m96::VideoCaptureApi::ANDROID_API2_LIMITED;
      return true;
    case media_m96::mojom::VideoCaptureApi::FUCHSIA_CAMERA3:
      *output = media_m96::VideoCaptureApi::FUCHSIA_CAMERA3;
      return true;
    case media_m96::mojom::VideoCaptureApi::VIRTUAL_DEVICE:
      *output = media_m96::VideoCaptureApi::VIRTUAL_DEVICE;
      return true;
    case media_m96::mojom::VideoCaptureApi::UNKNOWN:
      *output = media_m96::VideoCaptureApi::UNKNOWN;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
media_m96::mojom::VideoCaptureTransportType EnumTraits<
    media_m96::mojom::VideoCaptureTransportType,
    media_m96::VideoCaptureTransportType>::ToMojom(media_m96::VideoCaptureTransportType
                                                   input) {
  switch (input) {
    case media_m96::VideoCaptureTransportType::MACOSX_USB_OR_BUILT_IN:
      return media_m96::mojom::VideoCaptureTransportType::MACOSX_USB_OR_BUILT_IN;
    case media_m96::VideoCaptureTransportType::OTHER_TRANSPORT:
      return media_m96::mojom::VideoCaptureTransportType::OTHER_TRANSPORT;
  }
  NOTREACHED();
  return media_m96::mojom::VideoCaptureTransportType::OTHER_TRANSPORT;
}

// static
bool EnumTraits<media_m96::mojom::VideoCaptureTransportType,
                media_m96::VideoCaptureTransportType>::
    FromMojom(media_m96::mojom::VideoCaptureTransportType input,
              media_m96::VideoCaptureTransportType* output) {
  switch (input) {
    case media_m96::mojom::VideoCaptureTransportType::MACOSX_USB_OR_BUILT_IN:
      *output = media_m96::VideoCaptureTransportType::MACOSX_USB_OR_BUILT_IN;
      return true;
    case media_m96::mojom::VideoCaptureTransportType::OTHER_TRANSPORT:
      *output = media_m96::VideoCaptureTransportType::OTHER_TRANSPORT;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
bool StructTraits<media_m96::mojom::VideoCaptureControlSupportDataView,
                  media_m96::VideoCaptureControlSupport>::
    Read(media_m96::mojom::VideoCaptureControlSupportDataView data,
         media_m96::VideoCaptureControlSupport* out) {
  out->pan = data.pan();
  out->tilt = data.tilt();
  out->zoom = data.zoom();
  return true;
}

// static
bool StructTraits<media_m96::mojom::VideoCaptureFormatDataView,
                  media_m96::VideoCaptureFormat>::
    Read(media_m96::mojom::VideoCaptureFormatDataView data,
         media_m96::VideoCaptureFormat* out) {
  if (!data.ReadFrameSize(&out->frame_size))
    return false;
  out->frame_rate = data.frame_rate();
  if (!data.ReadPixelFormat(&out->pixel_format))
    return false;
  return true;
}

// static
bool StructTraits<media_m96::mojom::VideoCaptureParamsDataView,
                  media_m96::VideoCaptureParams>::
    Read(media_m96::mojom::VideoCaptureParamsDataView data,
         media_m96::VideoCaptureParams* out) {
  if (!data.ReadRequestedFormat(&out->requested_format))
    return false;
  if (!data.ReadBufferType(&out->buffer_type))
    return false;
  if (!data.ReadResolutionChangePolicy(&out->resolution_change_policy))
    return false;
  if (!data.ReadPowerLineFrequency(&out->power_line_frequency))
    return false;
  out->enable_face_detection = data.enable_face_detection();
  return true;
}

// static
bool StructTraits<media_m96::mojom::VideoCaptureDeviceDescriptorDataView,
                  media_m96::VideoCaptureDeviceDescriptor>::
    Read(media_m96::mojom::VideoCaptureDeviceDescriptorDataView data,
         media_m96::VideoCaptureDeviceDescriptor* output) {
  std::string display_name;
  if (!data.ReadDisplayName(&display_name))
    return false;
  output->set_display_name(display_name);
  if (!data.ReadDeviceId(&(output->device_id)))
    return false;
  if (!data.ReadModelId(&(output->model_id)))
    return false;
  if (!data.ReadFacingMode(&(output->facing)))
    return false;
  if (!data.ReadCaptureApi(&(output->capture_api)))
    return false;
  media_m96::VideoCaptureControlSupport control_support;
  if (!data.ReadControlSupport(&control_support))
    return false;
  output->set_control_support(control_support);
  if (!data.ReadTransportType(&(output->transport_type)))
    return false;
  return true;
}

// static
bool StructTraits<media_m96::mojom::VideoCaptureDeviceInfoDataView,
                  media_m96::VideoCaptureDeviceInfo>::
    Read(media_m96::mojom::VideoCaptureDeviceInfoDataView data,
         media_m96::VideoCaptureDeviceInfo* output) {
  if (!data.ReadDescriptor(&(output->descriptor)))
    return false;
  if (!data.ReadSupportedFormats(&(output->supported_formats)))
    return false;
  return true;
}

// static
bool StructTraits<media_m96::mojom::VideoCaptureFeedbackDataView,
                  media_m96::VideoCaptureFeedback>::
    Read(media_m96::mojom::VideoCaptureFeedbackDataView data,
         media_m96::VideoCaptureFeedback* output) {
  output->max_framerate_fps = data.max_framerate_fps();
  output->max_pixels = data.max_pixels();
  output->resource_utilization = data.resource_utilization();
  output->require_mapped_frame = data.require_mapped_frame();
  if (!data.ReadMappedSizes(&(output->mapped_sizes)))
    return false;
  return true;
}

}  // namespace mojo
