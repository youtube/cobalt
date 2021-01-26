// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/blittergles/blitter_surface.h"

#include <GLES2/gl2.h>

#include "starboard/common/log.h"
#include "starboard/shared/blittergles/blitter_context.h"
#include "starboard/shared/blittergles/blitter_internal.h"
#include "starboard/shared/gles/gl_call.h"

SbBlitterSurfacePrivate::~SbBlitterSurfacePrivate() {
  SbBlitterContextPrivate::ScopedCurrentContext scoped_current_context;
  GL_CALL(glDeleteTextures(1, &color_texture_handle));
  if (render_target != NULL) {
    GL_CALL(glDeleteFramebuffers(1, &render_target->framebuffer_handle));
    delete render_target;
  }
}

bool SbBlitterSurfacePrivate::SetTexture(void* pixel_data) {
  SbBlitterContextPrivate::ScopedCurrentContext scoped_current_context;
  if (scoped_current_context.InitializationError()) {
    return false;
  }
  glGenTextures(1, &color_texture_handle);
  if (color_texture_handle == 0) {
    SB_DLOG(ERROR) << ": Error creating new texture.";
    return false;
  }
  glBindTexture(GL_TEXTURE_2D, color_texture_handle);
  if (glGetError() != GL_NO_ERROR) {
    GL_CALL(glDeleteTextures(1, &color_texture_handle));
    color_texture_handle = 0;
    SB_DLOG(ERROR) << ": Error binding new texture.";
    return false;
  }
  GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
  GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

  GLint pixel_format =
      info.format == kSbBlitterSurfaceFormatRGBA8 ? GL_RGBA : GL_ALPHA;
  GL_CALL(glPixelStorei(GL_PACK_ALIGNMENT, 1));
  glTexImage2D(GL_TEXTURE_2D, 0, pixel_format, info.width, info.height, 0,
               pixel_format, GL_UNSIGNED_BYTE, pixel_data);
  if (glGetError() != GL_NO_ERROR) {
    GL_CALL(glDeleteTextures(1, &color_texture_handle));
    color_texture_handle = 0;
    SB_DLOG(ERROR) << ": Error allocating new texture backing.";
    return false;
  }

  GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
  return true;
}
