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

#include "starboard/android/shared/decode_target.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <android/native_window_jni.h>
#include <jni.h>

#include <functional>
#include <mutex>

#include "starboard/android/shared/protected_video_plane.h"
#include "starboard/android/shared/video_surface_texture_bridge.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/shared/gles/gl_call.h"
#include "third_party/jni_zero/jni_zero.h"

namespace starboard {
namespace {

void RunOnContextRunner(void* context) {
  std::function<void()>* closure = static_cast<std::function<void()>*>(context);
  (*closure)();
}

}  // namespace

// static
scoped_refptr<DecodeTarget> DecodeTarget::Create(
    SbDecodeTargetGraphicsContextProvider* provider,
    bool protected_path) {
  scoped_refptr<DecodeTarget> decode_target(
      new DecodeTarget(provider, protected_path));
  if (decode_target->surface_texture_.is_null() ||
      decode_target->surface_.is_null()) {
    // The GLES context runner did not run CreateOnContextRunner() (see
    // StarboardRendererWrapper::GraphicsContextRunner). Nothing GL-side was
    // created, so the target can be destroyed here, off the GL context.
    SB_LOG(ERROR) << "Decode target was not initialized by the GLES context "
                     "runner.";
    return nullptr;
  }
  return decode_target;
}

DecodeTarget::DecodeTarget(SbDecodeTargetGraphicsContextProvider* provider,
                           bool protected_path)
    : provider_(provider), protected_path_(protected_path) {
  std::function<void()> closure =
      std::bind(&DecodeTarget::CreateOnContextRunner, this);
  SbDecodeTargetRunInGlesContext(provider, &RunOnContextRunner, &closure);
}

bool DecodeTarget::GetInfo(SbDecodeTargetInfo* out_info) {
  SB_CHECK(out_info);

  *out_info = info_;
  return true;
}

DecodeTarget::~DecodeTarget() {
  if (native_window_) {
    ANativeWindow_release(native_window_);
  }

  if (info_.planes[0].texture == 0) {
    return;
  }
  if (hooks_id_) {
    DeleteProtectedDecodeTargetTexture(provider_, hooks_id_,
                                       info_.planes[0].texture);
    return;
  }
  glDeleteTextures(1, &info_.planes[0].texture);
  SB_CHECK_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
}

void DecodeTarget::CreateOnContextRunner() {
  // Setup the GL texture that Android's MediaCodec library will target with
  // the decoder.  We don't call glTexImage2d() on it, Android will handle
  // the creation of the content when SurfaceTexture::updateTexImage() is
  // called.
  GLuint texture;
  // The name SurfaceTexture attaches to. It differs from |texture| only for an
  // embedder texture, whose native (driver) name is not the embedder's name.
  GLuint native_texture;
  // Clear decoders keep the system-GL texture, so that clear WebGL and the
  // compositor can sample it.
  hooks_id_ =
      protected_path_ ? GetProtectedDecodeTargetTextureHooksId(provider_) : 0;
  if (hooks_id_) {
    SB_CHECK(CreateProtectedDecodeTargetTexture(provider_, hooks_id_, &texture,
                                                &native_texture));
  } else {
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
    native_texture = texture;
  }

  JNIEnv* env = jni_zero::AttachCurrentThread();
  // Wrap the GL texture in an Android SurfaceTexture object.
  surface_texture_ =
      VideoSurfaceTextureBridge::CreateVideoSurfaceTexture(env, native_texture);
  SB_CHECK(surface_texture_);

  // We will also need an Android Surface object in order to obtain a
  // ANativeWindow object that we can pass into the AMediaCodec library.
  surface_ = VideoSurfaceTextureBridge::CreateSurface(env, surface_texture_);
  SB_CHECK(surface_);

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

  if (!hooks_id_) {
    GL_CALL(glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0));
  }
}

}  // namespace starboard
