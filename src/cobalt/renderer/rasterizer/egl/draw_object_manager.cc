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

void DrawObjectManager::AddOpaqueDraw(scoped_ptr<DrawObject> object,
                                      OnscreenType onscreen_type,
                                      OffscreenType offscreen_type) {
  if (offscreen_type != kOffscreenNone) {
    offscreen_order_[offscreen_type].push_back(object.get());
  }

  draw_objects_[onscreen_type].push_back(object.release());
}

void DrawObjectManager::AddTransparentDraw(scoped_ptr<DrawObject> object,
                                           OnscreenType onscreen_type,
                                           OffscreenType offscreen_type,
                                           const math::RectF& bounds) {
  // Try to sort the transparent object next to another object of its type.
  // However, this can only be done as long as its bounds do not overlap with
  // the other object while swapping draw order.
  size_t position = transparent_object_info_.size();
  while (position > 0) {
    if (transparent_object_info_[position - 1].type <= onscreen_type) {
      break;
    }
    if (transparent_object_info_[position - 1].bounds.Intersects(bounds)) {
      break;
    }
    --position;
  }

  if (offscreen_type != kOffscreenNone) {
    offscreen_order_[offscreen_type].push_back(object.get());
  }

  transparent_object_info_.insert(
      transparent_object_info_.begin() + position,
      TransparentObjectInfo(onscreen_type, bounds));
  draw_objects_[kOnscreenTransparent].insert(
      draw_objects_[kOnscreenTransparent].begin() + position,
      object.release());
}

void DrawObjectManager::ExecuteOffscreenRasterize(GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  graphics_state->DisableDepthTest();
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
  for (int type = 0; type < kOnscreenCount; ++type) {
    for (size_t index = 0; index < draw_objects_[type].size(); ++index) {
      draw_objects_[type][index]->ExecuteOnscreenUpdateVertexBuffer(
          graphics_state, program_manager);
    }
  }
}

void DrawObjectManager::ExecuteOnscreenRasterize(GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  graphics_state->EnableDepthTest();

  for (int type = 0; type < kOnscreenCount; ++type) {
    if (type == kOnscreenTransparent) {
      graphics_state->EnableBlend();
      graphics_state->DisableDepthWrite();

      // Transparent objects must be drawn in the order they were added to
      // maintain correctness.
      for (size_t index = 0; index < draw_objects_[type].size(); ++index) {
        draw_objects_[type][index]->ExecuteOnscreenRasterize(graphics_state,
            program_manager);
      }
    } else {
      graphics_state->DisableBlend();
      graphics_state->EnableDepthWrite();

      // Opaque objects can be drawn in front-to-back order to take advantage
      // of z-culling.
      for (size_t index = draw_objects_[type].size(); index > 0;) {
        --index;
        draw_objects_[type][index]->ExecuteOnscreenRasterize(graphics_state,
            program_manager);
      }
    }
  }
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
