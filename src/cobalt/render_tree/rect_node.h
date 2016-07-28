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

#ifndef COBALT_RENDER_TREE_RECT_NODE_H_
#define COBALT_RENDER_TREE_RECT_NODE_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/base/type_id.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/render_tree/border.h"
#include "cobalt/render_tree/brush.h"
#include "cobalt/render_tree/movable.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/render_tree/rounded_corners.h"

namespace cobalt {
namespace render_tree {

// A filled rectangle with a border and rounded corners.
class RectNode : public Node {
 public:
  class Builder {
   public:
    DECLARE_AS_MOVABLE(Builder);

    explicit Builder(const math::RectF& rect);
    Builder(const math::RectF& rect, scoped_ptr<Border> border);
    Builder(const math::RectF& rect, scoped_ptr<Border> border,
            scoped_ptr<RoundedCorners> rounded_corners);
    Builder(const math::RectF& rect, scoped_ptr<Brush> background_brush);
    Builder(const math::RectF& rect,
            scoped_ptr<RoundedCorners> rounded_corners);
    Builder(const math::RectF& rect, scoped_ptr<Brush> background_brush,
            scoped_ptr<Border> border);
    Builder(const math::RectF& rect, scoped_ptr<Brush> background_brush,
            scoped_ptr<RoundedCorners> rounded_corners);
    Builder(const math::RectF& rect, scoped_ptr<Brush> background_brush,
            scoped_ptr<Border> border,
            scoped_ptr<RoundedCorners> rounded_corners);
    explicit Builder(const Builder& other);
    explicit Builder(Moved moved);

    // The destination rectangle (size includes border).
    math::RectF rect;

    // A solid or gradient brush to fill the rectangle with.
    // This can be null if a background brush is not specified.
    scoped_ptr<Brush> background_brush;

    // A border arounds a RectNode.
    scoped_ptr<Border> border;

    // Defines the radii of an ellipse that defines the shape of the corner of
    // the outer border edge.
    scoped_ptr<RoundedCorners> rounded_corners;
  };

  RectNode(const math::RectF& rect, scoped_ptr<Border> border)
      : data_(rect, border.Pass()) {
    DCheckData(data_);
  }
  RectNode(const math::RectF& rect, scoped_ptr<Border> border,
           scoped_ptr<RoundedCorners> rounded_corners)
      : data_(rect, border.Pass(), rounded_corners.Pass()) {
    DCheckData(data_);
  }
  RectNode(const math::RectF& rect, scoped_ptr<Brush> background_brush)
      : data_(rect, background_brush.Pass()) {
    DCheckData(data_);
  }
  RectNode(const math::RectF& rect, scoped_ptr<Brush> background_brush,
           scoped_ptr<Border> border)
      : data_(rect, background_brush.Pass(), border.Pass()) {
    DCheckData(data_);
  }
  RectNode(const math::RectF& rect, scoped_ptr<Brush> background_brush,
           scoped_ptr<RoundedCorners> rounded_corners)
      : data_(rect, background_brush.Pass(), rounded_corners.Pass()) {
    DCheckData(data_);
  }
  RectNode(const math::RectF& rect, scoped_ptr<Brush> background_brush,
           scoped_ptr<Border> border,
           scoped_ptr<RoundedCorners> rounded_corners)
      : data_(rect, background_brush.Pass(), border.Pass(),
              rounded_corners.Pass()) {
    DCheckData(data_);
  }
  explicit RectNode(const Builder& builder) : data_(builder) {
    DCheckData(data_);
  }
  explicit RectNode(Builder::Moved builder) : data_(builder) {
    DCheckData(data_);
  }

  void Accept(NodeVisitor* visitor) OVERRIDE;
  math::RectF GetBounds() const OVERRIDE;

  base::TypeId GetTypeId() const OVERRIDE {
    return base::GetTypeId<RectNode>();
  }

  const Builder& data() const { return data_; }

 private:
  void DCheckData(const Builder& data) {
    DCHECK(data.background_brush || data.border || data.rounded_corners);
  }

  const Builder data_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_RECT_NODE_H_
