// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/flex/flex_layout_algorithm.h"

#include <memory>
#include <optional>

#include "base/not_fatal_until.h"
#include "base/types/optional_util.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/baseline_utils.h"
#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/disable_layout_side_effects_scope.h"
#include "third_party/blink/renderer/core/layout/flex/devtools_flex_info.h"
#include "third_party/blink/renderer/core/layout/flex/flex_child_iterator.h"
#include "third_party/blink/renderer/core/layout/flex/flex_item_iterator.h"
#include "third_party/blink/renderer/core/layout/flex/flex_line.h"
#include "third_party/blink/renderer/core/layout/flex/flex_line_breaker.h"
#include "third_party/blink/renderer/core/layout/flex/layout_flexible_box.h"
#include "third_party/blink/renderer/core/layout/flex/line_flexer.h"
#include "third_party/blink/renderer/core/layout/geometry/box_strut.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_input_node.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/logical_fragment.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/space_utils.h"
#include "third_party/blink/renderer/core/layout/table/table_node.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/heap/collection_support/clear_collection_scope.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/text/writing_mode_utils.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

template <typename Value>
class PhysicalToFlex {
  STACK_ALLOCATED();

 public:
  PhysicalToFlex(WritingDirectionMode writing_direction,
                 bool is_column,
                 Value top,
                 Value right,
                 Value bottom,
                 Value left)
      : logical_(writing_direction, top, right, bottom, left),
        is_column_(is_column) {}

  Value MainStart() const {
    return is_column_ ? logical_.BlockStart() : logical_.InlineStart();
  }
  Value MainEnd() const {
    return is_column_ ? logical_.BlockEnd() : logical_.InlineEnd();
  }
  Value CrossStart() const {
    return is_column_ ? logical_.InlineStart() : logical_.BlockStart();
  }
  Value CrossEnd() const {
    return is_column_ ? logical_.InlineEnd() : logical_.BlockEnd();
  }

 private:
  PhysicalToLogical<Value> logical_;
  bool is_column_;
};

class BaselineAccumulator {
  STACK_ALLOCATED();

 public:
  explicit BaselineAccumulator(const ComputedStyle& style)
      : font_baseline_(style.GetFontBaseline()) {}

  void AccumulateItem(const LogicalBoxFragment& fragment,
                      const LayoutUnit block_offset,
                      bool is_first_line,
                      bool is_last_line) {
    if (is_first_line) {
      if (!first_fallback_baseline_) {
        first_fallback_baseline_ =
            block_offset + fragment.FirstBaselineOrSynthesize(font_baseline_);
      }
    }

    if (is_last_line) {
      last_fallback_baseline_ =
          block_offset + fragment.LastBaselineOrSynthesize(font_baseline_);
    }
  }

  void AccumulateLine(const FlexLine& line,
                      bool is_first_line,
                      bool is_last_line) {
    if (is_first_line) {
      if (line.major_baseline != LayoutUnit::Min()) {
        first_major_baseline_ = line.cross_axis_offset + line.major_baseline;
      }
      if (line.minor_baseline != LayoutUnit::Min()) {
        first_minor_baseline_ =
            line.cross_axis_offset + line.line_cross_size - line.minor_baseline;
      }
    }

    if (is_last_line) {
      if (line.major_baseline != LayoutUnit::Min()) {
        last_major_baseline_ = line.cross_axis_offset + line.major_baseline;
      }
      if (line.minor_baseline != LayoutUnit::Min()) {
        last_minor_baseline_ =
            line.cross_axis_offset + line.line_cross_size - line.minor_baseline;
      }
    }
  }

  std::optional<LayoutUnit> FirstBaseline() const {
    if (first_major_baseline_)
      return *first_major_baseline_;
    if (first_minor_baseline_)
      return *first_minor_baseline_;
    return first_fallback_baseline_;
  }
  std::optional<LayoutUnit> LastBaseline() const {
    if (last_minor_baseline_)
      return *last_minor_baseline_;
    if (last_major_baseline_)
      return *last_major_baseline_;
    return last_fallback_baseline_;
  }

 private:
  FontBaseline font_baseline_;

  std::optional<LayoutUnit> first_major_baseline_;
  std::optional<LayoutUnit> first_minor_baseline_;
  std::optional<LayoutUnit> first_fallback_baseline_;

  std::optional<LayoutUnit> last_major_baseline_;
  std::optional<LayoutUnit> last_minor_baseline_;
  std::optional<LayoutUnit> last_fallback_baseline_;
};

LayoutUnit RowGap(const ComputedStyle& style,
                  LogicalSize percentage_resolution_size) {
  if (const std::optional<Length>& row_gap = style.RowGap()) {
    return MinimumValueForLength(
        *row_gap,
        percentage_resolution_size.block_size.ClampIndefiniteToZero());
  }
  return LayoutUnit();
}

LayoutUnit ColumnGap(const ComputedStyle& style,
                     LogicalSize percentage_resolution_size) {
  if (const std::optional<Length>& column_gap = style.ColumnGap()) {
    return MinimumValueForLength(
        *column_gap,
        percentage_resolution_size.inline_size.ClampIndefiniteToZero());
  }
  return LayoutUnit();
}

}  // anonymous namespace

FlexLayoutAlgorithm::FlexLayoutAlgorithm(
    const LayoutAlgorithmParams& params,
    const HashMap<wtf_size_t, LayoutUnit>* cross_size_adjustments)
    : LayoutAlgorithm(params),
      is_webkit_box_(Style().IsDeprecatedFlexbox()),
      is_column_(Style().ResolvedIsColumnFlexDirection()),
      is_wrap_reverse_(Style().FlexWrap() == EFlexWrap::kWrapReverse),
      is_reverse_direction_(Style().ResolvedIsReverseFlexDirection()),
      is_multi_line_(Style().FlexWrap() != EFlexWrap::kNowrap),
      is_horizontal_flow_(Style().IsHorizontalWritingMode() ? !is_column_
                                                            : is_column_),
      is_cross_size_definite_(IsContainerCrossSizeDefinite()),
      child_percentage_size_(
          CalculateChildPercentageSize(GetConstraintSpace(),
                                       Node(),
                                       ChildAvailableSize())),
      gap_between_items_(is_column_
                             ? RowGap(Style(), child_percentage_size_)
                             : ColumnGap(Style(), child_percentage_size_)),
      gap_between_lines_(is_column_ ? ColumnGap(Style(), child_percentage_size_)
                                    : RowGap(Style(), child_percentage_size_)),
      cross_size_adjustments_(cross_size_adjustments) {
  // TODO(layout-dev): Devtools support when there are multiple fragments.
  if (Node().GetLayoutBox()->NeedsDevtoolsInfo() &&
      !InvolvedInBlockFragmentation(container_builder_))
    layout_info_for_devtools_ = std::make_unique<DevtoolsFlexInfo>();
}

void FlexLayoutAlgorithm::SetupRelayoutData(const FlexLayoutAlgorithm& previous,
                                            RelayoutType relayout_type) {
  LayoutAlgorithm::SetupRelayoutData(previous, relayout_type);

  if (relayout_type == kRelayoutIgnoringChildScrollbarChanges) {
    ignore_child_scrollbar_changes_ = true;
  } else {
    ignore_child_scrollbar_changes_ = previous.ignore_child_scrollbar_changes_;
  }
}

StyleContentAlignmentData FlexLayoutAlgorithm::ResolvedJustifyContent() const {
  if (is_webkit_box_) {
    const EBoxPack box_pack = Style().BoxPack();
    const ContentPosition position = ([&]() {
      // -webkit-box row-reverse currently flips the start/end (e.g. it always
      // uses "start" rather than "flex-start"). Firefox doesn't have this
      // quirk, we should attempt to remove it.
      const bool is_row_reverse =
          !RuntimeEnabledFeatures::LayoutDisableWebkitBoxQuirksEnabled() &&
          Style().ResolvedIsRowReverseFlexDirection();
      switch (box_pack) {
        case EBoxPack::kCenter:
          return ContentPosition::kCenter;
        case EBoxPack::kJustify:
        case EBoxPack::kStart:
          return is_row_reverse ? ContentPosition::kFlexEnd
                                : ContentPosition::kFlexStart;
        case EBoxPack::kEnd:
          return is_row_reverse ? ContentPosition::kFlexStart
                                : ContentPosition::kFlexEnd;
      }
    })();
    const ContentDistributionType distribution =
        box_pack == EBoxPack::kJustify ? ContentDistributionType::kSpaceBetween
                                       : ContentDistributionType::kDefault;
    return StyleContentAlignmentData(
        position, distribution,
        RuntimeEnabledFeatures::LayoutDisableWebkitBoxQuirksEnabled()
            ? OverflowAlignment::kDefault
            : OverflowAlignment::kSafe);
  }

  const auto writing_direction = GetConstraintSpace().GetWritingDirection();
  const StyleContentAlignmentData& justify_content = Style().JustifyContent();

  // Coerce "left"/"right" their logical variants.
  ContentPosition position = justify_content.GetPosition();
  if (position == ContentPosition::kLeft ||
      position == ContentPosition::kRight) {
    if (is_column_) {
      if (writing_direction.IsHorizontal()) {
        // The main-axis is in the top-down direction, fallback to start.
        position = ContentPosition::kStart;
      } else {
        LogicalToPhysical physical(
            writing_direction, ContentPosition::kStart, ContentPosition::kEnd,
            ContentPosition::kStart, ContentPosition::kEnd);
        position = position == ContentPosition::kLeft ? physical.Left()
                                                      : physical.Right();
      }
    } else {
      position =
          ((position == ContentPosition::kLeft) == writing_direction.IsLtr())
              ? ContentPosition::kStart
              : ContentPosition::kEnd;
    }
  }

  return StyleContentAlignmentData(position, justify_content.Distribution(),
                                   justify_content.Overflow());
}

ItemPosition FlexLayoutAlgorithm::ResolvedAlignSelf(
    const ComputedStyle& child_style,
    bool is_out_of_flow) const {
  // Any auto-margins coerce the alignment to flex-start.
  if (!is_out_of_flow) {
    if (is_horizontal_flow_) {
      if (child_style.MarginTop().IsAuto() ||
          child_style.MarginBottom().IsAuto()) {
        return ItemPosition::kFlexStart;
      }
    } else {
      if (child_style.MarginLeft().IsAuto() ||
          child_style.MarginRight().IsAuto()) {
        return ItemPosition::kFlexStart;
      }
    }
  }

  // -webkit-box has a relatively simple alignment mapping (no need to coerce
  // "self-start", etc).
  if (is_webkit_box_) {
    switch (Style().BoxAlign()) {
      case EBoxAlignment::kBaseline:
        return ItemPosition::kBaseline;
      case EBoxAlignment::kCenter:
        return ItemPosition::kCenter;
      case EBoxAlignment::kStretch:
        return ItemPosition::kStretch;
      case EBoxAlignment::kStart:
        return ItemPosition::kFlexStart;
      case EBoxAlignment::kEnd:
        return ItemPosition::kFlexEnd;
    }
  }

  ItemPosition align =
      child_style
          .ResolvedAlignSelf(
              {ItemPosition::kStretch, OverflowAlignment::kDefault}, &Style())
          .GetPosition();
  DCHECK_NE(align, ItemPosition::kAuto);
  DCHECK_NE(align, ItemPosition::kNormal);
  DCHECK_NE(align, ItemPosition::kLeft) << "left, right are only for justify";
  DCHECK_NE(align, ItemPosition::kRight) << "left, right are only for justify";

  if (align == ItemPosition::kStart) {
    return ItemPosition::kFlexStart;
  }
  if (align == ItemPosition::kEnd) {
    return ItemPosition::kFlexEnd;
  }

  LogicalToLogical<ItemPosition> logical(
      child_style.GetWritingDirection(),
      GetConstraintSpace().GetWritingDirection(), ItemPosition::kFlexStart,
      ItemPosition::kFlexEnd, ItemPosition::kFlexStart, ItemPosition::kFlexEnd);
  if (align == ItemPosition::kSelfStart) {
    return is_column_ ? logical.InlineStart() : logical.BlockStart();
  }
  if (align == ItemPosition::kSelfEnd) {
    return is_column_ ? logical.InlineEnd() : logical.BlockEnd();
  }

  if (is_wrap_reverse_) {
    if (align == ItemPosition::kFlexStart) {
      align = ItemPosition::kFlexEnd;
    } else if (align == ItemPosition::kFlexEnd) {
      align = ItemPosition::kFlexStart;
    }
  }

  return align;
}

LayoutUnit FlexLayoutAlgorithm::MainAxisContentExtent(
    LayoutUnit sum_hypothetical_main_size) const {
  if (is_column_) {
    // Even though we only pass border_padding in the third parameter, the
    // return value includes scrollbar, so subtract scrollbar to get content
    // size.
    // We add |border_scrollbar_padding| to the fourth parameter because
    // |content_size| needs to be the size of the border box. We've overloaded
    // the term "content".
    const LayoutUnit border_scrollbar_padding =
        BorderScrollbarPadding().BlockSum();
    return ComputeBlockSizeForFragment(
               GetConstraintSpace(), Node(), BorderPadding(),
               sum_hypothetical_main_size.ClampNegativeToZero() +
                   border_scrollbar_padding,
               container_builder_.InlineSize()) -
           border_scrollbar_padding;
  }
  return ChildAvailableSize().inline_size;
}

LayoutUnit FlexLayoutAlgorithm::BaselineAscent(
    const FlexItem& item,
    const PhysicalBoxFragment& fragment) const {
  LogicalBoxFragment baseline_fragment(item.baseline_writing_direction,
                                       fragment);

  const bool is_last_baseline = item.alignment == ItemPosition::kLastBaseline;
  const auto font_baseline = Style().GetFontBaseline();
  LayoutUnit baseline =
      is_last_baseline
          ? baseline_fragment.LastBaselineOrSynthesize(font_baseline)
          : baseline_fragment.FirstBaselineOrSynthesize(font_baseline);
  if (is_wrap_reverse_ != is_last_baseline) {
    baseline = baseline_fragment.BlockSize() - baseline;
  }

  const PhysicalToFlex margins(
      GetConstraintSpace().GetWritingDirection(), is_column_,
      item.initial_margins.top, item.initial_margins.right,
      item.initial_margins.bottom, item.initial_margins.left);
  return item.baseline_group == BaselineGroup::kMajor
             ? margins.CrossStart() + baseline
             : margins.CrossEnd() + baseline;
}

bool FlexLayoutAlgorithm::ShouldApplyAutoMinSize(const BlockNode& child) const {
  // webkit-box treats min-size: auto as 0.
  if (is_webkit_box_) {
    return false;
  }
  if (child.ShouldApplySizeContainment()) {
    return false;
  }
  // Note that the spec uses "scroll container", but it's resolved to just look
  // at the computed value of overflow not being scrollable, see:
  // https://github.com/w3c/csswg-drafts/issues/7714#issuecomment-1879319762
  const auto& child_style = child.Style();
  if (child_style.IsScrollContainer()) {
    return false;
  }
  const Length& min =
      is_horizontal_flow_ ? child_style.MinWidth() : child_style.MinHeight();
  return min.HasAuto();
}

