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
#include "media/base/video_frame.h"

namespace media {

class VideoCapture {
 public:
  // TODO(wjia): this type should be defined in a common place and
  // shared with device manager.
  typedef uint64 VideoCaptureId;

  // Current status of the video capture device in the browser process. Browser
  // process sends information about the current capture state and error to the
  // renderer process using this type.
  enum State {
    kStarted,
    kPaused,
    kStopped,
    kError,
  };

  // Parameters for starting video capture and device information.
  struct CaptureParams {
    CaptureParams() {}
    ~CaptureParams() {}

    uint32 width;
    uint32 height;
    uint32 frame_rate;
    VideoCaptureId session_id;
  };

  // TODO(wjia): consider merging with media::VideoFrame if possible.
  class VideoFrameBuffer : public base::RefCountedThreadSafe<VideoFrameBuffer> {
   public:
    VideoFrameBuffer() {}
    ~VideoFrameBuffer() {}

    uint32 width;
    uint32 height;
    int stride;
    size_t buffer_size;
    void* memory_pointer;
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

    // Notify client that a buffer is available.
    virtual void OnBufferReady(VideoCapture* capture,
                               scoped_refptr<VideoFrameBuffer> buffer) = 0;
  };

  struct VideoCaptureCapability {
    uint32 width;  // desired width.
    uint32 height;  // desired height.
    uint32 max_fps;  // desired maximum frame rate.
    uint32 expected_capture_delay;  // expected delay in millisecond.
    media::VideoFrame::Format raw_type;  // desired video type.
    bool interlaced;  // need interlace format.
    bool resolution_fixed;  // indicate requested resolution can't be altered.
  };

  VideoCapture() {}
  virtual ~VideoCapture() {}

  // Request video capture to start capturing with |capability|.
  // Also register |handler| with video capture for event handling.
  virtual void StartCapture(EventHandler* handler,
                            const VideoCaptureCapability& capability) = 0;

  // Request video capture to stop capturing for client |handler|.
  virtual void StopCapture(EventHandler* handler) = 0;

  // TODO(wjia): Add FeedBuffer when buffer sharing is needed between video
  // capture and downstream module.
  // virtual void FeedBuffer(scoped_refptr<VideoFrameBuffer> buffer) = 0;

  virtual bool CaptureStarted() = 0;
  virtual uint32 CaptureWidth() = 0;
  virtual uint32 CaptureHeight() = 0;
  virtual uint32 CaptureFrameRate() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCapture);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_H_
