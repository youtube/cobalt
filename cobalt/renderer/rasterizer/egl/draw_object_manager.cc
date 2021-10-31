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

#include "starboard/configuration.h"
#if SB_API_VERSION >= 12 || SB_HAS(GLES2)

#include "cobalt/renderer/rasterizer/egl/draw_object_manager.h"

#include <algorithm>
#include <memory>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

DrawObjectManager::DrawObjectManager(
    const base::Closure& reset_external_rasterizer,
    const base::Closure& flush_external_offscreen_draws)
    : reset_external_rasterizer_(reset_external_rasterizer),
      flush_external_offscreen_draws_(flush_external_offscreen_draws),
      current_draw_id_(0) {}

uint32_t DrawObjectManager::AddBatchedExternalDraw(
    std::unique_ptr<DrawObject> draw_object, base::TypeId draw_type,
    const backend::RenderTarget* render_target,
    const math::RectF& draw_bounds) {
  external_offscreen_draws_.emplace_back(std::move(draw_object), draw_type,
                                         kBlendExternal, render_target,
                                         draw_bounds, ++current_draw_id_);
  return current_draw_id_;
}

uint32_t DrawObjectManager::AddOnscreenDraw(
    std::unique_ptr<DrawObject> draw_object, BlendType blend_type,
    base::TypeId draw_type, const backend::RenderTarget* render_target,
    const math::RectF& draw_bounds) {
  // See if this draw object can be merged with the last one.
  if (!onscreen_draws_.empty()) {
    DrawInfo& last_draw = onscreen_draws_.back();
    if (last_draw.render_target == render_target &&
        last_draw.draw_type == draw_type &&
        (last_draw.blend_type & blend_type) != 0 &&
        last_draw.draw_object->TryMerge(draw_object.get())) {
      last_draw.draw_bounds.Union(draw_bounds);
      last_draw.blend_type =
          static_cast<BlendType>(last_draw.blend_type & blend_type);
      last_draw.draw_id = ++current_draw_id_;
      return current_draw_id_;
    }
  }

  onscreen_draws_.emplace_back(std::move(draw_object), draw_type, blend_type,
                               render_target, draw_bounds, ++current_draw_id_);
  return current_draw_id_;
}

uint32_t DrawObjectManager::AddOffscreenDraw(
    std::unique_ptr<DrawObject> draw_object, BlendType blend_type,
    base::TypeId draw_type, const backend::RenderTarget* render_target,
    const math::RectF& draw_bounds) {
  // See if this draw object can be merged with the last one.
  if (!offscreen_draws_.empty()) {
    DrawInfo& last_draw = offscreen_draws_.back();
    if (last_draw.render_target == render_target &&
        last_draw.draw_type == draw_type &&
        (last_draw.blend_type & blend_type) != 0 &&
        last_draw.draw_object->TryMerge(draw_object.get())) {
      last_draw.draw_bounds.Union(draw_bounds);
      last_draw.blend_type =
          static_cast<BlendType>(last_draw.blend_type & blend_type);
      last_draw.draw_id = ++current_draw_id_;
      return current_draw_id_;
    }
  }

  offscreen_draws_.emplace_back(std::move(draw_object), draw_type, blend_type,
                                render_target, draw_bounds, ++current_draw_id_);
  return current_draw_id_;
}

void DrawObjectManager::RemoveDraws(uint32_t last_valid_draw_id) {
  TRACE_EVENT0("cobalt::renderer", "RemoveDraws");
  RemoveDraws(&onscreen_draws_, last_valid_draw_id);
  RemoveDraws(&offscreen_draws_, last_valid_draw_id);
  RemoveDraws(&external_offscreen_draws_, last_valid_draw_id);
}

