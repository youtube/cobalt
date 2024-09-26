// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_result.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

namespace blink {

NGInlineItemResult::NGInlineItemResult(const NGInlineItem* item,
                                       unsigned index,
                                       const NGTextOffsetRange& text_offset,
                                       bool break_anywhere_if_overflow,
                                       bool should_create_line_box,
                                       bool has_unpositioned_floats)
    : item(item),
      item_index(index),
      text_offset(text_offset),
      break_anywhere_if_overflow(break_anywhere_if_overflow),
      should_create_line_box(should_create_line_box),
      has_unpositioned_floats(has_unpositioned_floats) {}

void NGInlineItemResult::ShapeHyphen() {
  DCHECK(!hyphen_string);
  DCHECK(!hyphen_shape_result);
  DCHECK(item);
  DCHECK(item->Style());
  const ComputedStyle& style = *item->Style();
  hyphen_string = style.HyphenString();
  HarfBuzzShaper shaper(hyphen_string);
  hyphen_shape_result = shaper.Shape(&style.GetFont(), style.Direction());
}

#if DCHECK_IS_ON()
void NGInlineItemResult::CheckConsistency(bool allow_null_shape_result) const {
  DCHECK(item);
  if (item->Type() == NGInlineItem::kText) {
    text_offset.AssertNotEmpty();
    if (allow_null_shape_result && !shape_result)
      return;
    DCHECK(shape_result);
    DCHECK_EQ(Length(), shape_result->NumCharacters());
    DCHECK_EQ(StartOffset(), shape_result->StartIndex());
    DCHECK_EQ(EndOffset(), shape_result->EndIndex());
  }
}
#endif

void NGInlineItemResult::Trace(Visitor* visitor) const {
  visitor->Trace(layout_result);
  if (positioned_float)
    visitor->Trace(positioned_float.value());
}

}  // namespace blink
