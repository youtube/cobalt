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

namespace {

// For block-level boxes, precisely calculates the used width and determines
// whether the value depends on a containing block (it does not if the
// computed value is an absolute length).
//
// For inline-block boxes, approximates the used width by treating "auto"
// in the same way as for block boxes, and determines whether the exact value
// depends on children (and thus the second pass of the layout is needed).
class UsedWidthBasedOnContainingBlockProvider : public UsedWidthProvider {
 public:
  UsedWidthBasedOnContainingBlockProvider(float containing_block_width,
                                          Box::Level box_level);

  void VisitAuto() OVERRIDE;

  // Determines whether the exact value of used width depends on children (and
  // thus whether the second pass of layout is needed).
  bool width_depends_on_child_boxes() const {
    return width_depends_on_child_boxes_;
  }

 private:
  float GetUsedWidthAssumingBlockLevelInFlowBox() const;
  float GetAvailableWidthAssumingShrinkToFit() const;

  const Box::Level box_level_;

  bool width_depends_on_child_boxes_;
};

UsedWidthBasedOnContainingBlockProvider::
    UsedWidthBasedOnContainingBlockProvider(float containing_block_width,
                                            Box::Level box_level)
    : UsedWidthProvider(containing_block_width),
      box_level_(box_level),
      width_depends_on_child_boxes_(false) {}

void UsedWidthBasedOnContainingBlockProvider::VisitAuto() {
  // TODO(***REMOVED***): Handle absolutely positioned boxes:
  //               http://www.w3.org/TR/CSS21/visudet.html#abs-non-replaced-width
  switch (box_level_) {
    case Box::kBlockLevel:
      set_used_width(GetUsedWidthAssumingBlockLevelInFlowBox());
      break;
    case Box::kInlineLevel:
      // The width is shrink-to-fit but we don't know the sizes of children yet,
      // so we need to lay out child boxes within the available width and adjust
      // the used width later on.
      set_used_width(GetAvailableWidthAssumingShrinkToFit());
      width_depends_on_child_boxes_ = true;
      break;
    default:
      NOTREACHED();
      break;
  }
}

// The following constraints must hold among the used values of the properties:
//     margin-left + border-left-width + padding-left
//   + width
//   + padding-right + border-right-width + margin-right
//   = width of containing block
//
//   http://www.w3.org/TR/CSS21/visudet.html#blockwidth
float UsedWidthBasedOnContainingBlockProvider::
    GetUsedWidthAssumingBlockLevelInFlowBox() const {
  // TODO(***REMOVED***): Implement margins, borders, and paddings.
  return containing_block_width();
}

// The available width is the width of the containing block minus the used
// values of "margin-left", "border-left-width", "padding-left",
// "padding-right", "border-right-width", "margin-right".
//   http://www.w3.org/TR/CSS21/visudet.html#shrink-to-fit-float
float
UsedWidthBasedOnContainingBlockProvider::GetAvailableWidthAssumingShrinkToFit()
    const {
  // TODO(***REMOVED***): Implement margins, borders, and paddings.
  return containing_block_width();
}

}  // namespace

float BlockFormattingBlockContainerBox::GetUsedWidthBasedOnContainingBlock(
    float containing_block_width, bool* width_depends_on_containing_block,
    bool* width_depends_on_child_boxes) const {
  UsedWidthBasedOnContainingBlockProvider used_width_provider(
      containing_block_width, GetLevel());
  computed_style()->width()->Accept(&used_width_provider);
  *width_depends_on_containing_block =
      used_width_provider.width_depends_on_containing_block();
  *width_depends_on_child_boxes =
      used_width_provider.width_depends_on_child_boxes();
  return used_width_provider.used_width();
}

namespace {

// Calculates the used height if it does not depend on children.
// Otherwise |used_height()| will return zero and
// |height_depends_on_child_boxes()| will return true.
class UsedHeightBasedOnContainingBlockProvider : public UsedHeightProvider {
 public:
  UsedHeightBasedOnContainingBlockProvider(float containing_block_height,
                                           Box::Level box_level);

  void VisitAuto() OVERRIDE;

  // Determines whether the value of used height must be calculated after their
  // exact sizes are known.
  bool height_depends_on_child_boxes() const {
    return height_depends_on_child_boxes_;
  }

 private:
  float GetUsedHeightAssumingBlockLevelInFlowBox() const;

  const Box::Level box_level_;

  bool height_depends_on_child_boxes_;
};

UsedHeightBasedOnContainingBlockProvider::
    UsedHeightBasedOnContainingBlockProvider(float containing_block_height,
                                             Box::Level box_level)
    : UsedHeightProvider(containing_block_height),
      box_level_(box_level),
      height_depends_on_child_boxes_(false) {}

void UsedHeightBasedOnContainingBlockProvider::VisitAuto() {
  // TODO(***REMOVED***): Handle absolutely positioned boxes:
  //               http://www.w3.org/TR/CSS21/visudet.html#abs-non-replaced-height

  // The height depends on children but we don't know their sizes yet.
  set_used_height(0);
  height_depends_on_child_boxes_ = true;
}

}  // namespace

float BlockFormattingBlockContainerBox::GetUsedHeightBasedOnContainingBlock(
    float containing_block_height, bool* height_depends_on_child_boxes) const {
  UsedHeightBasedOnContainingBlockProvider used_height_provider(
      containing_block_height, GetLevel());
  computed_style()->height()->Accept(&used_height_provider);
  *height_depends_on_child_boxes =
      used_height_provider.height_depends_on_child_boxes();
  return used_height_provider.used_height();
}

scoped_ptr<FormattingContext>
BlockFormattingBlockContainerBox::UpdateUsedRectOfChildren(
    const LayoutParams& child_layout_params) {
  // Lay out child boxes in the normal flow.
  //   http://www.w3.org/TR/CSS21/visuren.html#normal-flow
  // TODO(***REMOVED***): Handle absolutely positioned boxes:
  //               http://www.w3.org/TR/CSS21/visuren.html#absolute-positioning
  scoped_ptr<BlockFormattingContext> block_formatting_context(
      new BlockFormattingContext(child_layout_params));
  for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    block_formatting_context->UpdateUsedRect(child_box);
  }
  return block_formatting_context.PassAs<FormattingContext>();
}

float BlockFormattingBlockContainerBox::GetUsedWidthBasedOnChildBoxes(
    const FormattingContext& formatting_context) const {
  const BlockFormattingContext& block_formatting_context =
      *base::polymorphic_downcast<const BlockFormattingContext*>(
          &formatting_context);
  return block_formatting_context.shrink_to_fit_width();
}

float BlockFormattingBlockContainerBox::GetUsedHeightBasedOnChildBoxes(
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
                              cssom::TransitionSet::EmptyTransitionSet(),
                              used_style_provider()));
    last_anonymous_block_box = last_anonymous_block_box_ptr.get();
    child_boxes_.push_back(last_anonymous_block_box_ptr.release());
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
