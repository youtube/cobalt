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

  size = top_left.vertical +
         std::max(bottom_left.vertical, bottom_right.vertical);
  if (size > rect.height()) {
    scale = std::min(rect.height() / size, scale);
  }

  size = top_right.vertical +
         std::max(bottom_right.vertical, bottom_left.vertical);
  if (size > rect.height()) {
    scale = std::min(rect.height() / size, scale);
  }

  return RoundedCorners(RoundedCorner(top_left.horizontal * scale,
                                      top_left.vertical * scale),
                        RoundedCorner(top_right.horizontal * scale,
                                      top_right.vertical * scale),
                        RoundedCorner(bottom_right.horizontal * scale,
                                      bottom_right.vertical * scale),
                        RoundedCorner(bottom_left.horizontal * scale,
                                      bottom_left.vertical * scale));
}

bool RoundedCorners::IsNormalized(const math::RectF& rect) const {
  return
      // Adjacent corners must not overlap.
      top_left.horizontal + top_right.horizontal <= rect.width() &&
      bottom_left.horizontal + bottom_right.horizontal <= rect.width() &&
      top_left.vertical + bottom_left.vertical <= rect.height() &&
      top_right.vertical + bottom_right.vertical <= rect.height() &&
      // Opposing corners must not overlap.
      top_left.horizontal + bottom_right.horizontal <= rect.width() &&
      bottom_left.horizontal + top_right.horizontal <= rect.width() &&
      top_left.vertical + bottom_right.vertical <= rect.height() &&
      top_right.vertical + bottom_left.vertical <= rect.height();
}

}  // namespace render_tree
}  // namespace cobalt
