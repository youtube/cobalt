// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_LAYOUT_RENDER_TREE_ANIMATIONS_H_
#define COBALT_LAYOUT_RENDER_TREE_ANIMATIONS_H_

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "cobalt/cssom/css_computed_style_data.h"
#include "cobalt/cssom/css_computed_style_declaration.h"
#include "cobalt/web_animations/animation_set.h"
#include "cobalt/web_animations/baked_animation_set.h"

namespace cobalt {
namespace layout {

// This file declares the function AddAnimations(), which is
// a convenience function that can be used by code that creates render tree
// nodes (e.g. layout box tree processing).

// This callback function defines a function that is expected to populate a
// given CSS style object for a render tree node type.
class PopulateBaseStyleForRenderTreeNode {
 public:
  typedef base::Callback<void(
      const scoped_refptr<const cssom::CSSComputedStyleData>&,
      const scoped_refptr<cssom::MutableCSSComputedStyleData>&)>
      Function;
};

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
      const scoped_refptr<const cssom::CSSComputedStyleData>&,
      typename T::Builder*)> Function;
};

// Helper function that applies an animation set to a base style to produce
// an animated style that is then passed into the provided
// ApplyStyleToRenderTreeNode<T>::Function callback function.
template <typename T>
void ApplyAnimation(
    const typename ApplyStyleToRenderTreeNode<T>::Function&
        apply_style_function,
    const web_animations::BakedAnimationSet& animations,
    const scoped_refptr<cssom::CSSComputedStyleData>& base_style,
    typename T::Builder* node_builder, base::TimeDelta time_elapsed) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> animated_style =
      new cssom::MutableCSSComputedStyleData();
  animated_style->AssignFrom(*base_style);
  animations.Apply(time_elapsed, animated_style.get());
  apply_style_function.Run(animated_style, node_builder);
}

// If animations exist, this function will add an animation which represents
// the animations to the passed in ApplyAnimation.  The animation will
// target the passed in render tree node.
template <typename T>
void AddAnimations(
    const typename PopulateBaseStyleForRenderTreeNode::Function&
        populate_base_style_function,
    const typename ApplyStyleToRenderTreeNode<T>::Function&
        apply_style_function,
    const cssom::CSSComputedStyleDeclaration& css_computed_style_declaration,
    const scoped_refptr<T>& target_node,
    render_tree::animations::AnimateNode::Builder* node_animation_map_builder) {
  DCHECK(!css_computed_style_declaration.animations()->IsEmpty());

  // Populate the base style.
  scoped_refptr<cssom::MutableCSSComputedStyleData> base_style =
      new cssom::MutableCSSComputedStyleData();
  populate_base_style_function.Run(css_computed_style_declaration.data(),
                                   base_style);

  web_animations::BakedAnimationSet baked_animation_set(
      *css_computed_style_declaration.animations());

  node_animation_map_builder->Add(
      target_node,
      base::Bind(&ApplyAnimation<T>, apply_style_function, baked_animation_set,
                 base_style),
      baked_animation_set.end_time());
}

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_RENDER_TREE_ANIMATIONS_H_
