// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_anchor_query_map.h"

#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/ng_logical_link.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

namespace {

// Represents a fragmentainer. This is in the logical coordinate system
// because the size of the fragmentation context may not have determined yet.
// In that case, physical coordinates can't be computed yet.
struct FragmentainerContext {
  STACK_ALLOCATED();

 public:
  LogicalOffset offset;
  // The block offset when all fragments are stitched in the block direction.
  // That is, the sum of block offsets of preceding fragments.
  LayoutUnit stitched_offset;
  WritingModeConverter converter;
};

// This struct is a variation of |NGAnchorReference|, using the stitched
// coordinate system for the block-fragmented out-of-flow positioned objects.
struct NGStitchedAnchorReference
    : public GarbageCollected<NGStitchedAnchorReference> {
  NGStitchedAnchorReference(const NGPhysicalFragment& fragment_ref,
                            const LogicalRect& rect,
                            const FragmentainerContext& fragmentainer)
      : fragment(&fragment_ref),
        rect_in_first_fragmentainer(rect),
        first_fragmentainer_offset(fragmentainer.offset),
        first_fragmentainer_stitched_offset(fragmentainer.stitched_offset) {
    DCHECK(fragment);
  }

  LogicalRect StitchedRect() const {
    LogicalRect stitched_rect = rect_in_first_fragmentainer;
    stitched_rect.offset.block_offset += first_fragmentainer_stitched_offset;
    return stitched_rect;
  }

  NGLogicalAnchorReference* StitchedAnchorReference() const {
    DCHECK(fragment);
    return MakeGarbageCollected<NGLogicalAnchorReference>(
        *fragment, StitchedRect(), /* is_invalid */ false);
  }

  void Unite(const LogicalRect& other_rect,
             const LogicalOffset& fragmentainer_offset) {
    // To unite fragments in the physical coordinate system as defined in the
    // spec while keeping the |reference.rect| relative to the first
    // fragmentainer, make the |fragmentainer_offset| relative to the first
    // fragmentainer.
    const LogicalRect other_rect_in_first_fragmentainer =
        other_rect + (fragmentainer_offset - first_fragmentainer_offset);
    rect_in_first_fragmentainer.Unite(other_rect_in_first_fragmentainer);
  }

  void Trace(Visitor* visitor) const { visitor->Trace(fragment); }

  Member<const NGPhysicalFragment> fragment;
  // The |rect_in_first_fragmentainer| is relative to the first fragmentainer,
  // so that it can a) unite following fragments in the physical coordinate
  // system, and b) compute the result in the stitched coordinate system.
  LogicalRect rect_in_first_fragmentainer;
  LogicalOffset first_fragmentainer_offset;
  // The block offset when all fragments are stitched in the block direction.
  LayoutUnit first_fragmentainer_stitched_offset;
};

// This creates anchor queries in the stitched coordinate system. The result
// can be converted to a |NGLogicalAnchorQuery|.
struct NGStitchedAnchorQuery
    : public GarbageCollected<NGStitchedAnchorQuery>,
      public NGAnchorQueryBase<NGStitchedAnchorReference> {
  using Base = NGAnchorQueryBase<NGStitchedAnchorReference>;

  // Convert |this| to a |NGLogicalAnchorQuery|. The result is a regular
  // |NGLogicalAnchorQuery| except that its coordinate system is stitched
  // (i.e., as if they weren't fragmented.)
  NGLogicalAnchorQuery* StitchedAnchorQuery() const {
    auto* anchor_query = MakeGarbageCollected<NGLogicalAnchorQuery>();
    for (const auto entry : *this)
      anchor_query->Set(entry.key, entry.value->StitchedAnchorReference());
    return anchor_query;
  }

  enum class Conflict {
    // The first entry wins. The calls must be in the tree order.
    kFirstInCallOrder,
    // Overwrite existing entry if the new one is before the existing one.
    kOverwriteIfBefore,
  };

  void AddAnchorQuery(const NGPhysicalFragment& fragment,
                      const PhysicalOffset& offset_from_fragmentainer,
                      const FragmentainerContext& fragmentainer) {
    const NGPhysicalAnchorQuery* anchor_query = fragment.AnchorQuery();
    if (!anchor_query)
      return;
    for (auto entry : *anchor_query) {
      DCHECK(entry.value->fragment);
      AddAnchorReference(entry.key, *entry.value->fragment,
                         entry.value->rect + offset_from_fragmentainer,
                         fragmentainer, Conflict::kFirstInCallOrder);
    }
  }

  void AddAnchorReference(const NGAnchorKey& key,
                          const NGPhysicalFragment& fragment,
                          const PhysicalRect& physical_rect_in_fragmentainer,
                          const FragmentainerContext& fragmentainer,
                          Conflict conflict) {
    const LogicalRect rect_in_fragmentainer =
        fragmentainer.converter.ToLogical(physical_rect_in_fragmentainer);
    auto* new_value = MakeGarbageCollected<NGStitchedAnchorReference>(
        fragment, rect_in_fragmentainer, fragmentainer);
    const auto result = Base::insert(key, new_value);
    if (result.is_new_entry)
      return;

    // If this is a fragment of the existing box, unite it with other fragments.
    NGStitchedAnchorReference* existing = *result.stored_value;
    const LayoutObject* existing_object = existing->fragment->GetLayoutObject();
    DCHECK(existing_object);
    const LayoutObject* new_object = new_value->fragment->GetLayoutObject();
    DCHECK(new_object);
    if (existing_object == new_object) {
      existing->Unite(rect_in_fragmentainer, fragmentainer.offset);
      return;
    }

    // If this is the same anchor-name on a different box, the first one in the
    // pre-order wins. Normally, the call order is in the layout-order, which is
    // pre-order of the box tree. But OOFs may be laid out later, check the tree
    // order in such case.
    switch (conflict) {
      case Conflict::kFirstInCallOrder:
        DCHECK(existing_object->IsBeforeInPreOrder(*new_object));
        break;
      case Conflict::kOverwriteIfBefore:
        if (new_object->IsBeforeInPreOrder(*existing_object))
          *existing = *new_value;
        break;
    }
  }
};

// This collects |NGStitchedAnchorQuery| for each containing block.
struct NGStitchedAnchorQueries {
  STACK_ALLOCATED();

