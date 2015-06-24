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

#include "base/debug/trace_event.h"
#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/cssom/initial_style.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/rgba_color_value.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/html_html_element.h"
#include "cobalt/dom/html_body_element.h"
#include "cobalt/layout/block_formatting_block_container_box.h"
#include "cobalt/layout/box_generator.h"
#include "cobalt/layout/computed_style.h"
#include "cobalt/layout/declared_style.h"
#include "cobalt/layout/html_elements.h"
#include "cobalt/layout/specified_style.h"
#include "cobalt/layout/used_style.h"
#include "cobalt/render_tree/animations/node_animations_map.h"

namespace cobalt {
namespace layout {

namespace {

// The containing block in which the root element lives is a rectangle called
// the initial containing block. For continuous media, it has the dimensions
// of the viewport and is anchored at the canvas origin. This calculates the
// initial style for the initial containing block.
//   http://www.w3.org/TR/CSS2/visudet.html#containing-block-details
scoped_refptr<cssom::CSSStyleDeclarationData>
CreateInitialContainingBlockComputedStyle(const math::SizeF& viewport_size) {
  scoped_refptr<cssom::CSSStyleDeclarationData>
      initial_containing_block_computed_style =
          new cssom::CSSStyleDeclarationData();
  // Although the specification is silent about that, we override the otherwise
  // transparent background color of the initial containing block to ensure that
  // we always fill the entire viewport.
  initial_containing_block_computed_style->set_background_color(
      new cssom::RGBAColorValue(0xffffffff));
  initial_containing_block_computed_style->set_display(
      cssom::KeywordValue::GetBlock());
  initial_containing_block_computed_style->set_height(
      new cssom::LengthValue(viewport_size.height(), cssom::kPixelsUnit));
  initial_containing_block_computed_style->set_width(
      new cssom::LengthValue(viewport_size.width(), cssom::kPixelsUnit));

  // For the root element, which has no parent element, the inherited value is
  // the initial value of the property.
  //   http://www.w3.org/TR/css-cascade-3/#inheriting
  scoped_refptr<const cssom::CSSStyleDeclarationData> initial_style =
      cssom::GetInitialStyle();
  PromoteToSpecifiedStyle(initial_containing_block_computed_style,
                          initial_style);
  PromoteToComputedStyle(initial_containing_block_computed_style, initial_style,
                         NULL);

  return initial_containing_block_computed_style;
}

// Conditionally copies the background property. Returns true if anything is
// copied.
// The background color is copied if it is not transparent
// The background image is copied if it is not 'None'.
bool ConditionalCopyBackgroundStyle(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& source_style,
    scoped_refptr<cssom::CSSStyleDeclarationData> destination_style) {
  bool background_color_is_transparent =
      GetUsedColor(source_style->background_color()).a() == 0.0f;
  bool background_image_is_none =
      source_style->background_image() == cssom::KeywordValue::GetNone();

  if (!background_color_is_transparent || !background_image_is_none) {
    if (!background_color_is_transparent) {
      destination_style->set_background_color(source_style->background_color());
    }
    if (!background_image_is_none) {
      destination_style->set_background_image(source_style->background_image());
    }
    return true;
  }
  return false;
}

// This propagates the computed background style of the <html> or <body> element
// to the given style for the initial containing block.
//   http://www.w3.org/TR/css3-background/#body-background
void PropagateBackgroundStyleToInitialStyle(
    const scoped_refptr<dom::HTMLElement>& root,
    scoped_refptr<cssom::CSSStyleDeclarationData>
        initial_containing_block_computed_style) {
  dom::HTMLHtmlElement* html_element = root->owner_document()->html();
  if (html_element) {
    // Propagate the background style from the <html> element if there is any
    if (!ConditionalCopyBackgroundStyle(
            html_element->computed_style(),
            initial_containing_block_computed_style)) {
      dom::HTMLBodyElement* body_element = root->owner_document()->body();
      if (body_element) {
        // Otherwise, propagate the background style from the <body> element.
        ConditionalCopyBackgroundStyle(body_element->computed_style(),
                                       initial_containing_block_computed_style);
      }
    }
  }
}

// This creates the initial containing block after adding background color
// and image to the initial style, when needed.
//   http://www.w3.org/TR/CSS2/visudet.html#containing-block-details
scoped_ptr<BlockLevelBlockContainerBox> CreateInitialContainingBlock(
    const scoped_refptr<dom::HTMLElement>& root,
    scoped_refptr<cssom::CSSStyleDeclarationData>
        initial_containing_block_computed_style,
    const UsedStyleProvider* used_style_provider) {
  // The background color and image style may need to be propagated up from the
  // <body> element to the parent <html> element.
  //   http://www.w3.org/TR/css3-background/#body-background
  PropagateBackgroundStyleToInitialStyle(
      root, initial_containing_block_computed_style);
  return make_scoped_ptr(new BlockLevelBlockContainerBox(
      initial_containing_block_computed_style,
      cssom::TransitionSet::EmptyTransitionSet(), used_style_provider));
}

}  // namespace

RenderTreeWithAnimations Layout(
    const scoped_refptr<dom::HTMLElement>& root,
    const math::SizeF& viewport_size,
    const scoped_refptr<cssom::CSSStyleSheet>& user_agent_style_sheet,
    render_tree::ResourceProvider* resource_provider,
    icu::BreakIterator* line_break_iterator,
    loader::ImageCache* image_cache) {
  TRACE_EVENT0("cobalt::layout", "Layout()");

  scoped_refptr<dom::Document> document = root->owner_document();
  DCHECK(document) << "Element should be attached to document in order to "
                      "participate in layout.";

  // Update the matching rules of all elements in the subtree under root.
  {
    TRACE_EVENT0("cobalt::layout", "UpdateMatchingRules");
    UpdateMatchingRules(root, user_agent_style_sheet, document->style_sheets());
  }

  // Create the initial style for the initial containing block.
  scoped_refptr<cssom::CSSStyleDeclarationData>
      initial_containing_block_computed_style;
  {
    TRACE_EVENT0("cobalt::layout", "CreateInitialContainingBlockComputedStyle");
    initial_containing_block_computed_style =
        CreateInitialContainingBlockComputedStyle(viewport_size);
  }

  base::Time style_change_event_time;
  {
    // Determine the official time that this style change event took place. This
    // is needed (as opposed to repeatedly calling base::Time::Now()) because
    // all animations that may be triggered here must start at the exact same
    // time if they were triggered in the same style change event.
    //   http://www.w3.org/TR/css3-transitions/#starting
    TRACE_EVENT0("cobalt::layout", "Lock in Style Change Time");
    style_change_event_time = base::Time::Now();
  }

  // Update the computed style of all elements in the subtree under root.
  {
    TRACE_EVENT0("cobalt::layout", "UpdateComputedStyle");
    UpdateComputedStyles(root, initial_containing_block_computed_style,
                         style_change_event_time);
  }

  // Create initial containing block.
  UsedStyleProvider used_style_provider(resource_provider, image_cache);
  scoped_ptr<BlockLevelBlockContainerBox> initial_containing_block;
  {
    TRACE_EVENT0("cobalt::layout", "CreateInitialContainingBlock");
    initial_containing_block = CreateInitialContainingBlock(
        root, initial_containing_block_computed_style, &used_style_provider);
  }

  // Generate boxes.
  {
    TRACE_EVENT0("cobalt::layout", "BoxGeneration");
    BoxGenerator root_box_generator(initial_containing_block->computed_style(),
                                    &used_style_provider, line_break_iterator,
                                    initial_containing_block.get());
    root->Accept(&root_box_generator);
    BoxGenerator::Boxes root_boxes = root_box_generator.PassBoxes();
    for (BoxGenerator::Boxes::iterator root_box_iterator = root_boxes.begin();
         root_box_iterator != root_boxes.end(); ++root_box_iterator) {
      // Transfer the ownership of the root box from |ScopedVector|
      // to |scoped_ptr|.
      scoped_ptr<Box> root_box(*root_box_iterator);
      *root_box_iterator = NULL;

      initial_containing_block->AddChild(root_box.Pass());
    }
  }

  // Layout.
  {
    TRACE_EVENT0("cobalt::layout", "UpdateUsedSizes");
    initial_containing_block->set_used_left(0);
    initial_containing_block->set_used_top(0);
    initial_containing_block->UpdateUsedSizeIfInvalid(LayoutParams());
  }

  // Add to render tree.
  render_tree::animations::NodeAnimationsMap::Builder
      node_animations_map_builder;
  render_tree::CompositionNode::Builder render_tree_root_builder;
  {
    TRACE_EVENT0("cobalt::layout", "AddToRenderTree");
    initial_containing_block->AddToRenderTree(&render_tree_root_builder,
                                              &node_animations_map_builder);
  }

  return RenderTreeWithAnimations(
      new render_tree::CompositionNode(render_tree_root_builder.Pass()),
      new render_tree::animations::NodeAnimationsMap(
          node_animations_map_builder.Pass()));
}

}  // namespace layout
}  // namespace cobalt