namespace {

enum AxisEdge { kStart, kCenter, kEnd };

// Maps the resolved justify-content value to a static-position edge.
AxisEdge MainAxisStaticPositionEdge(
    const StyleContentAlignmentData& justify_content,
    bool is_reverse_direction) {
  const ContentPosition content_position = justify_content.GetPosition();
  DCHECK_NE(content_position, ContentPosition::kLeft);
  DCHECK_NE(content_position, ContentPosition::kRight);
  if (content_position == ContentPosition::kFlexEnd)
    return is_reverse_direction ? AxisEdge::kStart : AxisEdge::kEnd;

  if (content_position == ContentPosition::kCenter ||
      justify_content.Distribution() == ContentDistributionType::kSpaceAround ||
      justify_content.Distribution() == ContentDistributionType::kSpaceEvenly) {
    return AxisEdge::kCenter;
  }

  if (content_position == ContentPosition::kStart)
    return AxisEdge::kStart;
  if (content_position == ContentPosition::kEnd)
    return AxisEdge::kEnd;

  return is_reverse_direction ? AxisEdge::kEnd : AxisEdge::kStart;
}

// Maps the resolved alignment value to a static-position edge.
AxisEdge CrossAxisStaticPositionEdge(const ItemPosition alignment,
                                     bool is_wrap_reverse) {
  // AlignmentForChild already accounted for wrap-reverse for kFlexStart and
  // kFlexEnd, but not kStretch. kStretch is supposed to act like kFlexStart.
  if (is_wrap_reverse && alignment == ItemPosition::kStretch) {
    return AxisEdge::kEnd;
  }

  if (alignment == ItemPosition::kFlexEnd ||
      alignment == ItemPosition::kLastBaseline)
    return AxisEdge::kEnd;

  if (alignment == ItemPosition::kCenter)
    return AxisEdge::kCenter;

  return AxisEdge::kStart;
}

}  // namespace

void FlexLayoutAlgorithm::HandleOutOfFlowPositionedItems(
    LayoutUnit total_intrinsic_block_size,
    HeapVector<Member<LayoutBox>>& oof_children) {
  if (oof_children.empty())
    return;

  HeapVector<Member<LayoutBox>> oofs;
  std::swap(oofs, oof_children);

  bool should_process_block_end = true;
  bool should_process_block_center = true;
  const LayoutUnit previous_consumed_block_size =
      GetBreakToken() ? GetBreakToken()->ConsumedBlockSize() : LayoutUnit();

  // We will attempt to add OOFs in the fragment in which their static
  // position belongs. However, the last fragment has the most up-to-date flex
  // size information (e.g. any expanded rows, etc), so for center aligned
  // items, we could end up with an incorrect static position.
  if (InvolvedInBlockFragmentation(container_builder_)) [[unlikely]] {
    should_process_block_end = !container_builder_.DidBreakSelf() &&
                               !container_builder_.ShouldBreakInside();
    if (should_process_block_end) {
      // Recompute the total block size in case |total_intrinsic_block_size|
      // changed as a result of fragmentation.
      total_block_size_ = ComputeBlockSizeForFragment(
          GetConstraintSpace(), Node(), BorderPadding(),
          total_intrinsic_block_size, container_builder_.InlineSize());
    } else {
      LayoutUnit center = total_block_size_ / 2;
      should_process_block_center = center - previous_consumed_block_size <=
                                    FragmentainerCapacityForChildren();
    }
  }

  using InlineEdge = LogicalStaticPosition::InlineEdge;
  using BlockEdge = LogicalStaticPosition::BlockEdge;

  BoxStrut border_scrollbar_padding = BorderScrollbarPadding();
  border_scrollbar_padding.block_start =
      OriginalBorderScrollbarPaddingBlockStart();

  LogicalSize total_fragment_size = {container_builder_.InlineSize(),
                                     total_block_size_};
  total_fragment_size =
      ShrinkLogicalSize(total_fragment_size, border_scrollbar_padding);

  const StyleContentAlignmentData justify_content = ResolvedJustifyContent();
  const AxisEdge main_axis_edge =
      MainAxisStaticPositionEdge(justify_content, is_reverse_direction_);

  for (LayoutBox* oof_child : oofs) {
    BlockNode child(oof_child);

    const ItemPosition position =
        ResolvedAlignSelf(child.Style(), /* is_out_of_flow */ true);
    AxisEdge cross_axis_edge =
        CrossAxisStaticPositionEdge(position, is_wrap_reverse_);

    AxisEdge inline_axis_edge = is_column_ ? cross_axis_edge : main_axis_edge;
    AxisEdge block_axis_edge = is_column_ ? main_axis_edge : cross_axis_edge;

    InlineEdge inline_edge;
    BlockEdge block_edge;
    LogicalOffset offset = border_scrollbar_padding.StartOffset();

    // Determine the static-position based off the axis-edge.
    if (block_axis_edge == AxisEdge::kStart) {
      DCHECK(!IsBreakInside(GetBreakToken()));
      block_edge = BlockEdge::kBlockStart;
    } else if (block_axis_edge == AxisEdge::kCenter) {
      if (!should_process_block_center) {
        oof_children.emplace_back(oof_child);
        continue;
      }
      block_edge = BlockEdge::kBlockCenter;
      offset.block_offset += total_fragment_size.block_size / 2;
    } else {
      if (!should_process_block_end) {
        oof_children.emplace_back(oof_child);
        continue;
      }
      block_edge = BlockEdge::kBlockEnd;
      offset.block_offset += total_fragment_size.block_size;
    }

    if (inline_axis_edge == AxisEdge::kStart) {
      inline_edge = InlineEdge::kInlineStart;
    } else if (inline_axis_edge == AxisEdge::kCenter) {
      inline_edge = InlineEdge::kInlineCenter;
      offset.inline_offset += total_fragment_size.inline_size / 2;
    } else {
      inline_edge = InlineEdge::kInlineEnd;
      offset.inline_offset += total_fragment_size.inline_size;
    }

    // Make the child offset relative to our fragment.
    offset.block_offset -= previous_consumed_block_size;

    container_builder_.AddOutOfFlowChildCandidate(child, offset, inline_edge,
                                                  block_edge);
  }
}

void FlexLayoutAlgorithm::SetReadingFlowNodes(
    const HeapVector<FlexLine>& flex_lines) {
  const auto& style = Style();
  const EReadingFlow reading_flow = style.ReadingFlow();
  if (reading_flow != EReadingFlow::kFlexVisual &&
      reading_flow != EReadingFlow::kFlexFlow) {
    return;
  }
  bool should_sort_by_reading_order = false;
  Vector<const BlockNode*, 16> reordered_flex_nodes;
  reordered_flex_nodes.ReserveInitialCapacity(flex_items_.size());
  auto AddItemIfNeeded = [&](const wtf_size_t item_index) {
    const BlockNode& block_node = flex_items_[item_index].block_node;
    // Add flex item if it is a DOM node.
    if (block_node.GetDOMNode()) {
      reordered_flex_nodes.emplace_back(&block_node);
      // We optimize to only sort by reading-order if at least one flex item's
      // reading-order value is not the default (0).
      if (block_node.Style().ReadingOrder() != 0) {
        should_sort_by_reading_order = true;
      }
    }
  };
  // Given CSS reading-flow, flex-flow, flex-direction; read values
  // in correct order.
  auto AddFlexItems = [&](const FlexLine& line) {
    if (reading_flow == EReadingFlow::kFlexFlow && is_reverse_direction_) {
      for (const wtf_size_t item_index : base::Reversed(line.item_indices)) {
        AddItemIfNeeded(item_index);
      }
    } else {
      for (const wtf_size_t item_index : line.item_indices) {
        AddItemIfNeeded(item_index);
      }
    }
  };
  if (reading_flow == EReadingFlow::kFlexFlow && is_wrap_reverse_) {
    for (const auto& line : base::Reversed(flex_lines)) {
      AddFlexItems(line);
    }
  } else {
    for (const auto& line : flex_lines) {
      AddFlexItems(line);
    }
  }

  // A flex reading flow container's items should be further sorted by the
  // reading-order property (default to 0).
  if (should_sort_by_reading_order) {
    auto CompareFlexItemsForReadingOrder = [](const auto& lhs,
                                              const auto& rhs) {
      return lhs->Style().ReadingOrder() < rhs->Style().ReadingOrder();
    };
    std::stable_sort(reordered_flex_nodes.begin(), reordered_flex_nodes.end(),
                     CompareFlexItemsForReadingOrder);
  }
  HeapVector<Member<blink::Node>> reading_flow_nodes;
  reading_flow_nodes.ReserveInitialCapacity(reordered_flex_nodes.size());
  for (const BlockNode* flex_block_node : reordered_flex_nodes) {
    reading_flow_nodes.push_back(flex_block_node->GetDOMNode());
  }
  container_builder_.SetReadingFlowNodes(std::move(reading_flow_nodes));
}

bool FlexLayoutAlgorithm::IsContainerCrossSizeDefinite() const {
  // A column flexbox's cross axis is an inline size, so is definite.
  if (is_column_)
    return true;

  return ChildAvailableSize().block_size != kIndefiniteSize;
}

bool FlexLayoutAlgorithm::DoesItemStretch(const BlockNode& child,
                                          ItemPosition alignment) const {
  // Note: Unresolvable % cross size doesn't count as auto for stretchability.
  // As discussed in https://github.com/w3c/csswg-drafts/issues/4312.
  return alignment == ItemPosition::kStretch &&
         DoesItemComputedCrossSizeHaveAuto(child);
}

bool FlexLayoutAlgorithm::DoesItemComputedCrossSizeHaveAuto(
    const BlockNode& child) const {
  const ComputedStyle& child_style = child.Style();
  if (is_horizontal_flow_) {
    return child_style.Height().HasAuto();
  }
  return child_style.Width().HasAuto();
}

bool FlexLayoutAlgorithm::WillChildCrossSizeBeContainerCrossSize(
    const BlockNode& child,
    ItemPosition alignment) const {
  return !is_multi_line_ && is_cross_size_definite_ &&
         DoesItemStretch(child, alignment);
}

ConstraintSpace FlexLayoutAlgorithm::BuildSpaceForIntrinsicInlineSize(
    const BlockNode& child,
    ItemPosition alignment) const {
  MinMaxConstraintSpaceBuilder builder(GetConstraintSpace(), Style(), child,
                                       /* is_new_fc */ true);
  builder.SetAvailableBlockSize(ChildAvailableSize().block_size);
  builder.SetPercentageResolutionBlockSize(child_percentage_size_.block_size);
  builder.SetReplacedPercentageResolutionBlockSize(
      child_percentage_size_.block_size);
  if (!is_column_ && WillChildCrossSizeBeContainerCrossSize(child, alignment)) {
    builder.SetBlockAutoBehavior(AutoSizeBehavior::kStretchExplicit);
  }
  return builder.ToConstraintSpace();
}

ConstraintSpace FlexLayoutAlgorithm::BuildSpaceForIntrinsicBlockSize(
    const BlockNode& flex_item,
    ItemPosition alignment,
    std::optional<LayoutUnit> override_inline_size) const {
  const ComputedStyle& child_style = flex_item.Style();
  ConstraintSpaceBuilder space_builder(GetConstraintSpace(),
                                       child_style.GetWritingDirection(),
                                       /* is_new_fc */ true);
  SetOrthogonalFallbackInlineSizeIfNeeded(Style(), flex_item, &space_builder);
  space_builder.SetCacheSlot(LayoutResultCacheSlot::kMeasure);
  space_builder.SetIsPaintedAtomically(true);

  if (WillChildCrossSizeBeContainerCrossSize(flex_item, alignment)) {
    if (is_column_)
      space_builder.SetInlineAutoBehavior(AutoSizeBehavior::kStretchExplicit);
    else
      space_builder.SetBlockAutoBehavior(AutoSizeBehavior::kStretchExplicit);
  }

  // For determining the intrinsic block-size we make %-block-sizes resolve
  // against an indefinite size.
  LogicalSize child_percentage_size = child_percentage_size_;
  if (is_column_) {
    child_percentage_size.block_size = kIndefiniteSize;
    space_builder.SetIsInitialBlockSizeIndefinite(true);
  }
  if (override_inline_size.has_value()) {
    LogicalSize available_size = ChildAvailableSize();
    available_size.inline_size = *override_inline_size;
    space_builder.SetIsFixedInlineSize(true);
    space_builder.SetAvailableSize(available_size);
  } else {
    space_builder.SetAvailableSize(ChildAvailableSize());
  }
  space_builder.SetPercentageResolutionSize(child_percentage_size);
  // TODO(dgrogan): The SetReplacedPercentageResolutionSize calls in this file
  // may be untested. Write a test or determine why they're unnecessary.
  space_builder.SetReplacedPercentageResolutionSize(child_percentage_size);
  return space_builder.ToConstraintSpace();
}

ConstraintSpace FlexLayoutAlgorithm::BuildSpaceForFlexBasis(
    const BlockNode& flex_item) const {
  ConstraintSpaceBuilder space_builder(GetConstraintSpace(),
                                       flex_item.Style().GetWritingDirection(),
                                       /* is_new_fc */ true);
  SetOrthogonalFallbackInlineSizeIfNeeded(Style(), flex_item, &space_builder);

  // This space is only used for resolving lengths, not for layout. We only
  // need the available and percentage sizes.
  space_builder.SetAvailableSize(ChildAvailableSize());
  space_builder.SetPercentageResolutionSize(child_percentage_size_);
  space_builder.SetReplacedPercentageResolutionSize(child_percentage_size_);
  return space_builder.ToConstraintSpace();
}

