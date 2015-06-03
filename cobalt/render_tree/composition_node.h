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
#include "cobalt/math/matrix3_f.h"
#include "cobalt/render_tree/movable.h"
#include "cobalt/render_tree/node.h"

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
// You would construct a CompositionNode via a CompositionNode::Builder object,
// which is essentially a CompositionNode that permits mutations.
// The CompositionNode::Builder class can be used to build the constructor
// parameter of the CompositionNode.  This allows mutations (such as adding
// a new child node) to occur within the mutable object, while permitting the
// actual render tree node, CompositionNode, to be immutable.
// Since the CompositionNode::Builder can be a heavy-weight object, it provides
// a Pass() method allowing one to specify move semantics when passing it in
// to a CompositionNode constructor.
// For example the following function takes two child render tree nodes and
// returns a CompositionNode that has both of them as children.
//
// scoped_refptr<CompositionNode> ComposeChildren(
//     const scoped_refptr<Node>& child1,
//     const scoped_refptr<Node>& child2) {
//   CompositionNode::Builder composition_node_builder;
//   composition_node_builder.AddChild(child1, math::Matrix3F::Identity());
//   composition_node_builder.AddChild(child2, math::Matrix3F::Identity());
//   return make_scoped_refptr(new CompositionNode(
//       composition_node_builder.Pass()));
// }
//
class CompositionNode : public Node {
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

  class Builder {
   public:
    DECLARE_AS_MOVABLE(Builder);

    Builder() : composed_children_(new ComposedChildren()) {}
    explicit Builder(const Builder& other) :
        composed_children_(new ComposedChildren(*other.composed_children_)) {}
    explicit Builder(Moved moved)
        : composed_children_(moved->composed_children_.Pass()) {}

    // Add a child node to the list of children we are building.  When a
    // CompositionNode is constructed from this CompositionNode::Builder, it
    // will have as children all nodes who were passed into the builder via this
    // method.
    void AddChild(const scoped_refptr<Node>& node,
                  const math::Matrix3F& transform);

    // Returns the specified child as a pointer so that it can be modified.
    ComposedChild* GetChild(int child_index) {
      DCHECK_GE(child_index, 0);
      return &((*composed_children_)[static_cast<std::size_t>(child_index)]);
    }

    // A list of render tree nodes in a draw order.
    const ComposedChildren& composed_children() const {
      return *composed_children_;
    }

   private:
    scoped_ptr<ComposedChildren> composed_children_;
  };

  explicit CompositionNode(const Builder& builder) : data_(builder) {}
  explicit CompositionNode(Builder::Moved builder) : data_(builder) {}

  void Accept(NodeVisitor* visitor) OVERRIDE;
  math::RectF GetBounds() const OVERRIDE;

  const Builder& data() const { return data_; }

 private:
  const Builder data_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // RENDER_TREE_COMPOSITION_NODE_H_
