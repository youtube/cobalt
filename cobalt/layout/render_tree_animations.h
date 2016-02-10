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

#ifndef COBALT_LAYOUT_RENDER_TREE_ANIMATIONS_H_
#define COBALT_LAYOUT_RENDER_TREE_ANIMATIONS_H_

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "cobalt/cssom/computed_style_state.h"
#include "cobalt/cssom/css_style_declaration_data.h"
#include "cobalt/web_animations/animation_set.h"
#include "cobalt/web_animations/baked_animation_set.h"

namespace cobalt {
namespace layout {

// This file declares the function AddAnimations(), which is
// a convenience function that can be used by code that creates render tree
// nodes (e.g. layout box tree processing).

// This callback function defines a function that is expected to apply a given
// CSS style to a render tree node builder object.  A function meeting this
// specification can be passed into AddAnimations() in order to
// transfer animated CSS values into a render tree Node.  A function that
// satisfies this declaration may also be used to setup a render tree node
// that is not animated.
template <typename T>
class ApplyStyleToRenderTreeNode {
 public:
  typedef base::Callback<void(
      const scoped_refptr<const cssom::CSSStyleDeclarationData>&,
      typename T::Builder*)> Function;
};

// Helper function that applies a animation set to a base style to produce
// an animated style that is then passed into the provided
// ApplyStyleToRenderTreeNode<T>::Function callback function.
template <typename T>
void AnimateNode(
    const typename ApplyStyleToRenderTreeNode<T>::Function&
        apply_style_function,
    const web_animations::BakedAnimationSet& animations,
    const scoped_refptr<cssom::CSSStyleDeclarationData>& base_style,
    typename T::Builder* node_builder, base::TimeDelta time_elapsed) {
  scoped_refptr<cssom::CSSStyleDeclarationData> animated_style =
      new cssom::CSSStyleDeclarationData();
  animated_style->AssignFrom(*base_style);
  animations.Apply(time_elapsed, animated_style);
  apply_style_function.Run(animated_style, node_builder);
}

// If animations exist, this function will add an animation which represents
// the animations to the passed in NodeAnimationsMap.  The animation will
// target the passed in render tree node.
template <typename T>
void AddAnimations(const typename ApplyStyleToRenderTreeNode<T>::Function&
                       apply_style_function,
                   const cssom::ComputedStyleState& computed_style_state,
                   const scoped_refptr<T>& target_node,
                   render_tree::animations::NodeAnimationsMap::Builder*
                       node_animation_map_builder) {
  DCHECK(!computed_style_state.animations()->IsEmpty());
  scoped_refptr<cssom::CSSStyleDeclarationData> base_style_copy =
      new cssom::CSSStyleDeclarationData();
  base_style_copy->AssignFrom(*computed_style_state.style());

  node_animation_map_builder->Add(
      target_node, base::Bind(&AnimateNode<T>, apply_style_function,
                              web_animations::BakedAnimationSet(
                                  *computed_style_state.animations()),
                              base_style_copy));
}

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_RENDER_TREE_ANIMATIONS_H_
