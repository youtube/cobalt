/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/render_tree/animations/node_animations_map.h"

#include "base/debug/trace_event.h"
#include "base/optional.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/render_tree/filter_node.h"
#include "cobalt/render_tree/image_node.h"
#include "cobalt/render_tree/matrix_transform_node.h"
#include "cobalt/render_tree/node_visitor.h"
#include "cobalt/render_tree/punch_through_video_node.h"
#include "cobalt/render_tree/rect_node.h"
#include "cobalt/render_tree/rect_shadow_node.h"
#include "cobalt/render_tree/text_node.h"

namespace cobalt {
namespace render_tree {
namespace animations {

void NodeAnimationsMap::Builder::AddInternal(
    const scoped_refptr<Node>& target_node,
    const scoped_refptr<AnimationListBase>& animation_list) {
  DCHECK(node_animation_map_.find(target_node) == node_animation_map_.end())
      << "The target render tree node already has an associated animation "
         "list.";

  node_animation_map_[target_node.get()] = animation_list;
  node_refs_.push_back(target_node);
}

void NodeAnimationsMap::Builder::Merge(
    const NodeAnimationsMap::Builder& other) {
#if !defined(NDEBUG)
  for (InternalMap::const_iterator iter = node_animation_map_.begin();
       iter != node_animation_map_.end(); ++iter) {
    DCHECK(other.node_animation_map_.find(iter->first) ==
           other.node_animation_map_.end()) <<
        "Only mutually exclusive NodeAnimationMaps can be merged!";
  }
#endif
  node_animation_map_.insert(
      other.node_animation_map_.begin(), other.node_animation_map_.end());
  node_refs_.insert(
      node_refs_.end(), other.node_refs_.begin(), other.node_refs_.end());
}

AnimationListBase* NodeAnimationsMap::GetAnimationsForNode(Node* target) const {
  Builder::InternalMap::const_iterator found =
      data_.node_animation_map_.find(target);
  if (found == data_.node_animation_map_.end()) return NULL;
  return found->second.get();
}

// This visitor object contains the logic for how to apply animation to each
// render tree node.  Things are most interesting for node types that may
// contain child render tree nodes, like CompositionNode.  In this case,
// they will need to be animated not only if animations directly affect them,
// but also if any of their children have been animated.
class NodeAnimationsMap::NodeVisitor : public render_tree::NodeVisitor {
 public:
  explicit NodeVisitor(const NodeAnimationsMap& animations,
                       const base::TimeDelta& time_elapsed);

  void Visit(CompositionNode* composition_node) OVERRIDE;
  void Visit(FilterNode* image_node) OVERRIDE;
  void Visit(ImageNode* image_node) OVERRIDE;
  void Visit(MatrixTransformNode* matrix_transform_node) OVERRIDE;
  void Visit(PunchThroughVideoNode* punch_through_video_node) OVERRIDE;
  void Visit(RectNode* rect_node) OVERRIDE;
  void Visit(RectShadowNode* rect_shadow_node) OVERRIDE;
  void Visit(TextNode* text_node) OVERRIDE;

  bool has_animated_node() const { return !!animated_node_; }

  // NodeVisitor objects are intended to be "one-shot"
  // visit objects.  The "return value" (e.g. the animated node) can be
  // retrieved by calling animated_node() after Visit() has been called.
  const scoped_refptr<Node>& animated_node() { return *animated_node_; }

  // Helper function to apply all animations in a AnimationList to a specific
  // node builder.
  template <typename T>
  static void ApplyAnimations(typename T::Builder* builder,
                              const AnimationListBase* node_animations,
                              base::TimeDelta time_elapsed);

  // Helper function for applying animations to simple nodes that don't have any
  // children or special cases.
  template <typename T>
  static base::optional<scoped_refptr<Node> >
  MaybeCloneLeafNodeAndApplyAnimations(
      T* node, const NodeAnimationsMap& animations,
      base::TimeDelta time_elapsed);

