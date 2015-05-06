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

namespace cobalt {
namespace layout {

Box::Level AnonymousBlockBox::GetLevel() const { return kBlockLevel; }

AnonymousBlockBox* AnonymousBlockBox::AsAnonymousBlockBox() { return this; }

bool AnonymousBlockBox::TryAddChild(scoped_ptr<Box>* /*child_box*/) {
  NOTREACHED();
  return false;
}

void AnonymousBlockBox::AddInlineLevelChild(scoped_ptr<Box> child_box) {
  DCHECK_EQ(kInlineLevel, child_box->GetLevel());
  child_boxes_.push_back(child_box.release());
}

void AnonymousBlockBox::DumpClassName(std::ostream* stream) const {
  *stream << "AnonymousBlockBox ";
}

scoped_ptr<FormattingContext> AnonymousBlockBox::LayoutChildren(
    const LayoutParams& child_layout_params) {
  // Lay out child boxes in the normal flow.
  //   http://www.w3.org/TR/CSS21/visuren.html#normal-flow
  // TODO(***REMOVED***): Handle absolutely positioned boxes:
  //               http://www.w3.org/TR/CSS21/visuren.html#absolute-positioning
  scoped_ptr<InlineFormattingContext> inline_formatting_context(
      new InlineFormattingContext(
          child_layout_params.containing_block_size.width()));
  for (ChildBoxes::iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end();) {
    Box* child_box = *child_box_iterator;
    ++child_box_iterator;
    child_box->Layout(child_layout_params);

    scoped_ptr<Box> child_box_after_split =
        inline_formatting_context->QueryUsedPositionAndMaybeSplit(child_box);
    if (child_box_after_split) {
      // Re-insert the rest of the child box and attempt to lay it out in
      // the next iteration of the loop.
      // TODO(***REMOVED***): If every child box is split, this becomes an O(N^2)
      //               operation where N is the number of child boxes. Consider
      //               using a new vector instead and swap its contents after
      //               the layout is done.
      child_box_iterator = child_boxes_.insert(child_box_iterator,
                                               child_box_after_split.release());
    }
  }
  inline_formatting_context->EndQueries();
  return inline_formatting_context.PassAs<FormattingContext>();
}

float AnonymousBlockBox::GetChildDependentUsedWidth(
    const FormattingContext& formatting_context) const {
  const InlineFormattingContext& inline_formatting_context =
      *base::polymorphic_downcast<const InlineFormattingContext*>(
          &formatting_context);
  return inline_formatting_context.GetShrinkToFitWidth();
}

float AnonymousBlockBox::GetChildDependentUsedHeight(
    const FormattingContext& formatting_context) const {
  // TODO(***REMOVED***): Implement for block-level non-replaced elements in normal
  //               flow when "overflow" computes to "visible":
  //               http://www.w3.org/TR/CSS21/visudet.html#normal-block
  // TODO(***REMOVED***): Implement for block-level, non-replaced elements in normal
  //               flow when "overflow" does not compute to "visible" and
  //               for inline-block, non-replaced elements:
  //               http://www.w3.org/TR/CSS21/visudet.html#block-root-margin
  NOTIMPLEMENTED();
  const InlineFormattingContext& inline_formatting_context =
      *base::polymorphic_downcast<const InlineFormattingContext*>(
          &formatting_context);
  return inline_formatting_context.GetLastLineBoxUsedBottom();  // Hack-hack.
}

}  // namespace layout
}  // namespace cobalt
