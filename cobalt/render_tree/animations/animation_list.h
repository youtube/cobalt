// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDER_TREE_ANIMATIONS_ANIMATION_LIST_H_
#define COBALT_RENDER_TREE_ANIMATIONS_ANIMATION_LIST_H_

#include <list>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
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

template <typename T>
class Animation {
 public:
  // An Animation<T>::Function represents a single animation that can be applied
  // on any render_tree::Node object of the specified template type argument.
  //
  // As an example, one could create an animation that linearly interpolates
  // between two color values on a TextNode object by first definining the
  // animation function (assuming ColorRGBA has operator*() defined):
  //
  //   void InterpolateTextColor(
  //       ColorRGBA final_color, base::TimeDelta duration,
  //       TextNode::Builder* text_node, base::TimeDelta time_elapsed) {
  //     if (time_elapsed < duration) {
  //       double progress = time_elapsed.InSecondsF() / duration.InSecondsF();
  //       text_node->color =
  //           text_node->color * (1 - progress) + final_color * progress;
  //     } else {
  //       text_node->color = final_color;
  //     }
  //   }
  //
  // You can then use base::Bind to package this function into a base::Callback
  // that matches the Animation<T>::Function signature:
  //
  //   AnimationList<TextNode>::Builder animation_list_builder;
  //   animation_list_builder.push_back(
  //       base::Bind(&InterpolateTextColor,
  //                  ColorRGBA(0.0f, 1.0f, 0.0f),
  //                  base::TimeDelta::FromSeconds(1)));
  //
  // You can now create an AnimationList object from the AnimationList::Builder
  // and ultimately add that to a AnimateNode::Builder object so that it can be
  // mapped to a specific TextNode that it should be applied to.
  typedef base::Callback<void(typename T::Builder*, base::TimeDelta)> Function;
  typedef base::Callback<void(typename T::Builder*)> TimeIndependentFunction;

  // Helper function to convert from time independent function to a time
  // dependent function.
  static void CallTimeIndependentFunction(
      const TimeIndependentFunction& time_independent_function,
      typename T::Builder* builder, base::TimeDelta time) {
    UNREFERENCED_PARAMETER(time);
    time_independent_function.Run(builder);
  }
};

// The AnimationListBase is used so that we can acquire a non-template handle
// to an AnimationList, which helps reduce code required in node_animation.h by
// letting us collect animation lists in a single collection, at the cost of
// needing to type cast in a few places.
class AnimationListBase : public base::RefCountedThreadSafe<AnimationListBase> {
 public:
  virtual base::TimeDelta GetExpiry() const = 0;
  virtual base::TimeDelta GetDependsOnTimeExpiry() const = 0;

 protected:
  virtual ~AnimationListBase() {}
  friend class base::RefCountedThreadSafe<AnimationListBase>;
};

// Since animation functions are templated on a specific render tree Node type,
// so must AnimationLists.
template <typename T>
class AnimationList : public AnimationListBase {
 public:
  // The actual data structure used internally to store the list of animations.
  // The decision to use a std::list here instead of say, an std::vector, is
  // because it is anticipated that AnimationLists will commonly have only
  // 1 element in them, since std::list does not make an attempt to reserve
  // capacity in advance, it can save on memory in these instances.
  typedef std::list<typename Animation<T>::Function> InternalList;

  // An object that provides a means to setting up a list before constructing
  // the immutable AnimationList.
  struct Builder {
    DECLARE_AS_MOVABLE(Builder);

    Builder() : expiry(base::TimeDelta::Max()),
                depends_on_time_expiry(base::TimeDelta::Max()) {}
    explicit Builder(Moved moved) { animations.swap(moved->animations); }
    explicit Builder(const typename Animation<T>::Function& single_animation,
                     base::TimeDelta expiry)
        : expiry(expiry), depends_on_time_expiry(expiry) {
      animations.push_back(single_animation);
    }
    explicit Builder(
        const typename Animation<T>::TimeIndependentFunction& single_animation,
        base::TimeDelta expiry)
        : expiry(expiry),
          // Since this animation is time independent, we mark its
          // time-dependent expiration to be already expired.
          depends_on_time_expiry(-base::TimeDelta::Max()) {
      // Transform from a time independent function to a time dependent
      // function, since we store all functions as time dependent functions.
      animations.push_back(base::Bind(Animation<T>::CallTimeIndependentFunction,
                                      single_animation));
    }

    InternalList animations;
    // When do the animations expire?  base::TimeDelta::Max() implies that they
    // never expire.
    base::TimeDelta expiry;

    // Similar to |expiry|, but only set for animations that depend on the
    // time parameter passed into their animation callback functions.
    // Optimizations can be performed for animations that don't depend on this.
    base::TimeDelta depends_on_time_expiry;
  };

  explicit AnimationList(typename Builder::Moved builder) : data_(builder) {}
  // Convenience constructor to allow for easy construction of AnimationLists
  // containing a single Animation.  |expiry| indicates the time at which the
  // animation ceases, or base::TimeDelta::Max() if that never occurs.
  explicit AnimationList(
      const typename Animation<T>::Function& single_animation,
      base::TimeDelta expiry)
      : data_(single_animation, expiry) {}
  explicit AnimationList(
      const typename Animation<T>::TimeIndependentFunction& single_animation,
      base::TimeDelta expiry)
      : data_(single_animation, expiry) {}

  const Builder& data() const { return data_; }

  base::TimeDelta GetExpiry() const override { return data_.expiry; }
  base::TimeDelta GetDependsOnTimeExpiry() const override {
    return data_.depends_on_time_expiry;
  }

 private:
  ~AnimationList() override {}

  const Builder data_;

  friend class AnimationListBase;
};

}  // namespace animations
}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_ANIMATIONS_ANIMATION_LIST_H_
