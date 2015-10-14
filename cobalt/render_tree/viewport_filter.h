/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef RENDER_TREE_VIEWPORT_FILTER_H_
#define RENDER_TREE_VIEWPORT_FILTER_H_

#include "cobalt/math/matrix3_f.h"
#include "cobalt/math/rect_f.h"

namespace cobalt {
namespace render_tree {

// An ViewportFilter can be used to specify the viewport of source content.
// Only the contents of the source render tree that lie within the specified
// rectangle will be rendered.
class ViewportFilter {
 public:
  explicit ViewportFilter(const math::RectF& viewport) : viewport_(viewport) {}

  void set_viewport(const math::RectF& viewport) { viewport_ = viewport; }
  const math::RectF& viewport() const { return viewport_; }

 private:
  math::RectF viewport_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // RENDER_TREE_VIEWPORT_FILTER_H_
