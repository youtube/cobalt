// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_row_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_child_layout_context.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_layout_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/mathml/mathml_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_operator_element.h"

namespace blink {
namespace {

inline LayoutUnit InlineOffsetForDisplayMathCentering(
    bool is_display_block_math,
    LayoutUnit available_inline_size,
    LayoutUnit max_row_inline_size) {
  if (is_display_block_math) {
    return ((available_inline_size - max_row_inline_size) / 2)
        .ClampNegativeToZero();
  }
  return LayoutUnit();
}

static void DetermineOperatorSpacing(const NGBlockNode& node,
                                     LayoutUnit* lspace,
                                     LayoutUnit* rspace) {
  if (auto properties = GetMathMLEmbellishedOperatorProperties(node)) {
    *lspace = properties->lspace;
    *rspace = properties->rspace;
  }
}

}  // namespace

NGMathRowLayoutAlgorithm::NGMathRowLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params) {
  DCHECK(params.space.IsNewFormattingContext());
  DCHECK(!ConstraintSpace().HasBlockFragmentation());
}

void NGMathRowLayoutAlgorithm::LayoutRowItems(
    ChildrenVector* children,
    LayoutUnit* max_row_block_baseline,
    LogicalSize* row_total_size) {
  const bool should_add_space =
      Node().IsMathRoot() || !GetMathMLEmbellishedOperatorProperties(Node());
  const auto baseline_type = Style().GetFontBaseline();

  // https://w3c.github.io/mathml-core/#dfn-algorithm-for-stretching-operators-along-the-block-axis
  const bool inherits_block_stretch_size_constraint =
      ConstraintSpace().TargetStretchBlockSizes().has_value();
  const bool inherits_inline_stretch_size_constraint =
      !inherits_block_stretch_size_constraint &&
      ConstraintSpace().HasTargetStretchInlineSize();

  NGConstraintSpace::MathTargetStretchBlockSizes stretch_sizes;
  if (!inherits_block_stretch_size_constraint &&
      !inherits_inline_stretch_size_constraint) {
    auto UpdateBlockStretchSizes =
        [&](const NGLayoutResult* result) {
          NGBoxFragment fragment(
              ConstraintSpace().GetWritingDirection(),
              To<NGPhysicalBoxFragment>(result->PhysicalFragment()));
          LayoutUnit ascent = fragment.FirstBaselineOrSynthesize(baseline_type);
          stretch_sizes.ascent = std::max(stretch_sizes.ascent, ascent),
          stretch_sizes.descent =
              std::max(stretch_sizes.descent, fragment.BlockSize() - ascent);
        };

    // "Perform layout without any stretch size constraint on all the items of
    // LNotToStretch."
    bool should_layout_remaining_items_with_zero_block_stretch_size = true;
    for (NGLayoutInputNode child = Node().FirstChild(); child;
         child = child.NextSibling()) {
      if (child.IsOutOfFlowPositioned() ||
          IsBlockAxisStretchyOperator(To<NGBlockNode>(child)))
        continue;
      const auto child_constraint_space = CreateConstraintSpaceForMathChild(
          Node(), ChildAvailableSize(), ConstraintSpace(), child,
          NGCacheSlot::kMeasure);
      const auto* child_layout_result = To<NGBlockNode>(child).Layout(
          child_constraint_space, nullptr /* break_token */);
      UpdateBlockStretchSizes(child_layout_result);
      should_layout_remaining_items_with_zero_block_stretch_size = false;
    }

    if (UNLIKELY(should_layout_remaining_items_with_zero_block_stretch_size)) {
      // "If LNotToStretch is empty, perform layout with stretch size constraint
      // 0 on all the items of LToStretch."
      for (NGLayoutInputNode child = Node().FirstChild(); child;
           child = child.NextSibling()) {
        if (child.IsOutOfFlowPositioned())
          continue;
        DCHECK(IsBlockAxisStretchyOperator(To<NGBlockNode>(child)));
        NGConstraintSpace::MathTargetStretchBlockSizes zero_stretch_sizes;
        const auto child_constraint_space = CreateConstraintSpaceForMathChild(
            Node(), ChildAvailableSize(), ConstraintSpace(), child,
            NGCacheSlot::kMeasure, zero_stretch_sizes);
        const auto* child_layout_result = To<NGBlockNode>(child).Layout(
            child_constraint_space, nullptr /* break_token */);
        UpdateBlockStretchSizes(child_layout_result);
      }
    }
  }

  // Layout in-flow children in a row.
  LayoutUnit inline_offset, max_row_ascent, max_row_descent;
  for (NGLayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    if (child.IsOutOfFlowPositioned()) {
      // TODO(rbuis): OOF should be "where child would have been if not
      // absolutely positioned".
      // Issue: https://github.com/mathml-refresh/mathml/issues/16
      container_builder_.AddOutOfFlowChildCandidate(
          To<NGBlockNode>(child), BorderScrollbarPadding().StartOffset());
      continue;
    }
    NGConstraintSpace child_constraint_space;
    if (inherits_block_stretch_size_constraint &&
        IsBlockAxisStretchyOperator(To<NGBlockNode>(child))) {
      child_constraint_space = CreateConstraintSpaceForMathChild(
          Node(), ChildAvailableSize(), ConstraintSpace(), child,
          NGCacheSlot::kLayout, *ConstraintSpace().TargetStretchBlockSizes());
    } else if (inherits_inline_stretch_size_constraint &&
               IsInlineAxisStretchyOperator(To<NGBlockNode>(child))) {
      child_constraint_space = CreateConstraintSpaceForMathChild(
          Node(), ChildAvailableSize(), ConstraintSpace(), child,
          NGCacheSlot::kLayout, absl::nullopt,
          ConstraintSpace().TargetStretchInlineSize());
    } else if (!inherits_block_stretch_size_constraint &&
               !inherits_inline_stretch_size_constraint &&
               IsBlockAxisStretchyOperator(To<NGBlockNode>(child))) {
      child_constraint_space = CreateConstraintSpaceForMathChild(
          Node(), ChildAvailableSize(), ConstraintSpace(), child,
          NGCacheSlot::kLayout, stretch_sizes);
    } else {
      child_constraint_space = CreateConstraintSpaceForMathChild(
          Node(), ChildAvailableSize(), ConstraintSpace(), child);
    }
    const auto* child_layout_result = To<NGBlockNode>(child).Layout(
        child_constraint_space, nullptr /* break_token */);
    LayoutUnit lspace, rspace;
    if (should_add_space)
      DetermineOperatorSpacing(To<NGBlockNode>(child), &lspace, &rspace);
    const auto& physical_fragment =
        To<NGPhysicalBoxFragment>(child_layout_result->PhysicalFragment());
    NGBoxFragment fragment(ConstraintSpace().GetWritingDirection(),
                           physical_fragment);

    NGBoxStrut margins = ComputeMarginsFor(child_constraint_space,
                                           child.Style(), ConstraintSpace());
    inline_offset += margins.inline_start;

    LayoutUnit ascent =
        margins.block_start + fragment.FirstBaselineOrSynthesize(baseline_type);
    *max_row_block_baseline = std::max(*max_row_block_baseline, ascent);

    // TODO(crbug.com/1125136): take into account italic correction.
    if (should_add_space)
      inline_offset += lspace;

    children->emplace_back(
        To<NGBlockNode>(child), margins,
        LogicalOffset{inline_offset, margins.block_start - ascent},
        std::move(child_layout_result));

    inline_offset += fragment.InlineSize() + margins.inline_end;

    if (should_add_space)
      inline_offset += rspace;

    max_row_ascent = std::max(max_row_ascent, ascent + margins.block_start);
    max_row_descent = std::max(
        max_row_descent, fragment.BlockSize() + margins.block_end - ascent);
    row_total_size->inline_size =
        std::max(row_total_size->inline_size, inline_offset);
  }
  row_total_size->block_size = max_row_ascent + max_row_descent;
}

