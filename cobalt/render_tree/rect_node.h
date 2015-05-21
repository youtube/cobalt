/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef RENDER_TREE_RECT_NODE_H_
#define RENDER_TREE_RECT_NODE_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/render_tree/brush.h"
#include "cobalt/render_tree/movable.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/math/rect_f.h"

namespace cobalt {
namespace render_tree {

// TODO(***REMOVED***): Define the border, don't forget no border case.
class Border;

// TODO(***REMOVED***): Define border radiuses.
class BorderRadiuses;

// TODO(***REMOVED***): Define the shadow.
class Shadow;

// A filled rectangle with a border and rounded corners.
class RectNode : public Node {
 public:
  class Builder {
   public:
    DECLARE_AS_MOVABLE(Builder);

    Builder(const math::SizeF& size, scoped_ptr<Brush> background_brush);
    explicit Builder(const Builder& other);
    explicit Builder(Moved moved);

    // A size of a rectangle (size includes border).
    math::SizeF size;

    // A solid or gradient brush to fill the rectangle with.
    // This can be null if a background brush is not specified.
    scoped_ptr<Brush> background_brush;
  };

  RectNode(const math::SizeF& size, scoped_ptr<Brush> background_brush)
      : data_(size, background_brush.Pass()) {
    DCheckData(data_);
  }
  explicit RectNode(const Builder& builder) : data_(builder) {
    DCheckData(data_);
  }
  explicit RectNode(Builder::Moved builder) : data_(builder) {
    DCheckData(data_);
  }

  // A type-safe branching.
  void Accept(NodeVisitor* visitor) OVERRIDE;

  const Builder& data() const { return data_; }

 private:
  void DCheckData(const Builder& data) { DCHECK(data.background_brush); }

  const Builder data_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // RENDER_TREE_RECT_NODE_H_