 private:
  const NodeAnimationsMap& animations_;
  const base::TimeDelta& time_elapsed_;

  base::optional<scoped_refptr<Node> > animated_node_;
};

NodeAnimationsMap::NodeVisitor::NodeVisitor(const NodeAnimationsMap& animations,
                                            const base::TimeDelta& time_elapsed)
    : animations_(animations), time_elapsed_(time_elapsed) {}

template <typename T>
void NodeAnimationsMap::NodeVisitor::ApplyAnimations(
    typename T::Builder* builder, const AnimationListBase* node_animations,
    base::TimeDelta time_elapsed) {
  // Cast to the specific type we expect these animations to have.
  const AnimationList<T>* typed_node_animations =
      base::polymorphic_downcast<const AnimationList<T>*>(node_animations);

  // Iterate through each animation applying them one at a time.
  for (typename AnimationList<T>::InternalList::const_iterator iter =
           typed_node_animations->data().animations.begin();
       iter != typed_node_animations->data().animations.end(); ++iter) {
    iter->Run(builder, time_elapsed);
  }
}

template <typename T>
base::optional<scoped_refptr<Node> >
NodeAnimationsMap::NodeVisitor::MaybeCloneLeafNodeAndApplyAnimations(
    T* node, const NodeAnimationsMap& animations,
    base::TimeDelta time_elapsed) {
  const AnimationListBase* node_animations =
      animations.GetAnimationsForNode(node);
  if (node_animations != NULL) {
    typename T::Builder builder(node->data());

    ApplyAnimations<T>(&builder, node_animations, time_elapsed);

    return scoped_refptr<Node>(new T(builder));
  } else {
    // No animations were applied to this node, return NULL to signify this.
    return base::nullopt;
  }
}

void NodeAnimationsMap::NodeVisitor::Visit(CompositionNode* composition_node) {
  // We declare the builder as an optional because we may not need one if we
  // have no animations and none of our children are animated.
  base::optional<CompositionNode::Builder> builder;

  const CompositionNode::Children& original_children =
      composition_node->data().children();

  // Iterate through our children and visit them to apply animations.  After
  // each one, check to see if it actually was animated, and if so, replace
  // this composition node's reference to that child with the newly animated
  // one.
  for (size_t i = 0; i < original_children.size(); ++i) {
    NodeVisitor child_visitor(animations_, time_elapsed_);
    original_children[i]->Accept(&child_visitor);

    // If a child was animated, then we must adjust our child node reference to
    // the newly animated one.
    if (child_visitor.has_animated_node()) {
      if (!builder) {
        builder.emplace(composition_node->data());
      }
      *builder->GetChild(static_cast<int>(i)) = child_visitor.animated_node();
    }
  }

  // Check if any animations apply directly to us, and if so, apply them.
  const AnimationListBase* animations =
      animations_.GetAnimationsForNode(composition_node);
  if (animations != NULL) {
    if (!builder) {
      builder.emplace(composition_node->data());
    }
    ApplyAnimations<CompositionNode>(&builder.value(), animations,
                                     time_elapsed_);
  }

  // Return the animated node.  If no animations took place, leave
  // animated_node_ as NULL to signify that no animations took place.
  if (builder) {
    animated_node_ = scoped_refptr<Node>(new CompositionNode(builder->Pass()));
  }
}

void NodeAnimationsMap::NodeVisitor::Visit(FilterNode* filter_node) {
  base::optional<FilterNode::Builder> builder;

  // First check to see if our source node has any animations, and if so,
  // animate it.
  NodeVisitor child_visitor(animations_, time_elapsed_);
  filter_node->data().source->Accept(&child_visitor);

  if (child_visitor.has_animated_node()) {
    // If our source node was animated, then so must we be, so create a
    // builder and set it up to point to our animated source node.
    builder.emplace(filter_node->data());
    builder->source = child_visitor.animated_node();
  }

  // Apply any animations to the filter node itself.
  const AnimationListBase* animations =
      animations_.GetAnimationsForNode(filter_node);
  if (animations != NULL) {
    if (!builder) {
      builder.emplace(filter_node->data());
    }
    ApplyAnimations<FilterNode>(&builder.value(), animations, time_elapsed_);
  }

  // Return the animated node.  If no animations took place, leave
  // animated_node_ as NULL to signify that no animations took place.
  if (builder) {
    animated_node_ = scoped_refptr<Node>(new FilterNode(*builder));
  }
}

void NodeAnimationsMap::NodeVisitor::Visit(ImageNode* image_node) {
  animated_node_ = MaybeCloneLeafNodeAndApplyAnimations(
      image_node, animations_, time_elapsed_);
}

void NodeAnimationsMap::NodeVisitor::Visit(
    MatrixTransformNode* matrix_transform_node) {
  base::optional<MatrixTransformNode::Builder> builder;

  // First check to see if our source node has any animations, and if so,
  // animate it.
  NodeVisitor child_visitor(animations_, time_elapsed_);
  matrix_transform_node->data().source->Accept(&child_visitor);

  if (child_visitor.has_animated_node()) {
    // If our source node was animated, then so must we be, so create a
    // builder and set it up to point to our animated source node.
    builder.emplace(matrix_transform_node->data());
    builder->source = child_visitor.animated_node();
  }

  // Apply any animations to the filter node itself.
  const AnimationListBase* animations =
      animations_.GetAnimationsForNode(matrix_transform_node);
  if (animations != NULL) {
    if (!builder) {
      builder.emplace(matrix_transform_node->data());
    }
    ApplyAnimations<MatrixTransformNode>(&builder.value(), animations,
                                         time_elapsed_);
  }

  // Return the animated node.  If no animations took place, leave
  // animated_node_ as NULL to signify that no animations took place.
  if (builder) {
    animated_node_ = scoped_refptr<Node>(new MatrixTransformNode(*builder));
  }
}

void NodeAnimationsMap::NodeVisitor::Visit(
    PunchThroughVideoNode* punch_through_video_node) {
  animated_node_ = MaybeCloneLeafNodeAndApplyAnimations(
      punch_through_video_node, animations_, time_elapsed_);
}

void NodeAnimationsMap::NodeVisitor::Visit(RectNode* rect_node) {
  // We do not use MaybeCloneLeafNodeAndApplyAnimations() here so that we can
  // construct our RectNode via a move of the RectNode::Builder object, instead
  // of a copy.
  const AnimationListBase* node_animations =
      animations_.GetAnimationsForNode(rect_node);
  if (node_animations != NULL) {
    RectNode::Builder builder(rect_node->data());

    ApplyAnimations<RectNode>(&builder, node_animations, time_elapsed_);

    // Move construct our RectNode from the RectNode::Builder object.
    animated_node_ = scoped_refptr<Node>(new RectNode(builder.Pass()));
  }
}

void NodeAnimationsMap::NodeVisitor::Visit(RectShadowNode* rect_shadow_node) {
  animated_node_ = MaybeCloneLeafNodeAndApplyAnimations(
      rect_shadow_node, animations_, time_elapsed_);
}

void NodeAnimationsMap::NodeVisitor::Visit(TextNode* text_node) {
  animated_node_ = MaybeCloneLeafNodeAndApplyAnimations(
      text_node, animations_, time_elapsed_);
}

scoped_refptr<Node> NodeAnimationsMap::Apply(const scoped_refptr<Node>& root,
                                             base::TimeDelta time_elapsed) {
  TRACE_EVENT0("cobalt::renderer", "NodeAnimationsMap::Apply()");
  NodeVisitor root_visitor(*this, time_elapsed);
  root->Accept(&root_visitor);
  return root_visitor.has_animated_node() ? root_visitor.animated_node() : root;
}

}  // namespace animations
}  // namespace render_tree
}  // namespace cobalt
