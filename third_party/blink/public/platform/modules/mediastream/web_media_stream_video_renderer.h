// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_MEDIASTREAM_WEB_MEDIA_STREAM_VIDEO_RENDERER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_MEDIASTREAM_WEB_MEDIA_STREAM_VIDEO_RENDERER_H_

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"

namespace media {
class VideoFrame;
}

namespace blink {

// Interface returned by MediaStreamRendererFactory that provides controls for
// the flow of video frame callbacks being made.
// TODO(wjia): remove ref count.
class WebMediaStreamVideoRenderer
    : public base::RefCountedThreadSafe<WebMediaStreamVideoRenderer> {
 public:
  typedef base::RepeatingCallback<void(scoped_refptr<media::VideoFrame>)>
      RepaintCB;

  // Start to provide video frames to the caller.
  virtual void Start() = 0;

  // Stop to provide video frames to the caller.
  virtual void Stop() = 0;

  // Resume to provide video frames to the caller after being paused.
  virtual void Resume() = 0;

  // Put the provider in pause state and the caller will not receive video
  // frames, but the provider might still generate video frames which are
  // thrown away.
  virtual void Pause() = 0;

 protected:
  friend class base::RefCountedThreadSafe<WebMediaStreamVideoRenderer>;

  virtual ~WebMediaStreamVideoRenderer() {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_MEDIASTREAM_WEB_MEDIA_STREAM_VIDEO_RENDERER_H_
