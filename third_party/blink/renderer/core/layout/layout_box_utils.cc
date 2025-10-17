// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_box_utils.h"

#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/pagination_utils.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/tree_traversal_utils.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/heap/collection_support/clear_collection_scope.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

class OutOfFlowDescendant {
  DISALLOW_NEW();

 public:
  OutOfFlowDescendant(LayoutBox& box,
                      PhysicalOffset offset_from_root_fragmentation_context)
      : box_(&box),
        offset_from_root_fragmentation_context_(
            offset_from_root_fragmentation_context) {
    DCHECK(box.IsOutOfFlowPositioned());
  }

  void UpdateLocation() {
    // Need to locate the containing block manually here. OOF fragments in a
    // fragmentation context are direct children of the enclosing fragmentainer,
    // not the actual containing block.
    const LayoutBlock* actual_containing_block = box_->ContainingBlock();
    const PhysicalBoxFragment& first_container_fragment =
        *actual_containing_block->GetPhysicalFragment(0);
    box_->SetLocation(
        offset_from_root_fragmentation_context_ -
        first_container_fragment.OffsetFromRootFragmentationContext());
  }

  void Trace(Visitor* v) const { v->Trace(box_); }

 private:
  Member<LayoutBox> box_;
  PhysicalOffset offset_from_root_fragmentation_context_;
};

using OutOfFlowDescendants = HeapVector<OutOfFlowDescendant>;

void SetChildLocation(const PhysicalBoxFragment& parent_node_fragment,
                      const PhysicalBoxFragment& child_fragment,
                      PhysicalOffset child_fragment_offset,
                      const PhysicalBoxFragment* parent_fragmentainer,
                      PhysicalOffset parent_fragmentainer_offset,
                      OutOfFlowDescendants* oof_descendants) {
  if (!child_fragment.IsFirstForNode()) {
    return;
  }

  auto* child_layout_box =
      To<LayoutBox>(child_fragment.GetMutableLayoutObject());

  if (parent_fragmentainer && child_fragment.IsOutOfFlowPositioned()) {
    // The containing block of an out-of-flow positioned element may be an
    // in-flow one in a subsequent fragmentainer. We therefore need to handle
    // all in-flow elements in the fragmentation context before we can calculate
    // the offset of an OOF relatively to its actual containing block. Store the
    // offset relatively to the root fragmentation context for now, and get back
    // to resolving it properly later.
    oof_descendants->push_back(OutOfFlowDescendant(
        *child_layout_box,
        parent_fragmentainer->OffsetFromRootFragmentationContext() +
            child_fragment_offset));
    return;
  }

  // The offset will be relative to the first fragment of the containing
  // LayoutBox. A fragmentainer doesn't create a LayoutBox, so its offset must
  // be included when the parent node is a fragmentation context root (e.g. a
  // multicol container) and the child is a direct CSS box child.
  PhysicalOffset offset = parent_fragmentainer_offset + child_fragment_offset;

  if (!parent_node_fragment.IsFirstForNode()) {
    // Add the distance between the first container fragment for the node, and
    // this container fragment. Skip this for page areas (which have no layout
    // object, and instead have the accumulated offset correctly baked in).
    if (auto* parent_box =
            To<LayoutBox>(parent_node_fragment.GetLayoutObject())) {
      const PhysicalBoxFragment* first_parent_fragment =
          parent_box->GetPhysicalFragment(0);
      offset += parent_node_fragment.OffsetFromRootFragmentationContext() -
                first_parent_fragment->OffsetFromRootFragmentationContext();
    }
  }

  child_layout_box->SetLocation(offset);

  if (parent_fragmentainer) {
    // If the fragmentainer size has changed without affecting layout of one of
    // its children (because of e.g. fixed size), some of that child's fragments
    // may still need to move. Pre-paint needs this.
    child_layout_box->SetShouldCheckForPaintInvalidation();
  }
}