ConstraintSpace FlexLayoutAlgorithm::BuildSpaceForLayout(
    const BlockNode& flex_item_node,
    ItemPosition alignment,
    LayoutUnit item_main_axis_final_size,
    bool is_initial_block_size_indefinite,
    std::optional<LayoutUnit> override_inline_size,
    std::optional<LayoutUnit> line_cross_size_for_stretch,
    std::optional<LayoutUnit> block_offset_for_fragmentation,
    bool min_block_size_should_encompass_intrinsic_size) const {
  const ComputedStyle& child_style = flex_item_node.Style();
  ConstraintSpaceBuilder space_builder(GetConstraintSpace(),
                                       child_style.GetWritingDirection(),
                                       /* is_new_fc */ true);
  SetOrthogonalFallbackInlineSizeIfNeeded(Style(), flex_item_node,
                                          &space_builder);
  space_builder.SetIsPaintedAtomically(true);

  LogicalSize available_size;
  if (is_column_) {
    available_size.inline_size = line_cross_size_for_stretch
                                     ? *line_cross_size_for_stretch
                                     : ChildAvailableSize().inline_size;

    if (override_inline_size) {
      DCHECK(!line_cross_size_for_stretch.has_value())
          << "We only override inline size when we are calculating intrinsic "
             "width of multiline column flexboxes, and we don't do any "
             "stretching during the intrinsic width calculation.";
      available_size.inline_size = *override_inline_size;
      space_builder.SetIsFixedInlineSize(true);
    }
    available_size.block_size = item_main_axis_final_size;
    space_builder.SetIsFixedBlockSize(true);
    if (line_cross_size_for_stretch ||
        WillChildCrossSizeBeContainerCrossSize(flex_item_node, alignment)) {
      space_builder.SetInlineAutoBehavior(AutoSizeBehavior::kStretchExplicit);
    }
  } else {
    DCHECK(!override_inline_size.has_value());
    available_size.inline_size = item_main_axis_final_size;
    available_size.block_size = line_cross_size_for_stretch
                                    ? *line_cross_size_for_stretch
                                    : ChildAvailableSize().block_size;
    space_builder.SetIsFixedInlineSize(true);
    if (line_cross_size_for_stretch ||
        WillChildCrossSizeBeContainerCrossSize(flex_item_node, alignment)) {
      space_builder.SetBlockAutoBehavior(AutoSizeBehavior::kStretchExplicit);
    }
  }
  if (is_initial_block_size_indefinite) {
    space_builder.SetIsInitialBlockSizeIndefinite(true);
  }
  if (!line_cross_size_for_stretch &&
      DoesItemStretch(flex_item_node, alignment)) {
    // For the first layout pass of stretched items, the goal is to determine
    // the post-flexed, pre-stretched cross-axis size. Stretched items will
    // later get a final layout with a potentially different cross size so use
    // the "measure" slot for this layout. We will use the "layout" cache slot
    // for the item's final layout.
    //
    // Setting the "measure" cache slot on the space writes the result
    // into both the "measure" and "layout" cache slots. So the stretch
    // layout will reuse this "measure" result if it can.
    space_builder.SetCacheSlot(LayoutResultCacheSlot::kMeasure);
  } else if (block_offset_for_fragmentation &&
             GetConstraintSpace().HasBlockFragmentation()) {
    if (min_block_size_should_encompass_intrinsic_size)
      space_builder.SetMinBlockSizeShouldEncompassIntrinsicSize();
    SetupSpaceBuilderForFragmentation(container_builder_, flex_item_node,
                                      *block_offset_for_fragmentation,
                                      &space_builder);
  }

  space_builder.SetAvailableSize(available_size);
  space_builder.SetPercentageResolutionSize(child_percentage_size_);
  space_builder.SetReplacedPercentageResolutionSize(child_percentage_size_);
  return space_builder.ToConstraintSpace();
}

void FlexLayoutAlgorithm::ConstructAndAppendFlexItems(
    Phase phase,
    HeapVector<Member<LayoutBox>>* oof_children) {

  wtf_size_t item_index = 0;
  FlexChildIterator iterator(Node());
  flex_items_.ReserveInitialCapacity(iterator.size());

  for (BlockNode child = iterator.NextChild(); child;
       child = iterator.NextChild()) {
    if (child.IsOutOfFlowPositioned()) {
      if (phase == Phase::kLayout) {
        DCHECK(oof_children);
        oof_children->emplace_back(child.GetLayoutBox());
      }
      continue;
    }

    const ComputedStyle& child_style = child.Style();
    const ItemPosition alignment = ResolvedAlignSelf(child_style);

    std::optional<LayoutUnit> max_content_contribution;
    if (phase == Phase::kColumnWrapIntrinsicSize) {
      auto space = BuildSpaceForIntrinsicInlineSize(child, alignment);
      MinMaxSizesResult child_contributions =
          ComputeMinAndMaxContentContribution(Style(), child, space);
      max_content_contribution = child_contributions.sizes.max_size;
      BoxStrut child_margins =
          ComputeMarginsFor(space, child.Style(), GetConstraintSpace());
      child_contributions.sizes += child_margins.InlineSum();

      largest_min_content_contribution_ =
          std::max(child_contributions.sizes.min_size,
                   largest_min_content_contribution_);
    }

    const auto child_writing_mode = child_style.GetWritingMode();
    const bool is_main_axis_inline_axis =
        IsHorizontalWritingMode(child_writing_mode) == is_horizontal_flow_;

    ConstraintSpace flex_basis_space = BuildSpaceForFlexBasis(child);

    PhysicalBoxStrut physical_child_margins =
        ComputePhysicalMargins(flex_basis_space, child_style);

    BoxStrut border_padding_in_child_writing_mode =
        ComputeBorders(flex_basis_space, child) +
        ComputePadding(flex_basis_space, child_style);

    PhysicalBoxStrut physical_border_padding(
        border_padding_in_child_writing_mode.ConvertToPhysical(
            child_style.GetWritingDirection()));

    const uint8_t main_axis_auto_margin_count =
        is_horizontal_flow_ ? child_style.MarginLeft().IsAuto() +
                                  child_style.MarginRight().IsAuto()
                            : child_style.MarginTop().IsAuto() +
                                  child_style.MarginBottom().IsAuto();
    const LayoutUnit main_axis_border_padding =
        is_horizontal_flow_ ? physical_border_padding.HorizontalSum()
                            : physical_border_padding.VerticalSum();
    const auto child_space = BuildSpaceForIntrinsicBlockSize(
        child, alignment, max_content_contribution);

    bool depends_on_min_max_sizes = false;
    auto MinMaxSizesFunc = [&](SizeType type) -> MinMaxSizesResult {
      depends_on_min_max_sizes = true;
      // We want the child's intrinsic inline sizes in its writing mode, so
      // pass child's writing mode as the first parameter, which is nominally
      // |container_writing_mode|.
      return child.ComputeMinMaxSizes(child_writing_mode, type, child_space);
    };

    auto InlineSizeFunc = [&]() -> LayoutUnit {
      return CalculateInitialFragmentGeometry(child_space, child,
                                              /* break_token */ nullptr)
          .border_box_size.inline_size;
    };

    const LayoutResult* layout_result = nullptr;
    auto BlockSizeFunc = [&](SizeType type) -> LayoutUnit {
      // This function mirrors the logic within `BlockNode::ComputeMinMaxSizes`.

      // Don't apply any special aspect-ratio treatment for replaced elements.
      if (child.IsReplaced()) {
        return ComputeReplacedSize(child, child_space,
                                   border_padding_in_child_writing_mode,
                                   ReplacedSizeMode::kIgnoreBlockLengths)
            .block_size;
      }

      const bool has_aspect_ratio = !child_style.AspectRatio().IsAuto();
      if (has_aspect_ratio && type == SizeType::kContent) {
        const LayoutUnit inline_size = InlineSizeFunc();
        if (inline_size != kIndefiniteSize) {
          return BlockSizeFromAspectRatio(border_padding_in_child_writing_mode,
                                          child_style.LogicalAspectRatio(),
                                          child_style.BoxSizingForAspectRatio(),
                                          inline_size);
        }
      }

      LayoutUnit intrinsic_size;
      if (child.ShouldApplyBlockSizeContainment()) {
        // If we have block-size containment we can avoid layout for
        // determining the intrinsic size.
        intrinsic_size = ClampIntrinsicBlockSize(
            child_space, child, /* break_token */ nullptr,
            border_padding_in_child_writing_mode,
            /* current_intrinsic_block_size */ LayoutUnit());
      } else {
        if (!layout_result) {
          std::optional<DisableLayoutSideEffectsScope> disable_side_effects;
          if (phase != Phase::kLayout &&
              !Node().GetLayoutBox()->NeedsLayout()) {
            disable_side_effects.emplace();
          }
          layout_result = child.Layout(child_space);
          DCHECK(layout_result);
        }
        intrinsic_size = layout_result->IntrinsicBlockSize();
      }

      // Constrain the intrinsic-size by the transferred min/max constraints.
      if (has_aspect_ratio) {
        const MinMaxSizes inline_min_max = ComputeMinMaxInlineSizes(
            flex_basis_space, child, border_padding_in_child_writing_mode,
            /* auto_min_length */ nullptr, MinMaxSizesFunc,
            TransferredSizesMode::kIgnore);
        const MinMaxSizes min_max = ComputeTransferredMinMaxBlockSizes(
            child_style.LogicalAspectRatio(), inline_min_max,
            border_padding_in_child_writing_mode,
            child_style.BoxSizingForAspectRatio());
        return min_max.ClampSizeToMinAndMax(intrinsic_size);
      }

      return intrinsic_size;
    };

    const Length& flex_basis = child_style.FlexBasis();
    if (is_column_ && flex_basis.MayHavePercentDependence()) {
      has_column_percent_flex_basis_ = true;
    }

    // This bool is set to true while calculating the base size, the flex-basis
    // is "content" based (e.g. dependent on the child's content).
    bool is_used_flex_basis_indefinite = false;

    // An auto value for flex-basis says to defer to width or height.
    // Those might in turn have an auto value.  And in either case the
    // value might be calc-size(auto, ...).  Because of this, we might
    // need to handle resolving the length in the main axis twice.
    auto resolve_main_length = [&](const Length& used_flex_basis_length,
                                   const Length* auto_length) -> LayoutUnit {
      if (is_main_axis_inline_axis) {
        const LayoutUnit inline_size = ResolveMainInlineLength(
            flex_basis_space, child_style, border_padding_in_child_writing_mode,
            [&](SizeType type) -> MinMaxSizesResult {
              is_used_flex_basis_indefinite = true;
              return MinMaxSizesFunc(type);
            },
            used_flex_basis_length, auto_length);

        if (inline_size != kIndefiniteSize) {
          return inline_size;
        }

        // We weren't able to resolve the length (i.e. we were a unresolvable
        // %-age or similar), fallback to the max-content size.
        is_used_flex_basis_indefinite = true;
        return MinMaxSizesFunc(SizeType::kContent).sizes.max_size;
      }

      return ResolveMainBlockLength(
          flex_basis_space, child_style, border_padding_in_child_writing_mode,
          used_flex_basis_length, auto_length, [&](SizeType type) {
            is_used_flex_basis_indefinite = true;
            return BlockSizeFunc(type);
          });
    };

    const LayoutUnit flex_base_border_box = ([&]() -> LayoutUnit {
      std::optional<Length> auto_flex_basis_length;

      if (flex_basis.HasAuto()) {
        const Length& specified_length_in_main_axis =
            is_horizontal_flow_ ? child_style.Width() : child_style.Height();

        // 'auto' for items within a -webkit-box resolve as 'fit-content'.
        const Length& auto_size_length =
            (is_webkit_box_ &&
             (Style().BoxOrient() == EBoxOrient::kHorizontal ||
              Style().BoxAlign() != EBoxAlignment::kStretch))
                ? Length::FitContent()
                : Length::MaxContent();

        LayoutUnit auto_flex_basis_size = resolve_main_length(
            specified_length_in_main_axis, &auto_size_length);
        if (child_style.BoxSizing() == EBoxSizing::kContentBox) {
          auto_flex_basis_size -= main_axis_border_padding;
        }
        DCHECK_GE(auto_flex_basis_size, LayoutUnit());
        auto_flex_basis_length = Length::Fixed(auto_flex_basis_size);
      }

      LayoutUnit main_size = resolve_main_length(
          flex_basis, base::OptionalToPtr(auto_flex_basis_length));

      // Add the caption block-size only to sizes that are not content-based.
      if (!is_main_axis_inline_axis && !is_used_flex_basis_indefinite) {
        // 1. A table interprets forced block-size as the block-size of its
        //    captions and rows.
        // 2. The specified block-size of a table only applies to its rows.
        // 3. If the block-size resolved, add the caption block-size so that
        //    the forced block-size works correctly.
        if (const auto* table_child = DynamicTo<TableNode>(&child)) {
          main_size += table_child->ComputeCaptionBlockSize(child_space);
        }
      }

      return main_size;
    })();

    // Spec calls this "flex base size"
    // https://www.w3.org/TR/css-flexbox-1/#algo-main-item
    // Blink's FlexibleBoxAlgorithm expects it to be content + scrollbar widths,
    // but no padding or border.
    DCHECK_GE(flex_base_border_box, main_axis_border_padding);
    const LayoutUnit base_content_size =
        flex_base_border_box - main_axis_border_padding;

    std::optional<Length> auto_min_length;
    if (ShouldApplyAutoMinSize(child)) {
      const LayoutUnit content_size_suggestion = ([&]() -> LayoutUnit {
        const LayoutUnit content_size =
            is_main_axis_inline_axis
                ? MinMaxSizesFunc(SizeType::kContent).sizes.min_size
                : BlockSizeFunc(SizeType::kContent);

        // For non-replaced elements with an aspect-ratio ensure the size
        // provided by the aspect-ratio encompasses the min-intrinsic size.
        if (!child.IsReplaced() && !child_style.AspectRatio().IsAuto()) {
          return std::max(
              content_size,
              is_main_axis_inline_axis
                  ? MinMaxSizesFunc(SizeType::kIntrinsic).sizes.min_size
                  : BlockSizeFunc(SizeType::kIntrinsic));
        }

        return content_size;
      })();
      DCHECK_GE(content_size_suggestion, main_axis_border_padding);

      const LayoutUnit specified_size_suggestion = ([&]() -> LayoutUnit {
        const Length& specified_length_in_main_axis =
            is_horizontal_flow_ ? child_style.Width() : child_style.Height();
        if (specified_length_in_main_axis.HasAuto()) {
          return LayoutUnit::Max();
        }
        const LayoutUnit resolved_size =
            is_main_axis_inline_axis
                ? ResolveMainInlineLength(
                      flex_basis_space, child_style,
                      border_padding_in_child_writing_mode, MinMaxSizesFunc,
                      specified_length_in_main_axis, /* auto_length */ nullptr)
                : ResolveMainBlockLength(flex_basis_space, child_style,
                                         border_padding_in_child_writing_mode,
                                         specified_length_in_main_axis,
                                         /* auto_length */ nullptr,
                                         BlockSizeFunc);

        // Coerce an indefinite size to LayoutUnit::Max().
        return resolved_size == kIndefiniteSize ? LayoutUnit::Max()
                                                : resolved_size;
      })();

      LayoutUnit auto_min_size =
          std::min(specified_size_suggestion, content_size_suggestion);
      if (child_style.BoxSizing() == EBoxSizing::kContentBox) {
        auto_min_size -= main_axis_border_padding;
      }
      DCHECK_GE(auto_min_size, LayoutUnit());
      auto_min_length = Length::Fixed(auto_min_size);
    }

    MinMaxSizes min_max_sizes_in_main_axis_direction =
        is_main_axis_inline_axis
            ? ComputeMinMaxInlineSizes(
                  flex_basis_space, child, border_padding_in_child_writing_mode,
                  base::OptionalToPtr(auto_min_length), MinMaxSizesFunc,
                  TransferredSizesMode::kIgnore)
            : ComputeMinMaxBlockSizes(
                  flex_basis_space, child, border_padding_in_child_writing_mode,
                  base::OptionalToPtr(auto_min_length), BlockSizeFunc);

    min_max_sizes_in_main_axis_direction -= main_axis_border_padding;
    DCHECK_GE(min_max_sizes_in_main_axis_direction.min_size, LayoutUnit());
    DCHECK_GE(min_max_sizes_in_main_axis_direction.max_size, LayoutUnit());

    const BoxStrut initial_scrollbars = ComputeScrollbarsForNonAnonymous(child);

    auto AspectRatioProvidesBlockMainSize = [&]() -> bool {
      if (is_main_axis_inline_axis) {
        return false;
      }
      if (child.IsReplaced()) {
        return false;
      }
      return !child_style.AspectRatio().IsAuto() &&
             InlineSizeFunc() != kIndefiniteSize;
    };

    // For flex-items whose main-axis is the block-axis we treat the initial
    // block-size as indefinite if:
    //  - The flex container has an indefinite main-size.
    //  - The used flex-basis is indefinite.
    //  - The aspect-ratio doesn't provide the main-size.
    //
    // See: // https://drafts.csswg.org/css-flexbox/#definite-sizes
    const bool is_initial_block_size_indefinite =
        is_column_ && !is_main_axis_inline_axis &&
        ChildAvailableSize().block_size == kIndefiniteSize &&
        is_used_flex_basis_indefinite && !AspectRatioProvidesBlockMainSize();

    const float flex_grow = child_style.ResolvedFlexGrow(Style());
    const float flex_shrink = child_style.ResolvedFlexShrink(Style());

    const auto container_writing_direction =
        GetConstraintSpace().GetWritingDirection();
    const auto baseline_writing_mode = DetermineBaselineWritingMode(
        container_writing_direction, child_writing_mode,
        /* is_parallel_context */ !is_column_);
    const auto baseline_group = DetermineBaselineGroup(
        container_writing_direction, baseline_writing_mode,
        /* is_parallel_context */ !is_column_,
        /* is_last_baseline */ alignment == ItemPosition::kLastBaseline,
        /* is_flipped */ is_wrap_reverse_);

    flex_items_.emplace_back(
        child, item_index++, flex_grow, flex_shrink, base_content_size,
        min_max_sizes_in_main_axis_direction, main_axis_border_padding,
        physical_child_margins, initial_scrollbars, main_axis_auto_margin_count,
        alignment, baseline_writing_mode, baseline_group,
        is_initial_block_size_indefinite, is_used_flex_basis_indefinite,
        depends_on_min_max_sizes, is_horizontal_flow_,
        max_content_contribution);
    // Save the layout result so that we can maybe reuse it later.
    if (layout_result && !is_main_axis_inline_axis) {
      flex_items_.back().layout_result = layout_result;
    }
  }
}

