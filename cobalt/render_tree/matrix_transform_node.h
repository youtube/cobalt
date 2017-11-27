// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDER_TREE_MATRIX_TRANSFORM_NODE_H_
#define COBALT_RENDER_TREE_MATRIX_TRANSFORM_NODE_H_

#include <algorithm>
#include <vector>

#include "base/compiler_specific.h"
#include "cobalt/base/type_id.h"
#include "cobalt/math/matrix3_f.h"
#include "cobalt/render_tree/node.h"

namespace cobalt {
namespace render_tree {

// A MatrixTransformNode applies a specified affine matrix transform,
// |transform| to a specified sub render tree node, |source|.
class MatrixTransformNode : public Node {
 public:
  struct Builder {
    Builder(const scoped_refptr<Node>& source, const math::Matrix3F& transform)
        : source(source), transform(transform) {}

    bool operator==(const Builder& other) const {
      return source == other.source && transform == other.transform;
    }

    // The subtree that will be rendered with |transform| applied to it.
    scoped_refptr<Node> source;

    // The matrix transform that will be applied to the subtree.  It is an
    // affine 3x3 matrix to be applied to 2D points.
    math::Matrix3F transform;
  };

  MatrixTransformNode(const scoped_refptr<Node>& source,
                      const math::Matrix3F& transform)
      : data_(source, transform) {}

  explicit MatrixTransformNode(const Builder& builder) : data_(builder) {}

  void Accept(NodeVisitor* visitor) override;
  math::RectF GetBounds() const override;

  base::TypeId GetTypeId() const override {
    return base::GetTypeId<MatrixTransformNode>();
  }

  const Builder& data() const { return data_; }

 private:
  const Builder data_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_MATRIX_TRANSFORM_NODE_H_
