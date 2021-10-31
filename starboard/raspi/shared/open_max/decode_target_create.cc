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

#include "starboard/raspi/shared/open_max/decode_target_create.h"

#include <GLES2/gl2.h>

#include "starboard/common/log.h"
#include "starboard/decode_target.h"
#include "starboard/raspi/shared/open_max/decode_target_internal.h"
#include "starboard/shared/gles/gl_call.h"

namespace starboard {
namespace raspi {
namespace shared {
namespace open_max {

namespace {

struct CreateParams {
  int width;
  int height;
  SbDecodeTargetFormat format;
  EGLDisplay egl_display;
  EGLContext egl_context;

  SbDecodeTarget decode_target_out;
};

void CreateWithContextRunner(void* context) {
  CreateParams* params = static_cast<CreateParams*>(context);

  GLuint texture;
  GL_CALL(glGenTextures(1, &texture));
  GL_CALL(glBindTexture(GL_TEXTURE_2D, texture));
  GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
  GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
  GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

  GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, params->width, params->height,
                       0, GL_RGBA, GL_UNSIGNED_BYTE, NULL));

  SbDecodeTargetPrivate* target = new SbDecodeTargetPrivate;
  target->display = params->egl_display;
  target->info.format = params->format;
  // Data to be set by the decoder.
  target->info.is_opaque = false;
  target->info.width = params->width;
  target->info.height = params->height;
  target->info.planes[0].texture = texture;
  target->info.planes[0].gl_texture_target = GL_TEXTURE_2D;
  target->info.planes[0].width = params->width;
  target->info.planes[0].height = params->height;
  target->info.planes[0].content_region.left = 0;
  target->info.planes[0].content_region.top = 0;
  target->info.planes[0].content_region.right = params->width;
  target->info.planes[0].content_region.bottom = params->height;

  target->images[0] = eglCreateImageKHR(
      params->egl_display, params->egl_context, EGL_GL_TEXTURE_2D_KHR,
      (EGLClientBuffer)target->info.planes[0].texture, NULL);
  SB_DCHECK(eglGetError() == EGL_SUCCESS);
  SB_DCHECK(target->images[0] != EGL_NO_IMAGE_KHR);

  GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));

  params->decode_target_out = target;
}
}  // namespace

SbDecodeTarget DecodeTargetCreate(
    SbDecodeTargetGraphicsContextProvider* provider,
    SbDecodeTargetFormat format,
    int width,
    int height) {
  SB_DCHECK(format == kSbDecodeTargetFormat1PlaneRGBA);
  if (format != kSbDecodeTargetFormat1PlaneRGBA) {
    return kSbDecodeTargetInvalid;
  }

  CreateParams params;
  params.width = width;
  params.height = height;
  params.format = format;
  params.egl_display = provider->egl_display;
  params.egl_context = provider->egl_context;
  params.decode_target_out = kSbDecodeTargetInvalid;

  SbDecodeTargetRunInGlesContext(
      provider, &CreateWithContextRunner, static_cast<void*>(&params));

  return params.decode_target_out;
}

}  // namespace open_max
}  // namespace shared
}  // namespace raspi
}  // namespace starboard