const LayoutResult* FlexLayoutAlgorithm::Layout() {
  auto* result = LayoutInternal();
  switch (result->Status()) {
    case LayoutResult::kNeedsEarlierBreak:
      // If we found a good break somewhere inside this block, re-layout and
      // break at that location.
      DCHECK(result->GetEarlyBreak());
      return RelayoutAndBreakEarlier<FlexLayoutAlgorithm>(
          *result->GetEarlyBreak(), &column_early_breaks_);
    case LayoutResult::kNeedsRelayoutWithNoChildScrollbarChanges:
      DCHECK(!ignore_child_scrollbar_changes_);
      return Relayout<FlexLayoutAlgorithm>(
          kRelayoutIgnoringChildScrollbarChanges);
    case LayoutResult::kDisableFragmentation:
      DCHECK(GetConstraintSpace().HasBlockFragmentation());
      return RelayoutWithoutFragmentation<FlexLayoutAlgorithm>();
    case LayoutResult::kNeedsRelayoutWithRowCrossSizeChanges:
      return RelayoutWithNewRowSizes();
    default:
      return result;
  }
}

const LayoutResult* FlexLayoutAlgorithm::LayoutInternal() {
  // Freezing the scrollbars for the sub-tree shouldn't be strictly necessary,
  // but we do this just in case we trigger an unstable layout.
  std::optional<PaintLayerScrollableArea::FreezeScrollbarsScope>
      freeze_scrollbars;
  if (ignore_child_scrollbar_changes_)
    freeze_scrollbars.emplace();

  PaintLayerScrollableArea::DelayScrollOffsetClampScope delay_clamp_scope;

  Vector<EBreakBetween> row_break_between_outputs;
  HeapVector<FlexLine> flex_lines;
  HeapVector<Member<LayoutBox>> oof_children;
  FlexBreakTokenData::FlexBreakBeforeRow break_before_row =
      FlexBreakTokenData::kNotBreakBeforeRow;
  LayoutUnit total_intrinsic_block_size;

  ClearCollectionScope<HeapVector<FlexLine>> scope(&flex_lines);

  if (IsBreakInside(GetBreakToken())) {
    const auto* flex_data =
        To<FlexBreakTokenData>(GetBreakToken()->TokenData());
    total_intrinsic_block_size = flex_data->intrinsic_block_size;
    flex_lines = flex_data->flex_lines;
    row_break_between_outputs = flex_data->row_break_between;
    break_before_row = flex_data->break_before_row;
    oof_children = flex_data->oof_children;
  } else {
    PlaceFlexItems(&flex_lines, &oof_children, &total_intrinsic_block_size);
  }

  total_block_size_ = ComputeBlockSizeForFragment(
      GetConstraintSpace(), Node(), BorderPadding(), total_intrinsic_block_size,
      container_builder_.InlineSize());

  if (!IsBreakInside(GetBreakToken())) {
    ApplyReversals(&flex_lines);
    LayoutResult::EStatus status =
        GiveItemsFinalPositionAndSize(&flex_lines, &row_break_between_outputs);
    if (status != LayoutResult::kSuccess) {
      return container_builder_.Abort(status);
    }
  }

  LayoutUnit previously_consumed_block_size;
  if (GetBreakToken()) [[unlikely]] {
    previously_consumed_block_size = GetBreakToken()->ConsumedBlockSize();
  }

  intrinsic_block_size_ = BorderScrollbarPadding().block_start;
  LayoutUnit block_size;
  if (InvolvedInBlockFragmentation(container_builder_)) [[unlikely]] {
    const bool use_empty_line_block_size =
        flex_lines.empty() && Node().HasLineIfEmpty();
    if (use_empty_line_block_size) {
      intrinsic_block_size_ =
          (total_intrinsic_block_size - BorderScrollbarPadding().block_end -
           previously_consumed_block_size)
              .ClampNegativeToZero();
    }

    LayoutResult::EStatus status =
        GiveItemsFinalPositionAndSizeForFragmentation(
            &flex_lines, &row_break_between_outputs, &break_before_row,
            &total_intrinsic_block_size);
    if (status != LayoutResult::kSuccess) {
      return container_builder_.Abort(status);
    }

    intrinsic_block_size_ = ClampIntrinsicBlockSize(
        GetConstraintSpace(), Node(), GetBreakToken(), BorderScrollbarPadding(),
        intrinsic_block_size_ + BorderScrollbarPadding().block_end);

    block_size = ComputeBlockSizeForFragment(
        GetConstraintSpace(), Node(), BorderPadding(),
        previously_consumed_block_size + intrinsic_block_size_,
        container_builder_.InlineSize());
  } else {
    intrinsic_block_size_ = total_intrinsic_block_size;
    block_size = total_block_size_;
  }

  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size_);
  container_builder_.SetFragmentsTotalBlockSize(block_size);

  if (has_column_percent_flex_basis_)
    container_builder_.SetHasDescendantThatDependsOnPercentageBlockSize(true);
  if (layout_info_for_devtools_) [[unlikely]] {
    container_builder_.TransferFlexLayoutData(
        std::move(layout_info_for_devtools_));
  }

  if (InvolvedInBlockFragmentation(container_builder_)) [[unlikely]] {
    BreakStatus break_status = FinishFragmentation(&container_builder_);
    if (break_status != BreakStatus::kContinue) {
      if (break_status == BreakStatus::kNeedsEarlierBreak) {
        return container_builder_.Abort(LayoutResult::kNeedsEarlierBreak);
      }
      DCHECK_EQ(break_status, BreakStatus::kDisableFragmentation);
      return container_builder_.Abort(LayoutResult::kDisableFragmentation);
    }
  } else {
#if DCHECK_IS_ON()
    // If we're not participating in a fragmentation context, no block
    // fragmentation related fields should have been set.
    container_builder_.CheckNoBlockFragmentation();
#endif
  }

  SetReadingFlowNodes(flex_lines);
  HandleOutOfFlowPositionedItems(total_intrinsic_block_size, oof_children);

  // For rows, the break-before of the first row and the break-after of the
  // last row are propagated to the container. For columns, treat the set
  // of columns as a single row and propagate the combined break-before rules
  // for the first items in each column and break-after rules for last items in
  // each column.
  if (GetConstraintSpace().ShouldPropagateChildBreakValues()) {
    DCHECK(!row_break_between_outputs.empty());
    container_builder_.SetInitialBreakBefore(row_break_between_outputs.front());
    container_builder_.SetPreviousBreakAfter(row_break_between_outputs.back());
  }

  if (GetConstraintSpace().HasBlockFragmentation()) {
    container_builder_.SetBreakTokenData(
        MakeGarbageCollected<FlexBreakTokenData>(
            container_builder_.GetBreakTokenData(), flex_lines,
            row_break_between_outputs, oof_children, total_intrinsic_block_size,
            break_before_row));
  }

  // Un-freeze descendant scrollbars before we run the OOF layout part.
  freeze_scrollbars.reset();

  container_builder_.HandleOofsAndSpecialDescendants();

  return container_builder_.ToBoxFragment();
}

void FlexLayoutAlgorithm::PlaceFlexItems(
    HeapVector<FlexLine>* flex_lines,
    HeapVector<Member<LayoutBox>>* oof_children,
    LayoutUnit* total_intrinsic_block_size,
    bool is_computing_multiline_column_intrinsic_size) {
  DCHECK(oof_children || is_computing_multiline_column_intrinsic_size);
  ConstructAndAppendFlexItems(is_computing_multiline_column_intrinsic_size
                                  ? Phase::kColumnWrapIntrinsicSize
                                  : Phase::kLayout,
                              oof_children);

  const LayoutUnit line_break_size = MainAxisContentExtent(LayoutUnit::Max());
  const FlexLineBreakerResult result =
      BreakFlexItemsIntoLines(base::span(flex_items_), line_break_size,
                              gap_between_items_, is_multi_line_);

  // For column flexboxes we can now determine the intrinsic block-size, which
  // we use to flex all the lines to.
  const LayoutUnit main_axis_inner_size =
      MainAxisContentExtent(result.max_sum_hypothetical_main_size);

  LayoutUnit sum_line_cross_size;

  flex_lines->reserve(result.flex_lines.size());
  for (auto& line : result.flex_lines) {
    // Flex the items.
    LineFlexer(base::span(line.line_items), line.sum_hypothetical_main_size,
               line.sum_flex_base_size, main_axis_inner_size)
        .Run();

    Vector<wtf_size_t> item_indices;
    item_indices.ReserveInitialCapacity(line.line_items.size());

    LayoutUnit main_axis_free_space =
        main_axis_inner_size -
        (line.line_items.size() - 1) * gap_between_items_;
    LayoutUnit line_cross_size;
    LayoutUnit max_major_ascent = LayoutUnit::Min();
    LayoutUnit max_minor_ascent = LayoutUnit::Min();
    LayoutUnit max_major_descent = LayoutUnit::Min();
    LayoutUnit max_minor_descent = LayoutUnit::Min();
    unsigned main_axis_auto_margin_count = 0;

    for (wtf_size_t i = 0; i < line.line_items.size(); ++i) {
      FlexItem& flex_item = line.line_items[i];

      item_indices.push_back(flex_item.item_index);
      main_axis_free_space -= flex_item.FlexedMarginBoxSize();
      main_axis_auto_margin_count += flex_item.main_axis_auto_margin_count;

      const LayoutUnit cross_axis_size = ([&]() {
        const ConstraintSpace space =
            BuildSpaceForLayout(flex_item.block_node, flex_item.alignment,
                                flex_item.FlexedBorderBoxSize(),
                                flex_item.is_initial_block_size_indefinite,
                                flex_item.max_content_contribution);

        // We need to get the item's cross-axis size given its new main size.
        //
        // If the new main size is the item's inline-size, then we have to do a
        // layout to get its new block-size.
        // But if the new main size is the item's block-size, we can skip
        // layout in some cases and just calculate the inline-size directly.
        //
        // Even when we only need inline-size, we have to lay out the item if:
        //  * This is the item's last chance to layout (i.e. doesn't stretch).
        //  * The item has not yet been laid out.
        if (DoesItemStretch(flex_item.block_node, flex_item.alignment) &&
            flex_item.layout_result) {
          const auto& item_style = flex_item.block_node.Style();
          DCHECK_NE(is_horizontal_flow_, item_style.IsHorizontalWritingMode());
          const BoxStrut border_padding =
              ComputeBorders(space, flex_item.block_node) +
              ComputePadding(space, item_style);
          if (flex_item.block_node.IsReplaced()) {
            return ComputeReplacedSize(flex_item.block_node, space,
                                       border_padding)
                .inline_size;
          }
          return ComputeInlineSizeForFragment(space, flex_item.block_node,
                                              border_padding);
        }

        if (is_computing_multiline_column_intrinsic_size) {
          return *flex_item.max_content_contribution;
        }

        DCHECK((space.CacheSlot() == LayoutResultCacheSlot::kLayout) ||
               !flex_item.layout_result);
        flex_item.layout_result = flex_item.block_node.Layout(space);
        DCHECK_EQ(flex_item.layout_result->Status(), LayoutResult::kSuccess);
        const PhysicalSize size =
            flex_item.layout_result->GetPhysicalFragment().Size();
        return is_horizontal_flow_ ? size.height : size.width;
      })();

      // Calculate the size used to determine the line cross-axis size.
      //
      // Typically this is just the cross-axis size, however if we are baseline
      // aligned we need to track the baseline(s) max ascent/descent, and use
      // the "baseline" size instead.
      LayoutUnit cross_axis_margin_size =
          cross_axis_size + flex_item.CrossAxisMarginExtent();

      // TODO(crbug.com/1272533): We may not have a layout-result during
      // min/max calculations. This is incorrect, and we should produce a
      // layout-result when baseline aligned.
      if (flex_item.layout_result &&
          (flex_item.alignment == ItemPosition::kBaseline ||
           flex_item.alignment == ItemPosition::kLastBaseline)) {
        const LayoutUnit ascent = BaselineAscent(
            flex_item, To<PhysicalBoxFragment>(
                           flex_item.layout_result->GetPhysicalFragment()));
        const LayoutUnit descent = cross_axis_margin_size - ascent;
        if (flex_item.baseline_group == BaselineGroup::kMajor) {
          max_major_ascent = std::max(max_major_ascent, ascent);
          max_major_descent = std::max(max_major_descent, descent);
          cross_axis_margin_size = max_major_ascent + max_major_descent;
        } else {
          max_minor_ascent = std::max(max_minor_ascent, ascent);
          max_minor_descent = std::max(max_minor_descent, descent);
          cross_axis_margin_size = max_minor_ascent + max_minor_descent;
        }
      }
      line_cross_size = std::max(line_cross_size, cross_axis_margin_size);
    }

    flex_lines->emplace_back(std::move(item_indices), main_axis_free_space,
                             line_cross_size, max_major_ascent,
                             max_minor_ascent, main_axis_auto_margin_count);

    sum_line_cross_size += line_cross_size;
  }

  // Determine the intrinsic block-size if within the layout-pass.
  if (total_intrinsic_block_size) {
    *total_intrinsic_block_size = ([&]() {
      LayoutUnit size = BorderScrollbarPadding().BlockSum();
      if (!flex_lines->empty()) {
        if (is_column_) {
          // Take the largest hypothetical main-size.
          size += result.max_sum_hypothetical_main_size;
        } else {
          // Take the sum of all the line cross-sizes (and the gaps between
          // them).
          size += sum_line_cross_size;
          size += (flex_lines->size() - 1) * gap_between_lines_;
        }
      } else if (Node().HasLineIfEmpty()) {
        size += Node().EmptyLineBlockSize(GetBreakToken());
      }

      return ClampIntrinsicBlockSize(GetConstraintSpace(), Node(),
                                     GetBreakToken(), BorderScrollbarPadding(),
                                     size);
    })();
  }
}

