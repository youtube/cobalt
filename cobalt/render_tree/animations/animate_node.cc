/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/render_tree/animations/animate_node.h"

#include "base/debug/trace_event.h"
#include "cobalt/base/enable_if.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/render_tree/child_iterator.h"
#include "cobalt/render_tree/node_visitor.h"

namespace cobalt {
namespace render_tree {
namespace animations {

void AnimateNode::Builder::AddInternal(
    const scoped_refptr<Node>& target_node,
    const scoped_refptr<AnimationListBase>& animation_list) {
  DCHECK(node_animation_map_.find(target_node) == node_animation_map_.end())
      << "The target render tree node already has an associated animation "
         "list.";

  node_animation_map_[target_node.get()] = animation_list;
  node_refs_.push_back(target_node);
}

void AnimateNode::Builder::Merge(const AnimateNode::Builder& other) {
#if !defined(NDEBUG)
  for (InternalMap::const_iterator iter = node_animation_map_.begin();
       iter != node_animation_map_.end(); ++iter) {
    DCHECK(other.node_animation_map_.find(iter->first) ==
           other.node_animation_map_.end())
        << "Only mutually exclusive AnimateNode::Builders can be merged!";
  }
#endif
  node_animation_map_.insert(other.node_animation_map_.begin(),
                             other.node_animation_map_.end());
  node_refs_.insert(node_refs_.end(), other.node_refs_.begin(),
                    other.node_refs_.end());
}

// A helper render tree visitor class used to compile sub render-tree
// animations.  The same instance of the TraverseListBuilder class is reused for
// traversing all nodes in a tree.  After visiting a render tree,
// |traverse_list_| will contain a post-order traversal of animated nodes where
// children are visited from right to left, and this should be reversed to
// obtain a pre-order traversal of animated nodes where children are visited
// from left to right, which is much more natural.
class AnimateNode::TraverseListBuilder : public NodeVisitor {
 public:
  TraverseListBuilder(const AnimateNode::Builder::InternalMap& animation_map,
                      TraverseList* traverse_list)
      : animation_map_(animation_map), traverse_list_(traverse_list) {}

  void Visit(animations::AnimateNode* animate) OVERRIDE;
  void Visit(CompositionNode* composition) OVERRIDE { VisitNode(composition); }
  void Visit(FilterNode* text) OVERRIDE { VisitNode(text); }
  void Visit(ImageNode* image) OVERRIDE { VisitNode(image); }
  void Visit(MatrixTransformNode* transform) OVERRIDE { VisitNode(transform); }
  void Visit(PunchThroughVideoNode* punch_through) OVERRIDE {
    VisitNode(punch_through);
  }
  void Visit(RectNode* rect) OVERRIDE { VisitNode(rect); }
  void Visit(RectShadowNode* rect) OVERRIDE { VisitNode(rect); }
  void Visit(TextNode* text) OVERRIDE { VisitNode(text); }

 private:
  template <typename T>
  typename base::enable_if<!ChildIterator<T>::has_children>::type VisitNode(
      T* node);

  template <typename T>
  typename base::enable_if<ChildIterator<T>::has_children>::type VisitNode(
      T* node);
  void ProcessNode(Node* node, bool animated_children);

  // Adds a node to |traverse_list_|, indicating that it or its descendants are
  // involved in animation.
  void AddToTraverseList(
      Node* node, AnimateNode::Builder::InternalMap::const_iterator found);

  // A reference to the mapping from render tree node to animations.
  const AnimateNode::Builder::InternalMap& animation_map_;

  // A list of nodes that, if reversed, gives a pre-order traversal of only
  // animated nodes.
  // |traverse_list_| is the primary output of this visitor class.
  TraverseList* traverse_list_;

  // Signals to the caller of Visit() that this node was animated and so any
  // parent nodes should also be added to the traversal list.
  bool animated_;

  // If non-null, references a node that this visitor's parent should replace
  // this visitor's associated node with in its list of child nodes.
  scoped_refptr<Node> replace_with_;