void DrawObjectManager::RemoveDraws(DrawList* draw_list,
                                    uint32_t last_valid_draw_id) {
  // Objects in the draw list should have ascending draw IDs at this point.
  auto iter = draw_list->end();
  for (; iter != draw_list->begin(); --iter) {
    if ((iter - 1)->draw_id <= last_valid_draw_id) {
      break;
    }
  }
  if (iter != draw_list->end()) {
    draw_list->erase(iter, draw_list->end());
  }
}

void DrawObjectManager::AddRenderTargetDependency(
    const backend::RenderTarget* draw_target,
    const backend::RenderTarget* required_target) {
  // Check for circular dependencies.
  DCHECK(draw_target != required_target);

  // There should be very few unique render target dependencies, so just keep
  // them in a vector for simplicity.
  for (auto dep = draw_dependencies_.begin();; ++dep) {
    if (dep == draw_dependencies_.end()) {
      draw_dependencies_.emplace_back(draw_target, required_target);
      auto count = dependency_count_.find(draw_target->GetSerialNumber());
      if (count != dependency_count_.end()) {
        count->second += 1;
      } else {
        dependency_count_.emplace(draw_target->GetSerialNumber(), 1);
      }
      break;
    }
    if (dep->draw_target == draw_target &&
        dep->required_target == required_target) {
      return;
    }
  }

  // Propagate dependencies upward so that targets which depend on
  // |draw_target| will also depend on |required_target|.
  size_t count = draw_dependencies_.size();
  for (size_t index = 0; index < count; ++index) {
    if (draw_dependencies_[index].required_target == draw_target) {
      AddRenderTargetDependency(draw_dependencies_[index].draw_target,
                                required_target);
    }
  }
}

void DrawObjectManager::ExecuteOffscreenRasterize(
    GraphicsState* graphics_state, ShaderProgramManager* program_manager) {
  SortOffscreenDraws(&external_offscreen_draws_,
                     &sorted_external_offscreen_draws_);

  // Process draws handled by an external rasterizer.
  {
    TRACE_EVENT0("cobalt::renderer", "OffscreenExternalRasterizer");
    for (const DrawInfo* draw : sorted_external_offscreen_draws_) {
      // Batched external draws should not have any dependencies.
      DCHECK_EQ(draw->dependencies, 0);
      draw->draw_object->ExecuteRasterize(graphics_state, program_manager);
    }
    if (!flush_external_offscreen_draws_.is_null()) {
      flush_external_offscreen_draws_.Run();
    }
  }
  graphics_state->SetDirty();

  SortOffscreenDraws(&offscreen_draws_, &sorted_offscreen_draws_);
  SortOnscreenDraws(&onscreen_draws_, &sorted_onscreen_draws_);
  MergeSortedDraws(&sorted_offscreen_draws_);
  MergeSortedDraws(&sorted_onscreen_draws_);

  // Update the vertex buffer for all draws.
  {
    TRACE_EVENT0("cobalt::renderer", "UpdateVertexBuffer");
    ExecuteUpdateVertexBuffer(graphics_state, program_manager);
  }

  // Process the native offscreen draws.
  {
    TRACE_EVENT0("cobalt::renderer", "OffscreenNativeRasterizer");
    Rasterize(sorted_offscreen_draws_, graphics_state, program_manager);
  }
}

void DrawObjectManager::ExecuteUpdateVertexBuffer(
    GraphicsState* graphics_state, ShaderProgramManager* program_manager) {
  for (const DrawInfo* draw : sorted_offscreen_draws_) {
    draw->draw_object->ExecuteUpdateVertexBuffer(graphics_state,
                                                 program_manager);
  }
  for (const DrawInfo* draw : sorted_onscreen_draws_) {
    draw->draw_object->ExecuteUpdateVertexBuffer(graphics_state,
                                                 program_manager);
  }
  graphics_state->UpdateVertexBuffers();
}

void DrawObjectManager::ExecuteOnscreenRasterize(
    GraphicsState* graphics_state, ShaderProgramManager* program_manager) {
  Rasterize(sorted_onscreen_draws_, graphics_state, program_manager);
}

