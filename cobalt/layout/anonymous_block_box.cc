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

#include "cobalt/layout/anonymous_block_box.h"
#include "cobalt/layout/inline_formatting_context.h"
#include "cobalt/layout/used_style.h"

namespace cobalt {
namespace layout {

AnonymousBlockBox::AnonymousBlockBox(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const cssom::TransitionSet* transitions,
    const UsedStyleProvider* used_style_provider)
    : BlockContainerBox(computed_style, transitions, used_style_provider),
      used_font_(used_style_provider->GetUsedFont(
          computed_style->font_family(), computed_style->font_size(),
          computed_style->font_weight())) {}


Box::Level AnonymousBlockBox::GetLevel() const { return kBlockLevel; }

AnonymousBlockBox* AnonymousBlockBox::AsAnonymousBlockBox() { return this; }

bool AnonymousBlockBox::TryAddChild(scoped_ptr<Box>* /*child_box*/) {
  NOTREACHED();
  return false;
}

void AnonymousBlockBox::AddInlineLevelChild(scoped_ptr<Box> child_box) {
  DCHECK_EQ(kInlineLevel, child_box->GetLevel());
  PushBackDirectChild(child_box.Pass());
}

void AnonymousBlockBox::DumpClassName(std::ostream* stream) const {
  *stream << "AnonymousBlockBox ";
}

float AnonymousBlockBox::GetUsedWidthBasedOnContainingBlock(
    float containing_block_width, bool* width_depends_on_containing_block,
    bool* width_depends_on_child_boxes) const {
  *width_depends_on_containing_block = true;
  *width_depends_on_child_boxes = false;

  // Anonymous block boxes are ignored when resolving percentage values
  // that would refer to it: the closest non-anonymous ancestor box is used
  // instead.
  //   http://www.w3.org/TR/CSS21/visuren.html#anonymous-block-level
  return containing_block_width;
}

float AnonymousBlockBox::GetUsedHeightBasedOnContainingBlock(
    float containing_block_height, bool* height_depends_on_child_boxes) const {
  *height_depends_on_child_boxes = true;

  // Anonymous block boxes are ignored when resolving percentage values
  // that would refer to it: the closest non-anonymous ancestor box is used
  // instead.
  //   http://www.w3.org/TR/CSS21/visuren.html#anonymous-block-level
  return containing_block_height;
}

scoped_ptr<FormattingContext> AnonymousBlockBox::UpdateUsedRectOfChildren(
    const LayoutParams& child_layout_params) {
  // Lay out child boxes in the normal flow.
  //   http://www.w3.org/TR/CSS21/visuren.html#normal-flow
  // TODO(***REMOVED***): Handle absolutely positioned boxes:
  //               http://www.w3.org/TR/CSS21/visuren.html#absolute-positioning
  render_tree::FontMetrics font_metrics = used_font_->GetFontMetrics();
  scoped_ptr<InlineFormattingContext> inline_formatting_context(
      new InlineFormattingContext(child_layout_params, font_metrics.x_height));
  for (ChildBoxes::const_iterator child_box_iterator = child_boxes().begin();
       child_box_iterator != child_boxes().end();) {
    Box* child_box = *child_box_iterator;
    ++child_box_iterator;

    scoped_ptr<Box> child_box_after_split =
        inline_formatting_context->QueryUsedRectAndMaybeSplit(child_box);
    if (child_box_after_split) {
      // Re-insert the rest of the child box and attempt to lay it out in
      // the next iteration of the loop.
      // TODO(***REMOVED***): If every child box is split, this becomes an O(N^2)
      //               operation where N is the number of child boxes. Consider
      //               using a new vector instead and swap its contents after
      //               the layout is done.
      child_box_iterator =
          InsertDirectChild(child_box_iterator, child_box_after_split.Pass());
    }
  }
  inline_formatting_context->EndQueries();
  return inline_formatting_context.PassAs<FormattingContext>();
}

}  // namespace layout
}  // namespace cobalt
