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

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

void DrawObjectManager::AddDraw(scoped_ptr<DrawObject> object,
                                BlendType onscreen_blend,
                                OnscreenType onscreen_type,
                                OffscreenType offscreen_type,
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

  if (offscreen_type != kOffscreenNone) {
    offscreen_order_[offscreen_type].push_back(object.get());
  }

  object_info_.insert(object_info_.begin() + position,
                      ObjectInfo(onscreen_type, onscreen_blend, bounds));
  draw_objects_.insert(draw_objects_.begin() + position, object.release());
}

void DrawObjectManager::ExecuteOffscreenRasterize(GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  for (int type = 0; type < kOffscreenCount; ++type) {
    for (size_t index = 0; index < offscreen_order_[type].size(); ++index) {
      offscreen_order_[type][index]->ExecuteOffscreenRasterize(graphics_state,
          program_manager);
    }
  }
}

void DrawObjectManager::ExecuteOnscreenUpdateVertexBuffer(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  for (size_t index = 0; index < draw_objects_.size(); ++index) {
    draw_objects_[index]->ExecuteOnscreenUpdateVertexBuffer(
        graphics_state, program_manager);
  }
}

void DrawObjectManager::ExecuteOnscreenRasterize(GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  for (size_t index = 0; index < draw_objects_.size(); ++index) {
    if (object_info_[index].blend == kBlendNone) {
      graphics_state->DisableBlend();
    } else {
      graphics_state->EnableBlend();
    }

    draw_objects_[index]->ExecuteOnscreenRasterize(
        graphics_state, program_manager);
  }
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