void FlexLayoutAlgorithm::ApplyReversals(HeapVector<FlexLine>* flex_lines) {
  if (is_wrap_reverse_) {
    flex_lines->Reverse();
  }

  if (is_reverse_direction_) {
    for (auto& flex_line : *flex_lines) {
      flex_line.item_indices.Reverse();
    }
  }
}

namespace {

LayoutUnit InitialContentPositionOffset(const StyleContentAlignmentData& data,
                                        ContentPosition safe_position,
                                        LayoutUnit free_space,
                                        unsigned number_of_items,
                                        bool is_reverse) {
  switch (data.Distribution()) {
    case ContentDistributionType::kDefault:
      break;
    case ContentDistributionType::kSpaceBetween:
      if (free_space > LayoutUnit() && number_of_items > 1) {
        return LayoutUnit();
      }
      // Fallback to 'flex-start'.
      return is_reverse ? free_space : LayoutUnit();
    case ContentDistributionType::kSpaceAround:
      if (free_space > LayoutUnit() && number_of_items) {
        return free_space / (2 * number_of_items);
      }
      // Fallback to 'safe center'.
      return (free_space / 2).ClampNegativeToZero();
    case ContentDistributionType::kSpaceEvenly:
      if (free_space > LayoutUnit() && number_of_items) {
        return free_space / (number_of_items + 1);
      }
      // Fallback to 'safe center'.
      return (free_space / 2).ClampNegativeToZero();
    case ContentDistributionType::kStretch:
      // Fallback to 'flex-start'.
      return is_reverse ? free_space : LayoutUnit();
  }

  ContentPosition position = data.GetPosition();
  if (free_space <= LayoutUnit() &&
      data.Overflow() == OverflowAlignment::kSafe) {
    position = safe_position;
  }

  switch (position) {
    case ContentPosition::kCenter:
      return free_space / 2;
    case ContentPosition::kStart:
      return LayoutUnit();
    case ContentPosition::kEnd:
      return free_space;
    case ContentPosition::kFlexEnd:
      return is_reverse ? LayoutUnit() : free_space;
    case ContentPosition::kFlexStart:
    case ContentPosition::kNormal:
    case ContentPosition::kBaseline:
    case ContentPosition::kLastBaseline:
      return is_reverse ? free_space : LayoutUnit();
    case ContentPosition::kLeft:
    case ContentPosition::kRight:
      NOTREACHED();
  }
}

LayoutUnit ContentDistributionSpace(const StyleContentAlignmentData& data,
                                    LayoutUnit free_space,
                                    unsigned number_of_items) {
  if (free_space <= LayoutUnit() || number_of_items <= 1) {
    return LayoutUnit();
  }
  switch (data.Distribution()) {
    case ContentDistributionType::kDefault:
    case ContentDistributionType::kStretch:
      return LayoutUnit();
    case ContentDistributionType::kSpaceBetween:
      return free_space / (number_of_items - 1);
    case ContentDistributionType::kSpaceEvenly:
      return free_space / (number_of_items + 1);
    case ContentDistributionType::kSpaceAround:
      return free_space / number_of_items;
  }
}

}  // namespace

LayoutResult::EStatus FlexLayoutAlgorithm::GiveItemsFinalPositionAndSize(
    HeapVector<FlexLine>* flex_lines,
    Vector<EBreakBetween>* row_break_between_outputs) {
  DCHECK(!IsBreakInside(GetBreakToken()));

  const bool should_propagate_row_break_values =
      GetConstraintSpace().ShouldPropagateChildBreakValues();
  if (should_propagate_row_break_values) {
    DCHECK(row_break_between_outputs);
    // The last row break between will store the final break-after to be
    // propagated to the container.
    if (!is_column_) {
      *row_break_between_outputs =
          Vector<EBreakBetween>(flex_lines->size() + 1, EBreakBetween::kAuto);
    } else {
      // For flex columns, we only need to store two values - one for
      // the break-before value of all combined columns, and the second for
      // for the break-after values for all combined columns.
      *row_break_between_outputs =
          Vector<EBreakBetween>(2, EBreakBetween::kAuto);
    }
  }

  // Nothing to do if we don't have any flex-lines.
  if (flex_lines->empty()) {
    return LayoutResult::kSuccess;
  }

  const auto& style = Style();
  const WritingDirectionMode writing_direction =
      GetConstraintSpace().GetWritingDirection();

  const StyleContentAlignmentData justify_content = ResolvedJustifyContent();
  const StyleContentAlignmentData align_content = style.AlignContent();

  // Determine the cross-axis free-space.
  const wtf_size_t num_lines = flex_lines->size();
  const LayoutUnit cross_axis_content_size =
      (is_column_ ? (container_builder_.InlineSize() -
                     BorderScrollbarPadding().InlineSum())
                  : (total_block_size_ - BorderScrollbarPadding().BlockSum()))
          .ClampNegativeToZero();
  LayoutUnit cross_axis_free_space = cross_axis_content_size;
  for (const FlexLine& line : *flex_lines) {
    cross_axis_free_space -= line.line_cross_size;
  }
  cross_axis_free_space -= (num_lines - 1) * gap_between_lines_;

  const bool is_align_content_stretch =
      align_content.Distribution() == ContentDistributionType::kStretch ||
      (align_content.GetPosition() == ContentPosition::kNormal &&
       align_content.Distribution() == ContentDistributionType::kDefault);
  if (!is_multi_line_) {
    // A single line flexbox will always be the cross-axis content-size.
    flex_lines->back().line_cross_size = cross_axis_content_size;
    cross_axis_free_space = LayoutUnit();
  } else if (cross_axis_free_space >= LayoutUnit() &&
             is_align_content_stretch) {
    // Stretch lines in a multi-line flexbox to the available free-space.
    const LayoutUnit delta = cross_axis_free_space / num_lines;
    for (FlexLine& line : *flex_lines) {
      line.line_cross_size += delta;
    }
    cross_axis_free_space = LayoutUnit();
  }

  // -webkit-box has a weird quirk - an RTL box will overflow as if it was LTR.
  // NOTE: We should attempt to remove this in the future.
  const ContentPosition safe_justify_position =
      !RuntimeEnabledFeatures::LayoutDisableWebkitBoxQuirksEnabled() &&
              is_webkit_box_ && !is_column_ &&
              style.Direction() == TextDirection::kRtl
          ? ContentPosition::kEnd
          : ContentPosition::kStart;

  const LayoutUnit space_between_lines =
      ContentDistributionSpace(align_content, cross_axis_free_space, num_lines);
  LayoutUnit line_cross_axis_offset =
      (is_column_ ? BorderScrollbarPadding().inline_start
                  : BorderScrollbarPadding().block_start) +
      InitialContentPositionOffset(align_content, ContentPosition::kStart,
                                   cross_axis_free_space, num_lines,
                                   is_wrap_reverse_);

  BaselineAccumulator baseline_accumulator(style);
  LayoutResult::EStatus status = LayoutResult::kSuccess;

  for (wtf_size_t flex_line_idx = 0; flex_line_idx < flex_lines->size();
       ++flex_line_idx) {
    if (layout_info_for_devtools_) [[unlikely]] {
      layout_info_for_devtools_->lines.push_back(DevtoolsFlexInfo::Line());
    }

    FlexLine& flex_line = (*flex_lines)[flex_line_idx];
    flex_line.cross_axis_offset = line_cross_axis_offset;

    bool is_first_line = flex_line_idx == 0;
    bool is_last_line = flex_line_idx == flex_lines->size() - 1;
    if (!InvolvedInBlockFragmentation(container_builder_) && !is_column_) {
      baseline_accumulator.AccumulateLine(flex_line, is_first_line,
                                          is_last_line);
    }

    const bool should_apply_main_axis_auto_margin =
        flex_line.main_axis_auto_margin_count &&
        flex_line.main_axis_free_space > LayoutUnit();

    const LayoutUnit main_axis_free_space =
        should_apply_main_axis_auto_margin ? LayoutUnit()
                                           : flex_line.main_axis_free_space;
    const LayoutUnit main_axis_auto_margin =
        should_apply_main_axis_auto_margin
            ? flex_line.main_axis_free_space /
                  flex_line.main_axis_auto_margin_count
            : LayoutUnit();

    const wtf_size_t line_items_size = flex_line.item_indices.size();
    const LayoutUnit space_between_items = ContentDistributionSpace(
        justify_content, main_axis_free_space, line_items_size);
    LayoutUnit main_axis_offset =
        (is_column_ ? BorderScrollbarPadding().block_start
                    : BorderScrollbarPadding().inline_start) +
        InitialContentPositionOffset(justify_content, safe_justify_position,
                                     main_axis_free_space, line_items_size,
                                     is_reverse_direction_);

    for (wtf_size_t item_index : flex_line.item_indices) {
      const FlexItem& item = flex_items_[item_index];

      const LayoutResult* layout_result = nullptr;
      if (DoesItemStretch(item.block_node, item.alignment)) {
        ConstraintSpace child_space = BuildSpaceForLayout(
            item.block_node, item.alignment, item.FlexedBorderBoxSize(),
            item.is_initial_block_size_indefinite,
            /* override_inline_size */ std::nullopt, flex_line.line_cross_size);
        layout_result = item.block_node.Layout(child_space);
      } else {
        layout_result = item.layout_result;
      }

      const auto& item_style = item.block_node.Style();

      if (should_propagate_row_break_values) {
        auto item_break_before = JoinFragmentainerBreakValues(
            item_style.BreakBefore(), layout_result->InitialBreakBefore());
        auto item_break_after = JoinFragmentainerBreakValues(
            item_style.BreakAfter(), layout_result->FinalBreakAfter());

        // The break-before and break-after values of flex items in a flex row
        // are propagated to the row itself. Accumulate the BreakBetween values
        // for each row ahead of time so that they can be stored on the break
        // token for future use.
        //
        // https://drafts.csswg.org/css-flexbox-1/#pagination
        if (!is_column_) {
          (*row_break_between_outputs)[flex_line_idx] =
              JoinFragmentainerBreakValues(
                  (*row_break_between_outputs)[flex_line_idx],
                  item_break_before);
          (*row_break_between_outputs)[flex_line_idx + 1] =
              JoinFragmentainerBreakValues(
                  (*row_break_between_outputs)[flex_line_idx + 1],
                  item_break_after);
        } else {
          // Treat all columns as a "row" of columns, and accumulate the initial
          // and final break values for all columns, which will be propagated to
          // the container.
          if (item_index == flex_line.item_indices.front()) {
            (*row_break_between_outputs)[0] = JoinFragmentainerBreakValues(
                (*row_break_between_outputs)[0], item_break_before);
          }
          if (item_index == flex_line.item_indices.back()) {
            (*row_break_between_outputs)[1] = JoinFragmentainerBreakValues(
                (*row_break_between_outputs)[1], item_break_after);
          }
        }
      }

      const auto& physical_fragment =
          To<PhysicalBoxFragment>(layout_result->GetPhysicalFragment());
      const LogicalBoxFragment fragment(writing_direction, physical_fragment);
      const LayoutUnit cross_axis_size =
          is_column_ ? fragment.InlineSize() : fragment.BlockSize();

      PhysicalBoxStrut physical_margins = item.initial_margins;
      const PhysicalToFlex<LayoutUnit&> margin(
          writing_direction, is_column_, physical_margins.top,
          physical_margins.right, physical_margins.bottom,
          physical_margins.left);

      const LayoutUnit cross_axis_space = flex_line.line_cross_size -
                                          margin.CrossStart() -
                                          cross_axis_size - margin.CrossEnd();

      // Apply any auto margins.
      {
        const PhysicalToFlex is_margin_auto(writing_direction, is_column_,
                                            item_style.MarginTop().IsAuto(),
                                            item_style.MarginRight().IsAuto(),
                                            item_style.MarginBottom().IsAuto(),
                                            item_style.MarginLeft().IsAuto());

        // Cross-axis margins are handled in the typical way.
        const LayoutUnit margin_space = cross_axis_space.ClampNegativeToZero();
        if (is_margin_auto.CrossStart() && is_margin_auto.CrossEnd()) {
          margin.CrossStart() = margin_space / 2;
          margin.CrossEnd() = margin_space / 2;
        } else if (is_margin_auto.CrossStart()) {
          margin.CrossStart() = margin_space;
        } else if (is_margin_auto.CrossEnd()) {
          margin.CrossEnd() = margin_space;
        }

        // Main-axis margins are distributed to evenly across the whole line.
        if (is_margin_auto.MainStart()) {
          margin.MainStart() = main_axis_auto_margin;
        }
        if (is_margin_auto.MainEnd()) {
          margin.MainEnd() = main_axis_auto_margin;
        }
      }

      // Determine the cross-axis offset based on the item alignment.
      const LayoutUnit cross_axis_offset = ([&]() {
        const bool is_safe =
            !is_webkit_box_ &&
            item_style
                    .ResolvedAlignSelf(
                        {ItemPosition::kStretch, OverflowAlignment::kDefault},
                        &Style())
                    .Overflow() == OverflowAlignment::kSafe;
        const LayoutUnit space =
            is_safe ? cross_axis_space.ClampNegativeToZero() : cross_axis_space;

        LayoutUnit offset;
        switch (item.alignment) {
          case ItemPosition::kCenter:
            offset = space / 2;
            break;
          case ItemPosition::kFlexStart:
            break;
          case ItemPosition::kFlexEnd:
            offset = space;
            break;
          case ItemPosition::kStretch:
            offset = is_wrap_reverse_ ? space : LayoutUnit();
            break;
          case ItemPosition::kBaseline:
          case ItemPosition::kLastBaseline: {
            const bool is_major = item.baseline_group == BaselineGroup::kMajor;
            const LayoutUnit ascent = BaselineAscent(item, physical_fragment);
            const LayoutUnit max_ascent =
                is_major ? flex_line.major_baseline : flex_line.minor_baseline;
            const LayoutUnit baseline_delta = max_ascent - ascent;
            offset = is_major ? baseline_delta : space - baseline_delta;
            break;
          }
          default:
            NOTREACHED() << "All other values shouldn't be possible.";
        }

        return line_cross_axis_offset + offset + margin.CrossStart();
      })();

      main_axis_offset += margin.MainStart();

      const LogicalOffset offset =
          is_column_ ? LogicalOffset(cross_axis_offset, main_axis_offset)
                     : LogicalOffset(main_axis_offset, cross_axis_offset);

      main_axis_offset += item.FlexedBorderBoxSize() + margin.MainEnd() +
                          space_between_items + gap_between_items_;

      const BoxStrut logical_margins =
          physical_margins.ConvertToLogical(writing_direction);

      if (!InvolvedInBlockFragmentation(container_builder_)) {
        container_builder_.AddResult(*layout_result, offset, logical_margins);
        baseline_accumulator.AccumulateItem(fragment, offset.block_offset,
                                            is_first_line, is_last_line);
      } else {
        // Store the information we need for later if we have fragmentation.
        flex_line.line_items_data.emplace_back(
            item.block_node, item.item_index, offset, item.alignment,
            item.FlexedBorderBoxSize(), logical_margins.block_end,
            fragment.BlockSize(), item.is_initial_block_size_indefinite,
            item.is_used_flex_basis_indefinite,
            layout_result->HasDescendantThatDependsOnPercentageBlockSize());
      }

      if (PropagateFlexItemInfo(item, physical_fragment, physical_margins,
                                flex_line_idx, offset) ==
          LayoutResult::kNeedsRelayoutWithNoChildScrollbarChanges) {
        status = LayoutResult::kNeedsRelayoutWithNoChildScrollbarChanges;
      }
    }

    line_cross_axis_offset +=
        flex_line.line_cross_size + space_between_lines + gap_between_lines_;
  }

  if (auto first_baseline = baseline_accumulator.FirstBaseline())
    container_builder_.SetFirstBaseline(*first_baseline);
  if (auto last_baseline = baseline_accumulator.LastBaseline())
    container_builder_.SetLastBaseline(*last_baseline);

  // TODO(crbug.com/1131352): Avoid control-specific handling.
  if (Node().IsSlider()) {
    DCHECK(!InvolvedInBlockFragmentation(container_builder_));
    container_builder_.ClearBaselines();
  }

  // Signal if we need to relayout with new child scrollbar information.
  return status;
}

