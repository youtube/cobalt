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

#ifndef RENDERER_BACKEND_EGL_TEXTURE_H_
#define RENDERER_BACKEND_EGL_TEXTURE_H_

#include <GLES2/gl2.h>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/renderer/backend/egl/pbuffer_render_target.h"
#include "cobalt/renderer/backend/surface_info.h"
#include "cobalt/renderer/backend/texture.h"

namespace cobalt {
namespace renderer {
namespace backend {

class GraphicsContextEGL;

class TextureDataEGL : public TextureData {
 public:
  explicit TextureDataEGL(const SurfaceInfo& surface_info);

  const SurfaceInfo& GetSurfaceInfo() const OVERRIDE { return surface_info_; }
  int GetPitchInBytes() const OVERRIDE {
    return surface_info_.size.width() * surface_info_.BytesPerPixel();
  }

  uint8_t* GetMemory() OVERRIDE { return static_cast<uint8_t*>(memory_.get()); }

 private:
  SurfaceInfo surface_info_;

  // TODO(***REMOVED***): Store memory using a EGL PBuffer object which provides the
  //               implementation control over the memory.
  scoped_array<uint8_t> memory_;
};

class TextureEGL : public Texture {
 public:
  // Create a texture from source pixel data possibly filled in by the CPU.
  TextureEGL(const base::Closure& make_context_current_function,
             scoped_ptr<TextureDataEGL> texture_source_data,
             bool bgra_supported);
  // Create a texture from a pre-existing offscreen PBuffer render target.
  explicit TextureEGL(
      const base::Closure& make_context_current_function,
      const scoped_refptr<PBufferRenderTargetEGL>& render_target);
  ~TextureEGL();

  const SurfaceInfo& GetSurfaceInfo() const OVERRIDE;

  // Returns an index to the texture that can be passed to OpenGL functions.
  GLuint gl_handle() const { return gl_handle_; }

 private:
  // A function that can be called before issuing GL calls in order to ensure
  // that the proper context is current.
  base::Closure make_context_current_function_;

  // Metadata about the texture.
  SurfaceInfo surface_info_;

  // The OpenGL handle to the texture that can be passed into OpenGL functions.
  GLuint gl_handle_;

  // If the texture was constructed from a render target, we keep a reference
  // to the render target.
  scoped_refptr<PBufferRenderTargetEGL> source_render_target_;
};

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // RENDERER_BACKEND_EGL_TEXTURE_H_
