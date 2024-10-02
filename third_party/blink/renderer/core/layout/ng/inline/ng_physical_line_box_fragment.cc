// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_relative_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

namespace {

struct SameSizeAsNGPhysicalLineBoxFragment : NGPhysicalFragment {
  FontHeight metrics;
};

ASSERT_SIZE(NGPhysicalLineBoxFragment, SameSizeAsNGPhysicalLineBoxFragment);

}  // namespace

const NGPhysicalLineBoxFragment* NGPhysicalLineBoxFragment::Create(
    NGLineBoxFragmentBuilder* builder) {
  DCHECK_EQ(builder->children_.size(), 0u);
  return MakeGarbageCollected<NGPhysicalLineBoxFragment>(PassKey(), builder);
}

const NGPhysicalLineBoxFragment* NGPhysicalLineBoxFragment::Clone(
    const NGPhysicalLineBoxFragment& other) {
  return MakeGarbageCollected<NGPhysicalLineBoxFragment>(PassKey(), other);
}

NGPhysicalLineBoxFragment::NGPhysicalLineBoxFragment(
    PassKey key,
    NGLineBoxFragmentBuilder* builder)
    : NGPhysicalFragment(builder,
                         builder->GetWritingMode(),
                         kFragmentLineBox,
                         builder->line_box_type_),
      metrics_(builder->metrics_) {
  // A line box must have a metrics unless it's an empty line box.
  DCHECK(!metrics_.IsEmpty() || IsEmptyLineBox());
  base_direction_ = static_cast<unsigned>(builder->base_direction_);
  has_hanging_ = builder->hang_inline_size_ != 0;
  has_propagated_descendants_ = has_floating_descendants_for_paint_ ||
                                HasOutOfFlowPositionedDescendants() ||
                                builder->unpositioned_list_marker_;
}

NGPhysicalLineBoxFragment::NGPhysicalLineBoxFragment(
    PassKey key,
    const NGPhysicalLineBoxFragment& other)
    : NGPhysicalFragment(other), metrics_(other.metrics_) {
  base_direction_ = other.base_direction_;
  has_hanging_ = other.has_hanging_;
  has_propagated_descendants_ = other.has_propagated_descendants_;
}

NGPhysicalLineBoxFragment::~NGPhysicalLineBoxFragment() = default;

void NGPhysicalLineBoxFragment::Dispose() {}

FontHeight NGPhysicalLineBoxFragment::BaselineMetrics() const {
  // TODO(kojii): Computing other baseline types than the used one is not
  // implemented yet.
  // TODO(kojii): We might need locale/script to look up OpenType BASE table.
  return metrics_;
}

namespace {

// Include the inline-size of the line-box in the overflow.
// Do not update block offset and block size of |overflow|.
inline void AddInlineSizeToOverflow(const PhysicalRect& rect,
                                    const WritingMode container_writing_mode,
                                    PhysicalRect* overflow) {
  PhysicalRect inline_rect;
  inline_rect.offset = rect.offset;
  if (IsHorizontalWritingMode(container_writing_mode)) {
    inline_rect.size.width = rect.size.width;
    inline_rect.offset.top = overflow->offset.top;
    inline_rect.size.height = overflow->size.height;
  } else {
    inline_rect.size.height = rect.size.height;
    inline_rect.offset.left = overflow->offset.left;
    inline_rect.size.width = overflow->size.width;
  }
  overflow->UniteEvenIfEmpty(inline_rect);
}

}  // namespace

PhysicalRect NGPhysicalLineBoxFragment::ScrollableOverflow(
    const NGPhysicalBoxFragment& container,
    const ComputedStyle& container_style,
    TextHeightType height_type) const {
  const WritingMode container_writing_mode = container_style.GetWritingMode();
  PhysicalRect overflow;
  // Make sure we include the inline-size of the line-box in the overflow.
  AddInlineSizeToOverflow(LocalRect(), container_writing_mode, &overflow);

  return overflow;
}

PhysicalRect NGPhysicalLineBoxFragment::ScrollableOverflowForLine(
    const NGPhysicalBoxFragment& container,
    const ComputedStyle& container_style,
    const NGFragmentItem& line,
    const NGInlineCursor& cursor,
    TextHeightType height_type) const {
  DCHECK_EQ(&line, cursor.CurrentItem());
  DCHECK_EQ(line.LineBoxFragment(), this);

  PhysicalRect overflow;
  AddScrollableOverflowForInlineChild(container, container_style, line,
                                      has_hanging_, cursor, height_type,
                                      &overflow);

  // Make sure we include the inline-size of the line-box in the overflow.
  // Note, the bottom half-leading should not be included. crbug.com/996847
  const WritingMode container_writing_mode = container_style.GetWritingMode();
  AddInlineSizeToOverflow(line.RectInContainerFragment(),
                          container_writing_mode, &overflow);

  return overflow;
}

bool NGPhysicalLineBoxFragment::HasSoftWrapToNextLine() const {
  const auto* break_token = To<NGInlineBreakToken>(BreakToken());
  return break_token && !break_token->IsForcedBreak();
}

void NGPhysicalLineBoxFragment::TraceAfterDispatch(Visitor* visitor) const {
  NGPhysicalFragment::TraceAfterDispatch(visitor);
}

}  // namespace blink
