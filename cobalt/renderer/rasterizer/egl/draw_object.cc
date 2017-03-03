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
      scissor(0.0f, 0.0f,
              std::numeric_limits<float>::max(),
              std::numeric_limits<float>::max()),
      depth(GraphicsState::FarthestDepth()),
      opacity(1.0f) {}

DrawObject::BaseState::BaseState(const BaseState& other)
    : transform(other.transform),
      scissor(other.scissor),
      depth(other.depth),
      opacity(other.opacity) {}

DrawObject::DrawObject(const BaseState& base_state)
    : base_state_(base_state) {}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
