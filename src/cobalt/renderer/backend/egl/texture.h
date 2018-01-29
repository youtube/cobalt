// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_BACKEND_EGL_TEXTURE_H_
#define COBALT_RENDERER_BACKEND_EGL_TEXTURE_H_

#include <GLES2/gl2.h>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/renderer/backend/egl/render_target.h"
#include "cobalt/renderer/backend/egl/texture_data.h"

namespace cobalt {
namespace renderer {
namespace backend {

class GraphicsContextEGL;
class ResourceContext;

// An abstract class representing a texture in the cobalt rendering framework.
// This class is useful because the GraphicsContext class provides
// cross-platform methods for creating textures, and also for consuming them
// to do things like blit to the screen.  Platform-specific rendering libraries
// such as a Skia backend may also accept texture objects.
class TextureEGL {
 public:
  // Create a texture from source pixel data possibly filled in by the CPU.
  TextureEGL(GraphicsContextEGL* graphics_context,
             scoped_ptr<TextureDataEGL> texture_source_data,
             bool bgra_supported);
  // Create a texture from a pre-existing offscreen PBuffer render target.
  TextureEGL(GraphicsContextEGL* graphics_context,
             const RawTextureMemoryEGL* data, intptr_t offset,
             const math::Size& size, GLenum format, int pitch_in_bytes,
             bool bgra_supported);
  // Wrap and take ownership of a pre-existing OpenGL texture.
  // If provided, |delete_function| will be used to clean up resources instead
  // of manually calling gl_
  TextureEGL(GraphicsContextEGL* graphics_context, GLuint gl_handle,
             const math::Size& size, GLenum format, GLenum target,
             const base::Closure& delete_function);

  // Create a texture from a pre-existing offscreen render target.
  TextureEGL(GraphicsContextEGL* graphics_context,
             const scoped_refptr<RenderTargetEGL>& render_target);
  virtual ~TextureEGL();

  bool IsValid() const { return gl_handle_ != 0; }
  const math::Size& GetSize() const { return size_; }
  GLenum GetFormat() const { return format_; }
  GLenum GetTarget() const { return target_; }

  intptr_t GetPlatformHandle() { return static_cast<intptr_t>(gl_handle()); }

  // Returns an index to the texture that can be passed to OpenGL functions.
  GLuint gl_handle() const { return gl_handle_; }

  GraphicsContextEGL* graphics_context() { return graphics_context_; }

 private:
  // A reference to the graphics context that this texture is associated with.
  GraphicsContextEGL* graphics_context_;

  // Resolution of the texture pixel data.
  math::Size size_;

  // Pixel color format of the texture.
  GLenum format_;

  // The texture target type that should be passed in to GLES commands dealing
  // with this texture.
  GLenum target_;

  // The OpenGL handle to the texture that can be passed into OpenGL functions.
  GLuint gl_handle_;

  // If the texture was constructed from a render target, we keep a reference
  // to the render target.
  scoped_refptr<RenderTargetEGL> source_render_target_;

  // If non-null, will be called upon destruction instead of manually deleting
  // the texture via glDeleteTextures().
  base::Closure delete_function_;
};

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_BACKEND_EGL_TEXTURE_H_
