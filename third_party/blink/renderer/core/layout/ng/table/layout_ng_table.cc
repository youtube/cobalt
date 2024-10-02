// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_caption.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_cell.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_column.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_row.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_section.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_borders.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_helpers.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_utils.h"
#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_table_painters.h"

namespace blink {

namespace {

inline bool NeedsTableSection(const LayoutObject& object) {
  // Return true if 'object' can't exist in an anonymous table without being
  // wrapped in a table section box.
  EDisplay display = object.StyleRef().Display();
  return display != EDisplay::kTableCaption &&
         display != EDisplay::kTableColumnGroup &&
         display != EDisplay::kTableColumn;
}

}  // namespace

LayoutNGTable::LayoutNGTable(Element* element)
    : LayoutNGMixin<LayoutBlock>(element) {}

LayoutNGTable::~LayoutNGTable() = default;

LayoutNGTable* LayoutNGTable::CreateAnonymousWithParent(
    const LayoutObject& parent) {
  scoped_refptr<const ComputedStyle> new_style =
      parent.GetDocument().GetStyleResolver().CreateAnonymousStyleWithDisplay(
          parent.StyleRef(),
          parent.IsLayoutInline() ? EDisplay::kInlineTable : EDisplay::kTable);
  auto* new_table = MakeGarbageCollected<LayoutNGTable>(nullptr);
  new_table->SetDocumentForAnonymous(&parent.GetDocument());
  new_table->SetStyle(std::move(new_style));
  return new_table;
}

bool LayoutNGTable::IsFirstCell(const LayoutNGTableCell& cell) const {
  NOT_DESTROYED();
  const LayoutNGTableRow* row = cell.Row();
  if (row->FirstCell() != &cell) {
    return false;
  }
  const LayoutNGTableSection* section = row->Section();
  if (section->FirstRow() != row) {
    return false;
  }
  NGTableGroupedChildren grouped_children(
      NGBlockNode(const_cast<LayoutNGTable*>(this)));
  auto first_section = grouped_children.begin();
  return first_section != grouped_children.end() &&
         (*first_section).GetLayoutBox() == section;
}

LayoutNGTableSection* LayoutNGTable::FirstSection() const {
  NOT_DESTROYED();
  NGTableGroupedChildren grouped_children(
      NGBlockNode(const_cast<LayoutNGTable*>(this)));
  auto first_section = grouped_children.begin();
  if (first_section != grouped_children.end()) {
    return To<LayoutNGTableSection>((*first_section).GetLayoutBox());
  }
  return nullptr;
}

LayoutNGTableSection* LayoutNGTable::LastSection() const {
  NOT_DESTROYED();
  NGTableGroupedChildren grouped_children(
      NGBlockNode(const_cast<LayoutNGTable*>(this)));
  auto last_section = --grouped_children.end();
  if (last_section != grouped_children.end()) {
    return To<LayoutNGTableSection>((*last_section).GetLayoutBox());
  }
  return nullptr;
}

LayoutNGTableSection* LayoutNGTable::FirstNonEmptySection() const {
  NOT_DESTROYED();
  NGTableGroupedChildren grouped_children(
      NGBlockNode(const_cast<LayoutNGTable*>(this)));
  auto first_section = grouped_children.begin();
  if (first_section != grouped_children.end()) {
    auto* section_object =
        To<LayoutNGTableSection>((*first_section).GetLayoutBox());
    if ((*first_section).IsEmptyTableSection()) {
      return NextSection(section_object, kSkipEmptySections);
    }
    return section_object;
  }
  return nullptr;
}

LayoutNGTableSection* LayoutNGTable::LastNonEmptySection() const {
  NOT_DESTROYED();
  NGTableGroupedChildren grouped_children(
      NGBlockNode(const_cast<LayoutNGTable*>(this)));
  auto last_section = --grouped_children.end();
  if (last_section != grouped_children.end()) {
    auto* section_object =
        To<LayoutNGTableSection>((*last_section).GetLayoutBox());
    if ((*last_section).IsEmptyTableSection()) {
      return PreviousSection(section_object, kSkipEmptySections);
    }
    return section_object;
  }
  return nullptr;
}

LayoutNGTableSection* LayoutNGTable::NextSection(
    const LayoutNGTableSection* current,
    SkipEmptySectionsValue skip) const {
  NOT_DESTROYED();
  NGTableGroupedChildren grouped_children(
      NGBlockNode(const_cast<LayoutNGTable*>(this)));
  bool found = false;
  for (NGBlockNode section : grouped_children) {
    if (found &&
        (skip == kDoNotSkipEmptySections || !section.IsEmptyTableSection())) {
      return To<LayoutNGTableSection>(section.GetLayoutBox());
    }
    if (current == To<LayoutNGTableSection>(section.GetLayoutBox())) {
      found = true;
    }
  }
  return nullptr;
}

LayoutNGTableSection* LayoutNGTable::PreviousSection(
    const LayoutNGTableSection* current,
    SkipEmptySectionsValue skip) const {
  NOT_DESTROYED();
  NGTableGroupedChildren grouped_children(
      NGBlockNode(const_cast<LayoutNGTable*>(this)));
  auto stop = --grouped_children.begin();
  bool found = false;
  for (auto it = --grouped_children.end(); it != stop; --it) {
    NGBlockNode section = *it;
    if (found &&
        (skip == kDoNotSkipEmptySections || !section.IsEmptyTableSection())) {
      return To<LayoutNGTableSection>(section.GetLayoutBox());
    }
    if (current == To<LayoutNGTableSection>(section.GetLayoutBox())) {
      found = true;
    }
  }
  return nullptr;
}

LayoutNGTableSection* LayoutNGTable::FirstBody() const {
  NOT_DESTROYED();
  for (LayoutObject* child = FirstChild(); child;
       child = child->NextSibling()) {
    if (child->StyleRef().Display() == EDisplay::kTableRowGroup) {
      return To<LayoutNGTableSection>(child);
    }
  }
  return nullptr;
}

wtf_size_t LayoutNGTable::ColumnCount() const {
  NOT_DESTROYED();
  const NGLayoutResult* cached_layout_result = GetCachedLayoutResult(nullptr);
  if (!cached_layout_result)
    return 0;
  return cached_layout_result->TableColumnCount();
}

bool LayoutNGTable::HasCollapsedBorders() const {
  NOT_DESTROYED();
  return cached_table_borders_ && cached_table_borders_->IsCollapsed();
}

void LayoutNGTable::SetCachedTableBorders(
    scoped_refptr<const NGTableBorders> table_borders) {
  NOT_DESTROYED();
  cached_table_borders_ = std::move(table_borders);
}

void LayoutNGTable::InvalidateCachedTableBorders() {
  NOT_DESTROYED();
  // TODO(layout-dev) When cached borders are invalidated, we could do a
  // special kind of relayout where fragments can replace only TableBorders,
  // keep the geometry, and repaint.
  cached_table_borders_.reset();
}

const NGTableTypes::Columns* LayoutNGTable::GetCachedTableColumnConstraints() {
  NOT_DESTROYED();
  if (IsTableColumnsConstraintsDirty())
    cached_table_columns_.reset();
  return cached_table_columns_.get();
}

void LayoutNGTable::SetCachedTableColumnConstraints(
    scoped_refptr<const NGTableTypes::Columns> columns) {
  NOT_DESTROYED();
  cached_table_columns_ = std::move(columns);
  SetTableColumnConstraintDirty(false);
}

void LayoutNGTable::GridBordersChanged() {
  NOT_DESTROYED();
  InvalidateCachedTableBorders();
  if (StyleRef().BorderCollapse() == EBorderCollapse::kCollapse) {
    SetShouldDoFullPaintInvalidationWithoutLayoutChange(
        PaintInvalidationReason::kStyle);
    // If borders change, table fragment must be regenerated.
    SetNeedsLayoutAndIntrinsicWidthsRecalc(
        layout_invalidation_reason::kTableChanged);
  }
}

void LayoutNGTable::TableGridStructureChanged() {
  NOT_DESTROYED();
  // Callers must ensure table layout gets invalidated.
  InvalidateCachedTableBorders();
  if (StyleRef().BorderCollapse() == EBorderCollapse::kCollapse)
    SetShouldDoFullPaintInvalidation();
}

bool LayoutNGTable::HasBackgroundForPaint() const {
  NOT_DESTROYED();
  if (StyleRef().HasBackground())
    return true;
  DCHECK_GT(PhysicalFragmentCount(), 0u);
  const NGTableFragmentData::ColumnGeometries* column_geometries =
      GetPhysicalFragment(0)->TableColumnGeometries();
  if (column_geometries) {
    for (const auto& column_geometry : *column_geometries) {
      if (column_geometry.node.Style().HasBackground())
        return true;
    }
  }
  return false;
}

void LayoutNGTable::UpdateBlockLayout(bool relayout_children) {
  NOT_DESTROYED();

  if (IsOutOfFlowPositioned()) {
    UpdateOutOfFlowBlockLayout();
    return;
  }
  UpdateInFlowBlockLayout();
  UpdateMargins();
}

void LayoutNGTable::AddChild(LayoutObject* child, LayoutObject* before_child) {
  NOT_DESTROYED();
  TableGridStructureChanged();
  // Only TablesNG table parts are allowed.
  // TODO(1229581): Change this DCHECK to caption || column || section.
  DCHECK(child->IsLayoutNGObject() ||
         (!child->IsTableCaption() && !child->IsLayoutTableCol() &&
          !child->IsTableSection()));
  bool wrap_in_anonymous_section = !child->IsTableCaption() &&
                                   !child->IsLayoutTableCol() &&
                                   !child->IsTableSection();

  if (!wrap_in_anonymous_section) {
    if (before_child && before_child->Parent() != this)
      before_child = SplitAnonymousBoxesAroundChild(before_child);
    LayoutBox::AddChild(child, before_child);
    return;
  }

  if (!before_child && LastChild() && LastChild()->IsTableSection() &&
      LastChild()->IsAnonymous() && !LastChild()->IsBeforeContent()) {
    LastChild()->AddChild(child);
    return;
  }

  if (before_child && !before_child->IsAnonymous() &&
      before_child->Parent() == this) {
    LayoutNGTableSection* section =
        DynamicTo<LayoutNGTableSection>(before_child->PreviousSibling());
    if (section && section->IsAnonymous()) {
      section->AddChild(child);
      return;
    }
  }

  LayoutObject* last_box = before_child;
  while (last_box && last_box->Parent()->IsAnonymous() &&
         !last_box->IsTableSection() && NeedsTableSection(*last_box))
    last_box = last_box->Parent();
  if (last_box && last_box->IsAnonymous() && last_box->IsTablePart() &&
      !IsAfterContent(last_box)) {
    if (before_child == last_box)
      before_child = last_box->SlowFirstChild();
    last_box->AddChild(child, before_child);
    return;
  }

  if (before_child && !before_child->IsTableSection() &&
      NeedsTableSection(*before_child))
    before_child = nullptr;

  auto* section = LayoutNGTableSection::CreateAnonymousWithParent(*this);
  AddChild(section, before_child);
  section->AddChild(child);
}

void LayoutNGTable::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  TableGridStructureChanged();
  LayoutNGMixin<LayoutBlock>::RemoveChild(child);
}

