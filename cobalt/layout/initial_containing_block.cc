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

#include "cobalt/layout/initial_containing_block.h"

#include "base/trace_event/trace_event.h"
#include "cobalt/cssom/computed_style.h"
#include "cobalt/cssom/css_computed_style_declaration.h"
#include "cobalt/cssom/integer_value.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/rgba_color_value.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/html_body_element.h"
#include "cobalt/dom/html_html_element.h"
#include "cobalt/layout/base_direction.h"

namespace cobalt {
namespace layout {

// Conditionally copies the background property. Returns true if anything is
// copied.
bool PropagateBackgroundStyleAndTestIfChanged(
    const scoped_refptr<dom::HTMLElement>& element,
    scoped_refptr<cssom::MutableCSSComputedStyleData> destination_style) {
  if (!element || !element->computed_style() ||
      element->computed_style()->display() == cssom::KeywordValue::GetNone()) {
    return false;
  }

  const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style =
      element->computed_style();

  bool background_color_is_transparent =
      GetUsedColor(computed_style->background_color()).a() == 0.0f;

  cssom::PropertyListValue* background_image_list =
      base::polymorphic_downcast<cssom::PropertyListValue*>(
          computed_style->background_image().get());
  DCHECK_GT(background_image_list->value().size(), 0u);

  bool background_image_is_none =
      background_image_list->value().size() == 1 &&
      background_image_list->value()[0] == cssom::KeywordValue::GetNone();

  if (!background_color_is_transparent || !background_image_is_none) {
    // The background color is copied if it is not transparent.
    if (!background_color_is_transparent) {
      destination_style->set_background_color(
          computed_style->background_color());
    }
    // The background image is copied if it is not 'None'.
    if (!background_image_is_none) {
      destination_style->set_background_image(
          computed_style->background_image());
    }
    return true;
  }
  return false;
}

InitialContainingBlockCreationResults CreateInitialContainingBlock(
    const cssom::CSSComputedStyleData& default_initial_containing_block_style,
    const scoped_refptr<dom::Document>& document,
    UsedStyleProvider* used_style_provider,
    LayoutStatTracker* layout_stat_tracker) {
  TRACE_EVENT0("cobalt::layout", "CreateInitialContainingBlock");

  InitialContainingBlockCreationResults results;
  results.background_style_source = NULL;

  scoped_refptr<cssom::MutableCSSComputedStyleData>
      initial_containing_block_style = new cssom::MutableCSSComputedStyleData();
  initial_containing_block_style->AssignFrom(
      default_initial_containing_block_style);

  // Propagate the computed background style of the <html> or <body> element
  // to the given style for the initial containing block.
  //   https://www.w3.org/TR/css3-background/#body-background
  if (!PropagateBackgroundStyleAndTestIfChanged(
          document->html(), initial_containing_block_style)) {
    if (PropagateBackgroundStyleAndTestIfChanged(
            document->body(), initial_containing_block_style)) {
      results.background_style_source = document->body().get();
    }
  } else {
    results.background_style_source = document->html().get();
  }

  scoped_refptr<cssom::CSSComputedStyleDeclaration> initial_style_state =
      new cssom::CSSComputedStyleDeclaration();
  initial_style_state->SetData(initial_containing_block_style);
  initial_style_state->set_animations(new web_animations::AnimationSet());

  BaseDirection base_direction = kLeftToRightBaseDirection;
  auto html = document->html();
  if (html && html->GetUsedDirState() == dom::HTMLElement::kDirRightToLeft) {
    base_direction = kRightToLeftBaseDirection;
  }

  results.box = base::WrapRefCounted(new BlockLevelBlockContainerBox(
      initial_style_state, base_direction, used_style_provider,
      layout_stat_tracker));

  return results;
}

}  // namespace layout
}  // namespace cobalt