LayoutResult::EStatus
FlexLayoutAlgorithm::GiveItemsFinalPositionAndSizeForFragmentation(
    HeapVector<FlexLine>* flex_lines,
    Vector<EBreakBetween>* row_break_between_outputs,
    FlexBreakTokenData::FlexBreakBeforeRow* break_before_row,
    LayoutUnit* total_intrinsic_block_size) {
  DCHECK(InvolvedInBlockFragmentation(container_builder_));
  DCHECK(flex_lines);
  DCHECK(row_break_between_outputs);
  DCHECK(break_before_row);

  FlexItemIterator item_iterator(*flex_lines, GetBreakToken(), is_column_);

  Vector<bool> has_inflow_child_break_inside_line(flex_lines->size(), false);
  bool needs_earlier_break_in_column = false;
  LayoutResult::EStatus status = LayoutResult::kSuccess;
  LayoutUnit fragmentainer_space = FragmentainerSpaceLeftForChildren();

  HeapVector<FlexColumnBreakInfo> column_break_info;
  if (is_column_) {
    column_break_info = HeapVector<FlexColumnBreakInfo>(flex_lines->size());
  }

  LayoutUnit previously_consumed_block_size;
  LayoutUnit offset_in_stitched_container;
  if (IsBreakInside(GetBreakToken())) {
    previously_consumed_block_size = GetBreakToken()->ConsumedBlockSize();
    offset_in_stitched_container = previously_consumed_block_size;

    if (Style().BoxDecorationBreak() == EBoxDecorationBreak::kClone &&
        offset_in_stitched_container != LayoutUnit::Max()) {
      // We want to deal with item offsets that we would have had had we not
      // been fragmented, and then add unused space caused by fragmentation, and
      // then calculate a block-offset relatively to the current fragment. In
      // the slicing box decoration model, that's simply about adding and
      // subtracting previously consumed block-size.
      //
      // For the cloning box decoration model, we need to subtract space used by
      // all cloned box decorations that wouldn't have been there in the slicing
      // model. That is: all box decorations from previous fragments, except the
      // initial block-start decoration of the first fragment.
      int preceding_fragment_count = GetBreakToken()->SequenceNumber() + 1;
      offset_in_stitched_container -=
          preceding_fragment_count * BorderScrollbarPadding().BlockSum() -
          BorderScrollbarPadding().block_start;
    }
  }

  BaselineAccumulator baseline_accumulator(Style());
  bool broke_before_row =
      *break_before_row != FlexBreakTokenData::kNotBreakBeforeRow;
  for (auto entry = item_iterator.NextItem(broke_before_row);
       FlexItemData* flex_item = entry.flex_item;
       entry = item_iterator.NextItem(broke_before_row)) {
    wtf_size_t flex_item_idx = entry.flex_item_idx;
    wtf_size_t flex_line_idx = entry.flex_line_idx;
    FlexLine& flex_line = (*flex_lines)[flex_line_idx];
    const auto* item_break_token = To<BlockBreakToken>(entry.token);
    bool last_item_in_line =
        flex_item_idx == flex_line.line_items_data.size() - 1;

    bool is_first_line = flex_line_idx == 0;
    bool is_last_line = flex_line_idx == flex_lines->size() - 1;

    // A child break in a parallel flow doesn't affect whether we should
    // break here or not. But if the break happened in the same flow, we'll now
    // just finish layout of the fragment. No more siblings should be processed.
    if (!is_column_) {
      if (flex_line_idx != 0 &&
          has_inflow_child_break_inside_line[flex_line_idx - 1])
        break;
    } else {
      // If we are relaying out as a result of an early break, and we have early
      // breaks for more than one column, they will be stored in
      // |additional_early_breaks_|. Keep |early_break_| consistent with that of
      // the current column.
      if (additional_early_breaks_ &&
          flex_line_idx < additional_early_breaks_->size())
        early_break_ = (*additional_early_breaks_)[flex_line_idx];
      else if (early_break_ && flex_line_idx != 0)
        early_break_ = nullptr;

      if (has_inflow_child_break_inside_line[flex_line_idx]) {
        if (!last_item_in_line)
          item_iterator.NextLine();
        continue;
      }
    }

    LayoutUnit row_block_offset =
        !is_column_ ? flex_line.cross_axis_offset : LayoutUnit();
    const LogicalOffset original_offset = flex_item->offset;
    LogicalOffset offset = original_offset;

    // If a row or item broke before, subsequent items and lines need to be
    // adjusted by the expansion amount.
    LayoutUnit individual_item_adjustment;
    if (item_break_token && item_break_token->IsBreakBefore()) {
      if (item_break_token->IsForcedBreak()) {
        // We had previously updated the adjustment to subtract out the total
        // consumed block size up to the break. Now add the total consumed
        // block size in the previous fragmentainer to get the total amount
        // the item or row expanded by. This allows for things like margins
        // and alignment offsets to not get sliced by a forced break.
        flex_line.item_offset_adjustment += offset_in_stitched_container;
      } else if (!is_column_ && flex_item_idx == 0 && broke_before_row) {
        // If this is the first time we are handling a break before a row,
        // adjust the offset of items in the row to accommodate the break. The
        // following cases need to be considered:
        //
        // 1. If we are not the first line in the container, and the previous
        // sibling row overflowed the fragmentainer in the block axis, flex
        // items in the current row should be adjusted upward in the block
        // direction to account for the overflowed content.
        //
        // 2. Otherwise, the current row gap should be decreased by the amount
        // of extra space in the previous fragmentainer remaining after the
        // block-end of the previous row. The reason being that we should not
        // clamp row gaps between breaks, similarly to how flex item margins are
        // handled during fragmentation.
        //
        // 3. If the entire row gap was accounted for in the previous
        // fragmentainer, the block-offsets of the flex items in the current row
        // will need to be adjusted downward in the block direction to
        // accommodate the extra space consumed by the container.
        if (*break_before_row == FlexBreakTokenData::kAtStartOfBreakBeforeRow) {
          // Calculate the amount of space remaining in the previous
          // fragmentainer after the block-end of the previous flex row, if any.
          LayoutUnit previous_row_end =
              is_first_line ? LayoutUnit()
                            : (*flex_lines)[flex_line_idx - 1].LineCrossEnd();
          LayoutUnit previous_fragmentainer_unused_space =
              (offset_in_stitched_container - previous_row_end)
                  .ClampNegativeToZero();

          // If there was any remaining space after the previous flex line,
          // determine how much of the row gap was consumed in the previous
          // fragmentainer, if any.
          LayoutUnit consumed_row_gap;
          if (previous_fragmentainer_unused_space) {
            LayoutUnit total_row_block_offset =
                row_block_offset + flex_line.item_offset_adjustment;
            LayoutUnit row_gap = total_row_block_offset - previous_row_end;
            DCHECK_GE(row_gap, LayoutUnit());
            consumed_row_gap =
                std::min(row_gap, previous_fragmentainer_unused_space);
          }

          // Adjust the item offsets to account for any overflow or consumed row
          // gap in the previous fragmentainer.
          LayoutUnit row_adjustment = offset_in_stitched_container -
                                      previous_row_end - consumed_row_gap;
          flex_line.item_offset_adjustment += row_adjustment;
        }
      } else {
        LayoutUnit total_item_block_offset =
            offset.block_offset + flex_line.item_offset_adjustment;
        individual_item_adjustment =
            (offset_in_stitched_container - total_item_block_offset)
                .ClampNegativeToZero();
        // For items in a row, the offset adjustment due to a break before
        // should only apply to the item itself and not to the entire row.
        if (is_column_) {
          flex_line.item_offset_adjustment += individual_item_adjustment;
        }
      }
    }

    if (IsBreakInside(item_break_token)) {
      offset.block_offset = BorderScrollbarPadding().block_start;
    } else if (IsBreakInside(GetBreakToken())) {
      // Convert block offsets from stitched coordinate system offsets to being
      // relative to the current fragment. Include space taken up by any cloned
      // block-start decorations (i.e. exclude it from the adjustment).
      LayoutUnit offset_adjustment = offset_in_stitched_container -
                                     flex_line.item_offset_adjustment -
                                     BorderScrollbarPadding().block_start;

      offset.block_offset -= offset_adjustment;
      if (!is_column_) {
        offset.block_offset += individual_item_adjustment;
        row_block_offset -= offset_adjustment;
      }
    }

    const EarlyBreak* early_break_in_child = nullptr;
    if (early_break_) [[unlikely]] {
      if (!is_column_)
        container_builder_.SetLineCount(flex_line_idx);
      if (IsEarlyBreakTarget(*early_break_, container_builder_,
                             flex_item->block_node)) {
        container_builder_.AddBreakBeforeChild(flex_item->block_node,
                                               kBreakAppealPerfect,
                                               /* is_forced_break */ false);
        if (early_break_->Type() == EarlyBreak::kLine) {
          *break_before_row = FlexBreakTokenData::kAtStartOfBreakBeforeRow;
        }
        ConsumeRemainingFragmentainerSpace(offset_in_stitched_container,
                                           &flex_line);
        // For column flex containers, continue to the next column. For rows,
        // continue until we've processed all items in the current row.
        has_inflow_child_break_inside_line[flex_line_idx] = true;
        if (is_column_) {
          if (!last_item_in_line)
            item_iterator.NextLine();
        } else if (last_item_in_line) {
          DCHECK_EQ(status, LayoutResult::kSuccess);
          break;
        }
        last_line_idx_to_process_first_child_ = flex_line_idx;
        continue;
      } else {
        early_break_in_child =
            EnterEarlyBreakInChild(flex_item->block_node, *early_break_);
      }
    }

    // If we are re-laying out one or more rows with an updated cross-size,
    // adjust the row info to reflect this change (but only if this is the first
    // time we are processing the current row in this layout pass).
    if (cross_size_adjustments_) [[unlikely]] {
      DCHECK(!is_column_);
      // Maps don't allow keys of 0, so adjust the index by 1.
      if (cross_size_adjustments_->Contains(flex_line_idx + 1) &&
          (last_line_idx_to_process_first_child_ == kNotFound ||
           last_line_idx_to_process_first_child_ < flex_line_idx)) {
        LayoutUnit row_block_size_adjustment =
            cross_size_adjustments_->find(flex_line_idx + 1)->value;
        flex_line.line_cross_size += row_block_size_adjustment;

        // Adjust any subsequent row offsets to reflect the current row's new
        // size.
        AdjustOffsetForNextLine(flex_lines, flex_line_idx,
                                row_block_size_adjustment);
      }
    }

    std::optional<LayoutUnit> line_cross_size_for_stretch =
        DoesItemStretch(flex_item->block_node, flex_item->alignment)
            ? std::optional<LayoutUnit>(flex_line.line_cross_size)
            : std::nullopt;

    // If an item broke, its offset may have expanded (as the result of a
    // current or previous break before), in which case, we shouldn't expand by
    // the total line cross size. Otherwise, we would continue to expand the row
    // past the block-size of its items.
    if (line_cross_size_for_stretch && !is_column_ && item_break_token) {
      LayoutUnit updated_cross_size_for_stretch =
          line_cross_size_for_stretch.value();
      updated_cross_size_for_stretch -=
          offset_in_stitched_container -
          (original_offset.block_offset + flex_line.item_offset_adjustment) -
          item_break_token->ConsumedBlockSize();

      line_cross_size_for_stretch = updated_cross_size_for_stretch;
    }

    const bool min_block_size_should_encompass_intrinsic_size =
        MinBlockSizeShouldEncompassIntrinsicSize(*flex_item);
    ConstraintSpace child_space = BuildSpaceForLayout(
        flex_item->block_node, flex_item->alignment,
        flex_item->main_axis_final_size,
        flex_item->is_initial_block_size_indefinite,
        /* override_inline_size */ std::nullopt, line_cross_size_for_stretch,
        offset.block_offset, min_block_size_should_encompass_intrinsic_size);
    const LayoutResult* layout_result = flex_item->block_node.Layout(
        child_space, item_break_token, early_break_in_child);

    BreakStatus break_status = BreakStatus::kContinue;
    FlexColumnBreakInfo* current_column_break_info = nullptr;
    if (!early_break_ && GetConstraintSpace().HasBlockFragmentation()) {
      bool has_container_separation = false;
      if (!is_column_) {
        has_container_separation =
            offset.block_offset > row_block_offset &&
            (!item_break_token || (broke_before_row && flex_item_idx == 0 &&
                                   item_break_token->IsBreakBefore()));
        // Don't attempt to break before a row if the fist item is resuming
        // layout. In which case, the row should be resuming layout, as well.
        if (flex_item_idx == 0 &&
            (!item_break_token ||
             (item_break_token->IsBreakBefore() && broke_before_row))) {
          // Rows have no layout result, so if the row breaks before, we
          // will break before the first item in the row instead.
          bool row_container_separation = has_processed_first_line_;
          bool is_first_for_row = !item_break_token || broke_before_row;
          BreakStatus row_break_status = BreakBeforeRowIfNeeded(
              flex_line, row_block_offset,
              (*row_break_between_outputs)[flex_line_idx], flex_line_idx,
              flex_item->block_node, row_container_separation,
              is_first_for_row);
          if (row_break_status == BreakStatus::kBrokeBefore) {
            ConsumeRemainingFragmentainerSpace(offset_in_stitched_container,
                                               &flex_line);
            if (broke_before_row) {
              *break_before_row =
                  FlexBreakTokenData::kPastStartOfBreakBeforeRow;
            } else {
              *break_before_row = FlexBreakTokenData::kAtStartOfBreakBeforeRow;
            }
            DCHECK_EQ(status, LayoutResult::kSuccess);
            break;
          }
          *break_before_row = FlexBreakTokenData::kNotBreakBeforeRow;
          if (row_break_status == BreakStatus::kNeedsEarlierBreak) {
            status = LayoutResult::kNeedsEarlierBreak;
            break;
          }
          DCHECK_EQ(row_break_status, BreakStatus::kContinue);
        }
      } else {
        has_container_separation =
            !item_break_token &&
            ((last_line_idx_to_process_first_child_ != kNotFound &&
              last_line_idx_to_process_first_child_ >= flex_line_idx) ||
             offset.block_offset > LayoutUnit());

        // We may switch back and forth between columns, so we need to make sure
        // to use the break-after for the current column.
        if (flex_lines->size() > 1) {
          current_column_break_info = &column_break_info[flex_line_idx];
          container_builder_.SetPreviousBreakAfter(
              current_column_break_info->break_after);
        }
      }
      break_status = BreakBeforeChildIfNeeded(
          flex_item->block_node, *layout_result,
          FragmentainerOffsetForChildren() + offset.block_offset,
          has_container_separation, !is_column_, current_column_break_info);

      if (current_column_break_info) {
        current_column_break_info->break_after =
            container_builder_.PreviousBreakAfter();
      }
    }

    if (break_status == BreakStatus::kNeedsEarlierBreak) {
      if (current_column_break_info) {
        DCHECK(is_column_);
        DCHECK(current_column_break_info->early_break);
        if (!needs_earlier_break_in_column) {
          needs_earlier_break_in_column = true;
          container_builder_.SetEarlyBreak(
              current_column_break_info->early_break);
        }
        // Keep track of the early breaks for each column.
        AddColumnEarlyBreak(current_column_break_info->early_break,
                            flex_line_idx);
        if (!last_item_in_line)
          item_iterator.NextLine();
        continue;
      }
      status = LayoutResult::kNeedsEarlierBreak;
      break;
    }

    if (break_status == BreakStatus::kBrokeBefore) {
      ConsumeRemainingFragmentainerSpace(offset_in_stitched_container,
                                         &flex_line, current_column_break_info);
      // For column flex containers, continue to the next column. For rows,
      // continue until we've processed all items in the current row.
      has_inflow_child_break_inside_line[flex_line_idx] = true;
      if (is_column_) {
        if (!last_item_in_line)
          item_iterator.NextLine();
      } else if (last_item_in_line) {
        DCHECK_EQ(status, LayoutResult::kSuccess);
        break;
      }
      last_line_idx_to_process_first_child_ = flex_line_idx;
      continue;
    }

    const auto& physical_fragment =
        To<PhysicalBoxFragment>(layout_result->GetPhysicalFragment());

    LogicalBoxFragment fragment(GetConstraintSpace().GetWritingDirection(),
                                physical_fragment);

    bool is_at_block_end = !physical_fragment.GetBreakToken() ||
                           physical_fragment.GetBreakToken()->IsAtBlockEnd();
    LayoutUnit item_block_end = offset.block_offset + fragment.BlockSize();
    if (is_at_block_end) {
      // Only add the block-end margin if the item has reached the end of its
      // content. Then re-set it to avoid adding it more than once.
      item_block_end += flex_item->margin_block_end;
      flex_item->margin_block_end = LayoutUnit();
    } else {
      has_inflow_child_break_inside_line[flex_line_idx] = true;
    }

    // This item may have expanded due to fragmentation. Record how large the
    // shift was (if any). Only do this if the item has completed layout.
    if (is_column_) {
      LayoutUnit cloned_block_decorations;
      if (!is_at_block_end &&
          flex_item->block_node.Style().BoxDecorationBreak() ==
              EBoxDecorationBreak::kClone) {
        cloned_block_decorations = fragment.BoxDecorations().BlockSum();
      }

      // Cloned box decorations grow the border-box size of the flex item. In
      // flex layout, the main-axis size of a flex item is fixed (in the
      // constraint space). Make sure that this fixed size remains correct, by
      // adding cloned box decorations from each fragment.
      flex_item->main_axis_final_size += cloned_block_decorations;

      flex_item->total_remaining_block_size -=
          fragment.BlockSize() - cloned_block_decorations;
      if (flex_item->total_remaining_block_size < LayoutUnit() &&
          !physical_fragment.GetBreakToken()) {
        LayoutUnit expansion = -flex_item->total_remaining_block_size;
        flex_line.item_offset_adjustment += expansion;
      }
    } else if (!cross_size_adjustments_ &&
               !flex_item
                    ->has_descendant_that_depends_on_percentage_block_size) {
      // For rows, keep track of any expansion past the block-end of each
      // row so that we can re-run layout with the new row block-size.
      //
      // Include any cloned block-start box decorations. The line offset is in
      // the imaginary stitched container that we would have had had we not been
      // fragmented, and now we won't actual layout offsets for the current
      // fragment.
      LayoutUnit cloned_block_start_decoration =
          ClonedBlockStartDecoration(container_builder_);

      LayoutUnit line_block_end = flex_line.LineCrossEnd() -
                                  offset_in_stitched_container +
                                  cloned_block_start_decoration;
      if (line_block_end <= fragmentainer_space &&
          line_block_end >= LayoutUnit() &&
          offset_in_stitched_container != LayoutUnit::Max()) {
        LayoutUnit item_expansion;
        if (is_at_block_end) {
          item_expansion = item_block_end - line_block_end;
        } else {
          // We can't use the size of the fragment, as we don't
          // know how large the subsequent fragments will be (and how much
          // they'll expand the row).
          //
          // Instead of using the size of the fragment, expand the row to the
          // rest of the fragmentainer, with an additional epsilon. This epsilon
          // will ensure that we continue layout for children in this row in
          // the next fragmentainer. Without it we'd drop those subsequent
          // fragments.
          item_expansion = (fragmentainer_space - line_block_end).AddEpsilon();
        }

        // If the item expanded past the row, adjust any subsequent row offsets
        // to reflect the expansion.
        if (item_expansion > LayoutUnit()) {
          // Maps don't allow keys of 0, so adjust the index by 1.
          if (row_cross_size_updates_.empty() ||
              !row_cross_size_updates_.Contains(flex_line_idx + 1)) {
            row_cross_size_updates_.insert(flex_line_idx + 1, item_expansion);
            AdjustOffsetForNextLine(flex_lines, flex_line_idx, item_expansion);
          } else {
            auto it = row_cross_size_updates_.find(flex_line_idx + 1);
            CHECK_NE(it, row_cross_size_updates_.end(),
                     base::NotFatalUntil::M130);
            if (item_expansion > it->value) {
              AdjustOffsetForNextLine(flex_lines, flex_line_idx,
                                      item_expansion - it->value);
              it->value = item_expansion;
            }
          }
        }
      }
    }

    if (current_column_break_info) {
      DCHECK(is_column_);
      current_column_break_info->column_intrinsic_block_size =
          std::max(item_block_end,
                   current_column_break_info->column_intrinsic_block_size);
    }

    intrinsic_block_size_ = std::max(item_block_end, intrinsic_block_size_);
    container_builder_.AddResult(*layout_result, offset);
    if (current_column_break_info) {
      current_column_break_info->break_after =
          container_builder_.PreviousBreakAfter();
    }
    baseline_accumulator.AccumulateItem(fragment, offset.block_offset,
                                        is_first_line, is_last_line);
    if (last_item_in_line) {
      if (!has_inflow_child_break_inside_line[flex_line_idx])
        flex_line.has_seen_all_children = true;
      if (!has_processed_first_line_)
        has_processed_first_line_ = true;

      if (!physical_fragment.GetBreakToken() ||
          flex_line.has_seen_all_children) {
        if (flex_line_idx < flex_lines->size() - 1 && !is_column_ &&
            !item_iterator.HasMoreBreakTokens()) {
          // Add the offset adjustment of the current row to the next row so
          // that its items can also be adjusted by previous item expansion.
          // Only do this when the current row has completed layout and
          // the next row hasn't started layout yet.
          (*flex_lines)[flex_line_idx + 1].item_offset_adjustment +=
              flex_line.item_offset_adjustment;
        }
      }
    }
    last_line_idx_to_process_first_child_ = flex_line_idx;
  }

  if (needs_earlier_break_in_column ||
      status == LayoutResult::kNeedsEarlierBreak) {
    return LayoutResult::kNeedsEarlierBreak;
  }

  if (!row_cross_size_updates_.empty()) {
    DCHECK(!is_column_);
    return LayoutResult::kNeedsRelayoutWithRowCrossSizeChanges;
  }

  if (!container_builder_.HasInflowChildBreakInside() &&
      !item_iterator.NextItem(broke_before_row).flex_item) {
    container_builder_.SetHasSeenAllChildren();
  }

  if (auto first_baseline = baseline_accumulator.FirstBaseline())
    container_builder_.SetFirstBaseline(*first_baseline);
  if (auto last_baseline = baseline_accumulator.LastBaseline())
    container_builder_.SetLastBaseline(*last_baseline);

  // Update the |total_intrinsic_block_size_| in case things expanded.
  *total_intrinsic_block_size =
      std::max(*total_intrinsic_block_size,
               intrinsic_block_size_ + previously_consumed_block_size);

  return status;
}

