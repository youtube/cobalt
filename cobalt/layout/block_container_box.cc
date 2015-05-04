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

#include "cobalt/layout/block_container_box.h"

#include "cobalt/layout/block_formatting_context.h"
#include "cobalt/layout/computed_style.h"
#include "cobalt/layout/inline_formatting_context.h"
#include "cobalt/layout/used_style.h"

namespace cobalt {
namespace layout {

class AnonymousBlockBox : public BlockLevelBlockContainerBox {
 public:
  explicit AnonymousBlockBox(
      const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style)
      : BlockLevelBlockContainerBox(computed_style) {}

  virtual AnonymousBlockBox* AsAnonymousBlockBox() OVERRIDE { return this; }

 protected:
  // From |Box|.
  void DumpClassName(std::ostream* stream) const OVERRIDE;
};

void AnonymousBlockBox::DumpClassName(std::ostream* stream) const {
  *stream << "AnonymousBlockBox ";
}

BlockContainerBox::BlockContainerBox(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style)
    : ContainerBox(computed_style),
      formatting_context_(kInlineFormattingContext) {}

BlockContainerBox::~BlockContainerBox() {}

void BlockContainerBox::Layout(const LayoutOptions& layout_options) {
  switch (formatting_context_) {
    case kBlockFormattingContext:
      LayoutAssuming<BlockFormattingContext>(layout_options);
      break;
    case kInlineFormattingContext:
      LayoutAssuming<InlineFormattingContext>(layout_options);
      break;
  }
}

scoped_ptr<Box> BlockContainerBox::TrySplitAt(float /*available_width*/) {
  return scoped_ptr<Box>();
}

bool BlockContainerBox::IsCollapsed() const {
  DCHECK_EQ(kInlineLevel, GetLevel());
  return false;
}

bool BlockContainerBox::HasLeadingWhiteSpace() const {
  DCHECK_EQ(kInlineLevel, GetLevel());
  return false;
}

bool BlockContainerBox::HasTrailingWhiteSpace() const {
  DCHECK_EQ(kInlineLevel, GetLevel());
  return false;
}

void BlockContainerBox::CollapseLeadingWhiteSpace() {
  DCHECK_EQ(kInlineLevel, GetLevel());
  // Do nothing.
}

void BlockContainerBox::CollapseTrailingWhiteSpace() {
  DCHECK_EQ(kInlineLevel, GetLevel());
  // Do nothing.
}

bool BlockContainerBox::JustifiesLineExistence() const {
  DCHECK_EQ(kInlineLevel, GetLevel());
  return true;
}

bool BlockContainerBox::AffectsBaselineInBlockFormattingContext() const {
  return !!height_above_baseline_;
}

float BlockContainerBox::GetHeightAboveBaseline() const {
  // If the box does not have a baseline, align the bottom margin edge
  // with the parent's baseline.
  //   http://www.w3.org/TR/CSS21/visudet.html#line-height
  // TODO(***REMOVED***): Fix when margins are implemented.
  return height_above_baseline_.value_or(used_frame().height());
}

bool BlockContainerBox::TryAddChild(scoped_ptr<Box>* child_box) {
  AddChild(child_box->Pass());
  return true;
}

scoped_ptr<ContainerBox> BlockContainerBox::TrySplitAtEnd() {
  return scoped_ptr<ContainerBox>();
}

void BlockContainerBox::AddChild(scoped_ptr<Box> child_box) {
  switch (formatting_context_) {
    case kBlockFormattingContext:
      AddChildAssumingBlockFormattingContext(child_box.Pass());
      break;
    case kInlineFormattingContext:
      AddChildAssumingInlineFormattingContext(child_box.Pass());
      break;
  }
}

void BlockContainerBox::AddContentToRenderTree(
    render_tree::CompositionNode::Builder* composition_node_builder) const {
  // Render child boxes.
  // TODO(***REMOVED***): Take a stacking context into account:
  //               http://www.w3.org/TR/CSS21/visuren.html#z-index
  for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    child_box->AddToRenderTree(composition_node_builder);
  }
}

bool BlockContainerBox::IsTransformable() const { return true; }

void BlockContainerBox::DumpClassName(std::ostream* stream) const {
  *stream << "BlockContainerBox ";
}

void BlockContainerBox::DumpProperties(std::ostream* stream) const {
  ContainerBox::DumpProperties(stream);

  *stream << std::boolalpha << "affects_baseline_in_block_formatting_context="
          << AffectsBaselineInBlockFormattingContext() << " "
          << std::noboolalpha;

  *stream << "formatting_context=";
  switch (formatting_context_) {
    case kBlockFormattingContext:
      *stream << "block ";
      break;
    case kInlineFormattingContext:
      *stream << "inline ";
      break;
  }
}

void BlockContainerBox::DumpChildrenWithIndent(std::ostream* stream,
                                               int indent) const {
  ContainerBox::DumpChildrenWithIndent(stream, indent);

  for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    child_box->DumpWithIndent(stream, indent);
  }
}