 public:
  NGStitchedAnchorQueries(const LayoutBox& root,
                          const HeapHashSet<Member<const LayoutObject>>&
                              anchored_oof_containers_and_ancestors)
      : anchored_oof_containers_and_ancestors_(
            anchored_oof_containers_and_ancestors),
        root_(root) {}

  void AddChildren(base::span<const NGLogicalLink> children,
                   const NGFragmentItemsBuilder::ItemWithOffsetList* items,
                   const WritingModeConverter& converter) {
    const FragmentainerContext fragmentainer{{}, {}, converter};
    if (items) {
      for (const NGFragmentItemsBuilder::ItemWithOffset& item_with_offset :
           *items) {
        const NGFragmentItem& item = item_with_offset.item;
        if (const NGPhysicalBoxFragment* fragment = item.BoxFragment()) {
          AddBoxChild(*fragment, item.OffsetInContainerFragment(),
                      fragmentainer);
        }
      }
    }

    for (const NGLogicalLink& child : children) {
      DCHECK(!child->IsFragmentainerBox());
      DCHECK(!child->IsColumnSpanAll());
      const PhysicalOffset child_offset =
          converter.ToPhysical(child.offset, child->Size());
      AddChild(*child, child_offset, fragmentainer);
    }
  }

  void AddFragmentainerChildren(base::span<const NGLogicalLink> children,
                                WritingDirectionMode writing_direction) {
    LayoutUnit fragmentainer_stitched_offset;
    for (const NGLogicalLink& child : children) {
      if (child->IsFragmentainerBox()) {
        const FragmentainerContext fragmentainer{
            child.offset,
            fragmentainer_stitched_offset,
            {writing_direction, child->Size()}};
        AddChild(*child, /* offset_from_fragmentainer */ {}, fragmentainer);
        fragmentainer_stitched_offset +=
            child->Size()
                .ConvertToLogical(writing_direction.GetWritingMode())
                .block_size;
        continue;
      }

      // The containing block of the spanner is the multicol container itself.
      // https://drafts.csswg.org/css-multicol/#column-span
      // So anchor queries in column spanners should not be added to any
      // containing blocks in the multicol.
      DCHECK(child->IsColumnSpanAll());
    }
  }

  void AddChild(const NGPhysicalFragment& fragment,
                const PhysicalOffset& offset_from_fragmentainer,
                const FragmentainerContext& fragmentainer) {
    if (const auto* box = DynamicTo<NGPhysicalBoxFragment>(&fragment))
      AddBoxChild(*box, offset_from_fragmentainer, fragmentainer);
  }

