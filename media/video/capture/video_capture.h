// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains abstract classes used for media filter to handle video
// capture devices.

#ifndef MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_H_
#define MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_H_

#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "media/base/media_export.h"
#include "media/base/video_frame.h"
#include "media/video/capture/video_capture_types.h"

namespace media {

class MEDIA_EXPORT VideoCapture {
 public:
  // TODO(wjia): consider merging with media::VideoFrame if possible.
  class VideoFrameBuffer : public base::RefCountedThreadSafe<VideoFrameBuffer> {
   public:
    VideoFrameBuffer()
        : width(0),
          height(0),
          stride(0),
          buffer_size(0),
          memory_pointer(NULL) {}
    ~VideoFrameBuffer() {}

    int width;
    int height;
    int stride;
    size_t buffer_size;
    uint8* memory_pointer;
    base::Time timestamp;

   private:
    DISALLOW_COPY_AND_ASSIGN(VideoFrameBuffer);
  };

  // TODO(wjia): add error codes.
  // Callbacks provided by client for notification of events.
  class EventHandler {
   public:
    // Notify client that video capture has been started.
    virtual void OnStarted(VideoCapture* capture) = 0;

    // Notify client that video capture has been stopped.
    virtual void OnStopped(VideoCapture* capture) = 0;

    // Notify client that video capture has been paused.
    virtual void OnPaused(VideoCapture* capture) = 0;

    // Notify client that video capture has hit some error |error_code|.
    virtual void OnError(VideoCapture* capture, int error_code) = 0;

    // Notify client that the client has been removed and no more calls will be
    // received.
    virtual void OnRemoved(VideoCapture* capture) = 0;

    // Notify client that a buffer is available.
    virtual void OnBufferReady(VideoCapture* capture,
                               scoped_refptr<VideoFrameBuffer> buffer) = 0;

    // Notify client about device info.
    virtual void OnDeviceInfoReceived(
        VideoCapture* capture,
        const VideoCaptureParams& device_info) = 0;
  };

  // TODO(wjia): merge with similar struct in browser process and move it to
  // video_capture_types.h.
  struct VideoCaptureCapability {
    int width;  // desired width.
    int height;  // desired height.
    int max_fps;  // desired maximum frame rate.
    int expected_capture_delay;  // expected delay in millisecond.
    media::VideoFrame::Format raw_type;  // desired video type.
    bool interlaced;  // need interlace format.
  };

  VideoCapture() {}
  virtual ~VideoCapture() {}

  // Request video capture to start capturing with |capability|.
  // Also register |handler| with video capture for event handling.
  // |handler| must remain valid until it has received |OnRemoved()|.
  virtual void StartCapture(EventHandler* handler,
                            const VideoCaptureCapability& capability) = 0;

  // Request video capture to stop capturing for client |handler|.
  // |handler| must remain valid until it has received |OnRemoved()|.
  virtual void StopCapture(EventHandler* handler) = 0;

  // Feed buffer to video capture when done with it.
  virtual void FeedBuffer(scoped_refptr<VideoFrameBuffer> buffer) = 0;

  virtual bool CaptureStarted() = 0;
  virtual int CaptureWidth() = 0;
  virtual int CaptureHeight() = 0;
  virtual int CaptureFrameRate() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCapture);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_H_