void DrawObjectManager::Rasterize(const SortedDrawList& sorted_draw_list,
                                  GraphicsState* graphics_state,
                                  ShaderProgramManager* program_manager) {
  const backend::RenderTarget* current_target = nullptr;
  bool using_native_rasterizer = true;

  // Starting from an unknown state.
  graphics_state->SetDirty();

  for (const DrawInfo* draw : sorted_draw_list) {
    bool draw_uses_native_rasterizer = draw->blend_type != kBlendExternal;

    if (draw_uses_native_rasterizer) {
      if (!using_native_rasterizer) {
        // Transitioning from external to native rasterizer. Set the native
        // rasterizer's state as dirty.
        graphics_state->SetDirty();
      }

      if (draw->render_target != current_target) {
        current_target = draw->render_target;
        graphics_state->BindFramebuffer(current_target);
        graphics_state->Viewport(0, 0, current_target->GetSize().width(),
                                 current_target->GetSize().height());
      }

      if ((draw->blend_type & kBlendNone) != 0) {
        graphics_state->DisableBlend();
      } else if ((draw->blend_type & kBlendSrcAlpha) != 0) {
        graphics_state->EnableBlend();
      } else {
        NOTREACHED() << "Unsupported blend type";
      }
    } else {
      if (using_native_rasterizer) {
        // Transitioning from native to external rasterizer. Set the external
        // rasterizer's state as dirty.
        if (!reset_external_rasterizer_.is_null()) {
          reset_external_rasterizer_.Run();
        }
      }
    }

    draw->draw_object->ExecuteRasterize(graphics_state, program_manager);
    using_native_rasterizer = draw_uses_native_rasterizer;
  }
}

void DrawObjectManager::SortOffscreenDraws(DrawList* draw_list,
                                           SortedDrawList* sorted_draw_list) {
  TRACE_EVENT0("cobalt::renderer", "SortOffscreenDraws");

  // Sort offscreen draws to minimize GPU state changes.
  sorted_draw_list->reserve(draw_list->size());
  for (size_t draw_pos = 0; draw_pos < draw_list->size(); ++draw_pos) {
    auto* draw = &draw_list->at(draw_pos);
    bool draw_uses_native_rasterizer = draw->blend_type != kBlendExternal;
    bool next_uses_native_rasterizer =
        draw_pos + 1 < draw_list->size() &&
        draw_list->at(draw_pos + 1).blend_type != kBlendExternal;
    auto dependencies =
        dependency_count_.find(draw->render_target->GetSerialNumber());
    draw->dependencies =
        dependencies != dependency_count_.end() ? dependencies->second : 0;

    // Find an appropriate sort position for the current draw.
    auto sort_pos = draw_pos;
    for (; sort_pos > 0; --sort_pos) {
      auto* prev_draw = sorted_draw_list->at(sort_pos - 1);

      // Unlike onscreen draws, offscreen draws should be grouped by render
      // target. Ensure that render targets with fewer dependencies are first
      // in the draw list.
      if (prev_draw->dependencies > draw->dependencies) {
        continue;
      } else if (prev_draw->dependencies < draw->dependencies) {
        break;
      }
      if (prev_draw->render_target > draw->render_target) {
        continue;
      } else if (prev_draw->render_target < draw->render_target) {
        break;
      }

      // The rest of the sorting logic is the same between onscreen and
      // offscreen draws.
      if (prev_draw->draw_bounds.Intersects(draw->draw_bounds)) {
        break;
      }

      // Group native vs. non-native draws together.
      bool prev_uses_native_rasterizer =
          prev_draw->blend_type != kBlendExternal;
      if (draw_uses_native_rasterizer != next_uses_native_rasterizer &&
          draw_uses_native_rasterizer != prev_uses_native_rasterizer) {
        next_uses_native_rasterizer = prev_uses_native_rasterizer;
        continue;
      }
      if (draw_uses_native_rasterizer == next_uses_native_rasterizer &&
          draw_uses_native_rasterizer != prev_uses_native_rasterizer) {
        break;
      }
      next_uses_native_rasterizer = prev_uses_native_rasterizer;

      if (prev_draw->draw_type > draw->draw_type) {
        continue;
      } else if (prev_draw->draw_type < draw->draw_type) {
        break;
      }

      if (prev_draw->blend_type <= draw->blend_type) {
        break;
      }
    }

    sorted_draw_list->insert(sorted_draw_list->begin() + sort_pos, draw);
  }
}

