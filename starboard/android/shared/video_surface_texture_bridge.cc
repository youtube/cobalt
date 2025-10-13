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
}

void VideoSurfaceTextureBridge::SetOnFrameAvailableListener(
    JNIEnv* env,
    const ScopedJavaLocalRef<jobject>& surface_texture) const {
  Java_VideoSurfaceTexture_setOnFrameAvailableListener(
      env, surface_texture, reinterpret_cast<intptr_t>(this));
}

void VideoSurfaceTextureBridge::RemoveOnFrameAvailableListener(
    JNIEnv* env,
    const ScopedJavaLocalRef<jobject>& surface_texture) const {
  Java_VideoSurfaceTexture_removeOnFrameAvailableListener(env, surface_texture);
}

ScopedJavaLocalRef<jobject>
VideoSurfaceTextureBridge::CreateVideoSurfaceTexture(JNIEnv* env,
                                                     int gl_texture_id) {
  return Java_VideoSurfaceTexture_Constructor(env, gl_texture_id);
}

base::android::ScopedJavaGlobalRef<jobject>
VideoSurfaceTextureBridge::CreateSurface(JNIEnv* env, jobject surface_texture) {
  return base::android::ScopedJavaGlobalRef<jobject>(
      Java_VideoSurfaceTexture_createSurface(
          env, ScopedJavaLocalRef<jobject>(env, surface_texture)));
}

void VideoSurfaceTextureBridge::UpdateTexImage(
    JNIEnv* env,
    const ScopedJavaLocalRef<jobject>& surface_texture) {
  Java_VideoSurfaceTexture_updateTexImage(env, surface_texture);
}

void VideoSurfaceTextureBridge::GetTransformMatrix(
    JNIEnv* env,
    const ScopedJavaLocalRef<jobject>& surface_texture,
    jfloatArray mtx) {
  Java_VideoSurfaceTexture_getTransformMatrix(
      env, surface_texture, ScopedJavaLocalRef<jfloatArray>(env, mtx));
}

}  // namespace starboard
