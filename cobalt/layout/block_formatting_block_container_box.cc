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

#include "cobalt/cssom/keyword_value.h"
#include "cobalt/layout/anonymous_block_box.h"
#include "cobalt/layout/block_formatting_context.h"
#include "cobalt/layout/computed_style.h"
#include "cobalt/layout/used_style.h"

namespace cobalt {
namespace layout {

BlockFormattingBlockContainerBox::BlockFormattingBlockContainerBox(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const cssom::TransitionSet* transitions,
    const UsedStyleProvider* used_style_provider)
    : BlockContainerBox(computed_style, transitions, used_style_provider) {}

bool BlockFormattingBlockContainerBox::TryAddChild(scoped_ptr<Box>* child_box) {
  AddChild(child_box->Pass());
  return true;
}

void BlockFormattingBlockContainerBox::AddChild(scoped_ptr<Box> child_box) {
  switch (child_box->GetLevel()) {
    case kBlockLevel:
      // A block formatting context required, simply add a child.
      PushBackDirectChild(child_box.Pass());
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

namespace {

//   http://www.w3.org/TR/CSS21/visudet.html#blockwidth
float GetUsedWidthAssumingBlockLevelInFlowBox(float containing_block_width) {
  // TODO(***REMOVED***): Implement margins, borders, and paddings.
  return containing_block_width;
}

// The available width is the width of the containing block minus the used
// values of "margin-left", "border-left-width", "padding-left",
// "padding-right", "border-right-width", "margin-right".
//   http://www.w3.org/TR/CSS21/visudet.html#shrink-to-fit-float
float GetAvailableWidthAssumingShrinkToFit(float containing_block_width) {
  // TODO(***REMOVED***): Implement margins, borders, and paddings.
  return containing_block_width;
}

}  // namespace

float BlockFormattingBlockContainerBox::GetUsedWidthBasedOnContainingBlock(
    float containing_block_width, bool* width_depends_on_containing_block,
    bool* width_depends_on_child_boxes) const {
  *width_depends_on_child_boxes = false;

  UsedBoxMetrics horizontal_metrics = UsedBoxMetrics::ComputeHorizontal(
      containing_block_width, *computed_style());
  *width_depends_on_containing_block =
      horizontal_metrics.size_depends_on_containing_block;

  if (horizontal_metrics.size) {
    return *horizontal_metrics.size;
  } else {
    // Width is set to 'auto', so return an appropriate value for proceeding
    // with
    // layout of the children.
    *width_depends_on_containing_block = true;

    if (computed_style()->position() == cssom::KeywordValue::GetAbsolute()) {
      *width_depends_on_child_boxes = true;
      return GetAvailableWidthAssumingShrinkToFit(containing_block_width);
    } else {
      switch (GetLevel()) {
        case Box::kBlockLevel:
          return GetUsedWidthAssumingBlockLevelInFlowBox(
              containing_block_width);
          break;
        case Box::kInlineLevel:
          // The width is shrink-to-fit but we don't know the sizes of children
          // yet, so we need to lay out child boxes within the available width
          // and adjust the used width later on.
          *width_depends_on_child_boxes = true;
          return GetAvailableWidthAssumingShrinkToFit(containing_block_width);
          break;
        default:
          NOTREACHED();
          break;
      }
    }
  }

  NOTREACHED();
  return 0;
}

float BlockFormattingBlockContainerBox::GetUsedHeightBasedOnContainingBlock(
    float containing_block_height, bool* height_depends_on_child_boxes) const {
  UsedBoxMetrics vertical_metrics = UsedBoxMetrics::ComputeVertical(
      containing_block_height, *computed_style());

  if (vertical_metrics.size) {
    // If the height is absolutely specified, we can return that immediately.
    *height_depends_on_child_boxes = false;
    return *vertical_metrics.size;
  } else {
    // Height is set to 'auto' and depends on layout results.
    *height_depends_on_child_boxes = true;
    return 0;
  }
}

scoped_ptr<FormattingContext>
BlockFormattingBlockContainerBox::UpdateUsedRectOfChildren(
    const LayoutParams& child_layout_params) {
  // Lay out child boxes in the normal flow.
  //   http://www.w3.org/TR/CSS21/visuren.html#normal-flow
  scoped_ptr<BlockFormattingContext> block_formatting_context(
      new BlockFormattingContext(child_layout_params));
  for (ChildBoxes::const_iterator child_box_iterator = child_boxes().begin();
       child_box_iterator != child_boxes().end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    block_formatting_context->UpdateUsedRect(child_box);
  }
  return block_formatting_context.PassAs<FormattingContext>();
}

AnonymousBlockBox*
BlockFormattingBlockContainerBox::GetOrAddAnonymousBlockBox() {
  AnonymousBlockBox* last_anonymous_block_box =
      child_boxes().empty() ? NULL
                            : child_boxes().back()->AsAnonymousBlockBox();
  if (last_anonymous_block_box == NULL) {
    // TODO(***REMOVED***): Determine which transitions to propogate to the
    //               anonymous block box, instead of none at all.
    scoped_ptr<AnonymousBlockBox> last_anonymous_block_box_ptr(
        new AnonymousBlockBox(GetComputedStyleOfAnonymousBox(computed_style()),
                              cssom::TransitionSet::EmptyTransitionSet(),
                              used_style_provider()));
    last_anonymous_block_box = last_anonymous_block_box_ptr.get();
    PushBackDirectChild(last_anonymous_block_box_ptr.PassAs<Box>());
  }
  return last_anonymous_block_box;
}

BlockLevelBlockContainerBox::BlockLevelBlockContainerBox(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const cssom::TransitionSet* transitions,
    const UsedStyleProvider* used_style_provider)
    : BlockFormattingBlockContainerBox(computed_style, transitions,
                                       used_style_provider) {}

BlockLevelBlockContainerBox::~BlockLevelBlockContainerBox() {}

Box::Level BlockLevelBlockContainerBox::GetLevel() const { return kBlockLevel; }

InlineLevelBlockContainerBox::InlineLevelBlockContainerBox(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const cssom::TransitionSet* transitions,
    const UsedStyleProvider* used_style_provider)
    : BlockFormattingBlockContainerBox(computed_style, transitions,
                                       used_style_provider) {}

InlineLevelBlockContainerBox::~InlineLevelBlockContainerBox() {}

Box::Level InlineLevelBlockContainerBox::GetLevel() const {
  return kInlineLevel;
}

}  // namespace layout
}  // namespace cobalt
