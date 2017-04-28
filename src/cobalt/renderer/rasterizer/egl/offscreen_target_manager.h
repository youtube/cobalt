// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_EGL_OFFSCREEN_TARGET_MANAGER_H_
#define COBALT_RENDERER_RASTERIZER_EGL_OFFSCREEN_TARGET_MANAGER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/math/size.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/renderer/backend/egl/framebuffer.h"
#include "cobalt/renderer/backend/egl/graphics_context.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

// Manage allocating and caching offscreen render targets for render_tree::Node
// rendering. This uses atlases instead of individual render targets to
// minimize the cost of switching render targets.
class OffscreenTargetManager {
 public:
  struct TargetInfo {
    TargetInfo()
        : framebuffer(NULL),
          skia_canvas(NULL),
          is_scratch_surface(false) {}
    backend::FramebufferEGL* framebuffer;
    SkCanvas* skia_canvas;
    math::RectF region;
    bool is_scratch_surface;
  };

  OffscreenTargetManager(backend::GraphicsContextEGL* graphics_context,
                         GrContext* skia_context, size_t memory_limit);
  ~OffscreenTargetManager();

  // Update must be called once per frame, before any allocation requests are
  // made for that frame.
  void Update(const math::Size& frame_size);

  // Flush all render targets that have been used thus far.
  void Flush();

  // Return whether a cached version of the requested render target is
  // available. If a cache does exist, then the output parameters are set,
  // otherwise, they are untouched.
  // The returned values are only valid until the next call to Update().
  bool GetCachedOffscreenTarget(
      const render_tree::Node* node, const math::SizeF& size,
      TargetInfo* out_target_info);

  // Allocate an offscreen target of the specified size.
  // The returned values are only valid until the next call to Update().
  void AllocateOffscreenTarget(
      const render_tree::Node* node, const math::SizeF& size,
      TargetInfo* out_target_info);

 private:
  // Use an atlas for offscreen targets.
  struct OffscreenAtlas;

  void InitializeTargets(const math::Size& frame_size);
  OffscreenAtlas* CreateOffscreenAtlas(const math::Size& size);

  backend::GraphicsContextEGL* graphics_context_;
  GrContext* skia_context_;

  ScopedVector<OffscreenAtlas> offscreen_atlases_;
  scoped_ptr<OffscreenAtlas> offscreen_cache_;
  scoped_ptr<OffscreenAtlas> scratch_surface_;

  // Align offscreen targets to a particular size to more efficiently use the
  // offscreen target atlas. Use a power of 2 for the alignment so that a bit
  // mask can be used for the alignment calculation.
  math::Size offscreen_target_size_mask_;

  // Maximum number of bytes that can be used for offscreen atlases.
  size_t memory_limit_;
};

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_EGL_OFFSCREEN_TARGET_MANAGER_H_
