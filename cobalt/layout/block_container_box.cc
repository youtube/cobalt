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

#include "cobalt/layout/formatting_context.h"
#include "cobalt/layout/used_style.h"

namespace cobalt {
namespace layout {

BlockContainerBox::BlockContainerBox(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const cssom::TransitionSet* transitions)
    : ContainerBox(computed_style, transitions) {}

BlockContainerBox::~BlockContainerBox() {}

void BlockContainerBox::Layout(const LayoutParams& layout_params) {
  // Approximate the size of the containing block, so that relatively and
  // statically positioned child boxes can use it during their layout. The size
  // is calculated with the assumption that there are no children.
  //   http://www.w3.org/TR/CSS21/visudet.html#containing-block-details
  bool width_depends_on_child_boxes;
  bool height_depends_on_child_boxes;
  LayoutParams child_layout_params =
      GetChildLayoutOptions(layout_params, &width_depends_on_child_boxes,
                            &height_depends_on_child_boxes);

  // Do a first pass of the layout using the approximate size.
  child_layout_params.shrink_if_width_depends_on_containing_block =
      width_depends_on_child_boxes;
  scoped_ptr<FormattingContext> formatting_context =
      LayoutChildren(child_layout_params);

  // Calculate the exact width of the block container box.
  if (width_depends_on_child_boxes) {
    child_layout_params.containing_block_size.set_width(
        GetChildDependentUsedWidth(*formatting_context));

    // Do a second pass of the layout using the exact size.
    // TODO(***REMOVED***): Laying out the children twice has an exponential worst-case
    //               complexity (because every child could lay out itself twice
    //               as well). Figure out if there is a better way.
    child_layout_params.shrink_if_width_depends_on_containing_block = false;
    formatting_context = LayoutChildren(child_layout_params);
  }
  used_frame().set_width(child_layout_params.containing_block_size.width());

  // Calculate the exact height of the block container box.
  used_frame().set_height(
      height_depends_on_child_boxes
          ? GetChildDependentUsedHeight(*formatting_context)
          : child_layout_params.containing_block_size.height());
  maybe_height_above_baseline_ =
      formatting_context->maybe_height_above_baseline();
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
  return static_cast<bool>(maybe_height_above_baseline_);
}

float BlockContainerBox::GetHeightAboveBaseline() const {
  // If the box does not have a baseline, align the bottom margin edge
  // with the parent's baseline.
  //   http://www.w3.org/TR/CSS21/visudet.html#line-height
  // TODO(***REMOVED***): Fix when margins are implemented.
  return maybe_height_above_baseline_.value_or(used_frame().height());
}

scoped_ptr<ContainerBox> BlockContainerBox::TrySplitAtEnd() {
  return scoped_ptr<ContainerBox>();
}

void BlockContainerBox::AddContentToRenderTree(
    render_tree::CompositionNode::Builder* composition_node_builder,
    render_tree::animations::NodeAnimationsMap::Builder*
        node_animations_map_builder) const {
  // Render child boxes.
  // TODO(***REMOVED***): Take a stacking context into account:
  //               http://www.w3.org/TR/CSS21/visuren.html#z-index
  for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    child_box->AddToRenderTree(composition_node_builder,
                               node_animations_map_builder);
  }
}

bool BlockContainerBox::IsTransformable() const { return true; }

void BlockContainerBox::DumpProperties(std::ostream* stream) const {
  ContainerBox::DumpProperties(stream);

  *stream << std::boolalpha << "affects_baseline_in_block_formatting_context="
          << AffectsBaselineInBlockFormattingContext() << " "
          << std::noboolalpha;
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

LayoutParams BlockContainerBox::GetChildLayoutOptions(
    const LayoutParams& layout_params, bool* width_depends_on_child_boxes,
    bool* height_depends_on_child_boxes) const {
  bool width_depends_on_containing_block;
  LayoutParams child_layout_params;
  child_layout_params.containing_block_size.set_width(
      GetChildIndependentUsedWidth(layout_params.containing_block_size.width(),
                                   &width_depends_on_containing_block,
                                   width_depends_on_child_boxes));
  child_layout_params.containing_block_size.set_height(
      GetChildIndependentUsedHeight(
          layout_params.containing_block_size.height(),
          height_depends_on_child_boxes));

  if (layout_params.shrink_if_width_depends_on_containing_block &&
      width_depends_on_containing_block) {
    *width_depends_on_child_boxes = true;
  }

  return child_layout_params;
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

}  // namespace layout
}  // namespace cobalt
