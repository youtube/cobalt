// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/layout/flex_container_box.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "cobalt/cssom/computed_style.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/layout/anonymous_block_box.h"
#include "cobalt/layout/text_box.h"
#include "cobalt/layout/used_style.h"

namespace cobalt {
namespace layout {

FlexContainerBox::FlexContainerBox(
    const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
        css_computed_style_declaration,
    BaseDirection base_direction, UsedStyleProvider* used_style_provider,
    LayoutStatTracker* layout_stat_tracker)
    : BlockContainerBox(css_computed_style_declaration, base_direction,
                        used_style_provider, layout_stat_tracker),
      base_direction_(base_direction) {}

FlexContainerBox::~FlexContainerBox() {}

void FlexContainerBox::DetermineAvailableSpace(
    const LayoutParams& layout_params, bool main_direction_is_horizontal,
    bool width_depends_on_containing_block,
    const base::Optional<LayoutUnit>& maybe_width,
    bool height_depends_on_containing_block,
    const base::Optional<LayoutUnit>& maybe_height) {
  // Line Length Determination:
  //   https://www.w3.org/TR/css-flexbox-1/#line-sizing
  // 2. Determine the available main and cross space for the flex items.
  base::Optional<LayoutUnit> main_space;
  base::Optional<LayoutUnit> cross_space;
  bool main_space_depends_on_containing_block;

  base::Optional<LayoutUnit> min_width = GetUsedMinWidthIfNotAuto(
      computed_style(), layout_params.containing_block_size, NULL);
  base::Optional<LayoutUnit> max_width = GetUsedMaxWidthIfNotNone(
      computed_style(), layout_params.containing_block_size, NULL);
  base::Optional<LayoutUnit> min_height = GetUsedMinHeightIfNotAuto(
      computed_style(), layout_params.containing_block_size);
  base::Optional<LayoutUnit> max_height = GetUsedMaxHeightIfNotNone(
      computed_style(), layout_params.containing_block_size);

  // For each dimension, if that dimension of the flex container's content box
  // is a definite size, use that.
  if (main_direction_is_horizontal) {
    bool freeze_main_space =
        layout_params.freeze_width || layout_params.shrink_to_fit_width_forced;
    bool freeze_cross_space = layout_params.freeze_height;
    main_space_depends_on_containing_block =
        width_depends_on_containing_block && (!freeze_main_space);
    main_space = maybe_width;
    cross_space = maybe_height;
    min_main_space_ = min_width;
    max_main_space_ = max_width;
    min_cross_space_ = min_height;
    max_cross_space_ = max_height;

    // If that dimension of the flex container is being sized under a min or
    // max-content constraint, the available space in that dimension is that
    // constraint.
    if (freeze_main_space) {
      main_space = width();
    }
    if (freeze_cross_space) {
      cross_space = height();
    }
  } else {
    bool freeze_main_space = layout_params.freeze_height;
    bool freeze_cross_space =
        layout_params.freeze_height || layout_params.shrink_to_fit_width_forced;
    main_space_depends_on_containing_block =
        height_depends_on_containing_block && (!freeze_main_space);
    main_space = maybe_height;
    cross_space = maybe_width;
    min_main_space_ = min_height;
    max_main_space_ = max_height;
    min_cross_space_ = min_width;
    max_cross_space_ = max_width;

    // If that dimension of the flex container is being sized under a min or
    // max-content constraint, the available space in that dimension is that
    // constraint.
    if (freeze_main_space) {
      main_space = height();
    }
    if (freeze_cross_space) {
      cross_space = width();
    }
  }

  if (GetLevel() == kBlockLevel) {
    if (main_direction_is_horizontal) {
      if (!main_space && main_space_depends_on_containing_block) {
        // Otherwise, subtract the flex container's margin, border, and padding
        // from the space available to the flex container in that dimension and
        // use that value.
        base::Optional<LayoutUnit> margin_main_start =
            GetUsedMarginLeftIfNotAuto(computed_style(),
                                       layout_params.containing_block_size);
        base::Optional<LayoutUnit> margin_main_end =
            GetUsedMarginRightIfNotAuto(computed_style(),
                                        layout_params.containing_block_size);
        main_space = layout_params.containing_block_size.width() -
                     margin_main_start.value_or(LayoutUnit()) -
                     margin_main_end.value_or(LayoutUnit()) -
                     border_left_width() - border_right_width() -
                     padding_left() - padding_right();
      }
    } else {
      if (!cross_space) {
        // Otherwise, subtract the flex container's margin, border, and padding
        // from the space available to the flex container in that dimension and
        // use that value.
        base::Optional<LayoutUnit> margin_cross_start =
            GetUsedMarginLeftIfNotAuto(computed_style(),
                                       layout_params.containing_block_size);
        base::Optional<LayoutUnit> margin_cross_end =
            GetUsedMarginRightIfNotAuto(computed_style(),
                                        layout_params.containing_block_size);
        cross_space = layout_params.containing_block_size.width() -
                      margin_cross_start.value_or(LayoutUnit()) -
                      margin_cross_end.value_or(LayoutUnit()) -
                      border_left_width() - border_right_width() -
                      padding_left() - padding_right();
      }
    }
  }

  main_space_ = main_space;
  cross_space_ = cross_space;
}

// From |Box|.
void FlexContainerBox::UpdateContentSizeAndMargins(
    const LayoutParams& layout_params) {
  // Flex layout works with the flex items in order-modified document order, not
  // their original document order.
  //   https://www.w3.org/TR/css-flexbox-1/#layout-algorithm
  EnsureBoxesAreInOrderModifiedDocumentOrder();

  bool main_direction_is_horizontal = MainDirectionIsHorizontal();

  // Algorithm for Flex Layout:
  //   https://www.w3.org/TR/css-flexbox-1/#layout-algorithm

  // Initial Setup:
  // 1. Generate anonymous flex items.
  // This is performed during box generation.

  // Line Length Determination:
  //   https://www.w3.org/TR/css-flexbox-1/#line-sizing
  // 2. Determine the available main and cross space for the flex items.
  //      https://www.w3.org/TR/css-flexbox-1/#algo-available

  bool width_depends_on_containing_block;
  base::Optional<LayoutUnit> maybe_width = GetUsedWidthIfNotAuto(
      computed_style(), layout_params.containing_block_size,
      &width_depends_on_containing_block);
  bool height_depends_on_containing_block;
  base::Optional<LayoutUnit> maybe_height = GetUsedHeightIfNotAuto(
      computed_style(), layout_params.containing_block_size,
      &height_depends_on_containing_block);

  DetermineAvailableSpace(layout_params, main_direction_is_horizontal,
                          width_depends_on_containing_block, maybe_width,
                          height_depends_on_containing_block, maybe_height);

  // 3. Determine the flex base size and hypothetical main size of each item.
  //      https://www.w3.org/TR/css-flexbox-1/#algo-main-item
  LayoutUnit main_space = main_space_.value_or(LayoutUnit());
  LayoutUnit cross_space = cross_space_.value_or(LayoutUnit());
  SizeLayoutUnit available_space(
      main_direction_is_horizontal ? main_space : cross_space,
      main_direction_is_horizontal ? cross_space : main_space);

  LayoutParams child_layout_params;
  child_layout_params.containing_block_size = available_space;

  FlexFormattingContext flex_formatting_context(
      child_layout_params, main_direction_is_horizontal, DirectionIsReversed());

  std::vector<std::unique_ptr<FlexItem>> items;
  items.reserve(child_boxes().size());

  for (Boxes::const_iterator child_box_iterator = child_boxes().begin();
       child_box_iterator != child_boxes().end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    if (!child_box->IsAbsolutelyPositioned()) {
      flex_formatting_context.UpdateRect(child_box);

      auto item = FlexItem::Create(child_box, main_direction_is_horizontal);
      item->DetermineFlexBaseSize(main_space_,
                                  layout_params.shrink_to_fit_width_forced);
      item->DetermineHypotheticalMainSize(
          child_layout_params.containing_block_size);
      items.emplace_back(std::move(item));
    }
  }

  base::Optional<LayoutUnit> maybe_margin_left = GetUsedMarginLeftIfNotAuto(
      computed_style(), layout_params.containing_block_size);
  base::Optional<LayoutUnit> maybe_margin_right = GetUsedMarginRightIfNotAuto(
      computed_style(), layout_params.containing_block_size);
  base::Optional<LayoutUnit> maybe_left = GetUsedLeftIfNotAuto(
      computed_style(), layout_params.containing_block_size);
  base::Optional<LayoutUnit> maybe_right = GetUsedRightIfNotAuto(
      computed_style(), layout_params.containing_block_size);

  base::Optional<LayoutUnit> maybe_margin_top = GetUsedMarginTopIfNotAuto(
      computed_style(), layout_params.containing_block_size);
  base::Optional<LayoutUnit> maybe_margin_bottom = GetUsedMarginBottomIfNotAuto(
      computed_style(), layout_params.containing_block_size);

  set_margin_left(maybe_margin_left.value_or(LayoutUnit()));
  set_margin_right(maybe_margin_right.value_or(LayoutUnit()));
  set_margin_top(maybe_margin_top.value_or(LayoutUnit()));
  set_margin_bottom(maybe_margin_bottom.value_or(LayoutUnit()));

  if (IsAbsolutelyPositioned()) {
    UpdateWidthAssumingAbsolutelyPositionedBox(
        layout_params.containing_block_direction,
        layout_params.containing_block_size.width(), maybe_left, maybe_right,
        maybe_width, maybe_margin_left, maybe_margin_right, maybe_height);

    base::Optional<LayoutUnit> maybe_top = GetUsedTopIfNotAuto(
        computed_style(), layout_params.containing_block_size);
    base::Optional<LayoutUnit> maybe_bottom = GetUsedBottomIfNotAuto(
        computed_style(), layout_params.containing_block_size);

    UpdateHeightAssumingAbsolutelyPositionedBox(
        layout_params.containing_block_size.height(), maybe_top, maybe_bottom,
        maybe_height, maybe_margin_top, maybe_margin_bottom,
        flex_formatting_context);
  }

  LayoutUnit main_size = LayoutUnit();
  // 4. Determine the main size of the flex container using the rules of the
  // formatting context in which it participates.
  if (!layout_params.freeze_width) {
    UpdateContentWidthAndMargins(layout_params.containing_block_direction,
                                 layout_params.containing_block_size.width(),
                                 layout_params.shrink_to_fit_width_forced,
                                 width_depends_on_containing_block, maybe_left,
                                 maybe_right, maybe_margin_left,
                                 maybe_margin_right, main_space_, cross_space_);
  }
  if (main_direction_is_horizontal) {
    main_size = width();
  } else {
    if (!layout_params.freeze_height) {
      main_size =
          main_space_.value_or(flex_formatting_context.fit_content_main_size());
    } else {
      main_size = height();
    }
  }

  if (max_main_space_ && main_size > *max_main_space_) {
    main_size = *max_main_space_;
  }
  if (min_main_space_ && main_size < *min_main_space_) {
    main_size = *min_main_space_;
  }

  flex_formatting_context.SetContainerMainSize(main_size);

  // Main Size Determination:
  // 5. Collect flex items into flex lines.
  flex_formatting_context.set_multi_line(ContainerIsMultiLine());
  for (auto& item : items) {
    DCHECK(!item->box()->IsAbsolutelyPositioned());
    flex_formatting_context.CollectItemIntoLine(main_size, std::move(item));
  }

  if (main_direction_is_horizontal) {
    set_width(main_size);
  } else {
    if (!main_space_) {
      // For vertical containers with indefinite main space, us the fit content
      // main size. Note: For horizontal containers, this sizing is already
      // handled by UpdateContentWidthAndMargins().
      main_size = flex_formatting_context.fit_content_main_size();
    }
    set_height(main_size);
  }

  // Perform remaining steps of the layout of the items.
  flex_formatting_context.ResolveFlexibleLengthsAndCrossSizes(
      cross_space_, min_cross_space_, max_cross_space_,
      computed_style()->align_content());

  if (main_direction_is_horizontal) {
    set_height(flex_formatting_context.cross_size());
  } else {
    set_width(flex_formatting_context.cross_size());
  }

  UpdateRectOfPositionedChildBoxes(child_layout_params, layout_params);

  if (items.empty()) {
    baseline_ = GetPaddingBoxHeight() + border_bottom_width() + margin_bottom();
  } else {
    baseline_ = flex_formatting_context.GetBaseline();
  }
}

WrapResult FlexContainerBox::TryWrapAt(
    WrapAtPolicy wrap_at_policy, WrapOpportunityPolicy wrap_opportunity_policy,
    bool is_line_existence_justified, LayoutUnit available_width,
    bool should_collapse_trailing_white_space) {
  DCHECK(!IsAbsolutelyPositioned());
  // Wrapping is not allowed until the line's existence is justified, meaning
  // that wrapping cannot occur before the box. Given that this box cannot be
  // split, no wrappable point is available.
  if (!is_line_existence_justified) {
    return kWrapResultNoWrap;
  }

  return (GetLevel() == kInlineLevel) ? kWrapResultWrapBefore
                                      : kWrapResultNoWrap;
}

bool FlexContainerBox::TrySplitAtSecondBidiLevelRun() { return false; }

void FlexContainerBox::SetShouldCollapseLeadingWhiteSpace(
    bool should_collapse_leading_white_space) {
  DCHECK_EQ(kInlineLevel, GetLevel());
}

void FlexContainerBox::SetShouldCollapseTrailingWhiteSpace(
    bool should_collapse_trailing_white_space) {
  DCHECK_EQ(kInlineLevel, GetLevel());
}

bool FlexContainerBox::HasLeadingWhiteSpace() const {
  DCHECK_EQ(kInlineLevel, GetLevel());
  return false;
}

bool FlexContainerBox::HasTrailingWhiteSpace() const {
  DCHECK_EQ(kInlineLevel, GetLevel());
  return false;
}

bool FlexContainerBox::IsCollapsed() const {
  DCHECK_EQ(kInlineLevel, GetLevel());
  return false;
}

bool FlexContainerBox::JustifiesLineExistence() const {
  DCHECK_EQ(kInlineLevel, GetLevel());
  return true;
}

bool FlexContainerBox::AffectsBaselineInBlockFormattingContext() const {
  return true;
}

LayoutUnit FlexContainerBox::GetBaselineOffsetFromTopMarginEdge() const {
  return GetContentBoxOffsetFromMarginBox().y() + baseline_;
}

// From |ContainerBox|.
scoped_refptr<ContainerBox> FlexContainerBox::TrySplitAtEnd() {
  return scoped_refptr<ContainerBox>();
}

bool FlexContainerBox::TryAddChild(const scoped_refptr<Box>& child_box) {
  AddChild(child_box);
  return true;
}

void FlexContainerBox::AddChild(const scoped_refptr<Box>& child_box) {
  TextBox* text_box = child_box->AsTextBox();
  switch (child_box->GetLevel()) {
    case kBlockLevel:
      PushBackDirectChild(child_box);
      break;
    case kInlineLevel:
      if (text_box && !text_box->HasNonCollapsibleText()) {
        // Text boxes with only white space are not rendered, just as if
        // its text nodes were 'display:none'.
        //   https://www.w3.org/TR/css-flexbox-1/#flex-items
        return;
      }
      // An inline formatting context required,
      // add a child to an anonymous block box.
      GetOrAddAnonymousBlockBox()->AddInlineLevelChild(child_box);
      break;
  }
}

// From |Box|.
bool FlexContainerBox::IsTransformable() const { return true; }

bool FlexContainerBox::MainDirectionIsHorizontal() const {
  auto flex_direction = computed_style()->flex_direction();
  bool reverse_direction =
      flex_direction == cssom::KeywordValue::GetRowReverse() ||
      flex_direction == cssom::KeywordValue::GetColumnReverse();
  bool main_direction_is_horizontal =
      flex_direction == cssom::KeywordValue::GetRow() ||
      flex_direction == cssom::KeywordValue::GetRowReverse();

  // Ensure that all values for flex-direction are handled.
  DCHECK(main_direction_is_horizontal ^
         (flex_direction == cssom::KeywordValue::GetColumn() ||
          flex_direction == cssom::KeywordValue::GetColumnReverse()));

  DCHECK(reverse_direction ^
         (flex_direction == cssom::KeywordValue::GetRow() ||
          flex_direction == cssom::KeywordValue::GetColumn()));

  return main_direction_is_horizontal;
}

bool FlexContainerBox::DirectionIsReversed() const {
  auto flex_direction = computed_style()->flex_direction();
  bool reverse_direction =
      flex_direction == cssom::KeywordValue::GetRowReverse() ||
      flex_direction == cssom::KeywordValue::GetColumnReverse();

  // Ensure that all values for flex-direction are handled.
  DCHECK(reverse_direction ^
         (flex_direction == cssom::KeywordValue::GetRow() ||
          flex_direction == cssom::KeywordValue::GetColumn()));

  return reverse_direction;
}

bool FlexContainerBox::ContainerIsMultiLine() const {
  auto flex_wrap = computed_style()->flex_wrap();
  bool container_is_multiline =
      flex_wrap == cssom::KeywordValue::GetWrap() ||
      flex_wrap == cssom::KeywordValue::GetWrapReverse();

  // Ensure that all values for flex-wrap are handled.
  DCHECK(container_is_multiline ^
         (flex_wrap == cssom::KeywordValue::GetNowrap()));

  return container_is_multiline;
}

namespace {
// Return true if a is ordered before b.
bool CompareBoxOrder(const scoped_refptr<const Box>& a,
                     const scoped_refptr<const Box>& b) {
  int order_a = a->IsAbsolutelyPositioned() ? 0 : a->GetOrder();
  int order_b = b->IsAbsolutelyPositioned() ? 0 : b->GetOrder();
  return order_a < order_b;
}
}  // namespace

void FlexContainerBox::EnsureBoxesAreInOrderModifiedDocumentOrder() {
  // A flex container lays out its content in order-modified document order.
  // Items with the same ordinal group are laid out in the order they appear in
  // the source document. Absolutely-positioned children of a flex container
  // are treated as having 'order: 0'.
  //   https://www.w3.org/TR/css-flexbox-1/#order-modified-document-order

  // Note std::sort and std::qsort have undefined order of equal elements, so
  // they can only be used when being careful to not compare only by 'order'
  // value. However, since std::stable_sort does preserve the order of
  // equivalent elements, that can be used directly.
  std::stable_sort(child_boxes_.begin(), child_boxes_.end(), CompareBoxOrder);
}

AnonymousBlockBox* FlexContainerBox::GetLastChildAsAnonymousBlockBox() {
  return NULL;
}

AnonymousBlockBox* FlexContainerBox::GetOrAddAnonymousBlockBox() {
  AnonymousBlockBox* anonymous_block_box = GetLastChildAsAnonymousBlockBox();
  // If either the last box is not an anonymous block box, or the anonymous
  // block box already has a trailing line break and can't accept any additional
  // children, then create a new anonymous block box.
  if (anonymous_block_box == NULL ||
      anonymous_block_box->HasTrailingLineBreak()) {
    // TODO: Determine which animations to propagate to the anonymous block box,
    //       instead of none at all.
    scoped_refptr<cssom::CSSComputedStyleDeclaration>
        new_computed_style_declaration =
            new cssom::CSSComputedStyleDeclaration();
    new_computed_style_declaration->SetData(
        GetComputedStyleOfAnonymousBox(css_computed_style_declaration()));
    new_computed_style_declaration->set_animations(
        new web_animations::AnimationSet());
    scoped_refptr<AnonymousBlockBox> new_anonymous_block_box(
        new AnonymousBlockBox(new_computed_style_declaration, base_direction(),
                              used_style_provider(), layout_stat_tracker()));
    anonymous_block_box = new_anonymous_block_box.get();
    PushBackDirectChild(new_anonymous_block_box);
  }
  return anonymous_block_box;
}

std::unique_ptr<FormattingContext>
FlexContainerBox::UpdateRectOfInFlowChildBoxes(
    const LayoutParams& child_layout_params) {
  std::unique_ptr<FlexFormattingContext> flex_formatting_context(
      new FlexFormattingContext(child_layout_params,
                                MainDirectionIsHorizontal(),
                                DirectionIsReversed()));
  for (Boxes::const_iterator child_box_iterator = child_boxes().begin();
       child_box_iterator != child_boxes().end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    if (!child_box->IsAbsolutelyPositioned()) {
      flex_formatting_context->UpdateRect(child_box);
    }
  }
  return std::unique_ptr<FormattingContext>(flex_formatting_context.release());
}

BlockLevelFlexContainerBox::BlockLevelFlexContainerBox(
    const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
        css_computed_style_declaration,
    BaseDirection base_direction, UsedStyleProvider* used_style_provider,
    LayoutStatTracker* layout_stat_tracker)
    : FlexContainerBox(css_computed_style_declaration, base_direction,
                       used_style_provider, layout_stat_tracker) {}

BlockLevelFlexContainerBox::~BlockLevelFlexContainerBox() {}

Box::Level BlockLevelFlexContainerBox::GetLevel() const { return kBlockLevel; }

base::Optional<int> BlockLevelFlexContainerBox::GetBidiLevel() const {
  return base::Optional<int>();
}

#ifdef COBALT_BOX_DUMP_ENABLED
void BlockLevelFlexContainerBox::DumpClassName(std::ostream* stream) const {
  *stream << "BlockLevelFlexContainerBox ";
}
#endif  // COBALT_BOX_DUMP_ENABLED

InlineLevelFlexContainerBox::InlineLevelFlexContainerBox(
    const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
        css_computed_style_declaration,
    BaseDirection base_direction, const scoped_refptr<Paragraph>& paragraph,
    int32 text_position, UsedStyleProvider* used_style_provider,
    LayoutStatTracker* layout_stat_tracker)
    : FlexContainerBox(css_computed_style_declaration, base_direction,
                       used_style_provider, layout_stat_tracker),
      paragraph_(paragraph),
      text_position_(text_position) {}

InlineLevelFlexContainerBox::~InlineLevelFlexContainerBox() {}

Box::Level InlineLevelFlexContainerBox::GetLevel() const {
  return kInlineLevel;
}

base::Optional<int> InlineLevelFlexContainerBox::GetBidiLevel() const {
  return paragraph_->GetBidiLevel(text_position_);
}

#ifdef COBALT_BOX_DUMP_ENABLED
void InlineLevelFlexContainerBox::DumpClassName(std::ostream* stream) const {
  *stream << "InlineLevelFlexContainerBox ";
}
#endif  // COBALT_BOX_DUMP_ENABLED

}  // namespace layout
}  // namespace cobalt
