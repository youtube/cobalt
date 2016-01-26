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
#include "cobalt/base/type_id.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/render_tree/shadow.h"

namespace cobalt {
namespace render_tree {

// A filled rectangle with a border and rounded corners.
class RectShadowNode : public Node {
 public:
  class Builder {
   public:
    Builder(const math::SizeF& size, const Shadow& shadow)
        : size(size), shadow(shadow) {}

    // A size of a rectangle (size includes border).
    math::SizeF size;

    // The shadow parameters that will be cast from the specified rectangle.
    // None of the area within the rectangle will be shaded.
    Shadow shadow;
  };

  explicit RectShadowNode(const Builder& builder) : data_(builder) {}

  RectShadowNode(const math::SizeF& size, const Shadow& shadow)
      : data_(size, shadow) {}

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