void BlockContainerBox::AddChildAssumingBlockFormattingContext(
    scoped_ptr<Box> child_box) {
  FormattingContext required_formatting_context =
      ConvertLevelToFormattingContext(child_box->GetLevel());

  switch (required_formatting_context) {
    case kBlockFormattingContext:
      // Required and inferred formatting contexts match, simply add a child.
      child_boxes_.push_back(child_box.release());
      break;

    case kInlineFormattingContext:
      // If a block container box has a block-level box inside it, then we force
      // it to have only block-level boxes inside it.
      //   http://www.w3.org/TR/CSS21/visuren.html#anonymous-block-level
      GetOrAddAnonymousBlockBox()->AddChild(child_box.Pass());
      break;
  }
}

void BlockContainerBox::AddChildAssumingInlineFormattingContext(
    scoped_ptr<Box> child_box) {
  FormattingContext required_formatting_context =
      ConvertLevelToFormattingContext(child_box->GetLevel());

  switch (required_formatting_context) {
    case kBlockFormattingContext:
      // After a bunch of inline-level child boxes, a block-level child box
      // is added, inferring a block formatting context.
      // TODO(***REMOVED***): Discount out-of-flow boxes:
      //               http://www.w3.org/TR/CSS21/visuren.html#positioning-scheme
      formatting_context_ = kBlockFormattingContext;

      if (!child_boxes_.empty()) {
        // Wrap all previously added children into the anonymous block box.
        scoped_ptr<AnonymousBlockBox> anonymous_block_box(new AnonymousBlockBox(
            GetComputedStyleOfAnonymousBox(computed_style())));
        anonymous_block_box->SwapChildBoxes(&child_boxes_);
        child_boxes_.push_back(anonymous_block_box.release());
      }
      break;

    case kInlineFormattingContext:
      // All set.
      break;
  }

  // Finally, add a child box.
  child_boxes_.push_back(child_box.release());
}

BlockContainerBox::FormattingContext
BlockContainerBox::ConvertLevelToFormattingContext(Level level) {
  switch (level) {
    case kBlockLevel:
      return kBlockFormattingContext;
    case kInlineLevel:
      return kInlineFormattingContext;
    default:
      NOTREACHED();
      return static_cast<FormattingContext>(-1);
  }
}

AnonymousBlockBox* BlockContainerBox::GetOrAddAnonymousBlockBox() {
  AnonymousBlockBox* last_anonymous_block_box =
      child_boxes_.empty() ? NULL : child_boxes_.back()->AsAnonymousBlockBox();
  if (last_anonymous_block_box == NULL) {
    scoped_ptr<AnonymousBlockBox> last_anonymous_block_box_ptr(
        new AnonymousBlockBox(
            GetComputedStyleOfAnonymousBox(computed_style())));
    last_anonymous_block_box = last_anonymous_block_box_ptr.get();
    child_boxes_.push_back(last_anonymous_block_box_ptr.release());
  }
  return last_anonymous_block_box;
}

template <typename FormattingContextT>
void BlockContainerBox::LayoutAssuming(const LayoutOptions& layout_options) {
  // Approximate the size of the containing block, so that relatively and
  // statically positioned child boxes can use it during their layout. The size
  // is calculated with the assumption that there are no children.
  //   http://www.w3.org/TR/CSS21/visudet.html#containing-block-details
  bool width_depends_on_child_boxes;
  bool height_depends_on_child_boxes;
  LayoutOptions child_layout_options =
      GetChildLayoutOptions(layout_options, &width_depends_on_child_boxes,
                            &height_depends_on_child_boxes);

  // Do a first pass of the layout using the approximate size.
  child_layout_options.shrink_if_width_depends_on_containing_block =
      width_depends_on_child_boxes;
  scoped_ptr<FormattingContextT> formatting_context =
      LayoutChildrenAssuming<FormattingContextT>(child_layout_options);

  // Calculate the exact width of the block container box.
  if (width_depends_on_child_boxes) {
    child_layout_options.containing_block_size.set_width(
        GetChildDependentUsedWidth(*formatting_context));

    // Do a second pass of the layout using the exact size.
    // TODO(***REMOVED***): Laying out the children twice has an exponential worst-case
    //               complexity (because every child could lay out itself twice
    //               as well). Figure out if there is a better way.
    child_layout_options.shrink_if_width_depends_on_containing_block = false;
    formatting_context =
        LayoutChildrenAssuming<FormattingContextT>(child_layout_options);
  }
  used_frame().set_width(child_layout_options.containing_block_size.width());

  // Calculate the exact height of the block container box.
  used_frame().set_height(
      height_depends_on_child_boxes
          ? GetChildDependentUsedHeight(*formatting_context)
          : child_layout_options.containing_block_size.height());
  height_above_baseline_ = formatting_context->height_above_baseline();
}

