// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/video_surface_texture_bridge.h"

#include "cobalt/android/jni_headers/VideoSurfaceTexture_jni.h"

namespace starboard {
namespace {
using base::android::ScopedJavaLocalRef;
}  // namespace

void VideoSurfaceTextureBridge::SetOnFrameAvailableListener(
    JNIEnv* env,
    ScopedJavaLocalRef<jobject> surface_texture) const {
  Java_VideoSurfaceTexture_setOnFrameAvailableListener(
      env, surface_texture, reinterpret_cast<jlong>(this));
}

void VideoSurfaceTextureBridge::RemoveOnFrameAvailableListener(
    JNIEnv* env,
    ScopedJavaLocalRef<jobject> surface_texture) const {
  Java_VideoSurfaceTexture_removeOnFrameAvailableListener(env, surface_texture);
}

// static
jobject VideoSurfaceTextureBridge::CreateVideoSurfaceTexture(
    JNIEnv* env,
    int gl_texture_id) {
  SB_CHECK(env);
  return Java_VideoSurfaceTexture_Constructor(env, gl_texture_id).obj();
}

}  // namespace starboard