void LayoutNGTable::StyleDidChange(StyleDifference diff,
                                   const ComputedStyle* old_style) {
  NOT_DESTROYED();
  // StyleDifference handles changes in table-layout, border-spacing.
  if (old_style) {
    bool borders_changed =
        !old_style->BorderVisuallyEqual(StyleRef()) ||
        old_style->GetWritingDirection() != StyleRef().GetWritingDirection() ||
        old_style->IsFixedTableLayout() != StyleRef().IsFixedTableLayout() ||
        old_style->EmptyCells() != StyleRef().EmptyCells();
    bool collapse_changed =
        StyleRef().BorderCollapse() != old_style->BorderCollapse();
    if (borders_changed || collapse_changed)
      GridBordersChanged();
  }
  LayoutNGMixin<LayoutBlock>::StyleDidChange(diff, old_style);
}

LayoutBox* LayoutNGTable::CreateAnonymousBoxWithSameTypeAs(
    const LayoutObject* parent) const {
  NOT_DESTROYED();
  return CreateAnonymousWithParent(*parent);
}

PhysicalRect LayoutNGTable::OverflowClipRect(
    const PhysicalOffset& location,
    OverlayScrollbarClipBehavior overlay_scrollbar_clip_behavior) const {
  NOT_DESTROYED();
  PhysicalRect clip_rect;
  if (StyleRef().BorderCollapse() == EBorderCollapse::kCollapse) {
    clip_rect = PhysicalRect(location, Size());
    const auto overflow_clip = GetOverflowClipAxes();
    gfx::Rect infinite_rect = PhysicalRect::InfiniteIntRect();
    if ((overflow_clip & kOverflowClipX) == kNoOverflowClip) {
      clip_rect.offset.left = LayoutUnit(infinite_rect.x());
      clip_rect.size.width = LayoutUnit(infinite_rect.width());
    }
    if ((overflow_clip & kOverflowClipY) == kNoOverflowClip) {
      clip_rect.offset.top = LayoutUnit(infinite_rect.y());
      clip_rect.size.height = LayoutUnit(infinite_rect.height());
    }
  } else {
    clip_rect = LayoutNGMixin<LayoutBlock>::OverflowClipRect(
        location, overlay_scrollbar_clip_behavior);
  }
  // TODO(1142929)
  // We cannot handle table hidden overflow with captions correctly.
  // Correct handling would clip table grid content to grid content rect,
  // but not clip the captions.
  // Since we are not generating table's grid fragment, this is not
  // possible.
  // The current solution is to not clip if we have captions.
  // Maybe a fix is to do an additional clip in table painter?
  const LayoutBox* child = FirstChildBox();
  while (child) {
    if (child->IsTableCaption()) {
      // If there are captions, we cannot clip to content box.
      clip_rect.Unite(PhysicalRect(location, Size()));
      break;
    }
    child = child->NextSiblingBox();
  }
  return clip_rect;
}