  friend class AnimateNode;
};

void AnimateNode::TraverseListBuilder::Visit(animations::AnimateNode* animate) {
  if (!animate->traverse_list_.empty()) {
    // We merge all the information from this AnimateNode into the one we are
    // constructing now.  Simply append the sub-AnimateNode to |traverse_list_|,
    // but reverse it so its finalized pre-order, left to right traversal is
    // switched to the intermediate format of a post-order, right to left
    // traversal.
    size_t start_size = traverse_list_->size();
    traverse_list_->insert(traverse_list_->end(),
                           animate->traverse_list_.begin(),
                           animate->traverse_list_.end());
    std::reverse(traverse_list_->begin() + static_cast<int64>(start_size),
                 traverse_list_->end());
    animated_ = true;
  } else {
    animated_ = false;
  }

  // Now that we have merged the sub-AnimateNode's information with the one
  // under construction, remove this AnimateNode from the tree in order to
  // maintain the invariant that an AnimateNode does not contain any
  // AnimateNode descendants.
  replace_with_ = animate->source_;
}

template <typename T>
typename base::enable_if<!ChildIterator<T>::has_children>::type
AnimateNode::TraverseListBuilder::VisitNode(T* node) {
  // If we are dealing with a render tree node that has no children, all we
  // need to do is call ProcessNode().
  animated_ = false;
  replace_with_ = NULL;
  ProcessNode(node, false);
}

template <typename T>
typename base::enable_if<ChildIterator<T>::has_children>::type
AnimateNode::TraverseListBuilder::VisitNode(T* node) {
  ChildIterator<T> child_iterator(node, kChildIteratorDirectionBackwards);
  bool modified_children = false;
  bool animated_children = false;
  while (Node* child = child_iterator.GetCurrent()) {
    child->Accept(this);

    // Mark us as animated if any of our children are animated, so that we make
    // sure to add ourselves to |traverse_list_|.
    animated_children |= animated_;

    if (replace_with_) {
      // If the child node wishes to replace itself with a new node, then
      // replace it within its parent's child list.
      child_iterator.ReplaceCurrent(replace_with_);
      modified_children = true;
    }

    child_iterator.Next();
  }

  if (modified_children) {
    replace_with_ = new T(child_iterator.TakeReplacedChildrenBuilder());
  } else {
    replace_with_ = NULL;
  }

  // Finally, add this node to |traverse_list_| if it is animated either
  // directly or indirectly (i.e. because its children are animated).
  animated_ = false;
  ProcessNode(node, animated_children);
}

void AnimateNode::TraverseListBuilder::ProcessNode(Node* node,
                                                   bool animated_children) {
  // If this node is animated, add it to the |traverse_list_|.
  AnimateNode::Builder::InternalMap::const_iterator found =
      animation_map_.find(node);
  if (animated_children || found != animation_map_.end()) {
    AddToTraverseList(replace_with_ ? replace_with_.get() : node, found);
  }
}

// Adds a node to |traverse_list_| so that it can be quickly
// found in the future.
void AnimateNode::TraverseListBuilder::AddToTraverseList(
    Node* node, AnimateNode::Builder::InternalMap::const_iterator found) {
  if (found != animation_map_.end()) {
    traverse_list_->push_back(TraverseListEntry(node, found->second));
  } else {
    traverse_list_->push_back(TraverseListEntry(node));
  }
  animated_ = true;
}

// A helper render tree visitor class used to apply compiled sub render-tree
// animations.  Only one of these visitors is needed to visit an entire render
// tree.
class AnimateNode::ApplyVisitor : public NodeVisitor {
 public:
  ApplyVisitor(const TraverseList& traverse_list, base::TimeDelta time_offset);

  void Visit(animations::AnimateNode* /* animate */) OVERRIDE {
    // An invariant of AnimateNodes is that they should never contain descendant
    // AnimateNodes.
    NOTREACHED();
  }
  // Immediately switch to a templated visitor function.
  void Visit(CompositionNode* composition) OVERRIDE { VisitNode(composition); }
  void Visit(FilterNode* text) OVERRIDE { VisitNode(text); }
  void Visit(ImageNode* image) OVERRIDE { VisitNode(image); }
  void Visit(MatrixTransformNode* transform) OVERRIDE { VisitNode(transform); }
  void Visit(PunchThroughVideoNode* punch_through) OVERRIDE {
    VisitNode(punch_through);
  }
  void Visit(RectNode* rect) OVERRIDE { VisitNode(rect); }
  void Visit(RectShadowNode* rect) OVERRIDE { VisitNode(rect); }
  void Visit(TextNode* text) OVERRIDE { VisitNode(text); }

  // Returns the animated version of the node last visited.  This is how the
  // final animated result can be pulled from this visitor.
  const scoped_refptr<Node>& animated() const { return animated_; }

 private:
  template <typename T>
  typename base::enable_if<!ChildIterator<T>::has_children>::type VisitNode(
      T* node);
  template <typename T>
  typename base::enable_if<ChildIterator<T>::has_children>::type VisitNode(
      T* node);
  template <typename T>
  void ApplyAnimations(const TraverseListEntry& entry,
                       typename T::Builder* builder);
  TraverseListEntry AdvanceIterator(Node* node);

