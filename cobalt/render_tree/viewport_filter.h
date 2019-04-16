// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDER_TREE_VIEWPORT_FILTER_H_
#define COBALT_RENDER_TREE_VIEWPORT_FILTER_H_

#include "base/optional.h"
#include "cobalt/math/matrix3_f.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/render_tree/rounded_corners.h"

namespace cobalt {
namespace render_tree {

// An ViewportFilter can be used to specify the viewport of source content.
// Only the contents of the source render tree that lie within the specified
// rectangle will be rendered.
class ViewportFilter {
 public:
  explicit ViewportFilter(const math::RectF& viewport) : viewport_(viewport) {}
  explicit ViewportFilter(const math::RectF& viewport,
                          const RoundedCorners& rounded_corners)
      : viewport_(viewport), rounded_corners_(rounded_corners) {}

  bool operator==(const ViewportFilter& other) const {
    return viewport_ == other.viewport_ &&
           rounded_corners_ == other.rounded_corners_;
  }

  const math::RectF& viewport() const { return viewport_; }

  bool has_rounded_corners() const { return !!rounded_corners_; }
  const RoundedCorners& rounded_corners() const { return *rounded_corners_; }
  void set_rounded_corners(const RoundedCorners& rounded_corners) {
    rounded_corners_ = rounded_corners;
  }

 private:
  math::RectF viewport_;
  base::Optional<RoundedCorners> rounded_corners_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_VIEWPORT_FILTER_H_