#if DCHECK_IS_ON()
void LayoutNGTable::AddVisualEffectOverflow() {
  NOT_DESTROYED();
  // This is computed in |NGPhysicalBoxFragment::ComputeSelfInkOverflow| and
  // that we should not reach here.
  NOTREACHED();
}
#endif

LayoutUnit LayoutNGTable::BorderLeft() const {
  NOT_DESTROYED();
  // DCHECK(cached_table_borders_.get())
  // ScrollAnchoring fails this DCHECK.
  if (ShouldCollapseBorders() && cached_table_borders_.get()) {
    return cached_table_borders_->TableBorder()
        .ConvertToPhysical(Style()->GetWritingDirection())
        .left;
  }
  return LayoutNGMixin<LayoutBlock>::BorderLeft();
}

LayoutUnit LayoutNGTable::BorderRight() const {
  NOT_DESTROYED();
  // DCHECK(cached_table_borders_.get())
  // ScrollAnchoring fails this DCHECK.
  if (ShouldCollapseBorders() && cached_table_borders_.get()) {
    return cached_table_borders_->TableBorder()
        .ConvertToPhysical(Style()->GetWritingDirection())
        .right;
  }
  return LayoutNGMixin<LayoutBlock>::BorderRight();
}

LayoutUnit LayoutNGTable::BorderTop() const {
  NOT_DESTROYED();
  // DCHECK(cached_table_borders_.get())
  // ScrollAnchoring fails this DCHECK.
  if (ShouldCollapseBorders() && cached_table_borders_.get()) {
    return cached_table_borders_->TableBorder()
        .ConvertToPhysical(Style()->GetWritingDirection())
        .top;
  }
  return LayoutNGMixin<LayoutBlock>::BorderTop();
}

