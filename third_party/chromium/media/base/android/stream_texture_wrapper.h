// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_BASE_ANDROID_STREAM_TEXTURE_WRAPPER_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_BASE_ANDROID_STREAM_TEXTURE_WRAPPER_H_

#include "base/single_thread_task_runner.h"
#include "base/unguessable_token.h"
#include "third_party/chromium/media/base/video_frame.h"

namespace media_m96 {

// StreamTextureWrapper encapsulates a StreamTexture's creation, initialization
// and registration for later retrieval (in the Browser process).
class MEDIA_EXPORT StreamTextureWrapper {
 public:
  using StreamTextureWrapperInitCB = base::OnceCallback<void(bool)>;

  StreamTextureWrapper() {}

  // Initialize the underlying StreamTexture.
  // See StreamTextureWrapperImpl.
  virtual void Initialize(
      const base::RepeatingClosure& received_frame_cb,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
      StreamTextureWrapperInitCB init_cb) = 0;

  // Called whenever the video's natural size changes.
  // See StreamTextureWrapperImpl.
  virtual void UpdateTextureSize(const gfx::Size& natural_size) = 0;

  // Returns the latest frame.
  // See StreamTextureWrapperImpl.
  virtual scoped_refptr<VideoFrame> GetCurrentFrame() = 0;

  // Sends the StreamTexture to the browser process, to fulfill the request
  // identified by |request_token|.
  // See StreamTextureWrapperImpl.
  virtual void ForwardStreamTextureForSurfaceRequest(
      const base::UnguessableToken& request_token) = 0;

  // Clears the |received_frame_cb| passed in Initialize().
  // Should be safe to call from any thread.
  virtual void ClearReceivedFrameCBOnAnyThread() = 0;

  struct Deleter {
    inline void operator()(StreamTextureWrapper* ptr) const { ptr->Destroy(); }
  };

 protected:
  virtual ~StreamTextureWrapper() {}

  // Safely destroys the StreamTextureWrapper.
  // See StreamTextureWrapperImpl.
  virtual void Destroy() = 0;
};

typedef std::unique_ptr<StreamTextureWrapper, StreamTextureWrapper::Deleter>
    ScopedStreamTextureWrapper;

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_BASE_ANDROID_STREAM_TEXTURE_WRAPPER_H_
