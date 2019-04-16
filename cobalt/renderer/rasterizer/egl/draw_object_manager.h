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

#ifndef COBALT_RENDERER_RASTERIZER_EGL_DRAW_OBJECT_MANAGER_H_
#define COBALT_RENDERER_RASTERIZER_EGL_DRAW_OBJECT_MANAGER_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/callback.h"
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
    kBlendExternal = 1,

    // These draws use the native rasterizer, and the appropriate state must
    // be set during execution.
    kBlendSrcAlpha = 2,
    kBlendNone = 4,

    // This blend mode allows more flexibility when merging draw calls. During
    // rasterization, this type translates to kBlendNone, so make sure this
    // enum sorts next to kBlendNone to avoid unnecessary state changes.
    kBlendNoneOrSrcAlpha = kBlendNone | kBlendSrcAlpha,
  };

  DrawObjectManager(const base::Closure& reset_external_rasterizer,
                    const base::Closure& flush_external_offscreen_draws);

  // Add a draw object which uses an external rasterizer on an offscreen render
  // target. These draws are processed before all other offscreen draws. After
  // execution of these draws, |flush_external_offscreen_draws| is called, so
  // these batched draws do not need to flush individually.
  // Returns an ID which can be used to remove the queued draw.
  uint32_t AddBatchedExternalDraw(std::unique_ptr<DrawObject> draw_object,
                                  base::TypeId draw_type,
                                  const backend::RenderTarget* render_target,
                                  const math::RectF& draw_bounds);

  // Add a draw object that will render to an offscreen render target. There
  // must be a corresponding draw object added via AddOnscreenDraw() to
  // render the contents onto the main render target. All offscreen draws are
  // batched together and executed before any onscreen objects are processed
  // in order to minimize the cost of switching render targets.
  // Returns an ID which can be used to remove the queued draw.
  uint32_t AddOffscreenDraw(std::unique_ptr<DrawObject> draw_object,
                            BlendType blend_type, base::TypeId draw_type,
                            const backend::RenderTarget* render_target,
                            const math::RectF& draw_bounds);

  // Add a draw object to be processed when rendering to the main render
  // target. Although most draws are expected to go to the main render target,
  // some draws may touch offscreen targets (e.g. when those offscreen targets
  // are reused during the frame, so their contents must be rasterized just
  // before being used for the main render target). Switching render targets
  // has a major negative impact to performance, so it is preferable to avoid
  // reusing offscreen targets during the frame.
  // Returns an ID which can be used to remove the queued draw.
  uint32_t AddOnscreenDraw(std::unique_ptr<DrawObject> draw_object,
                           BlendType blend_type, base::TypeId draw_type,
                           const backend::RenderTarget* render_target,
                           const math::RectF& draw_bounds);

  // Remove all queued draws whose ID comes after the given last_valid_draw_id.
  // Calling RemoveDraws(0) will remove all draws that have been added.
  void RemoveDraws(uint32_t last_valid_draw_id);

  // Tell the draw object manager that the given render target has a dependency
  // on draws to another render target. This information is used to sort
  // offscreen draws.
  void AddRenderTargetDependency(const backend::RenderTarget* draw_target,
                                 const backend::RenderTarget* required_target);

  void ExecuteOffscreenRasterize(GraphicsState* graphics_state,
                                 ShaderProgramManager* program_manager);
  void ExecuteOnscreenRasterize(GraphicsState* graphics_state,
                                ShaderProgramManager* program_manager);

 private:
  struct DrawInfo {
    DrawInfo(std::unique_ptr<DrawObject> in_draw_object,
             base::TypeId in_draw_type, BlendType in_blend_type,
             const backend::RenderTarget* in_render_target,
             const math::RectF& in_draw_bounds, uint32_t in_draw_id)
        : draw_object(in_draw_object.release()),
          render_target(in_render_target),
          draw_bounds(in_draw_bounds),
          draw_type(in_draw_type),
          blend_type(in_blend_type),
          draw_id(in_draw_id) {}
    std::unique_ptr<DrawObject> draw_object;
    const backend::RenderTarget* render_target;
    math::RectF draw_bounds;
    base::TypeId draw_type;
    BlendType blend_type;
    union {
      uint32_t draw_id;
      uint32_t dependencies;
    };
  };

  struct RenderTargetDependency {
    RenderTargetDependency(const backend::RenderTarget* in_draw_target,
                           const backend::RenderTarget* in_required_target)
        : draw_target(in_draw_target), required_target(in_required_target) {}
    const backend::RenderTarget* draw_target;
    const backend::RenderTarget* required_target;
  };

  typedef std::vector<DrawInfo> DrawList;
  typedef std::vector<DrawInfo*> SortedDrawList;

  void ExecuteUpdateVertexBuffer(GraphicsState* graphics_state,
                                 ShaderProgramManager* program_manager);

  void Rasterize(const SortedDrawList& draw_list, GraphicsState* graphics_state,
                 ShaderProgramManager* program_manager);

  void RemoveDraws(DrawList* draw_list, uint32_t last_valid_draw_id);

  void SortOffscreenDraws(DrawList* draw_list,
                          SortedDrawList* sorted_draw_list);
  void SortOnscreenDraws(DrawList* draw_list, SortedDrawList* sorted_draw_list);
  void MergeSortedDraws(SortedDrawList* sorted_draw_list);

  base::Closure reset_external_rasterizer_;
  base::Closure flush_external_offscreen_draws_;

  DrawList onscreen_draws_;
  DrawList offscreen_draws_;
  DrawList external_offscreen_draws_;

  SortedDrawList sorted_onscreen_draws_;
  SortedDrawList sorted_offscreen_draws_;
  SortedDrawList sorted_external_offscreen_draws_;

  std::vector<RenderTargetDependency> draw_dependencies_;
  std::unordered_map<int32_t, uint32_t> dependency_count_;

  uint32_t current_draw_id_;
};

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_EGL_DRAW_OBJECT_MANAGER_H_
