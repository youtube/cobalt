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
                                      DrawType type) {
  draw_objects_[type].push_back(object.release());
}

void DrawObjectManager::AddTransparentDraw(scoped_ptr<DrawObject> object,
                                           DrawType type,
                                           const math::RectF& bounds) {
  // Try to sort the transparent object next to another object of its type.
  // However, this can only be done as long as its bounds do not overlap with
  // the other object while swapping draw order.
  size_t position = transparent_object_info_.size();
  while (position > 0) {
    if (transparent_object_info_[position - 1].type <= type) {
      break;
    }
    if (transparent_object_info_[position - 1].bounds.Intersects(bounds)) {
      break;
    }
    --position;
  }

  transparent_object_info_.insert(
      transparent_object_info_.begin() + position,
      TransparentObjectInfo(type, bounds));
  draw_objects_[kDrawTransparent].insert(
      draw_objects_[kDrawTransparent].begin() + position,
      object.release());
}

void DrawObjectManager::ExecutePreVertexBuffer(GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  for (int type = 0; type < kDrawCount; ++type) {
    for (size_t index = 0; index < draw_objects_[type].size(); ++index) {
      draw_objects_[type][index]->ExecutePreVertexBuffer(graphics_state,
          program_manager);
    }
  }
}

void DrawObjectManager::ExecuteUpdateVertexBuffer(GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  for (int type = 0; type < kDrawCount; ++type) {
    for (size_t index = 0; index < draw_objects_[type].size(); ++index) {
      draw_objects_[type][index]->ExecuteUpdateVertexBuffer(graphics_state,
          program_manager);
    }
  }
}

void DrawObjectManager::ExecuteRasterizeNormal(GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  for (int type = 0; type < kDrawCount; ++type) {
    if (type == kDrawTransparent) {
      graphics_state->EnableBlend();
      graphics_state->DisableDepthWrite();
    } else {
      graphics_state->DisableBlend();
      graphics_state->EnableDepthWrite();
    }

    for (size_t index = 0; index < draw_objects_[type].size(); ++index) {
      draw_objects_[type][index]->ExecuteRasterizeNormal(graphics_state,
          program_manager);
    }
  }
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
