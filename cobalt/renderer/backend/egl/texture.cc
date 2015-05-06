/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/renderer/backend/egl/texture.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "cobalt/renderer/backend/egl/graphics_context.h"
#include "cobalt/renderer/backend/egl/utils.h"

namespace cobalt {
namespace renderer {
namespace backend {

namespace {
void SetupInitialTextureParameters() {
  GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
  GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
  GL_CALL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  GL_CALL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
}

GLenum SurfaceInfoFormatToGL(SurfaceInfo::Format format) {
  switch (format) {
    case SurfaceInfo::kFormatRGBA8: return GL_RGBA;
    case SurfaceInfo::kFormatBGRA8: return GL_BGRA_EXT;
    default: DLOG(FATAL) << "Unsupported incoming pixel data format.";
  }
  return GL_INVALID_ENUM;
}
}  // namespace

TextureDataEGL::TextureDataEGL(const SurfaceInfo& surface_info)
    : surface_info_(surface_info),
      memory_(new uint8_t[GetPitchInBytes() * surface_info.size.height()]) {}

TextureEGL::TextureEGL(const base::Closure& make_context_current_function,
                       scoped_ptr<TextureDataEGL> texture_source_data,
                       bool bgra_supported)
    : make_context_current_function_(make_context_current_function) {
  make_context_current_function_.Run();

  surface_info_ = texture_source_data->GetSurfaceInfo();

  GL_CALL(glGenTextures(1, &gl_handle_));
  GL_CALL(glBindTexture(GL_TEXTURE_2D, gl_handle_));
  SetupInitialTextureParameters();

  GLenum gl_format = SurfaceInfoFormatToGL(surface_info_.format);
  if (gl_format == GL_BGRA_EXT) {
    DCHECK(bgra_supported);
  }

  // Copy pixel data over from the user provided source data into the OpenGL
  // driver to be used as a texture from now on.
  GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, gl_format, surface_info_.size.width(),
                       surface_info_.size.height(), 0, gl_format,
                       GL_UNSIGNED_BYTE, texture_source_data->GetMemory()));
  GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
}

TextureEGL::TextureEGL(
    const base::Closure& make_context_current_function,
    const scoped_refptr<PBufferRenderTargetEGL>& render_target)
    : make_context_current_function_(make_context_current_function) {
  make_context_current_function_.Run();

  source_render_target_ = render_target;

  // First we create the OpenGL texture object and maintain a handle to it.
  surface_info_ = render_target->GetSurfaceInfo();
  GL_CALL(glGenTextures(1, &gl_handle_));
  GL_CALL(glBindTexture(GL_TEXTURE_2D, gl_handle_));
  SetupInitialTextureParameters();

  // PBuffers should always be RGBA, so we enforce this here.
  GLenum gl_format = SurfaceInfoFormatToGL(surface_info_.format);
  DCHECK_EQ(GL_RGBA, gl_format);

  // This call attaches the EGL PBuffer object to the currently bound OpenGL
  // texture object, effectively allowing the PBO render target to be used
  // as a texture by referencing gl_handle_ from now on.
  EGL_CALL(eglBindTexImage(render_target->display(),
                           render_target->GetSurface(),
                           EGL_BACK_BUFFER));

  GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
}

TextureEGL::~TextureEGL() {
  make_context_current_function_.Run();

  if (source_render_target_) {
    EGL_CALL(eglReleaseTexImage(source_render_target_->display(),
                                source_render_target_->GetSurface(),
                                EGL_BACK_BUFFER));
  }
#if !defined(NDEBUG)
  GLint bound_texture;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound_texture);
  DCHECK_NE(bound_texture, gl_handle_);
#endif
  GL_CALL(glDeleteTextures(1, &gl_handle_));
}

const SurfaceInfo& TextureEGL::GetSurfaceInfo() const { return surface_info_; }

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt
