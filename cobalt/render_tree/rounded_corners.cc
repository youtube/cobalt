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

RoundedCorners RoundedCorners::Scale(float sx, float sy) const {
  return RoundedCorners(RoundedCorner(top_left.horizontal * sx,
                                      top_left.vertical * sy),
                        RoundedCorner(top_right.horizontal * sx,
                                      top_right.vertical * sy),
                        RoundedCorner(bottom_right.horizontal * sx,
                                      bottom_right.vertical * sy),
                        RoundedCorner(bottom_left.horizontal * sx,
                                      bottom_left.vertical * sy));
}

RoundedCorners RoundedCorners::Normalize(const math::RectF& rect) const {
  float scale = 1.0f;
  float size;

  // Normalize overlapping curves.
  // https://www.w3.org/TR/css3-background/#corner-overlap
  // Additionally, normalize opposing curves so the corners do not overlap.
  size = top_left.horizontal +
         std::max(top_right.horizontal, bottom_right.horizontal);
  if (size > rect.width()) {
    scale = rect.width() / size;
  }

  size = bottom_left.horizontal +
         std::max(bottom_right.horizontal, top_right.horizontal);
  if (size > rect.width()) {
    scale = std::min(rect.width() / size, scale);
  }

  size =
      top_left.vertical + std::max(bottom_left.vertical, bottom_right.vertical);
  if (size > rect.height()) {
    scale = std::min(rect.height() / size, scale);
  }

  size = top_right.vertical +
         std::max(bottom_right.vertical, bottom_left.vertical);
  if (size > rect.height()) {
    scale = std::min(rect.height() / size, scale);
  }

  scale = std::max(scale, 0.0f);
  return Scale(scale, scale);
}

bool RoundedCorners::IsNormalized(const math::RectF& rect) const {
  // Introduce a fuzz epsilon so that we are not strict about rounding errors
  // when computing Normalize().
  const float kEpsilon = 0.0001f;
  const float fuzzed_width = rect.width() + kEpsilon;
  const float fuzzed_height = rect.height() + kEpsilon;

  return
      // Adjacent corners must not overlap.
      top_left.horizontal + top_right.horizontal <= fuzzed_width &&
      bottom_left.horizontal + bottom_right.horizontal <= fuzzed_width &&
      top_left.vertical + bottom_left.vertical <= fuzzed_height &&
      top_right.vertical + bottom_right.vertical <= fuzzed_height &&
      // Opposing corners must not overlap.
      top_left.horizontal + bottom_right.horizontal <= fuzzed_width &&
      bottom_left.horizontal + top_right.horizontal <= fuzzed_width &&
      top_left.vertical + bottom_right.vertical <= fuzzed_height &&
      top_right.vertical + bottom_left.vertical <= fuzzed_height;
}

}  // namespace render_tree
}  // namespace cobalt
