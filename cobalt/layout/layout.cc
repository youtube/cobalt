/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/layout/layout.h"

#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/rgba_color_value.h"
#include "cobalt/layout/box_generator.h"
#include "cobalt/layout/computed_style.h"
#include "cobalt/layout/containing_block.h"
#include "cobalt/layout/initial_style.h"
#include "cobalt/layout/used_style.h"

namespace cobalt {
namespace layout {

// The containing block in which the root element lives is a rectangle called
// the initial containing block. For continuous media, it has the dimensions
// of the viewport and is anchored at the canvas origin.
//   http://www.w3.org/TR/CSS2/visudet.html#containing-block-details
scoped_ptr<ContainingBlock> CreateInitialContainingBlock(
    const math::SizeF& viewport_size, UsedStyleProvider* used_style_provider) {
  scoped_refptr<cssom::CSSStyleDeclarationData>
      initial_containing_block_computed_style =
          new cssom::CSSStyleDeclarationData();

  initial_containing_block_computed_style->set_background_color(
      new cssom::RGBAColorValue(0xffffffff));
  initial_containing_block_computed_style->set_color(
      InitialStyle::GetInstance()->color());
  initial_containing_block_computed_style->set_display(
      cssom::KeywordValue::GetBlock());
  initial_containing_block_computed_style->set_font_family(
      InitialStyle::GetInstance()->font_family());
  initial_containing_block_computed_style->set_font_size(
      InitialStyle::GetInstance()->font_size());
  initial_containing_block_computed_style->set_height(
      new cssom::LengthValue(viewport_size.height(), cssom::kPixelsUnit));
  initial_containing_block_computed_style->set_width(
      new cssom::LengthValue(viewport_size.width(), cssom::kPixelsUnit));
  initial_containing_block_computed_style->set_transform(
      InitialStyle::GetInstance()->transform());
  PromoteToComputedStyle(initial_containing_block_computed_style);
  return make_scoped_ptr(new ContainingBlock(
      NULL, initial_containing_block_computed_style, used_style_provider));
}

scoped_refptr<render_tree::Node> Layout(
    const scoped_refptr<dom::HTMLElement>& root_element,
    const math::SizeF& viewport_size,
    render_tree::ResourceProvider* resource_provider,
    const scoped_refptr<cssom::CSSStyleSheet>& user_agent_style_sheet) {
  UsedStyleProvider used_style_provider(resource_provider);

  scoped_ptr<ContainingBlock> initial_containing_block =
      CreateInitialContainingBlock(viewport_size, &used_style_provider);

  BoxGenerator box_generator(initial_containing_block.get(),
                             user_agent_style_sheet,
                             &used_style_provider);
  box_generator.set_is_root(true);
  root_element->Accept(&box_generator);

  LayoutOptions layout_options;
  layout_options.is_beginning_of_line = true;
  initial_containing_block->Layout(layout_options);

  render_tree::CompositionNode::Builder render_tree_root_builder;
  initial_containing_block->AddToRenderTree(&render_tree_root_builder);
  return new render_tree::CompositionNode(render_tree_root_builder.Pass());
}

}  // namespace layout
}  // namespace cobalt
