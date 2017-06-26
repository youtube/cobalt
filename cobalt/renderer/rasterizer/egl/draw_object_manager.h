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

#ifndef COBALT_RENDERER_RASTERIZER_EGL_DRAW_OBJECT_MANAGER_H_
#define COBALT_RENDERER_RASTERIZER_EGL_DRAW_OBJECT_MANAGER_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "cobalt/base/type_id.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/renderer/backend/render_target.h"
#include "cobalt/renderer/rasterizer/egl/draw_object.h"
#include "cobalt/renderer/rasterizer/egl/graphics_state.h"
#include "cobalt/renderer/rasterizer/egl/shader_program_manager.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

// Helper class to manage a set of draw objects. This facilitates sorting the
// objects to minimize GPU state changes.
class DrawObjectManager {
 public:
  enum BlendType {
    // These draws use an external rasterizer which sets the GPU state.
    kBlendExternal = 0,

    // These draws use the native rasterizer, and the appropriate state must
    // be set during execution.
    kBlendNone,
    kBlendSrcAlpha,
  };

  // Add a draw object to be processed when rendering to the main render
  // target. Although most draws are expected to go to the main render target,
  // some draws may touch offscreen targets (e.g. when those offscreen targets
  // are reused during the frame, so their contents must be rasterized just
  // before being used for the main render target). Switching render targets
  // has a major negative impact to performance, so it is preferable to avoid
  // reusing offscreen targets during the frame.
  void AddOnscreenDraw(scoped_ptr<DrawObject> object,
      BlendType onscreen_blend, base::TypeId draw_type,
      const math::RectF& bounds);

  // Add a draw object that will render to an offscreen render target. There
  // must be a corresponding draw object added via AddOnscreenDraw() to
  // render the contents onto the main render target. All offscreen draws are
  // batched together and executed before any onscreen objects are processed
  // in order to minimize the cost of switching render targets.
  void AddOffscreenDraw(scoped_ptr<DrawObject> draw_object,
      BlendType blend_type, base::TypeId draw_type,
      const backend::RenderTarget* render_target,
      const math::RectF& draw_bounds);

  void ExecuteOffscreenRasterize(GraphicsState* graphics_state,
      ShaderProgramManager* program_manager);
  void ExecuteOnscreenUpdateVertexBuffer(GraphicsState* graphics_state,
      ShaderProgramManager* program_manager);
  void ExecuteOnscreenRasterize(GraphicsState* graphics_state,
      ShaderProgramManager* program_manager);

 private:
  struct ObjectInfo {
    ObjectInfo(base::TypeId onscreen_type,
               BlendType blend_type,
               const math::RectF& object_bounds)
        : bounds(object_bounds),
          type(onscreen_type),
          blend(blend_type) {}
    math::RectF bounds;
    base::TypeId type;
    BlendType blend;
  };

  struct OffscreenDrawInfo {
    OffscreenDrawInfo(scoped_ptr<DrawObject> in_draw_object,
                      base::TypeId in_draw_type, BlendType in_blend_type,
                      const backend::RenderTarget* in_render_target,
                      const math::RectF& in_draw_bounds)
        : draw_object(in_draw_object.release()),
          render_target(in_render_target),
          draw_bounds(in_draw_bounds),
          draw_type(in_draw_type),
          blend_type(in_blend_type) {}
    std::unique_ptr<DrawObject> draw_object;
    const backend::RenderTarget* render_target;
    math::RectF draw_bounds;
    base::TypeId draw_type;
    BlendType blend_type;
  };

  ScopedVector<DrawObject> draw_objects_;
  std::vector<ObjectInfo> object_info_;

  std::vector<OffscreenDrawInfo> offscreen_draws_;
  std::vector<OffscreenDrawInfo> external_offscreen_draws_;
};

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_EGL_DRAW_OBJECT_MANAGER_H_
