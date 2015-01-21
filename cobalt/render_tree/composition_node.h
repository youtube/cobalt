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

#ifndef RENDER_TREE_COMPOSITION_NODE_H_
#define RENDER_TREE_COMPOSITION_NODE_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/move.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/math/matrix3_f.h"

#include <vector>

namespace cobalt {
namespace render_tree {

class ClipRegion;

// A composition specifies a set of child nodes that are to be rendered in
// order. It is the primary way to compose multiple child nodes together in a
// render tree. Each child node has an associated matrix which determines how
// the node will be transformed relative to the CompositionNode.  The node
// doesn't have its own shape or size, it is completely determined by its
// children.
// You would construct a CompositionNode via a CompositionNodeMutable object,
// which is essentially a CompositionNode that permits mutations.
// The CompositionNodeMutable class can be used to build the constructor
// parameter of the CompositionNode.  This allows mutations (such as adding
// a new child node) to occur within the mutable object, while permitting the
// actual render tree node, CompositionNode to be immutable.  Note that
// CompositionNodeMutable is NOT a render_tree::Node, and thus cannot be used
// in a render tree without being wrapped by a CompositionNode.
// For example the following function takes two child render tree nodes and
// returns a CompositionNode that has both of them as children.
//
// scoped_refptr<CompositionNode> ComposeChildren(
//     const scoped_refptr<Node>& child1,
//     const scoped_refptr<Node>& child2) {
//   scoped_ptr<CompositionNodeMutable> mutable_composition;
//   mutable_composition->AddChild(child1, math::Matrix3F::Identity());
//   mutable_composition->AddChild(child2, math::Matrix3F::Identity());
//   return make_scoped_refptr(new CompositionNode(mutable_composition.Pass()));
// }
//

class CompositionNodeMutable {
 public:
  // The ChildInfo structure packages together all information relevant to
  // each child, such as a reference to the actual child node and a matrix
  // determining how to transform it.
  struct ComposedChild {
    ComposedChild(const scoped_refptr<Node>& node,
                  const math::Matrix3F& transform)
        : node(node), transform(transform) {}

    scoped_refptr<Node> node;
    math::Matrix3F transform;
  };
  typedef std::vector<ComposedChild> ComposedChildren;

  CompositionNodeMutable() {}

  // Add a child node to the list of children we are building.  When a
  // CompositionNode is constructed from this CompositionNodeMutable, it will
  // have as children all nodes who were passed into the builder via this
  // method.
  void AddChild(const scoped_refptr<Node>& node,
                const math::Matrix3F& transform);

  const ComposedChildren& composed_children() const {
    return composed_children_;
  }

 private:
  ComposedChildren composed_children_;
};

class CompositionNode : public Node {
 public:
  typedef CompositionNodeMutable::ComposedChild ComposedChild;
  typedef CompositionNodeMutable::ComposedChildren ComposedChildren;

  explicit CompositionNode(
      scoped_ptr<CompositionNodeMutable> mutable_composition_node)
      : data_(mutable_composition_node.Pass()) {}

  // A type-safe branching.
  void Accept(NodeVisitor* visitor) OVERRIDE;

  // A list of render tree nodes in a draw order.
  const ComposedChildren& composed_children() const {
    return data_->composed_children();
  }

  // An opacity of the composition node. Opacity ranges from 0.0 for completely
  // transparent nodes to 1.0 for completely opaque ones. Note that if
  // children overlap, rendering the node with opacity is not the same as
  // rendering each child of the composition node with the same opacity because
  // overlapping regions of children will bleed through.
  float opacity() const;

  // An optional clip region.
  const ClipRegion* clip_region() const;

 private:
  scoped_ptr<const CompositionNodeMutable> data_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // RENDER_TREE_COMPOSITION_NODE_H_
