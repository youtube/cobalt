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

#ifndef RENDER_TREE_ANIMATIONS_ANIMATION_LIST_H_
#define RENDER_TREE_ANIMATIONS_ANIMATION_LIST_H_

#include <list>

#include "base/memory/ref_counted.h"
#include "cobalt/render_tree/animations/animation.h"
#include "cobalt/render_tree/movable.h"

namespace cobalt {
namespace render_tree {
namespace animations {

// An animation list is simply that, a list of animations.  Its usefulness over
// a raw std::list or std::vector is that it is a ref counted object and also
// guaranteed to be immutable (and thus, must be created by the associated
// builder object).  The property of being an immutable reference counted object
// allows this object to safely persist while being shared between multiple
// threads.

// The AnimationListBase is used so that we can acquire a non-template handle
// to an AnimationList, which helps reduce code required in node_animation.h by
// letting us collect animation lists in a single collection, at the cost of
// needing to type cast in a few places.

class AnimationListBase : public base::RefCountedThreadSafe<AnimationListBase> {
 protected:
  virtual ~AnimationListBase() {}
  friend class base::RefCountedThreadSafe<AnimationListBase>;
};

// Since animation objects are templated on a specific render tree Node type,
// so must AnimationLists.
template <typename T>
class AnimationList : public AnimationListBase {
 public:
  // The actual data structure used internally to store the list of animations.
  // The decision to use a std::list here instead of say, an std::vector, is
  // because it is anticipated that AnimationLists will commonly have only
  // 1 element in them, since std::list does not make an attempt to reserve
  // capacity in advance, it can save on memory in these instances.
  typedef std::list<scoped_refptr<Animation<T> > > InternalList;

  // An object that provides a means to setting up a list before constructing
  // the immutable AnimationList.
  struct Builder {
    DECLARE_AS_MOVABLE(Builder);

    Builder() {}
    explicit Builder(Moved moved) { animations.swap(moved->animations); }
    explicit Builder(const scoped_refptr<Animation<T> >& single_animation) {
      animations.push_back(single_animation);
    }

    InternalList animations;
  };

  explicit AnimationList(typename Builder::Moved builder) : data_(builder) {}
  // Convenience constructor to allow for easy construction of AnimationLists
  // containing a single Animation.
  explicit AnimationList(const scoped_refptr<Animation<T> >& single_animation)
      : data_(single_animation) {}

  const Builder& data() const { return data_; }

 private:
  ~AnimationList() OVERRIDE {}

  const Builder data_;

  friend class AnimationListBase;
};

}  // namespace animations
}  // namespace render_tree
}  // namespace cobalt

#endif  // RENDER_TREE_ANIMATIONS_ANIMATION_LIST_H_
