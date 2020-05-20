// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/configuration.h"
#if SB_API_VERSION >= 12 || SB_HAS(GLES2)

#include "cobalt/renderer/backend/egl/texture.h"

#include "base/bind.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/renderer/backend/egl/framebuffer_render_target.h"
#include "cobalt/renderer/backend/egl/graphics_context.h"
#include "cobalt/renderer/backend/egl/pbuffer_render_target.h"
#include "cobalt/renderer/backend/egl/resource_context.h"
#include "cobalt/renderer/backend/egl/texture_data.h"
#include "cobalt/renderer/backend/egl/texture_data_cpu.h"
#include "cobalt/renderer/backend/egl/utils.h"
#include "cobalt/renderer/egl_and_gles.h"

namespace cobalt {
namespace renderer {
namespace backend {

namespace {
void DoNothing() {
}
}  // namespace

TextureEGL::TextureEGL(GraphicsContextEGL* graphics_context,
                       std::unique_ptr<TextureDataEGL> texture_source_data,
                       bool bgra_supported)
    : graphics_context_(graphics_context),
      size_(texture_source_data->GetSize()),
      format_(texture_source_data->GetFormat()),
      target_(GL_TEXTURE_2D) {
  gl_handle_ =
      texture_source_data->ConvertToTexture(graphics_context_, bgra_supported);
}

TextureEGL::TextureEGL(GraphicsContextEGL* graphics_context,
                       const RawTextureMemoryEGL* data, intptr_t offset,
                       const math::Size& size, GLenum format,
                       int pitch_in_bytes, bool bgra_supported)
    : graphics_context_(graphics_context),
      size_(size),
      format_(format),
      target_(GL_TEXTURE_2D) {
  gl_handle_ = data->CreateTexture(graphics_context_, offset, size, format,
                                   pitch_in_bytes, bgra_supported);
}

TextureEGL::TextureEGL(GraphicsContextEGL* graphics_context, GLuint gl_handle,
                       const math::Size& size, GLenum format, GLenum target,
                       base::OnceClosure&& delete_function)
    : graphics_context_(graphics_context),
      size_(size),
      format_(format),
      gl_handle_(gl_handle),
      target_(target),
      delete_function_(std::move(delete_function)) {}

TextureEGL::TextureEGL(
    GraphicsContextEGL* graphics_context,
    const scoped_refptr<RenderTargetEGL>& render_target)
    : graphics_context_(graphics_context),
      size_(render_target->GetSize()),
      format_(GL_RGBA),
      target_(GL_TEXTURE_2D) {
  GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(graphics_context_);

  source_render_target_ = render_target;

  if (render_target->GetSurface() != EGL_NO_SURFACE) {
    // This is a PBufferRenderTargetEGL. Need to bind a texture to the surface.
    const PBufferRenderTargetEGL* pbuffer_target =
        base::polymorphic_downcast<const PBufferRenderTargetEGL*>
            (render_target.get());

    // First we create the OpenGL texture object and maintain a handle to it.
    GL_CALL(glGenTextures(1, &gl_handle_));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, gl_handle_));
    SetupInitialTextureParameters();

    // This call attaches the EGL PBuffer object to the currently bound OpenGL
    // texture object, effectively allowing the PBO render target to be used
    // as a texture by referencing gl_handle_ from now on.
    EGL_CALL(eglBindTexImage(pbuffer_target->display(),
                             pbuffer_target->GetSurface(),
                             EGL_BACK_BUFFER));

    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
  } else {
    // This is a FramebufferRenderTargetEGL. Wrap its color texture attachment.
    const FramebufferRenderTargetEGL* framebuffer_target =
        base::polymorphic_downcast<const FramebufferRenderTargetEGL*>
            (render_target.get());

    const TextureEGL* color_attachment = framebuffer_target->GetColorTexture();
    format_ = color_attachment->GetFormat();
    target_ = color_attachment->GetTarget();
    gl_handle_ = color_attachment->gl_handle();

    // Do not destroy the wrapped texture. Let the render target do that.
    delete_function_ = base::Bind(&DoNothing);
  }
}

TextureEGL::~TextureEGL() {
  GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(graphics_context_);

  if (source_render_target_ &&
      source_render_target_->GetSurface() != EGL_NO_SURFACE) {
    const PBufferRenderTargetEGL* pbuffer_target =
        base::polymorphic_downcast<const PBufferRenderTargetEGL*>
            (source_render_target_.get());
    EGL_CALL(eglReleaseTexImage(pbuffer_target->display(),
                                pbuffer_target->GetSurface(),
                                EGL_BACK_BUFFER));
  }

  if (!delete_function_.is_null()) {
    std::move(delete_function_).Run();
  } else {
    GL_CALL(glDeleteTextures(1, &gl_handle_));
  }
}

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION >= 12 || SB_HAS(GLES2)
