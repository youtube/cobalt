// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDER_TREE_MATRIX_TRANSFORM_3D_NODE_H_
#define COBALT_RENDER_TREE_MATRIX_TRANSFORM_3D_NODE_H_

#include <algorithm>
#include <vector>

#include "base/compiler_specific.h"
#include "cobalt/base/type_id.h"
#include "cobalt/render_tree/node.h"
#include "third_party/glm/glm/mat4x4.hpp"

namespace cobalt {
namespace render_tree {

// A MatrixTransform3DNode applies a specified 3D affine matrix transform,
// |transform| to a specified sub render tree node, |source|.  As of the time
// of this writing, the MatrixTransform3DNode will not affect anything except
// for FilterNodes with the MapToMeshFilter attached to them, in which case
// the 3D transform will be applied to the mesh.
class MatrixTransform3DNode : public Node {
 public:
  struct Builder {
    Builder(const scoped_refptr<Node>& source, const glm::mat4& transform)
        : source(source), transform(transform) {}

    bool operator==(const Builder& other) const {
      return source == other.source && transform == other.transform;
    }

    // The subtree that will be rendered with |transform| applied to it.
    scoped_refptr<Node> source;

    // The matrix transform that will be applied to the subtree.  It is an
    // affine 4x4 matrix.
    glm::mat4 transform;
  };

  MatrixTransform3DNode(const scoped_refptr<Node>& source,
                        const glm::mat4& transform)
      : data_(source, transform) {}

  explicit MatrixTransform3DNode(const Builder& builder) : data_(builder) {}

  void Accept(NodeVisitor* visitor) override;
  math::RectF GetBounds() const override;

  base::TypeId GetTypeId() const override {
    return base::GetTypeId<MatrixTransform3DNode>();
  }

  const Builder& data() const { return data_; }

 private:
  const Builder data_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_MATRIX_TRANSFORM_3D_NODE_H_