template <>
scoped_ptr<BlockFormattingContext> BlockContainerBox::LayoutChildrenAssuming<
    BlockFormattingContext>(const LayoutOptions& child_layout_options) {
  // Lay out child boxes in the normal flow.
  //   http://www.w3.org/TR/CSS21/visuren.html#normal-flow
  // TODO(***REMOVED***): Handle absolutely positioned boxes:
  //               http://www.w3.org/TR/CSS21/visuren.html#absolute-positioning
  scoped_ptr<BlockFormattingContext> block_formatting_context(
      new BlockFormattingContext());
  for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    child_box->Layout(child_layout_options);
    block_formatting_context->UpdateUsedPosition(child_box);
  }
  return block_formatting_context.Pass();
}

template <>
scoped_ptr<InlineFormattingContext> BlockContainerBox::LayoutChildrenAssuming<
    InlineFormattingContext>(const LayoutOptions& child_layout_options) {
  // Lay out child boxes in the normal flow.
  //   http://www.w3.org/TR/CSS21/visuren.html#normal-flow
  // TODO(***REMOVED***): Handle absolutely positioned boxes:
  //               http://www.w3.org/TR/CSS21/visuren.html#absolute-positioning
  scoped_ptr<InlineFormattingContext> inline_formatting_context(
      new InlineFormattingContext(
          child_layout_options.containing_block_size.width()));
  for (ChildBoxes::iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end();) {
    Box* child_box = *child_box_iterator;
    ++child_box_iterator;
    child_box->Layout(child_layout_options);

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
  return inline_formatting_context.Pass();
}

LayoutOptions BlockContainerBox::GetChildLayoutOptions(
    const LayoutOptions& layout_options, bool* width_depends_on_child_boxes,
    bool* height_depends_on_child_boxes) const {
  bool width_depends_on_containing_block;
  LayoutOptions child_layout_options;
  child_layout_options.containing_block_size.set_width(
      GetChildIndependentUsedWidth(layout_options.containing_block_size.width(),
                                   &width_depends_on_containing_block,
                                   width_depends_on_child_boxes));
  child_layout_options.containing_block_size.set_height(
      GetChildIndependentUsedHeight(
          layout_options.containing_block_size.height(),
          height_depends_on_child_boxes));

  if (layout_options.shrink_if_width_depends_on_containing_block &&
      width_depends_on_containing_block) {
    *width_depends_on_child_boxes = true;
  }

  return child_layout_options;
}

namespace {

class ChildIndependentUsedWidthProvider : public UsedWidthProvider {
 public:
  ChildIndependentUsedWidthProvider(Box::Level box_level,
                                    float containing_block_width);

  void VisitAuto() OVERRIDE;

  bool width_depends_on_child_boxes() const {
    return width_depends_on_child_boxes_;
  }

 private:
  float GetUsedWidthAssumingBlockLevelInFlowBox() const;
  float GetAvailableWidthAssumingShrinkToFit() const;

  const Box::Level box_level_;
  float containing_block_width_;

  bool width_depends_on_child_boxes_;
};

ChildIndependentUsedWidthProvider::ChildIndependentUsedWidthProvider(
    Box::Level box_level, float containing_block_width)
    : box_level_(box_level),
      containing_block_width_(containing_block_width),
      width_depends_on_child_boxes_(false) {}

void ChildIndependentUsedWidthProvider::VisitAuto() {
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
float
ChildIndependentUsedWidthProvider::GetUsedWidthAssumingBlockLevelInFlowBox()
    const {
  // TODO(***REMOVED***): Implement margins, borders, and paddings.
  return containing_block_width_;
}

// The available width is the width of the containing block minus the used
// values of "margin-left", "border-left-width", "padding-left",
// "padding-right", "border-right-width", "margin-right".
//   http://www.w3.org/TR/CSS21/visudet.html#shrink-to-fit-float
float ChildIndependentUsedWidthProvider::GetAvailableWidthAssumingShrinkToFit()
    const {
  // TODO(***REMOVED***): Implement margins, borders, and paddings.
  return containing_block_width_;
}

}  // namespace

float BlockContainerBox::GetChildIndependentUsedWidth(
    float containing_block_width, bool* width_depends_on_containing_block,
    bool* width_depends_on_child_boxes) const {
  ChildIndependentUsedWidthProvider used_width_provider(GetLevel(),
                                                        containing_block_width);
  computed_style()->width()->Accept(&used_width_provider);
  *width_depends_on_containing_block =
      used_width_provider.width_depends_on_containing_block();
  *width_depends_on_child_boxes =
      used_width_provider.width_depends_on_child_boxes();
  return used_width_provider.used_width();
}

namespace {

class ChildIndependentUsedHeightProvider : public UsedHeightProvider {
 public:
  ChildIndependentUsedHeightProvider(Box::Level box_level,
                                     float containing_block_height);

  void VisitAuto() OVERRIDE;

  bool height_depends_on_child_boxes() const {
    return height_depends_on_child_boxes_;
  }

 private:
  float GetUsedHeightAssumingBlockLevelInFlowBox() const;

  const Box::Level box_level_;
  float containing_block_height_;

  bool height_depends_on_child_boxes_;
};

ChildIndependentUsedHeightProvider::ChildIndependentUsedHeightProvider(
    Box::Level box_level, float containing_block_height)
    : box_level_(box_level),
      containing_block_height_(containing_block_height),
      height_depends_on_child_boxes_(false) {}

void ChildIndependentUsedHeightProvider::VisitAuto() {
  // TODO(***REMOVED***): Handle absolutely positioned boxes:
  //               http://www.w3.org/TR/CSS21/visudet.html#abs-non-replaced-height

  // The height depends on children but we don't know their sizes yet.
  set_used_height(0);
  height_depends_on_child_boxes_ = true;
}

}  // namespace

float BlockContainerBox::GetChildIndependentUsedHeight(
    float containing_block_height, bool* height_depends_on_child_boxes) const {
  ChildIndependentUsedHeightProvider used_height_provider(
      GetLevel(), containing_block_height);
  computed_style()->height()->Accept(&used_height_provider);
  *height_depends_on_child_boxes =
      used_height_provider.height_depends_on_child_boxes();
  return used_height_provider.used_height();
}

float BlockContainerBox::GetChildDependentUsedWidth(
    const BlockFormattingContext& block_formatting_context) const {
  return block_formatting_context.shrink_to_fit_width();
}

float BlockContainerBox::GetChildDependentUsedWidth(
    const InlineFormattingContext& inline_formatting_context) const {
  return inline_formatting_context.GetShrinkToFitWidth();
}

float BlockContainerBox::GetChildDependentUsedHeight(
    const BlockFormattingContext& block_formatting_context) const {
  // TODO(***REMOVED***): Implement for block-level non-replaced elements in normal
  //               flow when "overflow" computes to "visible":
  //               http://www.w3.org/TR/CSS21/visudet.html#normal-block
  // TODO(***REMOVED***): Implement for block-level, non-replaced elements in normal
  //               flow when "overflow" does not compute to "visible" and
  //               for inline-block, non-replaced elements:
  //               http://www.w3.org/TR/CSS21/visudet.html#block-root-margin
  NOTIMPLEMENTED();
  return block_formatting_context.last_child_box_used_bottom();  // Hack-hack.
}

float BlockContainerBox::GetChildDependentUsedHeight(
    const InlineFormattingContext& inline_formatting_context) const {
  // TODO(***REMOVED***): Implement for block-level non-replaced elements in normal
  //               flow when "overflow" computes to "visible":
  //               http://www.w3.org/TR/CSS21/visudet.html#normal-block
  // TODO(***REMOVED***): Implement for block-level, non-replaced elements in normal
  //               flow when "overflow" does not compute to "visible" and
  //               for inline-block, non-replaced elements:
  //               http://www.w3.org/TR/CSS21/visudet.html#block-root-margin
  NOTIMPLEMENTED();
  return inline_formatting_context.GetLastLineBoxUsedBottom();  // Hack-hack.
}

BlockLevelBlockContainerBox::BlockLevelBlockContainerBox(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style)
    : BlockContainerBox(computed_style) {}

BlockLevelBlockContainerBox::~BlockLevelBlockContainerBox() {}

Box::Level BlockLevelBlockContainerBox::GetLevel() const { return kBlockLevel; }

InlineLevelBlockContainerBox::InlineLevelBlockContainerBox(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style)
    : BlockContainerBox(computed_style) {}

InlineLevelBlockContainerBox::~InlineLevelBlockContainerBox() {}

Box::Level InlineLevelBlockContainerBox::GetLevel() const {
  return kInlineLevel;
}

}  // namespace layout
}  // namespace cobalt
