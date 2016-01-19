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

#include "cobalt/layout/initial_containing_block.h"

#include "base/debug/trace_event.h"
#include "cobalt/cssom/computed_style.h"
#include "cobalt/cssom/computed_style_state.h"
#include "cobalt/cssom/integer_value.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/rgba_color_value.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/html_body_element.h"
#include "cobalt/dom/html_html_element.h"

namespace cobalt {
namespace layout {

// Conditionally copies the background property. Returns true if anything is
// copied.
// The background color is copied if it is not transparent
// The background image is copied if it is not 'None'.
bool ConditionalCopyBackgroundStyle(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& source_style,
    scoped_refptr<cssom::CSSStyleDeclarationData> destination_style) {
  bool background_color_is_transparent =
      GetUsedColor(source_style->background_color()).a() == 0.0f;

  cssom::PropertyListValue* background_image_list =
      base::polymorphic_downcast<cssom::PropertyListValue*>(
          source_style->background_image().get());
  DCHECK_GT(background_image_list->value().size(), 0u);

  bool background_image_is_none =
      background_image_list->value().size() == 1 &&
      background_image_list->value()[0] == cssom::KeywordValue::GetNone();

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
//   https://www.w3.org/TR/css3-background/#body-background
void PropagateBackgroundStyleToInitialStyle(
    const scoped_refptr<dom::Document>& document,
    scoped_refptr<cssom::CSSStyleDeclarationData>
        initial_containing_block_computed_style) {
  dom::HTMLHtmlElement* html_element = document->html();
  if (html_element) {
    // Propagate the background style from the <html> element if there is any
    if (!ConditionalCopyBackgroundStyle(
            html_element->computed_style(),
            initial_containing_block_computed_style)) {
      dom::HTMLBodyElement* body_element = document->body();
      if (body_element) {
        // Otherwise, propagate the background style from the <body> element.
        ConditionalCopyBackgroundStyle(body_element->computed_style(),
                                       initial_containing_block_computed_style);
      }
    }
  }
}

scoped_refptr<BlockLevelBlockContainerBox> CreateInitialContainingBlock(
    const scoped_refptr<cssom::CSSStyleDeclarationData>&
        initial_containing_block_style,
    const scoped_refptr<dom::Document>& document,
    UsedStyleProvider* used_style_provider) {
  TRACE_EVENT0("cobalt::layout", "CreateInitialContainingBlock");

  // The background color and image style may need to be propagated up from the
  // <body> element to the parent <html> element.
  //   https://www.w3.org/TR/css3-background/#body-background
  PropagateBackgroundStyleToInitialStyle(
      document, initial_containing_block_style);

  scoped_refptr<cssom::ComputedStyleState> initial_style_state =
      new cssom::ComputedStyleState();
  initial_style_state->set_style(initial_containing_block_style);
  initial_style_state->set_animations(new web_animations::AnimationSet());
  return make_scoped_refptr(new BlockLevelBlockContainerBox(
      initial_style_state, used_style_provider));
}

}  // namespace layout
}  // namespace cobalt
