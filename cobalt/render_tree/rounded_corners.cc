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

#include "cobalt/render_tree/rounded_corners.h"

namespace cobalt {
namespace render_tree {

void RoundedCorners::Normalize(const math::RectF& rect) {
  float scale = 1.0f;
  float size;

  size = top_left.horizontal + top_right.horizontal;
  if (size > rect.width()) {
    scale = rect.width() / size;
  }

  size = bottom_left.horizontal + bottom_right.horizontal;
  if (size > rect.width()) {
    scale = std::min(rect.width() / size, scale);
  }

  size = top_left.vertical + bottom_left.vertical;
  if (size > rect.height()) {
    scale = std::min(rect.height() / size, scale);
  }

  size = top_right.vertical + bottom_right.vertical;
  if (size > rect.height()) {
    scale = std::min(rect.height() / size, scale);
  }

  top_left.horizontal *= scale;
  top_left.vertical *= scale;
  top_right.horizontal *= scale;
  top_right.vertical *= scale;
  bottom_left.horizontal *= scale;
  bottom_left.vertical *= scale;
  bottom_right.horizontal *= scale;
  bottom_right.vertical *= scale;
}

}  // namespace render_tree
}  // namespace cobalt
