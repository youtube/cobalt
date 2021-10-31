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

#ifndef COBALT_RENDER_TREE_RECT_SHADOW_NODE_H_
#define COBALT_RENDER_TREE_RECT_SHADOW_NODE_H_

#include "base/compiler_specific.h"
#include "base/logging.h"
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
    Builder(const Builder&) = default;
    Builder(const math::RectF& rect, const Shadow& shadow)
        : rect(rect), shadow(shadow), inset(false), spread(0.0f) {}

    Builder(const math::RectF& rect, const Shadow& shadow, bool inset,
            float spread)
        : rect(rect), shadow(shadow), inset(inset), spread(spread) {}

    bool operator==(const Builder& other) const {
      return rect == other.rect && rounded_corners == other.rounded_corners &&
             shadow == other.shadow && inset == other.inset &&
             spread == other.spread;
    }
    // The destination rectangle.
    math::RectF rect;

    // If specified, the source shadow rectangle shape will have rounded
    // corners.
    base::Optional<RoundedCorners> rounded_corners;

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

  // Forwarding constructor to the set of Builder constructors.
  template <typename... Args>
  RectShadowNode(Args&&... args) : data_(std::forward<Args>(args)...) {
    if (DCHECK_IS_ON()) {
      AssertValid();
    }
  }

  void Accept(NodeVisitor* visitor) override;
  math::RectF GetBounds() const override;

  base::TypeId GetTypeId() const override {
    return base::GetTypeId<RectShadowNode>();
  }

  const Builder& data() const { return data_; }

 private:
  void AssertValid() const;

  const Builder data_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_RECT_SHADOW_NODE_H_
