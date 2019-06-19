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

#include <memory>

#include "starboard/common/log.h"
#include "starboard/common/optional.h"
#include "starboard/memory.h"
#include "starboard/shared/blittergles/blitter_internal.h"
#include "starboard/shared/gles/gl_call.h"

namespace {

starboard::optional<GLuint> ComputeTextureHandle(SbBlitterSurface surface) {
  GLuint texture_handle = 0;
  glGenTextures(1, &texture_handle);
  if (texture_handle == 0) {
    SB_DLOG(ERROR) << ": Error creating new texture.";
    return starboard::nullopt;
  }
  glBindTexture(GL_TEXTURE_2D, texture_handle);
  if (glGetError() != GL_NO_ERROR) {
    GL_CALL(glDeleteTextures(1, &texture_handle));
    SB_DLOG(ERROR) << ": Error binding new texture.";
    return starboard::nullopt;
  }
  GL_CALL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
  GL_CALL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  GL_CALL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

  GLint pixel_format =
      surface->info.format == kSbBlitterSurfaceFormatRGBA8 ? GL_RGBA : GL_ALPHA;
  GL_CALL(glPixelStorei(GL_PACK_ALIGNMENT, 1));
  glTexImage2D(GL_TEXTURE_2D, 0, pixel_format, surface->info.width,
               surface->info.height, 0, pixel_format, GL_UNSIGNED_BYTE,
               surface->data);
  if (glGetError() != GL_NO_ERROR) {
    GL_CALL(glDeleteTextures(1, &texture_handle));
    SB_DLOG(ERROR) << ": Error allocating new texture backing.";
    return starboard::nullopt;
  }

  GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
  return texture_handle;
}

}  // namespace

SbBlitterSurfacePrivate::~SbBlitterSurfacePrivate() {
  if (color_texture_handle != 0) {
    GL_CALL(glDeleteTextures(1, &color_texture_handle));
  }
  if (render_target != NULL) {
    if (render_target->framebuffer_handle != 0) {
      GL_CALL(glDeleteFramebuffers(1, &render_target->framebuffer_handle));
    }
    delete render_target;
  }
  if (data != NULL) {
    SbMemoryDeallocate(data);
  }
}

bool SbBlitterSurfacePrivate::EnsureInitialized() {
  if (color_texture_handle != 0) {
    return true;
  }

  starboard::optional<GLuint> texture = ComputeTextureHandle(this);
  color_texture_handle = texture ? *texture : 0;
  if (texture && data != NULL) {
    SbMemoryDeallocate(data);
    data = NULL;
  }

  return texture.has_engaged();
}