void UpdateBoxChildLocations(const PhysicalBoxFragment& parent_fragment,
                             OutOfFlowDescendants* oof_descendants) {
  // First walk the inline formatting context, if any.
  if (const FragmentItems* items = parent_fragment.Items()) {
    for (InlineCursor cursor(parent_fragment, *items); cursor;
         cursor.MoveToNext()) {
      const PhysicalBoxFragment* child_box_fragment =
          cursor.Current().BoxFragment();
      if (!child_box_fragment) {
        continue;
      }
      auto* child_box_model_object = DynamicTo<LayoutBoxModelObject>(
          child_box_fragment->GetMutableLayoutObject());
      if (!child_box_model_object) {
        continue;
      }

      if (IsA<LayoutBox>(child_box_model_object)) {
        SetChildLocation(parent_fragment, *child_box_fragment,
                         cursor.Current().OffsetInContainerFragment(),
                         /*parent_fragmentainer=*/nullptr,
                         /*parent_fragmentainer_offset=*/PhysicalOffset(),
                         oof_descendants);
      }
      if (child_box_model_object->HasSelfPaintingLayer()) {
        child_box_model_object->Layer()->SetNeedsVisualOverflowRecalc();
      }
    }
  }

  // Then walk child box fragments.
  for (const auto& child : parent_fragment.Children()) {
    // Skip any line-boxes we have as children. This is handled when walking
    // the fragment items (see above).
    const auto* child_box_fragment =
        DynamicTo<PhysicalBoxFragment>(child.get());
    if (!child_box_fragment) {
      continue;
    }

    if (child_box_fragment->IsColumnBox()) {
      // This is a column (fragmentainer). Walk actual child nodes (inside the
      // fragmentainer).
      for (const auto& grandchild : child_box_fragment->Children()) {
        SetChildLocation(
            parent_fragment, *To<PhysicalBoxFragment>(grandchild.get()),
            grandchild.offset, /*parent_fragmentainer=*/child_box_fragment,
            /*parent_fragmentainer_offset=*/child.offset, oof_descendants);
      }
    } else {
      DCHECK(!child_box_fragment->IsFragmentainerBox());
      const PhysicalBoxFragment* page_area = nullptr;
      if (parent_fragment.IsFragmentainerBox()) {
        // When calculating for pages, each page area is passed as parent
        // fragment.
        DCHECK_EQ(parent_fragment.GetBoxType(), PhysicalFragment::kPageArea);
        page_area = &parent_fragment;
      }
      SetChildLocation(parent_fragment, *child_box_fragment, child.offset,
                       /*parent_fragmentainer=*/page_area,
                       /*parent_fragmentainer_offset=*/PhysicalOffset(),
                       oof_descendants);
    }
  }
}

class TraversalListener : public PhysicalFragmentTraversalListener {
  STACK_ALLOCATED();

 public:
  TraversalListener(const PhysicalBoxFragment& root_fragment,
                    PhysicalOffset start_offset,
                    OutOfFlowDescendants* oof_descendants)
      : oof_descendants_(oof_descendants), accumulated_offset_(start_offset) {
    if (ShouldForceEntireSubtreeUpdate(root_fragment)) {
      force_entire_subtree_update_++;
    }
  }

  static bool ShouldForceEntireSubtreeUpdate(
      const PhysicalBoxFragment& fragment) {
    auto* layout_box = DynamicTo<LayoutBox>(fragment.GetMutableLayoutObject());
    if (layout_box && layout_box->ShouldCheckForPaintInvalidation() &&
        layout_box->IsFragmentationContextRoot()) {
      // It's possible that this is a multicol container that has changed its
      // size, which may affect the column sizes, without necessarily affecting
      // the layout of any descendants (but the individual fragment offsets may
      // change, due to the column size change).
      return true;
    }
    return false;
  }

 private:
  NextStep HandleEntry(const PhysicalBoxFragment& fragment,
                       PhysicalOffset offset,
                       bool is_first_for_node) final {
    if (fragment.IsMonolithic()) {
      // Only nodes that are potentially fragmented need to go through this.
      return kSkipChildren;
    }

    PhysicalOffset new_accumulated_offset = accumulated_offset_ + offset;
    auto mutator = fragment.GetMutableForContainerLayout();
    mutator.SetOffsetFromRootFragmentationContext(new_accumulated_offset);

    const auto* layout_box = DynamicTo<LayoutBox>(fragment.GetLayoutObject());
    if (layout_box && layout_box->ChildLayoutBlockedByDisplayLock()) {
      return kSkipChildren;
    }

    int new_force_entire_subtree_update =
        force_entire_subtree_update_ + ShouldForceEntireSubtreeUpdate(fragment);
    bool update_children = new_force_entire_subtree_update ||
                           fragment.IsFragmentainerBox() ||
                           layout_box->ShouldCheckForPaintInvalidation();
    if (!update_children) {
      return kSkipChildren;
    }

    accumulated_offset_ = new_accumulated_offset;
    force_entire_subtree_update_ = new_force_entire_subtree_update;
    return kContinue;
  }

  void HandleExit(const PhysicalBoxFragment& fragment,
                  PhysicalOffset offset) final {
    // Update the location for children, unless the parent is a fragmentainer.
    // We want locations to be relative to the containing LayoutBox, and a
    // fragmentainer doesn't create a LayoutBox. Its offset will still be taken
    // into account, though. This will be updated when handling the parent of
    // the fragmentainer (i.e. the fragmentation context root).
    if (!fragment.IsFragmentainerBox()) {
      UpdateBoxChildLocations(fragment, oof_descendants_);
    }

    accumulated_offset_ -= offset;
    if (ShouldForceEntireSubtreeUpdate(fragment)) {
      force_entire_subtree_update_--;
      DCHECK_GE(force_entire_subtree_update_, 0);
    }
  }

  OutOfFlowDescendants* oof_descendants_;
  PhysicalOffset accumulated_offset_;

  // If larger than 0, the entire subtree needs to be walked.
  int force_entire_subtree_update_ = 0;
};

