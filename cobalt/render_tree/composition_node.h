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

#ifndef COBALT_RENDER_TREE_COMPOSITION_NODE_H_
#define COBALT_RENDER_TREE_COMPOSITION_NODE_H_

#include <algorithm>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/base/type_id.h"
#include "cobalt/math/vector2d_f.h"
#include "cobalt/render_tree/movable.h"
#include "cobalt/render_tree/node.h"

namespace cobalt {
namespace render_tree {

// A composition specifies a set of child nodes that are to be rendered in
// order. It is the primary way to compose multiple child nodes together in a
// render tree. This node doesn't have its own intrinsic shape or size, it is
// completely determined by its children.  An offset can be specified to be
// applied to all children of the composition node.
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
//   composition_node_builder.AddChild(child1);
//   composition_node_builder.AddChild(child2);
//   return make_scoped_refptr(new CompositionNode(
//       composition_node_builder.Pass()));
// }
//
class CompositionNode : public Node {
 public:
  typedef std::vector<scoped_refptr<Node> > Children;

  class Builder {
   public:
    DECLARE_AS_MOVABLE(Builder);

    Builder() {}
    explicit Builder(const math::Vector2dF& offset) : offset_(offset) {}
    Builder(Node* node, const math::Vector2dF& offset) : offset_(offset) {
      children_.push_back(node);
    }

    Builder(const Builder& other)
        : offset_(other.offset_), children_(other.children_) {}
    explicit Builder(Moved moved) : offset_(moved->offset_) {
      children_.swap(moved->children_);
    }

    bool operator==(const Builder& other) const {
      return offset_ == other.offset_ && children_ == other.children_;
    }

    // Add a child node to the list of children we are building.  When a
    // CompositionNode is constructed from this CompositionNode::Builder, it
    // will have as children all nodes who were passed into the builder via this
    // method.
    void AddChild(const scoped_refptr<Node>& node);

    // Returns the specified child as a pointer so that it can be modified.
    scoped_refptr<Node>* GetChild(int child_index) {
      DCHECK_GE(child_index, 0);
      DCHECK_LT(static_cast<std::size_t>(child_index), children_.size());
      return &(children_[static_cast<std::size_t>(child_index)]);
    }

    // A list of render tree nodes in a draw order.
    const Children& children() const { return children_; }

    const math::Vector2dF& offset() const { return offset_; }
    void set_offset(const math::Vector2dF& offset) { offset_ = offset; }

   private:
    // A translation offset to be applied to each member of this composition
    // node.
    math::Vector2dF offset_;

    // The set of children to be composed together under this CompositionNode.
    Children children_;
  };

  explicit CompositionNode(const Builder& builder)
      : data_(builder), cached_bounds_(ComputeBounds()) {}

  explicit CompositionNode(Builder::Moved builder)
      : data_(builder), cached_bounds_(ComputeBounds()) {}

  explicit CompositionNode(Builder&& builder)
      : data_(builder.Pass()), cached_bounds_(ComputeBounds()) {}

  CompositionNode(Node* node, const math::Vector2dF& offset)
      : data_(node, offset), cached_bounds_(ComputeBounds()) {}

  void Accept(NodeVisitor* visitor) override;
  math::RectF GetBounds() const override;

  base::TypeId GetTypeId() const override {
    return base::GetTypeId<CompositionNode>();
  }

  const Builder& data() const { return data_; }

 private:
  // Computes the bounding rectangle (given our children) for this composition
  // node, which is what GetBounds() will subsequently return.
  math::RectF ComputeBounds() const;

  const Builder data_;
  const math::RectF cached_bounds_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_COMPOSITION_NODE_H_