const NGLayoutResult* NGMathRowLayoutAlgorithm::Layout() {
  DCHECK(!IsBreakInside(BreakToken()));

  bool is_display_block_math =
      Node().IsMathRoot() && (Style().Display() == EDisplay::kBlockMath);

  LogicalSize max_row_size;
  LayoutUnit max_row_block_baseline;

  const LogicalSize border_box_size = container_builder_.InitialBorderBoxSize();

  ChildrenVector children;
  LayoutRowItems(&children, &max_row_block_baseline, &max_row_size);

  // Add children taking into account centering, baseline and
  // border/scrollbar/padding.
  LayoutUnit center_offset = InlineOffsetForDisplayMathCentering(
      is_display_block_math, container_builder_.InlineSize(),
      max_row_size.inline_size);

  LogicalOffset adjust_offset = BorderScrollbarPadding().StartOffset();
  adjust_offset += LogicalOffset{center_offset, max_row_block_baseline};
  for (auto& child_data : children) {
    child_data.offset += adjust_offset;
    container_builder_.AddResult(*child_data.result, child_data.offset);
    child_data.child.StoreMargins(ConstraintSpace(), child_data.margins);
  }

  container_builder_.SetBaselines(adjust_offset.block_offset);

  auto intrinsic_block_size =
      max_row_size.block_size + BorderScrollbarPadding().BlockSum();
  auto block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), BorderPadding(), intrinsic_block_size,
      border_box_size.inline_size);
  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);
  container_builder_.SetFragmentsTotalBlockSize(block_size);

  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), &container_builder_).Run();

  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult NGMathRowLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  if (auto result = CalculateMinMaxSizesIgnoringChildren(
          Node(), BorderScrollbarPadding()))
    return *result;

  MinMaxSizes sizes;
  bool depends_on_block_constraints = false;

  const bool should_add_space =
      Node().IsMathRoot() || !GetMathMLEmbellishedOperatorProperties(Node());

  for (NGLayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    if (child.IsOutOfFlowPositioned())
      continue;
    const auto child_result = ComputeMinAndMaxContentContributionForMathChild(
        Style(), ConstraintSpace(), To<NGBlockNode>(child),
        ChildAvailableSize().block_size);
    sizes += child_result.sizes;

    if (should_add_space) {
      LayoutUnit lspace, rspace;
      DetermineOperatorSpacing(To<NGBlockNode>(child), &lspace, &rspace);
      sizes += lspace + rspace;
    }
    depends_on_block_constraints |= child_result.depends_on_block_constraints;

    // TODO(crbug.com/1125136): take into account italic correction.
  }

  // Due to negative margins, it is possible that we calculated a negative
  // intrinsic width. Make sure that we never return a negative width.
  sizes.Encompass(LayoutUnit());

  DCHECK_LE(sizes.min_size, sizes.max_size);
  sizes += BorderScrollbarPadding().InlineSum();
  return MinMaxSizesResult(sizes, depends_on_block_constraints);
}

}  // namespace blink
