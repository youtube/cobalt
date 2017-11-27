// Copyright 2014 Google Inc. All Rights Reserved.
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

    bool operator==(const Builder& other) const;

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
    AssertValid();
  }
  RectNode(const math::RectF& rect, scoped_ptr<Border> border,
           scoped_ptr<RoundedCorners> rounded_corners)
      : data_(rect, border.Pass(), rounded_corners.Pass()) {
    AssertValid();
  }
  RectNode(const math::RectF& rect, scoped_ptr<Brush> background_brush)
      : data_(rect, background_brush.Pass()) {
    AssertValid();
  }
  RectNode(const math::RectF& rect, scoped_ptr<Brush> background_brush,
           scoped_ptr<Border> border)
      : data_(rect, background_brush.Pass(), border.Pass()) {
    AssertValid();
  }
  RectNode(const math::RectF& rect, scoped_ptr<Brush> background_brush,
           scoped_ptr<RoundedCorners> rounded_corners)
      : data_(rect, background_brush.Pass(), rounded_corners.Pass()) {
    AssertValid();
  }
  RectNode(const math::RectF& rect, scoped_ptr<Brush> background_brush,
           scoped_ptr<Border> border,
           scoped_ptr<RoundedCorners> rounded_corners)
      : data_(rect, background_brush.Pass(), border.Pass(),
              rounded_corners.Pass()) {
    AssertValid();
  }
  explicit RectNode(const Builder& builder) : data_(builder) {
    AssertValid();
  }
  explicit RectNode(Builder::Moved builder) : data_(builder) {
    AssertValid();
  }

  void Accept(NodeVisitor* visitor) override;
  math::RectF GetBounds() const override;

  base::TypeId GetTypeId() const override {
    return base::GetTypeId<RectNode>();
  }

  const Builder& data() const { return data_; }

 private:
  void AssertValid() const;

  const Builder data_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_RECT_NODE_H_
