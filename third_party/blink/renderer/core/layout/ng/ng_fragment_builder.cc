// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_fragment_builder.h"

#include "base/containers/contains.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"

namespace blink {

namespace {

bool IsInlineContainerForNode(const NGBlockNode& node,
                              const LayoutObject* inline_container) {
  return inline_container && inline_container->IsLayoutInline() &&
         inline_container->CanContainOutOfFlowPositionedElement(
             node.Style().GetPosition());
}

NGLogicalAnchorQuery::SetOptions AnchorQuerySetOptions(
    const NGPhysicalFragment& fragment,
    const NGLayoutInputNode& container,
    bool maybe_out_of_order_if_oof) {
  // If the |fragment| is not absolutely positioned, it's a valid anchor.
  // https://drafts.csswg.org/css-anchor-1/#determining
  if (!fragment.IsOutOfFlowPositioned())
    return NGLogicalAnchorQuery::SetOptions::kValidInOrder;

  // If the OOF |fragment| is not in a block fragmentation context, it's a child
  // of its containing block. Make it invalid.
  DCHECK(fragment.GetLayoutObject());
  if (!maybe_out_of_order_if_oof)
    return NGLogicalAnchorQuery::SetOptions::kInvalid;

  // |container| is null if it's an inline box.
  if (!container.GetLayoutBox())
    return NGLogicalAnchorQuery::SetOptions::kInvalid;

  // If the OOF |fragment| is in a block fragmentation context, it's a child of
  // the fragmentation context root. If its containing block is the |container|,
  // make it invalid.
  const LayoutObject* layout_object = fragment.GetLayoutObject();
  const LayoutObject* containing_block = layout_object->Container();
  DCHECK(containing_block);
  if (containing_block == container.GetLayoutBox())
    return NGLogicalAnchorQuery::SetOptions::kInvalid;
  // Otherwise its containing block is a descendant of the block fragmentation
  // context, so it's valid, but the call order is not in the tree order.
  return NGLogicalAnchorQuery::SetOptions::kValidOutOfOrder;
}

}  // namespace

void NGFragmentBuilder::ReplaceChild(wtf_size_t index,
                                     const NGPhysicalFragment& new_child,
                                     const LogicalOffset offset) {
  DCHECK_LT(index, children_.size());
  children_[index] = NGLogicalLink{std::move(&new_child), offset};
}

NGLogicalAnchorQuery& NGFragmentBuilder::EnsureAnchorQuery() {
  if (!anchor_query_)
    anchor_query_ = MakeGarbageCollected<NGLogicalAnchorQuery>();
  return *anchor_query_;
}

void NGFragmentBuilder::PropagateChildAnchors(
    const NGPhysicalFragment& child,
    const LogicalOffset& child_offset) {
  absl::optional<NGLogicalAnchorQuery::SetOptions> options;
  if (child.IsBox() &&
      (child.Style().AnchorName() || child.IsImplicitAnchor())) {
    // Set the child's `anchor-name` before propagating its descendants', so
    // that ancestors have precedence over their descendants.
    DCHECK(RuntimeEnabledFeatures::CSSAnchorPositioningEnabled());
    LogicalRect rect{child_offset,
                     child.Size().ConvertToLogical(GetWritingMode())};
    options = AnchorQuerySetOptions(
        child, node_, IsBlockFragmentationContextRoot() || HasItems());
    if (child.Style().AnchorName()) {
      EnsureAnchorQuery().Set(child.Style().AnchorName(), child, rect,
                              *options);
    }
    if (child.IsImplicitAnchor())
      EnsureAnchorQuery().Set(child.GetLayoutObject(), child, rect, *options);
  }

  // Propagate any descendants' anchor references.
  if (const NGPhysicalAnchorQuery* anchor_query = child.AnchorQuery()) {
    if (!options) {
      options = AnchorQuerySetOptions(
          child, node_, IsBlockFragmentationContextRoot() || HasItems());
    }
    const WritingModeConverter converter(GetWritingDirection(), child.Size());
    EnsureAnchorQuery().SetFromPhysical(*anchor_query, converter, child_offset,
                                        *options);
  }
}

// Propagate data in |child| to this fragment. The |child| will then be added as
// a child fragment or a child fragment item.
void NGFragmentBuilder::PropagateChildData(
    const NGPhysicalFragment& child,
    LogicalOffset child_offset,
    LogicalOffset relative_offset,
    const NGInlineContainer<LogicalOffset>* inline_container,
    absl::optional<LayoutUnit> adjustment_for_oof_propagation) {
  // Propagate anchors from the |child|. Anchors are in |OutOfFlowData| but the
  // |child| itself may have an anchor.
  PropagateChildAnchors(child, child_offset + relative_offset);

  if (adjustment_for_oof_propagation &&
      child.NeedsOOFPositionedInfoPropagation()) {
    PropagateOOFPositionedInfo(child, child_offset, relative_offset,
                               /* offset_adjustment */ LogicalOffset(),
                               inline_container,
                               *adjustment_for_oof_propagation);
  }

  // We only need to report if inflow or floating elements depend on the
  // percentage resolution block-size. OOF-positioned children resolve their
  // percentages against the "final" size of their parent.
  if (!has_descendant_that_depends_on_percentage_block_size_) {
    if (child.DependsOnPercentageBlockSize() && !child.IsOutOfFlowPositioned())
      has_descendant_that_depends_on_percentage_block_size_ = true;

    // We may have a child which has the following style:
    // <div style="position: relative; top: 50%;"></div>
    // We need to mark this as depending on our %-block-size for the its offset
    // to be correctly calculated. This is *slightly* too broad as it only
    // depends on the available block-size, rather than the %-block-size.
    const auto& child_style = child.Style();
    if (child.IsCSSBox() && child_style.GetPosition() == EPosition::kRelative) {
      if (IsHorizontalWritingMode(Style().GetWritingMode())) {
        if (child_style.UsedTop().IsPercentOrCalc() ||
            child_style.UsedBottom().IsPercentOrCalc()) {
          has_descendant_that_depends_on_percentage_block_size_ = true;
        }
      } else {
        if (child_style.UsedLeft().IsPercentOrCalc() ||
            child_style.UsedRight().IsPercentOrCalc()) {
          has_descendant_that_depends_on_percentage_block_size_ = true;
        }
      }
    }
  }

  // Compute |has_floating_descendants_for_paint_| to optimize tree traversal
  // in paint.
  if (!has_floating_descendants_for_paint_) {
    if (child.IsFloating() || child.IsLegacyLayoutRoot() ||
        (child.HasFloatingDescendantsForPaint() &&
         !child.IsPaintedAtomically()))
      has_floating_descendants_for_paint_ = true;
  }

  // The |has_adjoining_object_descendants_| is used to determine if a fragment
  // can be re-used when preceding floats are present.
  // If a fragment doesn't have any adjoining object descendants, and is
  // self-collapsing, it can be "shifted" anywhere.
  if (!has_adjoining_object_descendants_) {
    if (!child.IsFormattingContextRoot() &&
        child.HasAdjoiningObjectDescendants())
      has_adjoining_object_descendants_ = true;
  }

  // Collect any (block) break tokens, but skip break tokens for fragmentainers,
  // as they should only escape a fragmentation context at the discretion of the
  // fragmentation context. Also skip this if there's a pre-set break token, or
  // if we're only to add break tokens manually.
  if (has_block_fragmentation_ && !child.IsFragmentainerBox() &&
      !break_token_ && !should_add_break_tokens_manually_) {
    const NGBreakToken* child_break_token = child.BreakToken();
    switch (child.Type()) {
      case NGPhysicalFragment::kFragmentBox:
        if (child_break_token)
          child_break_tokens_.push_back(child_break_token);
        break;
      case NGPhysicalFragment::kFragmentLineBox:
        const auto* inline_break_token =
            To<NGInlineBreakToken>(child_break_token);
        if (inline_break_token) {
          // TODO(mstensho): Orphans / widows calculation is wrong when regular
          // inline layout gets interrupted by a block-in-inline. We need to
          // reset line_count_ when this happens.
          if (UNLIKELY(inline_break_token->BlockInInlineBreakToken())) {
            if (inline_break_token->BlockInInlineBreakToken()->IsAtBlockEnd()) {
              // We were resuming a block in inline, and we broke again, and
              // we're in a parallel flow. To be resumed in the next
              // fragmentainer.
              child_break_tokens_.push_back(inline_break_token);
              break;
            }
          }
          if (UNLIKELY(inline_break_token->SubBreakTokenInParallelFlow())) {
            // We broke inside a block inside an inline which establised a
            // parallel flow in the current fragmentainer. This creates two
            // inline break tokens - one for the actual inline content to resume
            // in the current fragmentainer, and one for the block-in-inline to
            // resume in the next fragmentainer. Look inside the break token for
            // actual inline layout (it will be picked up and resumed by the
            // current layout algorithm), and take the sub break token with the
            // block-in-inline and add it to the break token list, so that it
            // gets resumed in the next fragmentainer.
            const auto* sub_break_token =
                inline_break_token->SubBreakTokenInParallelFlow();
            DCHECK(sub_break_token->BlockInInlineBreakToken());
            child_break_tokens_.push_back(sub_break_token);
          }
        }

        // We only care about the break token from the last line box added. This
        // is where we'll resume if we decide to block-fragment. Note that
        // child_break_token is nullptr if this is the last line to be generated
        // from the node.
        last_inline_break_token_ = inline_break_token;
        line_count_++;
        break;
    }
  }
}

void NGFragmentBuilder::AddChildInternal(const NGPhysicalFragment* child,
                                         const LogicalOffset& child_offset) {
  // In order to know where list-markers are within the children list (for the
  // |NGSimplifiedLayoutAlgorithm|) we always place them as the first child.
  if (child->IsListMarker()) {
    children_.push_front(NGLogicalLink{std::move(child), child_offset});
    return;
  }

  if (child->IsTextControlPlaceholder()) {
    // ::placeholder should be followed by another block in order to paint
    // ::placeholder earlier.
    const wtf_size_t size = children_.size();
    if (size > 0) {
      children_.insert(size - 1, NGLogicalLink{std::move(child), child_offset});
      return;
    }
  }

  children_.push_back(NGLogicalLink{std::move(child), child_offset});
}

void NGFragmentBuilder::AddOutOfFlowChildCandidate(
    NGBlockNode child,
    const LogicalOffset& child_offset,
    NGLogicalStaticPosition::InlineEdge inline_edge,
    NGLogicalStaticPosition::BlockEdge block_edge) {
  DCHECK(child);
  oof_positioned_candidates_.emplace_back(
      child, NGLogicalStaticPosition{child_offset, inline_edge, block_edge},
      NGInlineContainer<LogicalOffset>());
}

void NGFragmentBuilder::AddOutOfFlowChildCandidate(
    const NGLogicalOutOfFlowPositionedNode& candidate) {
  oof_positioned_candidates_.emplace_back(candidate);
}

void NGFragmentBuilder::AddOutOfFlowInlineChildCandidate(
    NGBlockNode child,
    const LogicalOffset& child_offset,
    TextDirection inline_container_direction) {
  DCHECK(node_.IsInline() || layout_object_->IsLayoutInline());

  // As all inline-level fragments are built in the line-logical coordinate
  // system (Direction() is kLtr), we need to know the direction of the
  // parent element to correctly determine an OOF childs static position.
  AddOutOfFlowChildCandidate(child, child_offset,
                             IsLtr(inline_container_direction)
                                 ? NGLogicalStaticPosition::kInlineStart
                                 : NGLogicalStaticPosition::kInlineEnd,
                             NGLogicalStaticPosition::kBlockStart);
}

void NGFragmentBuilder::AddOutOfFlowFragmentainerDescendant(
    const NGLogicalOOFNodeForFragmentation& descendant) {
  oof_positioned_fragmentainer_descendants_.push_back(descendant);
}

void NGFragmentBuilder::AddOutOfFlowFragmentainerDescendant(
    const NGLogicalOutOfFlowPositionedNode& descendant) {
  DCHECK(!descendant.is_for_fragmentation);
  NGLogicalOOFNodeForFragmentation fragmentainer_descendant(descendant);
  AddOutOfFlowFragmentainerDescendant(fragmentainer_descendant);
}

void NGFragmentBuilder::AddOutOfFlowDescendant(
    const NGLogicalOutOfFlowPositionedNode& descendant) {
  oof_positioned_descendants_.push_back(descendant);
}

void NGFragmentBuilder::SwapOutOfFlowPositionedCandidates(
    HeapVector<NGLogicalOutOfFlowPositionedNode>* candidates) {
  DCHECK(candidates->empty());
  std::swap(oof_positioned_candidates_, *candidates);
}

void NGFragmentBuilder::AddMulticolWithPendingOOFs(
    const NGBlockNode& multicol,
    NGMulticolWithPendingOOFs<LogicalOffset>* multicol_info) {
  DCHECK(To<LayoutBlockFlow>(multicol.GetLayoutBox())->MultiColumnFlowThread());
  auto it = multicols_with_pending_oofs_.find(multicol.GetLayoutBox());
  if (it != multicols_with_pending_oofs_.end())
    return;
  multicols_with_pending_oofs_.insert(multicol.GetLayoutBox(), multicol_info);
}

void NGFragmentBuilder::SwapMulticolsWithPendingOOFs(
    MulticolCollection* multicols_with_pending_oofs) {
  DCHECK(multicols_with_pending_oofs->empty());
  std::swap(multicols_with_pending_oofs_, *multicols_with_pending_oofs);
}

void NGFragmentBuilder::SwapOutOfFlowFragmentainerDescendants(
    HeapVector<NGLogicalOOFNodeForFragmentation>* descendants) {
  DCHECK(descendants->empty());
  std::swap(oof_positioned_fragmentainer_descendants_, *descendants);
}

void NGFragmentBuilder::TransferOutOfFlowCandidates(
    NGFragmentBuilder* destination_builder,
    LogicalOffset additional_offset,
    const NGMulticolWithPendingOOFs<LogicalOffset>* multicol) {
  for (auto& candidate : oof_positioned_candidates_) {
    NGBlockNode node = candidate.Node();
    candidate.static_position.offset += additional_offset;
    if (multicol && multicol->fixedpos_containing_block.Fragment() &&
        node.Style().GetPosition() == EPosition::kFixed) {
      // A fixedpos containing block was found in |multicol|. Add the fixedpos
      // as a fragmentainer descendant instead.
      DCHECK(!candidate.inline_container.container);
      destination_builder->AddOutOfFlowFragmentainerDescendant(
          {node, candidate.static_position, multicol->fixedpos_inline_container,
           multicol->fixedpos_containing_block,
           multicol->fixedpos_containing_block,
           multicol->fixedpos_inline_container});
      continue;
    }
    destination_builder->AddOutOfFlowChildCandidate(candidate);
  }
  oof_positioned_candidates_.clear();
}

void NGFragmentBuilder::MoveOutOfFlowDescendantCandidatesToDescendants() {
  DCHECK(oof_positioned_descendants_.empty());
  std::swap(oof_positioned_candidates_, oof_positioned_descendants_);

  if (!layout_object_->IsInline())
    return;

  for (auto& candidate : oof_positioned_descendants_) {
    // If we are inside the inline algorithm, (and creating a fragment for a
    // <span> or similar), we may add a child (e.g. an atomic-inline) which has
    // OOF descandants.
    //
    // This checks if the object creating this box will be the container for
    // the given descendant.
    if (!candidate.inline_container.container &&
        IsInlineContainerForNode(candidate.Node(), layout_object_)) {
      candidate.inline_container = NGInlineContainer<LogicalOffset>(
          To<LayoutInline>(layout_object_),
          /* relative_offset */ LogicalOffset());
    }
  }
}

void NGFragmentBuilder::PropagateOOFPositionedInfo(
    const NGPhysicalFragment& fragment,
    LogicalOffset offset,
    LogicalOffset relative_offset,
    LogicalOffset offset_adjustment,
    const NGInlineContainer<LogicalOffset>* inline_container,
    LayoutUnit containing_block_adjustment,
    const NGContainingBlock<LogicalOffset>* containing_block,
    const NGContainingBlock<LogicalOffset>* fixedpos_containing_block,
    const NGInlineContainer<LogicalOffset>* fixedpos_inline_container,
    LogicalOffset additional_fixedpos_offset) {
  // Calling this method without any work to do is expensive, even if it ends up
  // skipping all its parts (probably due to its size). Make sure that we have a
  // reason to be here.
  DCHECK(fragment.NeedsOOFPositionedInfoPropagation());

  LogicalOffset adjusted_offset = offset + offset_adjustment + relative_offset;

  // Collect the child's out of flow descendants.
  const WritingModeConverter converter(GetWritingDirection(), fragment.Size());
  for (const auto& descendant : fragment.OutOfFlowPositionedDescendants()) {
    NGBlockNode node = descendant.Node();
    NGLogicalStaticPosition static_position =
        descendant.StaticPosition().ConvertToLogical(converter);

    NGInlineContainer<LogicalOffset> new_inline_container;
    if (descendant.inline_container.container) {
      new_inline_container.container = descendant.inline_container.container;
      new_inline_container.relative_offset =
          converter.ToLogical(descendant.inline_container.relative_offset,
                              PhysicalSize()) +
          relative_offset;
    } else if (inline_container &&
               IsInlineContainerForNode(node, inline_container->container)) {
      new_inline_container = *inline_container;
    }

    // If an OOF element is inside a fragmentation context, it will be laid out
    // once it reaches the fragmentation context root. However, if such OOF
    // elements have fixedpos descendants, those descendants will not find their
    // containing block if the containing block lives inside the fragmentation
    // context root. In this case, the containing block will be passed in via
    // |fixedpos_containing_block|. If one exists, add the fixedpos as a
    // fragmentainer descendant with the correct containing block and static
    // position. In the case of nested fragmentation, the fixedpos containing
    // block may be in an outer fragmentation context root. In such cases,
    // the fixedpos will be added as a fragmentainer descendant at a later time.
    // However, an |additional_fixedpos_offset| should be applied if one is
    // provided.
    if ((fixedpos_containing_block ||
         additional_fixedpos_offset != LogicalOffset()) &&
        node.Style().GetPosition() == EPosition::kFixed) {
      static_position.offset += additional_fixedpos_offset;
      // Relative offsets should be applied after fragmentation. However, if
      // there is any relative offset that occurrend before the fixedpos reached
      // its containing block, that relative offset should be applied to the
      // static position (before fragmentation).
      static_position.offset +=
          relative_offset - fixedpos_containing_block->RelativeOffset();
      if (fixedpos_inline_container)
        static_position.offset -= fixedpos_inline_container->relative_offset;
      // The containing block for fixed-positioned elements should normally
      // already be laid out, and therefore have a fragment - with one
      // exception: If this is the pagination root, it obviously won't have a
      // fragment, since it hasn't finished layout yet. But we still need to
      // propagate the fixed-positioned descendant, so that it gets laid out
      // inside the fragmentation context (and repeated on every page), instead
      // of becoming a direct child of the LayoutNGView fragment (and thus a
      // sibling of the page fragments).
      if (fixedpos_containing_block &&
          (fixedpos_containing_block->Fragment() || node_.IsPaginatedRoot())) {
        NGInlineContainer<LogicalOffset> new_fixedpos_inline_container;
        if (fixedpos_inline_container)
          new_fixedpos_inline_container = *fixedpos_inline_container;
        AddOutOfFlowFragmentainerDescendant(
            {node, static_position, new_fixedpos_inline_container,
             *fixedpos_containing_block, *fixedpos_containing_block,
             new_fixedpos_inline_container});
        continue;
      }
    }
    static_position.offset += adjusted_offset;

    // |oof_positioned_candidates_| should not have duplicated entries.
    DCHECK(!base::Contains(oof_positioned_candidates_, node,
                           &NGLogicalOutOfFlowPositionedNode::Node));
    oof_positioned_candidates_.emplace_back(node, static_position,
                                            new_inline_container);
  }

  NGFragmentedOutOfFlowData* oof_data = fragment.FragmentedOutOfFlowData();
  if (!oof_data)
    return;
  DCHECK(!oof_data->multicols_with_pending_oofs.empty() ||
         !oof_data->oof_positioned_fragmentainer_descendants.empty());
  const NGPhysicalBoxFragment* box_fragment =
      DynamicTo<NGPhysicalBoxFragment>(&fragment);
  bool is_column_spanner = box_fragment && box_fragment->IsColumnSpanAll();

  if (!oof_data->multicols_with_pending_oofs.empty()) {
    const auto& multicols_with_pending_oofs =
        oof_data->multicols_with_pending_oofs;
    for (auto& multicol : multicols_with_pending_oofs) {
      auto& multicol_info = multicol.value;
      LogicalOffset multicol_offset =
          converter.ToLogical(multicol_info->multicol_offset, PhysicalSize());

      LogicalOffset fixedpos_inline_relative_offset = converter.ToLogical(
          multicol_info->fixedpos_inline_container.relative_offset,
          PhysicalSize());
      NGInlineContainer<LogicalOffset> new_fixedpos_inline_container(
          multicol_info->fixedpos_inline_container.container,
          fixedpos_inline_relative_offset);
      const NGPhysicalFragment* fixedpos_containing_block_fragment =
          multicol_info->fixedpos_containing_block.Fragment();

      AdjustFixedposContainerInfo(box_fragment, relative_offset,
                                  &new_fixedpos_inline_container,
                                  &fixedpos_containing_block_fragment);

      // If a fixedpos containing block was found, the |multicol_offset|
      // should remain relative to the fixedpos containing block. Otherwise,
      // continue to adjust the |multicol_offset| to be relative to the current
      // |fragment|.
      LogicalOffset fixedpos_containing_block_offset;
      LogicalOffset fixedpos_containing_block_rel_offset;
      bool is_inside_column_spanner =
          multicol_info->fixedpos_containing_block.IsInsideColumnSpanner();
      if (fixedpos_containing_block_fragment) {
        fixedpos_containing_block_offset = converter.ToLogical(
            multicol_info->fixedpos_containing_block.Offset(),
            fixedpos_containing_block_fragment->Size());
        fixedpos_containing_block_rel_offset = RelativeInsetToLogical(
            multicol_info->fixedpos_containing_block.RelativeOffset(),
            GetWritingDirection());
        fixedpos_containing_block_rel_offset += relative_offset;
        // We want the fixedpos containing block offset to be the offset from
        // the containing block to the top of the fragmentation context root,
        // such that it includes the block offset contributions of previous
        // fragmentainers. The block contribution from previous fragmentainers
        // has already been applied. As such, avoid unnecessarily adding an
        // additional inline/block offset of any fragmentainers.
        if (!fragment.IsFragmentainerBox())
          fixedpos_containing_block_offset += offset;
        fixedpos_containing_block_offset.block_offset +=
            containing_block_adjustment;

        if (is_column_spanner)
          is_inside_column_spanner = true;
      } else {
        multicol_offset += adjusted_offset;
      }

      // TODO(layout-dev): Adjust any clipped container block-offset. For now,
      // just reset it, rather than passing an incorrect value.
      absl::optional<LayoutUnit> fixedpos_clipped_container_block_offset;

      AddMulticolWithPendingOOFs(
          NGBlockNode(multicol.key),
          MakeGarbageCollected<NGMulticolWithPendingOOFs<LogicalOffset>>(
              multicol_offset,
              NGContainingBlock<LogicalOffset>(
                  fixedpos_containing_block_offset,
                  fixedpos_containing_block_rel_offset,
                  fixedpos_containing_block_fragment,
                  fixedpos_clipped_container_block_offset,
                  is_inside_column_spanner,
                  multicol_info->fixedpos_containing_block
                      .RequiresContentBeforeBreaking()),
              new_fixedpos_inline_container));
    }
  }

  PropagateOOFFragmentainerDescendants(
      fragment, offset, relative_offset, containing_block_adjustment,
      containing_block, fixedpos_containing_block);
}

void NGFragmentBuilder::PropagateOOFFragmentainerDescendants(
    const NGPhysicalFragment& fragment,
    LogicalOffset offset,
    LogicalOffset relative_offset,
    LayoutUnit containing_block_adjustment,
    const NGContainingBlock<LogicalOffset>* containing_block,
    const NGContainingBlock<LogicalOffset>* fixedpos_containing_block,
    HeapVector<NGLogicalOOFNodeForFragmentation>* out_list) {
  NGFragmentedOutOfFlowData* oof_data = fragment.FragmentedOutOfFlowData();
  if (!oof_data || oof_data->oof_positioned_fragmentainer_descendants.empty())
    return;

  const WritingModeConverter converter(GetWritingDirection(), fragment.Size());
  const NGPhysicalBoxFragment* box_fragment =
      DynamicTo<NGPhysicalBoxFragment>(&fragment);
  bool is_column_spanner = box_fragment && box_fragment->IsColumnSpanAll();

  auto& out_of_flow_fragmentainer_descendants =
      oof_data->oof_positioned_fragmentainer_descendants;
  wtf_size_t next_idx;
  for (wtf_size_t idx = 0; idx < out_of_flow_fragmentainer_descendants.size();
       idx = next_idx) {
    next_idx = idx + 1;
    const auto& descendant = out_of_flow_fragmentainer_descendants[idx];
    const NGPhysicalFragment* containing_block_fragment =
        descendant.containing_block.Fragment();
    bool container_inside_column_spanner =
        descendant.containing_block.IsInsideColumnSpanner();
    bool fixedpos_container_inside_column_spanner =
        descendant.fixedpos_containing_block.IsInsideColumnSpanner();
    bool remove_descendant = false;

    if (!containing_block_fragment) {
      DCHECK(box_fragment);
      containing_block_fragment = box_fragment;
    } else if (box_fragment && box_fragment->IsFragmentationContextRoot()) {
      // If we find a multicol with OOF positioned fragmentainer descendants,
      // then that multicol is an inner multicol with pending OOFs. Those OOFs
      // will be laid out inside the inner multicol when we reach the
      // outermost fragmentation context, so we should not propagate those
      // OOFs up the tree any further. However, if the containing block is
      // inside a column spanner contained by the current fragmentation root, we
      // should continue to propagate that OOF up the tree so it can be laid out
      // in the next fragmentation context.
      if (container_inside_column_spanner) {
        // Reset the OOF node's column spanner tags so that we don't propagate
        // the OOF past the next fragmentation context root ancestor.
        container_inside_column_spanner = false;
        fixedpos_container_inside_column_spanner = false;
        remove_descendant = true;
      } else {
        DCHECK(!fixedpos_container_inside_column_spanner);
        continue;
      }
    }

    if (is_column_spanner)
      container_inside_column_spanner = true;

    LogicalOffset containing_block_offset =
        converter.ToLogical(descendant.containing_block.Offset(),
                            containing_block_fragment->Size());
    LogicalOffset containing_block_rel_offset = RelativeInsetToLogical(
        descendant.containing_block.RelativeOffset(), GetWritingDirection());
    containing_block_rel_offset += relative_offset;
    if (!fragment.IsFragmentainerBox())
      containing_block_offset += offset;
    containing_block_offset.block_offset += containing_block_adjustment;

    // If the containing block of the OOF is inside a clipped container, update
    // this offset.
    auto UpdatedClippedContainerBlockOffset =
        [&containing_block, &offset, &fragment,
         &containing_block_adjustment](const NGContainingBlock<PhysicalOffset>&
                                           descendant_containing_block) {
          absl::optional<LayoutUnit> clipped_container_offset =
              descendant_containing_block.ClippedContainerBlockOffset();
          if (!clipped_container_offset &&
              fragment.HasNonVisibleBlockOverflow()) {
            // We just found a clipped container.
            clipped_container_offset.emplace();
          }
          if (clipped_container_offset) {
            // We're inside a clipped container. Adjust the offset.
            if (!fragment.IsFragmentainerBox()) {
              *clipped_container_offset += offset.block_offset;
            }
            *clipped_container_offset += containing_block_adjustment;
          }
          if (!clipped_container_offset && containing_block &&
              containing_block->ClippedContainerBlockOffset()) {
            // We were not inside a clipped container, but we're contained by an
            // OOF which is inside one. E.g.: <clipped><relpos><abspos><abspos>
            // This happens when we're at the inner abspos in this example.
            clipped_container_offset =
                containing_block->ClippedContainerBlockOffset();
          }
          return clipped_container_offset;
        };

    absl::optional<LayoutUnit> clipped_container_block_offset =
        UpdatedClippedContainerBlockOffset(descendant.containing_block);

    LogicalOffset inline_relative_offset = converter.ToLogical(
        descendant.inline_container.relative_offset, PhysicalSize());
    NGInlineContainer<LogicalOffset> new_inline_container(
        descendant.inline_container.container, inline_relative_offset);

    // The static position should remain relative to its containing block
    // fragment.
    const WritingModeConverter containing_block_converter(
        GetWritingDirection(), containing_block_fragment->Size());
    NGLogicalStaticPosition static_position =
        descendant.StaticPosition().ConvertToLogical(
            containing_block_converter);

    // The relative offset should be applied after fragmentation. Subtract out
    // the accumulated relative offset from the inline container to the
    // containing block so that it can be re-applied at the correct time.
    if (new_inline_container.container && box_fragment &&
        containing_block_fragment == box_fragment)
      static_position.offset -= inline_relative_offset;

    LogicalOffset fixedpos_inline_relative_offset = converter.ToLogical(
        descendant.fixedpos_inline_container.relative_offset, PhysicalSize());
    NGInlineContainer<LogicalOffset> new_fixedpos_inline_container(
        descendant.fixedpos_inline_container.container,
        fixedpos_inline_relative_offset);
    const NGPhysicalFragment* fixedpos_containing_block_fragment =
        descendant.fixedpos_containing_block.Fragment();

    AdjustFixedposContainerInfo(
        box_fragment, relative_offset, &new_fixedpos_inline_container,
        &fixedpos_containing_block_fragment, &new_inline_container);

    LogicalOffset fixedpos_containing_block_offset;
    LogicalOffset fixedpos_containing_block_rel_offset;
    absl::optional<LayoutUnit> fixedpos_clipped_container_block_offset;
    if (fixedpos_containing_block_fragment) {
      fixedpos_containing_block_offset =
          converter.ToLogical(descendant.fixedpos_containing_block.Offset(),
                              fixedpos_containing_block_fragment->Size());
      fixedpos_containing_block_rel_offset = RelativeInsetToLogical(
          descendant.fixedpos_containing_block.RelativeOffset(),
          GetWritingDirection());
      fixedpos_containing_block_rel_offset += relative_offset;
      if (!fragment.IsFragmentainerBox())
        fixedpos_containing_block_offset += offset;
      fixedpos_containing_block_offset.block_offset +=
          containing_block_adjustment;

      fixedpos_clipped_container_block_offset =
          UpdatedClippedContainerBlockOffset(
              descendant.fixedpos_containing_block);

      if (is_column_spanner)
        fixedpos_container_inside_column_spanner = true;
    }

    if (!fixedpos_containing_block_fragment && fixedpos_containing_block) {
      fixedpos_containing_block_fragment =
          fixedpos_containing_block->Fragment();
      fixedpos_containing_block_offset = fixedpos_containing_block->Offset();
      fixedpos_containing_block_rel_offset =
          fixedpos_containing_block->RelativeOffset();
    }
    NGLogicalOOFNodeForFragmentation oof_node(
        descendant.Node(), static_position, new_inline_container,
        NGContainingBlock<LogicalOffset>(
            containing_block_offset, containing_block_rel_offset,
            containing_block_fragment, clipped_container_block_offset,
            container_inside_column_spanner,
            descendant.containing_block.RequiresContentBeforeBreaking()),
        NGContainingBlock<LogicalOffset>(
            fixedpos_containing_block_offset,
            fixedpos_containing_block_rel_offset,
            fixedpos_containing_block_fragment,
            fixedpos_clipped_container_block_offset,
            fixedpos_container_inside_column_spanner,
            descendant.fixedpos_containing_block
                .RequiresContentBeforeBreaking()),
        new_fixedpos_inline_container);

    if (out_list) {
      out_list->emplace_back(oof_node);
    } else {
      AddOutOfFlowFragmentainerDescendant(oof_node);

      // Remove any descendants that were propagated to the next fragmentation
      // context root (as a result of a column spanner).
      if (remove_descendant) {
        out_of_flow_fragmentainer_descendants.EraseAt(idx);
        next_idx = idx;
      }
    }
  }
}

void NGFragmentBuilder::AdjustFixedposContainerInfo(
    const NGPhysicalFragment* box_fragment,
    LogicalOffset relative_offset,
    NGInlineContainer<LogicalOffset>* fixedpos_inline_container,
    const NGPhysicalFragment** fixedpos_containing_block_fragment,
    const NGInlineContainer<LogicalOffset>* current_inline_container) const {
  DCHECK(fixedpos_inline_container);
  DCHECK(fixedpos_containing_block_fragment);
  if (!box_fragment)
    return;

  if (!*fixedpos_containing_block_fragment && box_fragment->GetLayoutObject()) {
    if (current_inline_container && current_inline_container->container &&
        current_inline_container->container->CanContainFixedPositionObjects()) {
      *fixedpos_inline_container = *current_inline_container;
      *fixedpos_containing_block_fragment = box_fragment;
    } else if (box_fragment->GetLayoutObject()
                   ->CanContainFixedPositionObjects()) {
      if (!fixedpos_inline_container->container &&
          box_fragment->GetLayoutObject()->IsLayoutInline()) {
        *fixedpos_inline_container = NGInlineContainer<LogicalOffset>(
            To<LayoutInline>(box_fragment->GetLayoutObject()), relative_offset);
      } else {
        *fixedpos_containing_block_fragment = box_fragment;
      }
    } else if (fixedpos_inline_container->container) {
      // Candidates whose containing block is inline are always positioned
      // inside closest parent block flow.
      if (box_fragment->GetLayoutObject() ==
          fixedpos_inline_container->container->ContainingBlock())
        *fixedpos_containing_block_fragment = box_fragment;
    }
  }
}

const NGLayoutResult* NGFragmentBuilder::Abort(NGLayoutResult::EStatus status) {
  return MakeGarbageCollected<NGLayoutResult>(
      NGLayoutResult::NGFragmentBuilderPassKey(), status, this);
}

#if DCHECK_IS_ON()

String NGFragmentBuilder::ToString() const {
  StringBuilder builder;
  builder.AppendFormat("NGFragmentBuilder %.2fx%.2f, Children %u\n",
                       InlineSize().ToFloat(), BlockSize().ToFloat(),
                       children_.size());
  for (auto& child : children_) {
    builder.Append(child.fragment->DumpFragmentTree(
        NGPhysicalFragment::DumpAll & ~NGPhysicalFragment::DumpHeaderText));
  }
  return builder.ToString();
}

#endif

}  // namespace blink
