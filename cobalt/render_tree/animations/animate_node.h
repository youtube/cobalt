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

#ifndef COBALT_RENDER_TREE_ANIMATIONS_ANIMATE_NODE_H_
#define COBALT_RENDER_TREE_ANIMATIONS_ANIMATE_NODE_H_

#include <map>
#include <vector>

#include "base/containers/small_map.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/render_tree/animations/animation_list.h"
#include "cobalt/render_tree/movable.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/render_tree/node_visitor.h"

namespace cobalt {
namespace render_tree {
namespace animations {

// An AnimateNode describes a set of animations that affect the subtree attached
// to the AnimateNode.  In order to create an AnimateNode, one must first
// populate an AnimateNode::Builder with a mapping of render tree node
// references to corresponding animations that should be applied to them.
// Construction of an AnimateNode object requires a populated
// AnimateNode::Builder instance as well as the sub-tree that the animations
// apply to.  Upon construction, AnimateNode will compile the set of animated
// nodes into a format specialized to the associated subtree such that it is
// much faster to apply the animations.  Once an AnimateNode is constructed,
// it can generate static render trees representing frames of animation via
// calls to AnimateNode::Apply(), which returns a static render tree.
// When AnimateNodes are constructed they maintain an invariant that
// AnimateNodes never have other AnimateNodes as descendants.  It does this
// by traversing the subtree on construction and if another AnimateNode is
// encountered, its information is merged into this root AnimateNode, and the
// sub-AnimateNode is removed from the tree.
class AnimateNode : public Node {
 public:
  // Manages a mapping of render tree nodes to corresponding animations.  Users
  // of AnimateNode should populate the Builder with animations and then use
  // that to construct the AnimateNode.
  class Builder {
   public:
    DECLARE_AS_MOVABLE(Builder);

    Builder() {}
    explicit Builder(Moved moved) {
      node_animation_map_ = moved->node_animation_map_;
      node_refs_.swap(moved->node_refs_);
    }

    // This method is a template so that we can ensure that animations are not
    // mismatched with render tree nodes of the wrong type.
    template <typename T>
    void Add(const scoped_refptr<T>& target_node,
             const scoped_refptr<AnimationList<T> >& animation_list) {
      AddInternal(target_node, animation_list);
    }

    // Convenience method to attach a single animation to a target node.
    template <typename T>
    void Add(const scoped_refptr<T>& target_node,
             const typename Animation<T>::Function& single_animation,
             base::TimeDelta expiry) {
      AddInternal(target_node,
                  scoped_refptr<AnimationListBase>(
                      new AnimationList<T>(single_animation, expiry)));
    }

    template <typename T>
    void Add(
        const scoped_refptr<T>& target_node,
        const typename Animation<T>::TimeIndependentFunction& single_animation,
        base::TimeDelta expiry) {
      AddInternal(target_node,
                  scoped_refptr<AnimationListBase>(
                      new AnimationList<T>(single_animation, expiry)));
    }

    template <typename T>
    void Add(const scoped_refptr<T>& target_node,
             const typename Animation<T>::Function& single_animation) {
      AddInternal(target_node,
                  scoped_refptr<AnimationListBase>(new AnimationList<T>(
                      single_animation, base::TimeDelta::Max())));
    }
    template <typename T>
    void Add(const scoped_refptr<T>& target_node,
             const typename Animation<T>::TimeIndependentFunction&
                 single_animation) {
      AddInternal(target_node,
                  scoped_refptr<AnimationListBase>(new AnimationList<T>(
                      single_animation, base::TimeDelta::Max())));
    }

    // Merge all mappings from another AnimateNode::Builder into this one.
    // There cannot be any keys that are in both the merge target and source.
    void Merge(const Builder& other);

    // Returns true if there are no animations added to this
    // AnimateNode::Builder.
    bool empty() const { return node_animation_map_.empty(); }

   private:
    // A non-template function that contains the logic for storing a target
    // node and animation list pair.
    void AddInternal(const scoped_refptr<Node>& target_node,
                     const scoped_refptr<AnimationListBase>& animation_list);

    // The primary internal data structure used to organize and store the
    // mapping between target render tree node and animation list.
    // In many cases there are not many active animations, and so we use a
    // base::SmallMap for this.  std::map was found to be more performant than
    // base::hash_map, so it is used as the fallback map.
    typedef base::SmallMap<std::map<Node*, scoped_refptr<AnimationListBase> >,
                           4> InternalMap;
    InternalMap node_animation_map_;
    std::vector<scoped_refptr<Node> > node_refs_;

    friend class AnimateNode;
  };

  AnimateNode(const Builder& builder, const scoped_refptr<Node>& source);

  // This will create an AnimateNode with no animations.  It is useful because
  // construction of AnimateNodes maintain an invariant that there are no
  // sub-AnimateNodes underneath them.  Thus, adding a AnimateNode, even if it
  // has no animations, to an existing tree will result in existing AnimateNodes
  // being merged into the new root AnimateNode, which can simplify the
  // underlying tree.
  explicit AnimateNode(const scoped_refptr<Node>& source);

