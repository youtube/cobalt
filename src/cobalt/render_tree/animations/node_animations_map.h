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

#ifndef COBALT_RENDER_TREE_ANIMATIONS_NODE_ANIMATIONS_MAP_H_
#define COBALT_RENDER_TREE_ANIMATIONS_NODE_ANIMATIONS_MAP_H_

#include <map>
#include <vector>

#include "base/containers/small_map.h"
#include "base/memory/ref_counted.h"
#include "cobalt/render_tree/animations/animation_list.h"
#include "cobalt/render_tree/movable.h"
#include "cobalt/render_tree/node.h"

namespace cobalt {
namespace render_tree {
namespace animations {

// A NodeAnimationsMap object represents a reference counted and immutable (so
// they can be easily shared between threads) set of animations, along with
// references to the render tree Nodes that they target.  NodeAnimationsMap
// objects contain animation information about corresponding render trees and
// are intended to be passed alongside them to modules that would like to
// animate render trees.
// NodeAnimationsMap objects essentially organize their collections of
// animations as an unordered map from render tree Node objects to
// AnimationLists. Thus, animation can be applied to a render tree by traversing
// it and for each element checking to see if any animations exist for them, and
// if so, apply them.  Indeed, the NodeAnimationsMap object's primary method,
// Apply(), does just this.
class NodeAnimationsMap : public base::RefCountedThreadSafe<NodeAnimationsMap> {
 public:
  // Use this NodeAnimationsMap::Builder object to construct your collection of
  // animations and corresponding render_tree Nodes.  When setup is complete,
  // construct a NodeAnimationsMap object with the Builder.
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
             const typename Animation<T>::Function& single_animation) {
      AddInternal(target_node, scoped_refptr<AnimationListBase>(
                                   new AnimationList<T>(single_animation)));
    }

    // Merge all mappings from another NodeAnimationsMap into this one.  There
    // cannot be any keys that are in both the merge target and source.
    void Merge(const NodeAnimationsMap& other) { Merge(other.data_); }
    void Merge(const NodeAnimationsMap::Builder& other);

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
    typedef base::SmallMap<
        std::map<Node*, scoped_refptr<AnimationListBase> >, 4> InternalMap;
    InternalMap node_animation_map_;
    std::vector<scoped_refptr<Node> > node_refs_;

    friend class NodeAnimationsMap;
  };

  // Since Builder objects are heavy-weight, this constructor allows them to
  // be move constructed into the NodeAnimationsMap object.
  explicit NodeAnimationsMap(Builder::Moved builder) : data_(builder) {}

  // Given the passed in current time, this method will apply all contained
  // animations to the passed in render tree, returning a new render tree that
  // has animations applied.  Nodes will only be newly created if they are
  // animated or one of their descendants are animated, otherwise the original
  // input node will be used and returned.
  scoped_refptr<Node> Apply(const scoped_refptr<Node>& root,
                            base::TimeDelta time_elapsed);

 private:
  ~NodeAnimationsMap() {}

  // Returns NULL if no animations exist for target node.
  AnimationListBase* GetAnimationsForNode(Node* target) const;

  const Builder data_;

  class NodeVisitor;
  friend class NodeVisitor;

  friend class base::RefCountedThreadSafe<NodeAnimationsMap>;
};

}  // namespace animations
}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_ANIMATIONS_NODE_ANIMATIONS_MAP_H_