  void AddBoxChild(const NGPhysicalBoxFragment& fragment,
                   const PhysicalOffset& offset_from_fragmentainer,
                   const FragmentainerContext& fragmentainer) {
    if (fragment.IsOutOfFlowPositioned()) {
      AddOutOfFlowChild(fragment, offset_from_fragmentainer, fragmentainer);
      return;
    }

    // Return early if the |fragment| doesn't have any anchors. No need to
    // traverse descendants.
    const NGPhysicalAnchorQuery* anchor_query = fragment.AnchorQuery();
    if (!anchor_query)
      return;

    // Create |NGStitchedAnchorQuery| if this is a containing block.
    if (const LayoutObject* layout_object = fragment.GetLayoutObject()) {
      if (!anchored_oof_containers_and_ancestors_.Contains(layout_object))
        return;
      if (layout_object->CanContainAbsolutePositionObjects() ||
          layout_object->CanContainFixedPositionObjects()) {
        EnsureStitchedAnchorQuery(*layout_object)
            .AddAnchorQuery(fragment, offset_from_fragmentainer, fragmentainer);
      }
    }

    if (fragment.IsFragmentationContextRoot()) {
      AddFragmentationContextRootChild(fragment, offset_from_fragmentainer,
                                       fragmentainer);
      return;
    }

    // Add inline children if any.
    if (const NGFragmentItems* items = fragment.Items()) {
      for (NGInlineCursor cursor(fragment, *items); cursor;
           cursor.MoveToNext()) {
        if (cursor.Current().IsInlineBox()) {
          DCHECK(cursor.Current().BoxFragment());
          AddBoxChild(*cursor.Current().BoxFragment(),
                      offset_from_fragmentainer +
                          cursor.Current()->OffsetInContainerFragment(),
                      fragmentainer);
        }
      }
    }

    // Add block children if any.
    for (const NGLink& child : fragment.Children()) {
      DCHECK(!child->IsFragmentainerBox());
      const auto child_offset_from_fragmentainer =
          offset_from_fragmentainer + child.offset;
      AddChild(*child, child_offset_from_fragmentainer, fragmentainer);
    }
  }

  void AddFragmentationContextRootChild(
      const NGPhysicalBoxFragment& fragment,
      const PhysicalOffset& offset_from_fragmentainer,
      const FragmentainerContext& fragmentainer) {
    DCHECK(fragment.IsFragmentationContextRoot());
    DCHECK(!fragment.Items());
    HeapVector<NGLogicalLink> children;
    for (const NGLink& child : fragment.Children()) {
      const LogicalOffset child_offset =
          fragmentainer.converter.ToLogical(
              offset_from_fragmentainer + child.offset, child->Size()) +
          fragmentainer.offset;
      children.push_back(NGLogicalLink{child.fragment, child_offset});
    }
    AddFragmentainerChildren(children,
                             fragmentainer.converter.GetWritingDirection());
  }

  void AddOutOfFlowChild(const NGPhysicalBoxFragment& fragment,
                         const PhysicalOffset& offset_from_fragmentainer,
                         const FragmentainerContext& fragmentainer) {
    DCHECK(fragment.IsOutOfFlowPositioned());
    if (!fragment.Style().AnchorName() && !fragment.IsImplicitAnchor() &&
        !fragment.AnchorQuery()) {
      return;
    }
    // OOF fragments in block-fragmentation context are children of the
    // fragmentainers, but they should be added to anchor queries of their
    // containing block chain. Traverse the containing block chain and add
    // references to all |LayoutObject|, up to the |root_|.
    const LayoutObject* layout_object = fragment.GetLayoutObject();
    DCHECK(layout_object);
    LayoutObject::AncestorSkipInfo skip_info(&root_);
    const LayoutObject* containing_block = layout_object->Container(&skip_info);
    // If the OOF is to be laid out in the fragmentation context, its containing
    // block should be a descendant of the |root_|.
    DCHECK(containing_block);
    DCHECK_NE(containing_block, &root_);
    DCHECK(!skip_info.AncestorSkipped());
    // Skip the first containing block, because the spec defines "If el has the
    // same containing block as query el, el is not absolutely positioned." That
    // said, for absolutely positioned anchors should be invalid for the first
    // containing block.
    // https://drafts.csswg.org/css-anchor-1/#determining
    containing_block = containing_block->Container(&skip_info);
    while (containing_block && containing_block != root_ &&
           !skip_info.AncestorSkipped()) {
      NGStitchedAnchorQuery& query =
          EnsureStitchedAnchorQuery(*containing_block);
      if (fragment.Style().AnchorName()) {
        query.AddAnchorReference(
            fragment.Style().AnchorName(), fragment,
            {offset_from_fragmentainer, fragment.Size()}, fragmentainer,
            NGStitchedAnchorQuery::Conflict::kOverwriteIfBefore);
      }
      if (fragment.IsImplicitAnchor()) {
        query.AddAnchorReference(
            layout_object, fragment,
            {offset_from_fragmentainer, fragment.Size()}, fragmentainer,
            NGStitchedAnchorQuery::Conflict::kOverwriteIfBefore);
      }
      query.AddAnchorQuery(fragment, offset_from_fragmentainer, fragmentainer);
      containing_block = containing_block->Container(&skip_info);
    }
  }

