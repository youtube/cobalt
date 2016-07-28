/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDER_TREE_RECT_SHADOW_NODE_H_
#define COBALT_RENDER_TREE_RECT_SHADOW_NODE_H_

#include "base/compiler_specific.h"
#include "base/optional.h"
#include "cobalt/base/type_id.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/render_tree/rounded_corners.h"
#include "cobalt/render_tree/shadow.h"

namespace cobalt {
namespace render_tree {

// A filled rectangle with a border and rounded corners.
class RectShadowNode : public Node {
 public:
  class Builder {
   public:
    Builder(const math::RectF& rect, const Shadow& shadow)
        : rect(rect), shadow(shadow), inset(false), spread(0.0f) {}

    Builder(const math::RectF& rect, const Shadow& shadow, bool inset,
            float spread)
        : rect(rect), shadow(shadow), inset(inset), spread(spread) {}

    // The destination rectangle.
    math::RectF rect;

    // If specified, the source shadow rectangle shape will have rounded
    // corners.
    base::optional<RoundedCorners> rounded_corners;

    // The shadow parameters that will be cast from the specified rectangle.
    // None of the area within the rectangle will be shaded.
    Shadow shadow;

    // If true, a shadow will be rendered within |rect| instead of outside of
    // it.  The shadow parameters define an inner rectangle that will NOT
    // be shaded.
    bool inset;

    // If set, will outset the shadow rectangle specified by the shadow
    // parameters by |spread|.  If the shadow is inset, it will inset the
    // shadow rectangle instead of outset it.
    float spread;
  };

  explicit RectShadowNode(const Builder& builder) : data_(builder) {}

  RectShadowNode(const math::RectF& rect, const Shadow& shadow)
      : data_(rect, shadow) {}

  RectShadowNode(const math::RectF& rect, const Shadow& shadow, bool inset,
                 float spread)
      : data_(rect, shadow, inset, spread) {}

  void Accept(NodeVisitor* visitor) OVERRIDE;
  math::RectF GetBounds() const OVERRIDE;

  base::TypeId GetTypeId() const OVERRIDE {
    return base::GetTypeId<RectShadowNode>();
  }

  const Builder& data() const { return data_; }

 private:
  const Builder data_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_RECT_SHADOW_NODE_H_
