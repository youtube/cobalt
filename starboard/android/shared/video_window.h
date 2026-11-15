// Copyright 2017 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef STARBOARD_ANDROID_SHARED_VIDEO_WINDOW_H_
#define STARBOARD_ANDROID_SHARED_VIDEO_WINDOW_H_

#include <android/native_window.h>
#include <jni.h>

#include "starboard/decode_target.h"
#include "third_party/jni_zero/jni_zero.h"

namespace starboard {

class VideoSurfaceHolder {
 public:
  // Used by unit tests to verify how the video surface is handled. While an
  // observer is set, CleanUpVideoSurface(), SetVideoSurfaceVisible(),
  // ShowVideoSurfaceWithNextFrame() and ResetVideoSurface() are reported to it
  // instead of being forwarded to the platform.
  class TestObserver {
   public:
    virtual ~TestObserver() = default;
    virtual void OnCleanUpVideoSurface() {}
    virtual void OnSetVideoSurfaceVisible(bool visible) {}
    virtual void OnShowVideoSurfaceWithNextFrame() {}
    virtual void OnResetVideoSurface() {}
  };

  // Also forgets that the surface was hidden, if it was.
  static void SetTestObserverForTesting(TestObserver* observer);

  // Makes |holder| the current holder of the video surface, as if it had
  // acquired the surface. Pass nullptr to clear it.
  static void SetVideoSurfaceHolderForTesting(VideoSurfaceHolder* holder);

  // Makes |surface| the video surface if there is none, so that tests don't
  // depend on the Activity having created its surface. Returns true if
  // |surface| was set, in which case it must be removed again with
  // RemoveVideoSurfaceForTesting().
  static bool SetVideoSurfaceForTestingIfNone(
      JNIEnv* env,
      const jni_zero::JavaRef<jobject>& surface);
  // Removes |surface| if it is still the video surface.
  static void RemoveVideoSurfaceForTesting(
      JNIEnv* env,
      const jni_zero::JavaRef<jobject>& surface);

  // Return true only if the video surface is available.
  static bool IsVideoSurfaceAvailable();

  // OnSurfaceDestroyed() will be invoked when surface is destroyed. When this
  // function is called, the decoder no longer owns the surface. Calling
  // AcquireVideoSurface(), ReleaseVideoSurface(), GetVideoWindowSize() or
  // ClearVideoWindow() in this function may cause dead lock.
  virtual void OnSurfaceDestroyed() = 0;

 protected:
  ~VideoSurfaceHolder() {}

  // Returns the surface which video should be rendered. Surface cannot be
  // acquired before last holder release the surface.
  jni_zero::ScopedJavaLocalRef<jobject> AcquireVideoSurface();

  // Release the surface to make the surface available for other holder.
  void ReleaseVideoSurface();

  // Returns true if the surface is held by a holder other than this one.
  bool IsVideoSurfaceHeldByAnotherHolder() const;

  // Returns true if the holder shows the video surface itself once it renders
  // to it, and supports a video codec parked on the surface by a previous
  // holder. For any other holder, AcquireVideoSurface() releases a parked codec
  // and shows the surface if a previous holder left it hidden.
  virtual bool ManagesParkedCodecAndSurfaceVisibility() const { return false; }

  // Cleans up the video surface, and posts the task to |gpu_provider|.
  void CleanUpVideoSurface(SbDecodeTargetGraphicsContextProvider* gpu_provider);

  // Like CleanUpVideoSurface(), but returns false without doing anything if the
  // video surface is being created or destroyed at the moment. To be used where
  // waiting for that could block the thread the surface change itself waits
  // for, see OnSurfaceDestroyed().
  bool TryCleanUpVideoSurface(
      SbDecodeTargetGraphicsContextProvider* gpu_provider);

  // Sets the visibility of the video surface immediately via SurfaceControl.
  void SetVideoSurfaceVisible(bool visible);

  // Shows the video surface together with the next frame rendered to it via
  // SurfaceView.applyTransactionToFrame(), falling back to showing it
  // immediately if attaching to the next frame fails.
  void ShowVideoSurfaceWithNextFrame();

  // Reset the video surface by re-creating video surface.
  void ResetVideoSurface();

 private:
  // Must be called with the video surface mutex held.
  void CleanUpVideoSurfaceLocked(
      SbDecodeTargetGraphicsContextProvider* gpu_provider);
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_VIDEO_WINDOW_H_
