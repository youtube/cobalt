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

#include "cobalt/renderer/rasterizer/egl/draw_object.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <algorithm>
#include <limits>

#include "cobalt/renderer/backend/egl/utils.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

DrawObject::BaseState::BaseState()
    : transform(math::Matrix3F::Identity()),
      scissor(0, 0,
              std::numeric_limits<int>::max(),
              std::numeric_limits<int>::max()),
      opacity(1.0f) {}

DrawObject::BaseState::BaseState(const BaseState& other)
    : transform(other.transform),
      scissor(other.scissor),
      opacity(other.opacity) {}

DrawObject::DrawObject(const BaseState& base_state)
    : base_state_(base_state) {}

// static
uint32_t DrawObject::GetGLRGBA(float r, float g, float b, float a) {
  // Ensure color bytes represent RGBA, regardless of endianness.
  union {
    uint32_t color32;
    uint8_t color8[4];
  } color_data;
  color_data.color8[0] = static_cast<uint8_t>(r * 255.0f);
  color_data.color8[1] = static_cast<uint8_t>(g * 255.0f);
  color_data.color8[2] = static_cast<uint8_t>(b * 255.0f);
  color_data.color8[3] = static_cast<uint8_t>(a * 255.0f);
  return color_data.color32;
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
