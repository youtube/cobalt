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

#include "starboard/android/shared/media/decode_target.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <android/native_window_jni.h>
#include <jni.h>

#include <functional>
#include <mutex>

#include "starboard/android/shared/media/video_surface_texture_bridge.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/shared/gles/gl_call.h"

namespace starboard {
namespace {

using base::android::AttachCurrentThread;

void RunOnContextRunner(void* context) {
  std::function<void()>* closure = static_cast<std::function<void()>*>(context);
  (*closure)();
}

}  // namespace

DecodeTarget::DecodeTarget(SbDecodeTargetGraphicsContextProvider* provider) {
  std::function<void()> closure =
      std::bind(&DecodeTarget::CreateOnContextRunner, this);
  SbDecodeTargetRunInGlesContext(provider, &RunOnContextRunner, &closure);
}

bool DecodeTarget::GetInfo(SbDecodeTargetInfo* out_info) {
  SB_DCHECK(out_info);

  *out_info = info_;
  return true;
}

DecodeTarget::~DecodeTarget() {
  ANativeWindow_release(native_window_);

  glDeleteTextures(1, &info_.planes[0].texture);
  SB_DCHECK_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
}

void DecodeTarget::CreateOnContextRunner() {
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

  JNIEnv* env = AttachCurrentThread();
  // Wrap the GL texture in an Android SurfaceTexture object.
  surface_texture_ =
      VideoSurfaceTextureBridge::CreateVideoSurfaceTexture(env, texture);
  SB_CHECK(surface_texture_);

  // We will also need an Android Surface object in order to obtain a
  // ANativeWindow object that we can pass into the AMediaCodec library.
  surface_ =
      VideoSurfaceTextureBridge::CreateSurface(env, surface_texture_.obj());
  SB_CHECK(surface_texture_);

  native_window_ = ANativeWindow_fromSurface(env, surface_.obj());

  // Setup our publicly accessible decode target information.
  info_.format = kSbDecodeTargetFormat1PlaneRGBA;
  info_.is_opaque = true;
  info_.width = 0;
  info_.height = 0;
  info_.planes[0].texture = texture;
  info_.planes[0].gl_texture_target = GL_TEXTURE_EXTERNAL_OES;
  info_.planes[0].width = 0;
  info_.planes[0].height = 0;

  // These values will be initialized when SbPlayerGetCurrentFrame() is called.
  info_.planes[0].content_region.left = 0;
  info_.planes[0].content_region.right = 0;
  info_.planes[0].content_region.top = 0;
  info_.planes[0].content_region.bottom = 0;

  GL_CALL(glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0));
}

}  // namespace starboard