  NGStitchedAnchorQuery& EnsureStitchedAnchorQuery(
      const LayoutObject& containing_block) {
    const auto result = anchor_queries_.insert(
        &containing_block, MakeGarbageCollected<NGStitchedAnchorQuery>());
    DCHECK(result.stored_value->value);
    return *result.stored_value->value;
  }

  HeapHashMap<const LayoutObject*, Member<NGStitchedAnchorQuery>>
      anchor_queries_;
  // The set of |LayoutObject| to traverse. When adding children, children not
  // in this set are skipped.
  const HeapHashSet<Member<const LayoutObject>>&
      anchored_oof_containers_and_ancestors_;
  const LayoutBox& root_;
};

}  // namespace

NGLogicalAnchorQueryMap::NGLogicalAnchorQueryMap(
    const LayoutBox& root_box,
    const NGLogicalLinkVector& children,
    const NGFragmentItemsBuilder::ItemWithOffsetList* items,
    const WritingModeConverter& converter)
    : root_box_(root_box),
      converter_(converter),
      writing_direction_(converter.GetWritingDirection()) {
  DCHECK(&root_box);
  SetChildren(children, items);
}

NGLogicalAnchorQueryMap::NGLogicalAnchorQueryMap(
    const LayoutBox& root_box,
    const NGLogicalLinkVector& children,
    WritingDirectionMode writing_direction)
    : root_box_(root_box), writing_direction_(writing_direction) {
  DCHECK(&root_box);
  SetChildren(children);
}

void NGLogicalAnchorQueryMap::SetChildren(
    const NGLogicalLinkVector& children,
    const NGFragmentItemsBuilder::ItemWithOffsetList* items) {
  children_ = &children;
  items_ = items;

  // Invalidate the cache when children may have changed.
  computed_for_ = nullptr;

  // To allow early returns, check if any child has anchor queries.
  has_anchor_queries_ = false;
  for (const NGLogicalLink& child : children) {
    if (child->HasAnchorQuery()) {
      has_anchor_queries_ = true;
      break;
    }
  }
}

const NGLogicalAnchorQuery& NGLogicalAnchorQueryMap::AnchorQuery(
    const LayoutObject& containing_block) const {
  DCHECK(&containing_block);
  DCHECK(containing_block.CanContainAbsolutePositionObjects() ||
         containing_block.CanContainFixedPositionObjects());

  if (!has_anchor_queries_)
    return NGLogicalAnchorQuery::Empty();

  // Update |queries_| if it hasn't computed for |containing_block|.
  if (!computed_for_ || !computed_for_->IsDescendantOf(&containing_block))
    Update(containing_block);

  const auto& it = queries_.find(&containing_block);
  if (it != queries_.end())
    return *it->value;
  return NGLogicalAnchorQuery::Empty();
}

// Update |queries_| for the given |layout_object| and its ancestors. This is
// `const`, modifies `mutable` caches only, so that other `const` functions such
// as |AnchorQuery| can call.
void NGLogicalAnchorQueryMap::Update(const LayoutObject& layout_object) const {
  // Compute descendants to collect anchor queries from. This helps reducing the
  // number of descendants to traverse.
  HeapHashSet<Member<const LayoutObject>> anchored_oof_containers_and_ancestors;
  for (const LayoutObject* runner = &layout_object;
       runner && runner != &root_box_; runner = runner->Parent()) {
    const auto result = anchored_oof_containers_and_ancestors.insert(runner);
    if (!result.is_new_entry)
      break;
  }

  // Traverse descendants and collect anchor queries for each containing block.
  NGStitchedAnchorQueries stitched_anchor_queries(
      root_box_, anchored_oof_containers_and_ancestors);
  if (converter_) {
    stitched_anchor_queries.AddChildren(*children_, items_, *converter_);
  } else {
    stitched_anchor_queries.AddFragmentainerChildren(*children_,
                                                     writing_direction_);
  }

  // TODO(kojii): Currently this clears and rebuilds all anchor queries on
  // incremental updates. It may be possible to reduce the computation when
  // there are previous results.
  queries_.clear();
  for (const auto& it : stitched_anchor_queries.anchor_queries_) {
    const auto result =
        queries_.insert(it.key, it.value->StitchedAnchorQuery());
    DCHECK(result.is_new_entry);
  }

  computed_for_ = &layout_object;
}

}  // namespace blink
