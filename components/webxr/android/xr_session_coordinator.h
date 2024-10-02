// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBXR_ANDROID_XR_SESSION_COORDINATOR_H_
#define COMPONENTS_WEBXR_ANDROID_XR_SESSION_COORDINATOR_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "device/vr/android/xr_java_coordinator.h"

namespace webxr {

class XrSessionCoordinator : public device::XrJavaCoordinator {
 public:
  explicit XrSessionCoordinator();
  ~XrSessionCoordinator() override;

  // XrJavaCoordinator:
  void RequestArSession(
      int render_process_id,
      int render_frame_id,
      bool use_overlay,
      bool can_render_dom_content,
      const device::CompositorDelegateProvider& compositor_delegate_provider,
      device::SurfaceReadyCallback ready_callback,
      device::SurfaceTouchCallback touch_callback,
      device::SurfaceDestroyedCallback destroyed_callback) override;
  void RequestVrSession(
      int render_process_id,
      int render_frame_id,
      const device::CompositorDelegateProvider& compositor_delegate_provider,
      device::SurfaceReadyCallback ready_callback,
      device::SurfaceTouchCallback touch_callback,
      device::SurfaceDestroyedCallback destroyed_callback) override;
  void EndSession() override;
  bool EnsureARCoreLoaded() override;
  base::android::ScopedJavaLocalRef<jobject> GetApplicationContext() override;

  // Methods called from the Java side.
  void OnDrawingSurfaceReady(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& surface,
      const base::android::JavaParamRef<jobject>& root_window,
      int rotation,
      int width,
      int height);
  void OnDrawingSurfaceTouch(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             bool primary,
                             bool touching,
                             int32_t pointer_id,
                             float x,
                             float y);
  void OnDrawingSurfaceDestroyed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_xr_session_coordinator_;

  device::SurfaceReadyCallback surface_ready_callback_;
  device::SurfaceTouchCallback surface_touch_callback_;
  device::SurfaceDestroyedCallback surface_destroyed_callback_;
};

}  // namespace webxr

#endif  // COMPONENTS_WEBXR_ANDROID_XR_SESSION_COORDINATOR_H_