  // Cannot visit this node.
  void Accept(NodeVisitor* visitor) override { visitor->Visit(this); }

  // Since we don't have any bounds on the animations contained in this tree,
  // we cannot know the bounds of the tree over all time.  Indeed animations
  // may not have any bounds as time goes to infinity.  Despite this, we return
  // the bounds of the initial render tree to enable AnimateNodes to be setup
  // as children of CompositionNodes.
  math::RectF GetBounds() const override { return source_->GetBounds(); }

  base::TypeId GetTypeId() const override {
    return base::GetTypeId<AnimateNode>();
  }

  struct AnimateResults {
    // The animated render tree, which is guaranteed to not contain any
    // AnimateNodes.  Note that it is returned as an AnimateNode...  The actual
    // animated render tree can be obtained via |animated->source()|, the
    // AnimateNode however allows one to animate the animated node so that
    // it can be checked if anything has actually been animated or not.
    scoped_refptr<AnimateNode> animated;

    // Can be called in order to return a bounding rectangle around all
    // nodes that are actively animated.  The parameter specifies a "since"
    // time.  Any animations that had already expired *before* the "since" time
    // will not be included in the returned bounding box.
    // This information is likely to be used to enable optimizations where only
    // regions of the screen that have changed are rendered.
    base::Callback<math::RectF(base::TimeDelta)> get_animation_bounds_since;
  };
  // Apply the animations to the sub render tree with the given |time_offset|.
  // Note that if |animated| in the results is equivalent to |this| on which
  // Apply() is called, then no animations have actually taken place, and a
  // re-render can be avoided.
  AnimateResults Apply(base::TimeDelta time_offset);

  // Returns the sub-tree for which the animations apply to.
  const scoped_refptr<Node> source() const { return source_; }

  // Returns the time at which all animations will have completed, or
  // base::TimeDelta::Max() if they will never complete.
  // It will be true that AnimateNode::Apply(x) == AnimateNode::Apply(y) for
  // all x, y >= expiry().
  const base::TimeDelta& expiry() const { return expiry_; }

  // Similar to |expiry()|, but returns the expiration for all animations whose
  // callback function actually depends on the time parameter passed into them.
  const base::TimeDelta& depends_on_time_expiry() const {
    return depends_on_time_expiry_;
  }

 private:
  // The compiled node animation list is a sequence of nodes that are either
  // animated themselves, or on the path to an animated node.  Only nodes in
  // this sequence need to be traversed.  TraverseListEntry is an entry in this
  // list, complete with animations to be applied to the given node, if any.
  struct TraverseListEntry {
    TraverseListEntry(Node* node,
                      const scoped_refptr<AnimationListBase>& animations,
                      bool did_animate_previously)
        : node(node),
          animations(animations),
          did_animate_previously(did_animate_previously) {}
    explicit TraverseListEntry(Node* node)
        : node(node), did_animate_previously(false) {}

    Node* node;
    scoped_refptr<AnimationListBase> animations;
    // Used when checking animation bounds to see which nodes were actually
    // animated and whose bounds we need to accumulate.
    bool did_animate_previously;
  };
  typedef std::vector<TraverseListEntry> TraverseList;

  class RefCountedTraversalList;

  // A helper render tree visitor class used to compile sub render-tree
  // animations.
  class TraverseListBuilder;
  // A helper render tree visitor class used to apply compiled sub render-tree
  // animations.  This class follows the traversal generated by
  // TraverseListBuilder.
  class ApplyVisitor;
  // A helper class to traverse an animated tree, and compute a bounding
  // rectangle for all active animations.
  class BoundsVisitor;

  // This private constructor is used by AnimateNode::Apply() to create a new
  // AnimateNode that matches the resulting animated render tree.
  AnimateNode(const TraverseList& traverse_list, scoped_refptr<Node> source,
              const base::TimeDelta& expiry,
              const base::TimeDelta& depends_on_time_expiry,
              const base::TimeDelta& snapshot_time)
      : traverse_list_(traverse_list),
        source_(source),
        expiry_(expiry),
        depends_on_time_expiry_(depends_on_time_expiry),
        snapshot_time_(snapshot_time) {}

  void CommonInit(const Builder::InternalMap& node_animation_map,
                  const scoped_refptr<Node>& source);

  static math::RectF GetAnimationBoundsSince(
      const scoped_refptr<RefCountedTraversalList>& traverse_list,
      base::TimeDelta time_offset, const scoped_refptr<Node>& animated,
      base::TimeDelta since);

  // The compiled traversal list through the sub-tree represented by |source_|
  // that guides us towards all nodes that need to be animated.
  TraverseList traverse_list_;
  scoped_refptr<Node> source_;
  base::TimeDelta expiry_;
  base::TimeDelta depends_on_time_expiry_;
  // Time when the |source_| render tree was last animated, if known.  This
  // will get set when Apply() is called to produce a new AnimateNode that can
  // then be Apply()d again.  It can be used during the second apply to check
  // if some animations have expired.
  base::optional<base::TimeDelta> snapshot_time_;
};

}  // namespace animations
}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_ANIMATIONS_ANIMATE_NODE_H_