LayoutResult::EStatus FlexLayoutAlgorithm::PropagateFlexItemInfo(
    const FlexItem& flex_item,
    const PhysicalBoxFragment& physical_fragment,
    const PhysicalBoxStrut& physical_margins,
    wtf_size_t flex_line_idx,
    LogicalOffset offset) {
  LayoutResult::EStatus status = LayoutResult::kSuccess;

  if (layout_info_for_devtools_) [[unlikely]] {
    // If this is a "devtools layout", execution speed isn't critical but we
    // have to not adversely affect execution speed of a regular layout.
    PhysicalRect item_rect;
    item_rect.size = physical_fragment.Size();

    LogicalSize logical_flexbox_size =
        LogicalSize(container_builder_.InlineSize(), total_block_size_);
    PhysicalSize flexbox_size = ToPhysicalSize(
        logical_flexbox_size, GetConstraintSpace().GetWritingMode());
    item_rect.offset =
        offset.ConvertToPhysical(GetConstraintSpace().GetWritingDirection(),
                                 flexbox_size, item_rect.size);
    // devtools uses margin box.
    item_rect.Expand(physical_margins);
    DCHECK_GE(layout_info_for_devtools_->lines.size(), 1u);
    DevtoolsFlexInfo::Item item(item_rect,
                                BaselineAscent(flex_item, physical_fragment));
    layout_info_for_devtools_->lines[flex_line_idx].items.push_back(item);
  }

  // Detect if the flex-item had its scrollbar state change. If so we need
  // to relayout as the input to the flex algorithm is incorrect.
  if (!ignore_child_scrollbar_changes_) {
    if (flex_item.initial_scrollbars !=
        ComputeScrollbarsForNonAnonymous(flex_item.block_node)) {
      status = LayoutResult::kNeedsRelayoutWithNoChildScrollbarChanges;
    }

    // The flex-item scrollbars may not have changed, but an descendant's
    // scrollbars might have causing the min/max sizes to be incorrect.
    if (flex_item.depends_on_min_max_sizes &&
        flex_item.block_node.GetLayoutBox()->IntrinsicLogicalWidthsDirty()) {
      status = LayoutResult::kNeedsRelayoutWithNoChildScrollbarChanges;
    }
  } else {
    DCHECK_EQ(flex_item.initial_scrollbars,
              ComputeScrollbarsForNonAnonymous(flex_item.block_node));
  }
  return status;
}

MinMaxSizesResult
FlexLayoutAlgorithm::ComputeMinMaxSizeOfMultilineColumnContainer() {
  UseCounter::Count(Node().GetDocument(),
                    WebFeature::kFlexNewColumnWrapIntrinsicSize);
  MinMaxSizes min_max_sizes;
  // The algorithm for determining the max-content width of a column-wrap
  // container is simply: Run layout on the container but give the items an
  // overridden available size, equal to the largest max-content width of any
  // item, when they are laid out. The container's max-content width is then
  // the farthest outer inline-end point of all the items.
  HeapVector<FlexLine> flex_lines;
  PlaceFlexItems(&flex_lines, /* oof_children */ nullptr,
                 /* total_intrinsic_block_size */ nullptr,
                 /* is_computing_multiline_column_intrinsic_size */ true);
  min_max_sizes.min_size = largest_min_content_contribution_;
  if (!flex_lines.empty()) {
    for (const auto& line : flex_lines) {
      min_max_sizes.max_size += line.line_cross_size;
    }
    min_max_sizes.max_size += (flex_lines.size() - 1) * gap_between_lines_;
  }

  DCHECK_GE(min_max_sizes.min_size, 0);
  DCHECK_LE(min_max_sizes.min_size, min_max_sizes.max_size);

  min_max_sizes += BorderScrollbarPadding().InlineSum();

  // This always depends on block constraints because if block constraints
  // change, this flexbox could get a different number of columns.
  return {min_max_sizes, /* depends_on_block_constraints */ true};
}

