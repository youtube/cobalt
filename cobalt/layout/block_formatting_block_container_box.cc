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

#include "cobalt/layout/block_formatting_block_container_box.h"

#include "cobalt/layout/anonymous_block_box.h"
#include "cobalt/layout/block_formatting_context.h"
#include "cobalt/layout/computed_style.h"

namespace cobalt {
namespace layout {

BlockFormattingBlockContainerBox::BlockFormattingBlockContainerBox(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const cssom::TransitionSet* transitions)
    : BlockContainerBox(computed_style, transitions) {}

bool BlockFormattingBlockContainerBox::TryAddChild(scoped_ptr<Box>* child_box) {
  AddChild(child_box->Pass());
  return true;
}

void BlockFormattingBlockContainerBox::AddChild(scoped_ptr<Box> child_box) {
  switch (child_box->GetLevel()) {
    case kBlockLevel:
      // A block formatting context required, simply add a child.
      child_boxes_.push_back(child_box.release());
      break;

    case kInlineLevel:
      // An inline formatting context required,
      // add a child to an anonymous block box.
      GetOrAddAnonymousBlockBox()->AddInlineLevelChild(child_box.Pass());
      break;
  }
}

void BlockFormattingBlockContainerBox::DumpClassName(
    std::ostream* stream) const {
  *stream << "BlockFormattingBlockContainerBox ";
}

scoped_ptr<FormattingContext> BlockFormattingBlockContainerBox::LayoutChildren(
    const LayoutParams& child_layout_params) {
  // Lay out child boxes in the normal flow.
  //   http://www.w3.org/TR/CSS21/visuren.html#normal-flow
  // TODO(***REMOVED***): Handle absolutely positioned boxes:
  //               http://www.w3.org/TR/CSS21/visuren.html#absolute-positioning
  scoped_ptr<BlockFormattingContext> block_formatting_context(
      new BlockFormattingContext());
  for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    child_box->Layout(child_layout_params);
    block_formatting_context->UpdateUsedPosition(child_box);
  }
  return block_formatting_context.PassAs<FormattingContext>();
}

float BlockFormattingBlockContainerBox::GetChildDependentUsedWidth(
    const FormattingContext& formatting_context) const {
  const BlockFormattingContext& block_formatting_context =
      *base::polymorphic_downcast<const BlockFormattingContext*>(
          &formatting_context);
  return block_formatting_context.shrink_to_fit_width();
}

float BlockFormattingBlockContainerBox::GetChildDependentUsedHeight(
    const FormattingContext& formatting_context) const {
  // TODO(***REMOVED***): Implement for block-level non-replaced elements in normal
  //               flow when "overflow" computes to "visible":
  //               http://www.w3.org/TR/CSS21/visudet.html#normal-block
  // TODO(***REMOVED***): Implement for block-level, non-replaced elements in normal
  //               flow when "overflow" does not compute to "visible" and
  //               for inline-block, non-replaced elements:
  //               http://www.w3.org/TR/CSS21/visudet.html#block-root-margin
  NOTIMPLEMENTED();
  const BlockFormattingContext& block_formatting_context =
      *base::polymorphic_downcast<const BlockFormattingContext*>(
          &formatting_context);
  return block_formatting_context.last_child_box_used_bottom();  // Hack-hack.
}

AnonymousBlockBox*
BlockFormattingBlockContainerBox::GetOrAddAnonymousBlockBox() {
  AnonymousBlockBox* last_anonymous_block_box =
      child_boxes_.empty() ? NULL : child_boxes_.back()->AsAnonymousBlockBox();
  if (last_anonymous_block_box == NULL) {
    // TODO(***REMOVED***): Determine which transitions to propogate to the
    //               anonymous block box, instead of none at all.
    scoped_ptr<AnonymousBlockBox> last_anonymous_block_box_ptr(
        new AnonymousBlockBox(GetComputedStyleOfAnonymousBox(computed_style()),
                              cssom::TransitionSet::EmptyTransitionSet()));
    last_anonymous_block_box = last_anonymous_block_box_ptr.get();
    child_boxes_.push_back(last_anonymous_block_box_ptr.release());
  }
  return last_anonymous_block_box;
}

BlockLevelBlockContainerBox::BlockLevelBlockContainerBox(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const cssom::TransitionSet* transitions)
    : BlockFormattingBlockContainerBox(computed_style, transitions) {}

BlockLevelBlockContainerBox::~BlockLevelBlockContainerBox() {}

Box::Level BlockLevelBlockContainerBox::GetLevel() const { return kBlockLevel; }

InlineLevelBlockContainerBox::InlineLevelBlockContainerBox(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const cssom::TransitionSet* transitions)
    : BlockFormattingBlockContainerBox(computed_style, transitions) {}

InlineLevelBlockContainerBox::~InlineLevelBlockContainerBox() {}

Box::Level InlineLevelBlockContainerBox::GetLevel() const {
  return kInlineLevel;
}

}  // namespace layout
}  // namespace cobalt
