// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDER_TREE_CHILD_ITERATOR_H_
#define COBALT_RENDER_TREE_CHILD_ITERATOR_H_

#include "cobalt/render_tree/clear_rect_node.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/render_tree/filter_node.h"
#include "cobalt/render_tree/image_node.h"
#include "cobalt/render_tree/lottie_node.h"
#include "cobalt/render_tree/matrix_transform_3d_node.h"
#include "cobalt/render_tree/matrix_transform_node.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/render_tree/punch_through_video_node.h"
#include "cobalt/render_tree/rect_node.h"
#include "cobalt/render_tree/rect_shadow_node.h"
#include "cobalt/render_tree/text_node.h"

namespace cobalt {
namespace render_tree {

class ClearRectNode;
class ImageNode;
class PunchThroughVideoNode;
class RectNode;
class RectShadowNode;
class TextNode;

// The ChildIterator template provides a way to iterate over the children of
// a node without concern for what type of node it is (though since it is a
// template, the compiler must know the type).  Thus, render_tree::NodeVisitor
// subclasses can forward visits to a ChildIterator if they want to traverse
// the tree without writing traversal code for specific render tree node types.
// Additionally, ReplaceCurrent() can be used to modify the list of children
// at a given node, allowing render tree modifications to be made by code that
// does not care about what type of render tree nodes it is dealing with.
// Note that child-related methods like GetCurrent(), Next(), and
// ReplaceCurrent() are only enabled for nodes that have children.  This can
// be checked via |ChildIterator<T>::has_children|.
//
// Example usage:
//
//    void Visit(CompositionNode* node) { VisitNode(node); }
//    void Visit(ImageNode* node) { VisitNode(node); }
//    ...
//
//    template <typename T>
//    typename base::enable_if<!ChildIterator<T>::has_children>::type
//    VisitNode(T* node) {
//      ProcessNode();
//    }
//
//    template <typename T>
//    typename base::enable_if<ChildIterator<T>::has_children>::type VisitNode(
//        T* node) {
//      ProcessNode();
//
//      // Visit children
//      ChildIterator<T> child_iterator(node);
//      while (Node* child = child_iterator.GetCurrent()) {
//        Visit(child);
//        child_iterator.Next();
//      }
//    }
//
//    void ProcessNode(Node* node) {
//      // Called on every node
//      ...
//    }
//

template <typename T, typename = void>
class ChildIterator {
 public:
  // This default class is used for all nodes that have no children.
  static const bool has_children = false;
};

enum ChildIteratorDirection {
  // Decides whether we start at the first or last child.
  kChildIteratorDirectionForwards,
  kChildIteratorDirectionBackwards,
};

// CompositionNodes may have multiple children, so we setup ChildIterator
// to iterate through each of them.
template <>
class ChildIterator<CompositionNode> {
 public:
  static const bool has_children = true;

  explicit ChildIterator(
      CompositionNode* composition,
      ChildIteratorDirection direction = kChildIteratorDirectionForwards)
      : composition_node_(composition),
        children_(composition_node_->data().children()),
        child_number_(0),
        direction_(direction) {}

  Node* GetCurrent() const {
    return done() ? NULL : children_[child_index()].get();
  }
  void Next() { ++child_number_; }

  void ReplaceCurrent(const scoped_refptr<Node>& new_child) {
    DCHECK(!done());
    if (!modified_children_builder_) {
      // If we haven't replaced any children yet, we start by constructing a new
      // modifiable version of this CompositionNode's data so that we can
      // modify its children.
      modified_children_builder_ = composition_node_->data();
    }
    // Update the modified children list with the newly replaced child.
    *modified_children_builder_->GetChild(static_cast<int>(child_index())) =
        new_child;
  }
  CompositionNode::Builder::Moved TakeReplacedChildrenBuilder() {
    return modified_children_builder_->Pass();
  }

 private:
  size_t child_index() const {
    DCHECK(!done());
    return direction_ == kChildIteratorDirectionForwards
               ? child_number_
               : children_.size() - child_number_ - 1;
  }
  bool done() const { return child_number_ >= children_.size(); }

  // The node whose children we are iterating through.
  CompositionNode* composition_node_;
  // A quick reference to the node's children.
  const CompositionNode::Children& children_;
  // The current child index we are at with our iteration.
  size_t child_number_;

  ChildIteratorDirection direction_;

  // The builder to use if we are modifying the children.  If constructed, it
  // starts as a copy of the origin CompositionNode's data.
  base::Optional<CompositionNode::Builder> modified_children_builder_;
};

// FilterNodes can have up to 1 child.
template <>
class ChildIterator<FilterNode> {
 public:
  static const bool has_children = true;

  explicit ChildIterator(
      FilterNode* filter,
      ChildIteratorDirection direction = kChildIteratorDirectionForwards)
      : filter_node_(filter), source_(filter->data().source.get()) {
  }

  Node* GetCurrent() const { return source_; }
  void Next() { source_ = NULL; }

  void ReplaceCurrent(const scoped_refptr<Node>& new_child) {
    DCHECK(GetCurrent());
    if (!modified_children_builder_) {
      modified_children_builder_ = filter_node_->data();
    }
    modified_children_builder_->source = new_child;
  }
  FilterNode::Builder TakeReplacedChildrenBuilder() {
    return *modified_children_builder_;
  }

 private:
  FilterNode* filter_node_;
  Node* source_;

  base::Optional<FilterNode::Builder> modified_children_builder_;
};

// MatrixTransform3DNodes can have up to 1 child.
template <>
class ChildIterator<MatrixTransform3DNode> {
 public:
  static const bool has_children = true;

  explicit ChildIterator(
      MatrixTransform3DNode* matrix_transform_3d,
      ChildIteratorDirection direction = kChildIteratorDirectionForwards)
      : matrix_transform_3d_node_(matrix_transform_3d),
        source_(matrix_transform_3d->data().source.get()) {
  }

  Node* GetCurrent() const { return source_; }
  void Next() { source_ = NULL; }

  void ReplaceCurrent(const scoped_refptr<Node>& new_child) {
    DCHECK(GetCurrent());
    if (!modified_children_builder_) {
      modified_children_builder_ = matrix_transform_3d_node_->data();
    }
    modified_children_builder_->source = new_child;
  }
  MatrixTransform3DNode::Builder TakeReplacedChildrenBuilder() {
    return *modified_children_builder_;
  }

 private:
  MatrixTransform3DNode* matrix_transform_3d_node_;
  Node* source_;

  base::Optional<MatrixTransform3DNode::Builder> modified_children_builder_;
};

// MatrixTransformNodes can have up to 1 child.
template <>
class ChildIterator<MatrixTransformNode> {
 public:
  static const bool has_children = true;

  explicit ChildIterator(
      MatrixTransformNode* matrix_transform,
      ChildIteratorDirection direction = kChildIteratorDirectionForwards)
      : matrix_transform_node_(matrix_transform),
        source_(matrix_transform->data().source.get()) {
  }

  Node* GetCurrent() const { return source_; }
  void Next() { source_ = NULL; }

  void ReplaceCurrent(const scoped_refptr<Node>& new_child) {
    DCHECK(GetCurrent());
    if (!modified_children_builder_) {
      modified_children_builder_ = matrix_transform_node_->data();
    }
    modified_children_builder_->source = new_child;
  }
  MatrixTransformNode::Builder TakeReplacedChildrenBuilder() {
    return *modified_children_builder_;
  }

 private:
  MatrixTransformNode* matrix_transform_node_;
  Node* source_;

  base::Optional<MatrixTransformNode::Builder> modified_children_builder_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_CHILD_ITERATOR_H_