void UpdateOffsetsFromRootFragmentationContext(
    const PhysicalBoxFragment& fragment,
    PhysicalOffset offset) {
  // Update the location for all in-flow descendants that participate in this
  // fragmentation context. Collect any out-of-flow positioned descendants for
  // later.
  OutOfFlowDescendants oof_descendants;
  TraversalListener listener(fragment, offset, &oof_descendants);
  ForAllBoxFragmentDescendants(fragment, kFragmentTraversalOptionNone,
                               listener);

  // Update for the root fragment. Collect any out-of-flow positioned children
  // there as well.
  UpdateBoxChildLocations(fragment, &oof_descendants);

  // Finally, walk through all the OOFs collected, and update their location.
  for (OutOfFlowDescendant& descendant : oof_descendants) {
    descendant.UpdateLocation();
  }
}

}  // anonymous namespace

LayoutUnit BoxInlineSize(const LayoutBox& box) {
  DCHECK_GT(box.PhysicalFragmentCount(), 0u);

  // TODO(almaher): We can't assume all fragments will have the same inline
  // size.
  return ToLogicalSize(box.GetPhysicalFragment(0u)->Size(),
                       box.StyleRef().GetWritingMode())
      .inline_size;
}

LayoutUnit BoxTotalBlockSize(const LayoutBox& box) {
  wtf_size_t num_fragments = box.PhysicalFragmentCount();
  DCHECK_GT(num_fragments, 0u);

  // Calculate the total block size by looking at the last two block fragments
  // with a non-zero block-size.
  LayoutUnit total_block_size;
  while (num_fragments > 0) {
    LayoutUnit block_size =
        ToLogicalSize(box.GetPhysicalFragment(num_fragments - 1)->Size(),
                      box.StyleRef().GetWritingMode())
            .block_size;
    if (block_size > LayoutUnit()) {
      total_block_size += block_size;
      break;
    }
    num_fragments--;
  }

  if (num_fragments > 1) {
    total_block_size += box.GetPhysicalFragment(num_fragments - 2)
                            ->GetBreakToken()
                            ->ConsumedBlockSize();
  }
  return total_block_size;
}

DeprecatedLayoutPoint ComputeBoxLocation(
    const PhysicalBoxFragment& child_fragment,
    PhysicalOffset offset,
    const PhysicalBoxFragment& container_fragment,
    const BlockBreakToken* previous_container_break_token) {
  DCHECK(!RuntimeEnabledFeatures::LayoutBoxVisualLocationEnabled());
  if (container_fragment.Style().IsFlippedBlocksWritingMode()) [[unlikely]] {
    // Move the physical offset to the right side of the child fragment,
    // relative to the right edge of the container fragment. This is the
    // block-start offset in vertical-rl, and the legacy engine expects always
    // expects the block offset to be relative to block-start.
    offset.left = container_fragment.Size().width - offset.left -
                  child_fragment.Size().width;
  }

  if (previous_container_break_token) [[unlikely]] {
    // Add the amount of block-size previously (in previous fragmentainers)
    // consumed by the container fragment. This will map the child's offset
    // nicely into the flow thread coordinate system used by the legacy engine.
    LayoutUnit consumed =
        previous_container_break_token->ConsumedBlockSizeForLegacy();
    if (container_fragment.Style().IsHorizontalWritingMode()) {
      offset.top += consumed;
    } else {
      offset.left += consumed;
    }
  }

  return offset.FaultyToDeprecatedLayoutPoint();
}

void UpdateChildLayoutBoxLocations(const PhysicalBoxFragment& fragment) {
  if (fragment.GetLayoutObject()->ChildLayoutBlockedByDisplayLock()) {
    return;
  }

  // We should only be here for nodes that don't participate in a fragmentation
  // context.
  DCHECK(fragment.IsOnlyForNode());

  if (!fragment.IsFragmentationContextRoot()) {
    DCHECK(fragment.IsOnlyForNode());
    UpdateBoxChildLocations(fragment, /*oof_descendants=*/nullptr);
    return;
  }

  // This is a root fragmentation context. It's either a multicol container that
  // does not participate in an outer fragmentation context, or it's a paginated
  // root. Everything inside has been laid out, and we can finally update the
  // locations for all parts of the entire subtree that participate in this
  // fragmentation context.
  if (!fragment.IsPaginatedRoot()) {
    // This is a multicol container.
    UpdateOffsetsFromRootFragmentationContext(fragment, PhysicalOffset());
  } else {
    // This is a paginated root. Offsets in pagination are not web-exposed, but
    // they are needed for PDF URL fragment target rectangles, and also by
    // certain web_tests/printing/ tests.
    const PhysicalBoxFragment* first_page_area = nullptr;
    const BlockBreakToken* previous_break_token = nullptr;
    for (const PhysicalFragmentLink& link : fragment.Children()) {
      const auto& page_container = To<PhysicalBoxFragment>(*link.fragment);
      const auto& page_area = GetPageArea(GetPageBorderBox(page_container));
      if (!first_page_area) {
        first_page_area = &page_area;
      }
      PhysicalRect rect = StitchedPageContentRect(page_area, *first_page_area,
                                                  previous_break_token);
      UpdateOffsetsFromRootFragmentationContext(page_area, rect.offset);
      previous_break_token = page_area.GetBreakToken();
    }
  }
}

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::OutOfFlowDescendant)