  // The time offset to be passed in to individual animations.
  base::TimeDelta time_offset_;
  // The animated version of the last node visited.
  scoped_refptr<Node> animated_;
  // A list of nodes that we are allowed to traverse into (i.e. a traversal that
  // guides us to animated nodes).  It assumes that a pre-order traversal will
  // be taken.
  const TraverseList& traverse_list_;
  // An iterator pointing to the next valid render tree node to visit.
  TraverseList::const_iterator iterator_;
};

AnimateNode::ApplyVisitor::ApplyVisitor(const TraverseList& traverse_list,
                                        base::TimeDelta time_offset)
    : time_offset_(time_offset), traverse_list_(traverse_list) {
  iterator_ = traverse_list_.begin();
}

template <typename T>
typename base::enable_if<!ChildIterator<T>::has_children>::type
AnimateNode::ApplyVisitor::VisitNode(T* node) {
  TraverseListEntry current_entry = AdvanceIterator(node);
  // If we don't have any children, then for this node to be visited, we must
  // have animations.
  DCHECK(current_entry.animations);
  typename T::Builder builder(node->data());
  ApplyAnimations<T>(current_entry, &builder);
  animated_ = new T(builder);
}

template <typename T>
typename base::enable_if<ChildIterator<T>::has_children>::type
AnimateNode::ApplyVisitor::VisitNode(T* node) {
  TraverseListEntry current_entry = AdvanceIterator(node);

  // Traverse the child nodes, but only the ones that are on the
  // |traverse_list_|.  In particular, the next node we are allowed to visit
  // is the one in the traverse list pointed to by |iterator_->node|.
  ChildIterator<T> child_iterator(node);
  bool children_modified = false;
  while (Node* child = child_iterator.GetCurrent()) {
    if (iterator_ == traverse_list_.end()) {
      // If we've reached the end of |traverse_list_| then we are done
      // iterating and it's time to return.
      break;
    }

    if (child == iterator_->node) {
      // If one of our children is next up on the path to animation, traverse
      // into it.
      child->Accept(this);
      // Traversing into the child means that it was animated, and so replaced
      // by an animated node while it was visited.  Thus, replace it in the
      // current node's child list with its animated version.
      child_iterator.ReplaceCurrent(animated_);
      children_modified = true;
    }

    child_iterator.Next();
  }

  if (current_entry.animations) {
    base::optional<typename T::Builder> builder;
    if (children_modified) {
      // Reuse the modified Builder object from child traversal if one of
      // our children was animated.
      builder.emplace(child_iterator.TakeReplacedChildrenBuilder());
    } else {
      // Create a fresh copy of the Builder object for this animated node, to
      // be passed into the animations.
      builder.emplace(node->data());
    }
    ApplyAnimations<T>(current_entry, &(*builder));
    animated_ = new T(*builder);
  } else {
    // If there were no animations targeting this node directly, then its
    // children must have been modified since otherwise it wouldn't be in
    // the traverse list.
    DCHECK(children_modified);
    animated_ = new T(child_iterator.TakeReplacedChildrenBuilder());
  }
}

template <typename T>
void AnimateNode::ApplyVisitor::ApplyAnimations(const TraverseListEntry& entry,
                                                typename T::Builder* builder) {
  TRACE_EVENT0("cobalt::renderer",
               "AnimateNode::ApplyVisitor::ApplyAnimations()");
  // Cast to the specific type we expect these animations to have.
  const AnimationList<T>* typed_node_animations =
      base::polymorphic_downcast<const AnimationList<T>*>(
          entry.animations.get());

  // Iterate through each animation applying them one at a time.
  for (typename AnimationList<T>::InternalList::const_iterator iter =
           typed_node_animations->data().animations.begin();
       iter != typed_node_animations->data().animations.end(); ++iter) {
    iter->Run(builder, time_offset_);
  }
}

AnimateNode::TraverseListEntry AnimateNode::ApplyVisitor::AdvanceIterator(
    Node* node) {
  // Check that the iterator that we are advancing past is indeed the one we
  // expect it to be.
  DCHECK_EQ(node, iterator_->node);
  return *(iterator_++);
}

AnimateNode::AnimateNode(const Builder& builder,
                         const scoped_refptr<Node>& source) {
  TRACE_EVENT0("cobalt::renderer", "AnimateNode::AnimateNode(builder, source)");
  CommonInit(builder.node_animation_map_, source);
}

AnimateNode::AnimateNode(const scoped_refptr<Node>& source) {
  TRACE_EVENT0("cobalt::renderer", "AnimateNode::AnimateNode(source)");
  CommonInit(Builder::InternalMap(), source);
}

scoped_refptr<Node> AnimateNode::Apply(base::TimeDelta time_offset) {
  TRACE_EVENT0("cobalt::renderer", "AnimateNode::Apply()");
  if (traverse_list_.empty()) {
    return source_;
  } else {
    ApplyVisitor apply_visitor(traverse_list_, time_offset);
    source_->Accept(&apply_visitor);
    return apply_visitor.animated();
  }
}

void AnimateNode::CommonInit(const Builder::InternalMap& node_animation_map,
                             const scoped_refptr<Node>& source) {
  TraverseListBuilder traverse_list_builder(node_animation_map,
                                            &traverse_list_);
  source->Accept(&traverse_list_builder);

  if (traverse_list_builder.replace_with_) {
    source_ = traverse_list_builder.replace_with_;
  } else {
    source_ = source;
  }

  if (!traverse_list_.empty()) {
    // We must adjust the resulting |traverse_list_| from an awkward
    // intermediate format of post-order right to left traversal to the more
    // natural pre-order left to right traversal expected by ApplyVisitor.
    // This can be done by simply reversing the list.
    std::reverse(traverse_list_.begin(), traverse_list_.end());
    DCHECK(source_.get() == traverse_list_.begin()->node);
  }
}

}  // namespace animations
}  // namespace render_tree
}  // namespace cobalt
