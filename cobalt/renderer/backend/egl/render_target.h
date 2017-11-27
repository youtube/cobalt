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

#ifndef COBALT_RENDERER_BACKEND_EGL_RENDER_TARGET_H_
#define COBALT_RENDERER_BACKEND_EGL_RENDER_TARGET_H_

#include <EGL/egl.h>

#include "cobalt/renderer/backend/render_target.h"

namespace cobalt {
namespace renderer {
namespace backend {

class RenderTargetEGL : public RenderTarget {
 public:
  RenderTargetEGL()
      : swap_count_(0)
      , has_been_made_current_(false)
      , content_preserved_on_swap_(false)
      , content_cleared_on_swap_(false)
      , is_surface_bad_(false)
  {}

  // An EGLSurface is needed for the EGL function eglMakeCurrent() which
  // associates a render target with a rendering context.
  virtual EGLSurface GetSurface() const = 0;

  // Since all RenderTargets defined at this level are EGL objects, they
  // will always be set via eglMakeCurrent() and so they can always be
  // referenced by OpenGL by binding framebuffer 0.
  intptr_t GetPlatformHandle() const override { return 0; }

  virtual bool IsWindowRenderTarget() const { return false; }

  // Returns whether the render target contents were preserved after the
  // target was displayed via eglSwapBuffers().
  bool ContentWasPreservedAfterSwap() const {
    return content_preserved_on_swap_ && !content_cleared_on_swap_;
  }

  int64 swap_count() { return swap_count_; }
  void increment_swap_count() { ++swap_count_; }

  bool has_been_made_current() const { return has_been_made_current_; }
  void set_has_been_made_current() { has_been_made_current_ = true; }

  bool is_surface_bad() const { return is_surface_bad_; }
  void set_surface_bad() { is_surface_bad_ = true; }

  void set_cleared_on_swap(bool cleared) { content_cleared_on_swap_ = cleared; }

 protected:
  virtual ~RenderTargetEGL() {}

  int64 swap_count_;
  bool has_been_made_current_;
  bool content_preserved_on_swap_;
  bool content_cleared_on_swap_;
  bool is_surface_bad_;
};

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_BACKEND_EGL_RENDER_TARGET_H_