MinMaxSizesResult FlexLayoutAlgorithm::ComputeMinMaxSizeOfRowContainerV3() {
  MinMaxSizes container_sizes;
  bool depends_on_block_constraints = false;

  // The intrinsic sizing algorithm uses lots of geometry and values from each
  // item (e.g. flex base size, used minimum and maximum sizes including
  // automatic minimum sizing), so re-use |ConstructAndAppendFlexItems| from the
  // layout algorithm, which calculates all that.
  // TODO(dgrogan): As an optimization, We can drop the call to
  // ComputeMinMaxSizes in |ConstructAndAppendFlexItems| during this phase if
  // the flex basis is not definite.
  ConstructAndAppendFlexItems(Phase::kRowIntrinsicSize);

  LayoutUnit largest_outer_min_content_contribution;
  for (const FlexItem& item : flex_items_) {
    const BlockNode& child = item.block_node;

    const ConstraintSpace space =
        BuildSpaceForIntrinsicInlineSize(child, item.alignment);
    MinMaxSizesResult min_max_content_contributions =
        ComputeMinAndMaxContentContribution(Style(), child, space);
    depends_on_block_constraints |=
        min_max_content_contributions.depends_on_block_constraints;

    MinMaxSizes item_final_contribution;
    const LayoutUnit flex_base_size_border_box =
        item.base_content_size + item.main_axis_border_padding;
    const LayoutUnit hypothetical_main_size_border_box =
        item.hypothetical_content_size + item.main_axis_border_padding;

    if (is_multi_line_) {
      const LayoutUnit main_axis_margins =
          is_horizontal_flow_ ? item.initial_margins.HorizontalSum()
                              : item.initial_margins.VerticalSum();
      largest_outer_min_content_contribution = std::max(
          largest_outer_min_content_contribution,
          min_max_content_contributions.sizes.min_size + main_axis_margins);
    } else {
      const LayoutUnit min_contribution =
          min_max_content_contributions.sizes.min_size;
      const bool cant_move = (min_contribution > flex_base_size_border_box &&
                              item.flex_grow == 0.f) ||
                             (min_contribution < flex_base_size_border_box &&
                              item.flex_shrink == 0.f);
      if (cant_move && !item.is_used_flex_basis_indefinite) {
        item_final_contribution.min_size = hypothetical_main_size_border_box;
      } else {
        item_final_contribution.min_size = min_contribution;
      }
    }

    const LayoutUnit max_contribution =
        min_max_content_contributions.sizes.max_size;
    const bool cant_move = (max_contribution > flex_base_size_border_box &&
                            item.flex_grow == 0.f) ||
                           (max_contribution < flex_base_size_border_box &&
                            item.flex_shrink == 0.f);
    if (cant_move && !item.is_used_flex_basis_indefinite) {
      item_final_contribution.max_size = hypothetical_main_size_border_box;
    } else {
      item_final_contribution.max_size = max_contribution;
    }

    container_sizes += item_final_contribution;

    const LayoutUnit main_axis_margins =
        is_horizontal_flow_ ? item.initial_margins.HorizontalSum()
                            : item.initial_margins.VerticalSum();
    container_sizes += main_axis_margins;
  }

  if (!flex_items_.empty()) {
    const LayoutUnit gap_inline_size =
        (flex_items_.size() - 1) * gap_between_items_;
    if (is_multi_line_) {
      container_sizes.min_size = largest_outer_min_content_contribution;
      container_sizes.max_size += gap_inline_size;
    } else {
      DCHECK_EQ(largest_outer_min_content_contribution, LayoutUnit())
          << "largest_outer_min_content_contribution is not filled in for "
             "singleline containers.";
      container_sizes += gap_inline_size;
    }
  }

  // Handle potential weirdness caused by items' negative margins.
#if DCHECK_IS_ON()
  if (container_sizes.max_size < container_sizes.min_size) {
    DCHECK(is_multi_line_)
        << container_sizes
        << " multiline row containers might have max < min due to negative "
           "margins, but singleline containers cannot.";
  }
#endif
  container_sizes.max_size =
      std::max(container_sizes.max_size, container_sizes.min_size);
  container_sizes.Encompass(LayoutUnit());

  container_sizes += BorderScrollbarPadding().InlineSum();
  return MinMaxSizesResult(container_sizes, depends_on_block_constraints);
}

MinMaxSizesResult FlexLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  if (auto result = CalculateMinMaxSizesIgnoringChildren(
          Node(), BorderScrollbarPadding()))
    return *result;

  if (is_column_ && is_multi_line_) {
    return ComputeMinMaxSizeOfMultilineColumnContainer();
  }

  if (RuntimeEnabledFeatures::LayoutFlexNewRowAlgorithmV3Enabled() &&
      !is_column_) {
    return ComputeMinMaxSizeOfRowContainerV3();
  }

  MinMaxSizes sizes;
  bool depends_on_block_constraints = false;

  int number_of_items = 0;
  FlexChildIterator iterator(Node());
  for (BlockNode child = iterator.NextChild(); child;
       child = iterator.NextChild()) {
    if (child.IsOutOfFlowPositioned())
      continue;
    number_of_items++;

    const ConstraintSpace space = BuildSpaceForIntrinsicInlineSize(
        child, ResolvedAlignSelf(child.Style()));
    MinMaxSizesResult child_result =
        ComputeMinAndMaxContentContribution(Style(), child, space);
    BoxStrut child_margins =
        ComputeMarginsFor(space, child.Style(), GetConstraintSpace());
    child_result.sizes += child_margins.InlineSum();

    depends_on_block_constraints |= child_result.depends_on_block_constraints;
    if (is_column_) {
      sizes.min_size = std::max(sizes.min_size, child_result.sizes.min_size);
      sizes.max_size = std::max(sizes.max_size, child_result.sizes.max_size);
    } else {
      sizes.max_size += child_result.sizes.max_size;
      if (is_multi_line_) {
        sizes.min_size = std::max(sizes.min_size, child_result.sizes.min_size);
      } else {
        sizes.min_size += child_result.sizes.min_size;
      }
    }
  }
  if (!is_column_ && number_of_items > 0) {
    LayoutUnit gap_inline_size = (number_of_items - 1) * gap_between_items_;
    sizes.max_size += gap_inline_size;
    if (!is_multi_line_) {
      sizes.min_size += gap_inline_size;
    }
  }
  sizes.max_size = std::max(sizes.max_size, sizes.min_size);

  // Due to negative margins, it is possible that we calculated a negative
  // intrinsic width. Make sure that we never return a negative width.
  sizes.Encompass(LayoutUnit());
  sizes += BorderScrollbarPadding().InlineSum();
  return MinMaxSizesResult(sizes, depends_on_block_constraints);
}

LayoutUnit FlexLayoutAlgorithm::FragmentainerSpaceAvailable(
    LayoutUnit block_offset) const {
  return (FragmentainerSpaceLeftForChildren() - block_offset)
      .ClampNegativeToZero();
}

void FlexLayoutAlgorithm::ConsumeRemainingFragmentainerSpace(
    LayoutUnit offset_in_stitched_container,
    FlexLine* flex_line,
    const FlexColumnBreakInfo* column_break_info) {
  if (To<BlockBreakToken>(container_builder_.LastChildBreakToken())
          ->IsForcedBreak()) {
    // This will be further adjusted by the total consumed block size once we
    // handle the break before in the next fragmentainer. This ensures that the
    // expansion is properly handled in the column balancing pass.
    LayoutUnit intrinsic_block_size = intrinsic_block_size_;
    if (column_break_info) {
      DCHECK(is_column_);
      intrinsic_block_size = column_break_info->column_intrinsic_block_size;
    }

    // Any cloned block-start box decorations shouldn't count here, since we're
    // calculating an offset into the imaginary stitched container that we would
    // have had had we not been fragmented. The space taken up by a cloned
    // border is unavailable to child content (flex items in this case).
    LayoutUnit cloned_block_start_decoration =
        ClonedBlockStartDecoration(container_builder_);

    flex_line->item_offset_adjustment -= intrinsic_block_size +
                                         offset_in_stitched_container -
                                         cloned_block_start_decoration;
  }

  if (!GetConstraintSpace().HasKnownFragmentainerBlockSize()) {
    return;
  }
  // The remaining part of the fragmentainer (the unusable space for child
  // content, due to the break) should still be occupied by this container.
  intrinsic_block_size_ += FragmentainerSpaceAvailable(intrinsic_block_size_);
}

BreakStatus FlexLayoutAlgorithm::BreakBeforeRowIfNeeded(
    const FlexLine& row,
    LayoutUnit row_block_offset,
    EBreakBetween row_break_between,
    wtf_size_t row_index,
    LayoutInputNode child,
    bool has_container_separation,
    bool is_first_for_row) {
  DCHECK(!is_column_);
  DCHECK(InvolvedInBlockFragmentation(container_builder_));

  LayoutUnit fragmentainer_block_offset =
      FragmentainerOffsetForChildren() + row_block_offset;
  LayoutUnit fragmentainer_block_size = FragmentainerCapacityForChildren();

  if (has_container_separation) {
    if (IsForcedBreakValue(GetConstraintSpace(), row_break_between)) {
      BreakBeforeChild(GetConstraintSpace(), child, /*layout_result=*/nullptr,
                       fragmentainer_block_offset, fragmentainer_block_size,
                       kBreakAppealPerfect, /*is_forced_break=*/true,
                       &container_builder_, row.line_cross_size);
      return BreakStatus::kBrokeBefore;
    }
  }

  bool breakable_at_start_of_container = IsBreakableAtStartOfResumedContainer(
      GetConstraintSpace(), container_builder_, is_first_for_row);
  BreakAppeal appeal_before = CalculateBreakAppealBefore(
      GetConstraintSpace(), LayoutResult::EStatus::kSuccess, row_break_between,
      has_container_separation, breakable_at_start_of_container);

  // Attempt to move past the break point, and if we can do that, also assess
  // the appeal of breaking there, even if we didn't.
  if (MovePastRowBreakPoint(
          appeal_before, fragmentainer_block_offset, row.line_cross_size,
          row_index, has_container_separation, breakable_at_start_of_container))
    return BreakStatus::kContinue;

  // We're out of space. Figure out where to insert a soft break. It will either
  // be before this row, or before an earlier sibling, if there's a more
  // appealing breakpoint there.
  if (!AttemptSoftBreak(GetConstraintSpace(), child,
                        /*layout_result=*/nullptr, fragmentainer_block_offset,
                        fragmentainer_block_size, appeal_before,
                        &container_builder_, row.line_cross_size)) {
    return BreakStatus::kNeedsEarlierBreak;
  }

  return BreakStatus::kBrokeBefore;
}

bool FlexLayoutAlgorithm::MovePastRowBreakPoint(
    BreakAppeal appeal_before,
    LayoutUnit fragmentainer_block_offset,
    LayoutUnit row_block_size,
    wtf_size_t row_index,
    bool has_container_separation,
    bool breakable_at_start_of_container) {
  if (!GetConstraintSpace().HasKnownFragmentainerBlockSize()) {
    // We only care about soft breaks if we have a fragmentainer block-size.
    // During column balancing this may be unknown.
    return true;
  }

  LayoutUnit space_left =
      FragmentainerCapacityForChildren() - fragmentainer_block_offset;

  // If the row starts past the end of the fragmentainer, we must break before
  // it.
  bool must_break_before = false;
  if (space_left < LayoutUnit()) {
    must_break_before = true;
  } else if (space_left == LayoutUnit()) {
    // If the row starts exactly at the end, we'll allow the row here if the
    // row has zero block-size. Otherwise we have to break before it.
    must_break_before = row_block_size != LayoutUnit();
  }
  if (must_break_before) {
#if DCHECK_IS_ON()
    bool refuse_break_before = space_left >= FragmentainerCapacityForChildren();
    DCHECK(!refuse_break_before);
#endif
    return false;
  }

  // Update the early break in case breaking before the row ends up being the
  // most appealing spot to break.
  if ((has_container_separation || breakable_at_start_of_container) &&
      (!container_builder_.HasEarlyBreak() ||
       appeal_before >= container_builder_.GetEarlyBreak().GetBreakAppeal())) {
    container_builder_.SetEarlyBreak(
        MakeGarbageCollected<EarlyBreak>(row_index, appeal_before));
  }

  // Avoiding breaks inside a row will be handled at the item level.
  return true;
}

void FlexLayoutAlgorithm::AddColumnEarlyBreak(EarlyBreak* breakpoint,
                                              wtf_size_t index) {
  DCHECK(is_column_);
  while (column_early_breaks_.size() <= index)
    column_early_breaks_.push_back(nullptr);
  column_early_breaks_[index] = breakpoint;
}

void FlexLayoutAlgorithm::AdjustOffsetForNextLine(
    HeapVector<FlexLine>* flex_lines,
    wtf_size_t flex_line_idx,
    LayoutUnit item_expansion) const {
  DCHECK_LT(flex_line_idx, flex_lines->size());
  if (flex_line_idx == flex_lines->size() - 1) {
    return;
  }
  (*flex_lines)[flex_line_idx + 1].item_offset_adjustment += item_expansion;
}

const LayoutResult* FlexLayoutAlgorithm::RelayoutWithNewRowSizes() {
  // We shouldn't update the row cross-sizes more than once per fragmentainer.
  DCHECK(!cross_size_adjustments_);

  // There should be no more than two row expansions per fragmentainer.
  DCHECK(!row_cross_size_updates_.empty());
  DCHECK_LE(row_cross_size_updates_.size(), 2u);

  LayoutAlgorithmParams params(Node(),
                               container_builder_.InitialFragmentGeometry(),
                               GetConstraintSpace(), GetBreakToken(),
                               early_break_, additional_early_breaks_);
  FlexLayoutAlgorithm algorithm_with_row_cross_sizes(params,
                                                     &row_cross_size_updates_);
  auto& new_builder = algorithm_with_row_cross_sizes.container_builder_;
  new_builder.SetBoxType(container_builder_.GetBoxType());
  algorithm_with_row_cross_sizes.ignore_child_scrollbar_changes_ =
      ignore_child_scrollbar_changes_;

  // We may have aborted layout due to an early break previously. Ensure that
  // the builder detects the correct space shortage, if so.
  if (early_break_) {
    new_builder.PropagateSpaceShortage(
        container_builder_.MinimalSpaceShortage());
  }
  return algorithm_with_row_cross_sizes.Layout();
}

// We are interested in cases where the flex item *may* expand due to
// fragmentation (lines pushed down by a fragmentation line, etc).
bool FlexLayoutAlgorithm::MinBlockSizeShouldEncompassIntrinsicSize(
    const FlexItemData& item) const {
  // If this item has (any) descendant that is percentage based, we can end
  // up in a situation where we'll constantly try and expand the row. E.g.
  // <div style="display: flex;">
  //   <div style="min-height: 100px;">
  //     <div style="height: 200%;"></div>
  //   </div>
  // </div>
  if (item.has_descendant_that_depends_on_percentage_block_size)
    return false;

  if (item.block_node.IsMonolithic()) {
    return false;
  }

  const auto& item_style = item.block_node.Style();

  // NOTE: We currently assume that writing-mode roots are monolithic, but
  // this may change in the future.
  DCHECK_EQ(GetConstraintSpace().GetWritingDirection().GetWritingMode(),
            item_style.GetWritingMode());

  if (is_column_) {
    bool can_shrink = item_style.ResolvedFlexShrink(Style()) != 0.f &&
                      ChildAvailableSize().block_size != kIndefiniteSize;

    // Only allow growth if the item can't shrink and the flex-basis is
    // content-based.
    if (item.is_used_flex_basis_indefinite && !can_shrink) {
      return true;
    }

    // Only allow growth if the item's block-size is auto and either the item
    // can't shrink or its min-height is auto.
    if (item_style.LogicalHeight().HasAutoOrContentOrIntrinsic() &&
        (!can_shrink || ShouldApplyAutoMinSize(item.block_node))) {
      return true;
    }
  } else {
    // Don't grow if the item's block-size should be the same as its container.
    if (WillChildCrossSizeBeContainerCrossSize(item.block_node,
                                               item.alignment) &&
        !Style().LogicalHeight().HasAutoOrContentOrIntrinsic()) {
      return false;
    }

    // Only allow growth if the item's cross size is auto.
    if (DoesItemComputedCrossSizeHaveAuto(item.block_node)) {
      return true;
    }
  }
  return false;
}

}  // namespace blink