void DrawObjectManager::SortOnscreenDraws(DrawList* draw_list,
                                          SortedDrawList* sorted_draw_list) {
  TRACE_EVENT0("cobalt::renderer", "SortOnscreenDraws");

  // Sort onscreen draws to minimize GPU state changes.
  sorted_draw_list->reserve(draw_list->size());
  for (size_t draw_pos = 0; draw_pos < draw_list->size(); ++draw_pos) {
    auto* draw = &draw_list->at(draw_pos);
    bool draw_uses_native_rasterizer = draw->blend_type != kBlendExternal;
    bool next_uses_native_rasterizer =
        draw_pos + 1 < draw_list->size() &&
        draw_list->at(draw_pos + 1).blend_type != kBlendExternal;

    // Find an appropriate sort position for the current draw.
    auto sort_pos = draw_pos;
    for (; sort_pos > 0; --sort_pos) {
      auto* prev_draw = sorted_draw_list->at(sort_pos - 1);

      // Do not sort across different render targets since their contents may
      // be generated just before consumed by a subsequent draw.
      if (prev_draw->render_target != draw->render_target) {
        break;
      }

      // The rest of the sorting logic is the same between onscreen and
      // offscreen draws.
      if (prev_draw->draw_bounds.Intersects(draw->draw_bounds)) {
        break;
      }

      // Group native vs. non-native draws together.
      bool prev_uses_native_rasterizer =
          prev_draw->blend_type != kBlendExternal;
      if (draw_uses_native_rasterizer != next_uses_native_rasterizer &&
          draw_uses_native_rasterizer != prev_uses_native_rasterizer) {
        next_uses_native_rasterizer = prev_uses_native_rasterizer;
        continue;
      }
      if (draw_uses_native_rasterizer == next_uses_native_rasterizer &&
          draw_uses_native_rasterizer != prev_uses_native_rasterizer) {
        break;
      }
      next_uses_native_rasterizer = prev_uses_native_rasterizer;

      if (prev_draw->draw_type > draw->draw_type) {
        continue;
      } else if (prev_draw->draw_type < draw->draw_type) {
        break;
      }

      if (prev_draw->blend_type <= draw->blend_type) {
        break;
      }
    }

    sorted_draw_list->insert(sorted_draw_list->begin() + sort_pos, draw);
  }
}

void DrawObjectManager::MergeSortedDraws(SortedDrawList* sorted_draw_list) {
  if (sorted_draw_list->size() <= 1) {
    return;
  }

  SortedDrawList old_draw_list(std::move(*sorted_draw_list));
  sorted_draw_list->clear();
  sorted_draw_list->reserve(old_draw_list.size());

  sorted_draw_list->emplace_back(old_draw_list[0]);
  for (size_t i = 1; i < old_draw_list.size(); ++i) {
    DrawInfo* last_draw = sorted_draw_list->back();
    DrawInfo* next_draw = old_draw_list[i];
    if (last_draw->render_target == next_draw->render_target &&
        last_draw->draw_type == next_draw->draw_type &&
        (last_draw->blend_type & next_draw->blend_type) != 0 &&
        last_draw->draw_object->TryMerge(next_draw->draw_object.get())) {
      last_draw->blend_type =
          static_cast<BlendType>(last_draw->blend_type & next_draw->blend_type);
    } else {
      sorted_draw_list->emplace_back(next_draw);
    }
  }
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION >= 12 || SB_HAS(GLES2)
