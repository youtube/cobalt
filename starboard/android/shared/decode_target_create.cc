// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/decode_target.h"

#include <android/native_window_jni.h>
#include <jni.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "starboard/android/shared/decode_target_internal.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/shared/gles/gl_call.h"

using starboard::android::shared::JniEnvExt;

namespace {
jobject CreateSurfaceTexture(int gl_texture_id) {
  JniEnvExt* env = JniEnvExt::Get();

  jobject local_surface_texture =
      env->NewObject("android/graphics/SurfaceTexture", "(I)V", gl_texture_id);
  env->AbortOnException();

  jobject global_surface_texture =
      env->ConvertLocalRefToGlobalRef(local_surface_texture);

  return global_surface_texture;
}

jobject CreateSurfaceFromSurfaceTexture(jobject surface_texture) {
  JniEnvExt* env = JniEnvExt::Get();

  jobject local_surface =
      env->NewObject("android/view/Surface",
                     "(Landroid/graphics/SurfaceTexture;)V", surface_texture);
  env->AbortOnException();

  jobject global_surface = env->ConvertLocalRefToGlobalRef(local_surface);

  return global_surface;
}
}  // namespace

SbDecodeTarget SbDecodeTargetCreate(void* display,
                                    void* context,
                                    SbDecodeTargetFormat format,
                                    int width,
                                    int height) {
  SB_UNREFERENCED_PARAMETER(display);
  SB_UNREFERENCED_PARAMETER(context);

  SB_DCHECK(format == kSbDecodeTargetFormat1PlaneRGBA);
  if (format != kSbDecodeTargetFormat1PlaneRGBA) {
    return kSbDecodeTargetInvalid;
  }

  // Setup the GL texture that Android's MediaCodec library will target with
  // the decoder.  We don't call glTexImage2d() on it, Android will handle
  // the creation of the content when SurfaceTexture::updateTexImage() is
  // called.
  GLuint texture;
  GL_CALL(glGenTextures(1, &texture));
  GL_CALL(glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture));
  GL_CALL(glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER,
                          GL_LINEAR));
  GL_CALL(glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER,
                          GL_LINEAR));
  GL_CALL(glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S,
                          GL_CLAMP_TO_EDGE));
  GL_CALL(glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T,
                          GL_CLAMP_TO_EDGE));

  SbDecodeTarget decode_target = new SbDecodeTargetPrivate;
  decode_target->data = new SbDecodeTargetPrivate::Data;

  // Wrap the GL texture in an Android SurfaceTexture object.
  decode_target->data->surface_texture = CreateSurfaceTexture(texture);

  // We will also need an Android Surface object in order to obtain a
  // ANativeWindow object that we can pass into the AMediaCodec library.
  decode_target->data->surface =
      CreateSurfaceFromSurfaceTexture(decode_target->data->surface_texture);

  decode_target->data->native_window =
      ANativeWindow_fromSurface(JniEnvExt::Get(), decode_target->data->surface);

  // Setup our publicly accessible decode target information.
  decode_target->data->info.format = format;
  decode_target->data->info.is_opaque = true;
  decode_target->data->info.width = width;
  decode_target->data->info.height = height;
  decode_target->data->info.planes[0].texture = texture;
  decode_target->data->info.planes[0].gl_texture_target =
      GL_TEXTURE_EXTERNAL_OES;
  decode_target->data->info.planes[0].width = width;
  decode_target->data->info.planes[0].height = height;

  // These values will be initialized when SbPlayerGetCurrentFrame() is called.
  decode_target->data->info.planes[0].content_region.left = 0;
  decode_target->data->info.planes[0].content_region.right = 0;
  decode_target->data->info.planes[0].content_region.top = 0;
  decode_target->data->info.planes[0].content_region.bottom = 0;

  GL_CALL(glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0));

  return decode_target;
}