LayoutUnit LayoutNGTable::BorderBottom() const {
  NOT_DESTROYED();
  // DCHECK(cached_table_borders_.get())
  // ScrollAnchoring fails this DCHECK.
  if (ShouldCollapseBorders() && cached_table_borders_.get()) {
    return cached_table_borders_->TableBorder()
        .ConvertToPhysical(Style()->GetWritingDirection())
        .bottom;
  }
  return LayoutNGMixin<LayoutBlock>::BorderBottom();
}

LayoutUnit LayoutNGTable::PaddingTop() const {
  NOT_DESTROYED();
  if (ShouldCollapseBorders())
    return LayoutUnit();
  return LayoutNGMixin<LayoutBlock>::PaddingTop();
}

LayoutUnit LayoutNGTable::PaddingBottom() const {
  NOT_DESTROYED();
  if (ShouldCollapseBorders())
    return LayoutUnit();
  return LayoutNGMixin<LayoutBlock>::PaddingBottom();
}

LayoutUnit LayoutNGTable::PaddingLeft() const {
  NOT_DESTROYED();
  if (ShouldCollapseBorders())
    return LayoutUnit();
  return LayoutNGMixin<LayoutBlock>::PaddingLeft();
}

LayoutUnit LayoutNGTable::PaddingRight() const {
  NOT_DESTROYED();
  if (ShouldCollapseBorders())
    return LayoutUnit();
  return LayoutNGMixin<LayoutBlock>::PaddingRight();
}

NGPhysicalBoxStrut LayoutNGTable::BorderBoxOutsets() const {
  NOT_DESTROYED();
  // DCHECK(cached_table_borders_.get())
  // ScrollAnchoring fails this DCHECK.
  if (PhysicalFragmentCount() > 0) {
    return GetPhysicalFragment(0)->Borders();
  }
  NOTREACHED();
  return {};
}

// Effective column index is index of columns with mergeable
// columns skipped. Used in a11y.
unsigned LayoutNGTable::AbsoluteColumnToEffectiveColumn(
    unsigned absolute_column_index) const {
  NOT_DESTROYED();
  if (!cached_table_columns_) {
    NOTREACHED();
    return absolute_column_index;
  }
  unsigned effective_column_index = 0;
  unsigned column_count = cached_table_columns_.get()->data.size();
  for (unsigned current_column_index = 0; current_column_index < column_count;
       ++current_column_index) {
    if (current_column_index != 0 &&
        !cached_table_columns_.get()->data[current_column_index].is_mergeable)
      ++effective_column_index;
    if (current_column_index == absolute_column_index)
      return effective_column_index;
  }
  return effective_column_index;
}

}  // namespace blink
