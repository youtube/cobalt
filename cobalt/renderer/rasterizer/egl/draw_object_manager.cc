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

#include "cobalt/renderer/rasterizer/egl/draw_object_manager.h"

#include "base/logging.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/renderer/rasterizer/egl/draw_callback.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

void DrawObjectManager::AddOnscreenDraw(scoped_ptr<DrawObject> object,
    BlendType onscreen_blend, base::TypeId onscreen_type,
    const math::RectF& bounds) {
  // Try to sort the object next to another object of its type. However, this
  // can only be done as long as its bounds do not overlap with the other
  // object while swapping draw order.
  size_t position = draw_objects_.size();
  while (position > 0) {
    if (object_info_[position - 1].type <= onscreen_type) {
      break;
    }
    if (object_info_[position - 1].bounds.Intersects(bounds)) {
      break;
    }
    --position;
  }

  object_info_.insert(object_info_.begin() + position,
                      ObjectInfo(onscreen_type, onscreen_blend, bounds));
  draw_objects_.insert(draw_objects_.begin() + position, object.release());
}

void DrawObjectManager::AddOffscreenDraw(scoped_ptr<DrawObject> draw_object,
    BlendType blend_type, base::TypeId draw_type,
    const backend::RenderTarget* render_target,
    const math::RectF& draw_bounds) {
  // Put all draws using kBlendExternal into their own draw list since they
  // use an external rasterizer which tracks its own state.
  auto* draw_list = &offscreen_draws_;
  if (blend_type == kBlendExternal) {
    draw_list = &external_offscreen_draws_;

    // Only the DrawCallback type should use kBlendExternal. All other draw
    // object types use the native rasterizer.
    DCHECK(base::polymorphic_downcast<DrawCallback*>(draw_object.get()));
  }

  // Sort draws to minimize GPU state changes.
  size_t position = draw_list->size();
  for (; position > 0; --position) {
    const OffscreenDrawInfo& prev_draw = (*draw_list)[position - 1];

    if (prev_draw.render_target > render_target) {
      continue;
    } else if (prev_draw.render_target < render_target) {
      break;
    }

    if (prev_draw.draw_bounds.Intersects(draw_bounds)) {
      break;
    }

    if (prev_draw.draw_type > draw_type) {
      continue;
    } else if (prev_draw.draw_type < draw_type) {
      break;
    }

    if (prev_draw.blend_type <= blend_type) {
      break;
    }
  }

  draw_list->insert(draw_list->begin() + position,
      OffscreenDrawInfo(draw_object.Pass(), draw_type, blend_type,
                        render_target, draw_bounds));
}

void DrawObjectManager::ExecuteOffscreenRasterize(GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  // Process draws handled by an external rasterizer.
  for (size_t index = 0; index < external_offscreen_draws_.size(); ++index) {
    external_offscreen_draws_[index].draw_object->ExecuteRasterize(
        graphics_state, program_manager);
  }

  // TODO: Process native rasterizer's draws.
  DCHECK_EQ(0, offscreen_draws_.size());
}

void DrawObjectManager::ExecuteOnscreenUpdateVertexBuffer(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  for (size_t index = 0; index < offscreen_draws_.size(); ++index) {
    offscreen_draws_[index].draw_object->ExecuteUpdateVertexBuffer(
        graphics_state, program_manager);
  }
  for (size_t index = 0; index < draw_objects_.size(); ++index) {
    draw_objects_[index]->ExecuteUpdateVertexBuffer(
        graphics_state, program_manager);
  }
}

void DrawObjectManager::ExecuteOnscreenRasterize(GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  for (size_t index = 0; index < draw_objects_.size(); ++index) {
    if (object_info_[index].blend == kBlendNone) {
      graphics_state->DisableBlend();
    } else if (object_info_[index].blend == kBlendSrcAlpha) {
      graphics_state->EnableBlend();
    }

    draw_objects_[index]->ExecuteRasterize(
        graphics_state, program_manager);
  }
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
