// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDER_TREE_SHADOW_H_
#define COBALT_RENDER_TREE_SHADOW_H_

#include <algorithm>

#include "cobalt/math/rect_f.h"
#include "cobalt/math/vector2d_f.h"
#include "cobalt/render_tree/color_rgba.h"

namespace cobalt {
namespace render_tree {

// Describes a shadow effect that can be applied as a filter or to a shape.
// The |blur_sigma| value is given in units of Gaussian standard deviations for
// the Gaussian kernel that will be used to blur them.
struct Shadow {
  Shadow(const math::Vector2dF& offset, float blur_sigma,
         const ColorRGBA& color)
      : offset(offset), blur_sigma(blur_sigma), color(color) {}

  bool operator==(const Shadow& other) const {
    return offset == other.offset && blur_sigma == other.blur_sigma &&
           color == other.color;
  }

  // Since the blur parameters represent standard deviations, most of the
  // blur is contained within 3 of them, so report that as the extent of the
  // blur.
  math::Vector2dF BlurExtent() const {
    return math::Vector2dF(blur_sigma * 3, blur_sigma * 3);
  }

  // Returns a bounding rectangle for the shadow if it were to be applied to
  // the input bounding rectangle.
  math::RectF ToShadowBounds(const math::RectF& source_bound) const {
    math::RectF ret(source_bound);
    ret.Offset(offset);

    math::Vector2dF blur_extent = BlurExtent();
    ret.Outset(blur_extent.x(), blur_extent.y());
    return ret;
  }

  math::Vector2dF offset;
  float blur_sigma;
  ColorRGBA color;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_SHADOW_H_
