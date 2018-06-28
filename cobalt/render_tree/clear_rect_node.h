// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDER_TREE_CLEAR_RECT_NODE_H_
#define COBALT_RENDER_TREE_CLEAR_RECT_NODE_H_

#include "base/compiler_specific.h"
#include "cobalt/base/type_id.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/render_tree/node.h"

namespace cobalt {
namespace render_tree {

// A simple rectangle, filled (without blending) with a specified color.
class ClearRectNode : public Node {
 public:
  class Builder {
   public:
    Builder(const Builder& other) = default;

    Builder(const math::RectF& rect, const ColorRGBA& color)
        : rect(rect), color(color) {}

    bool operator==(const Builder& other) const {
      return rect == other.rect && color == other.color;
    }

    // The destination rectangle.
    math::RectF rect;

    // The clear color.
    ColorRGBA color;
  };

  // Forwarding constructor to the set of Builder constructors.
  template <typename... Us>
  ClearRectNode(Us&&... args) : data_(std::forward<Us>(args)...) {}

  void Accept(NodeVisitor* visitor) override;
  math::RectF GetBounds() const override { return data_.rect; }

  base::TypeId GetTypeId() const override {
    return base::GetTypeId<ClearRectNode>();
  }

  const Builder& data() const { return data_; }

 private:
  const Builder data_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_CLEAR_RECT_NODE_H_
