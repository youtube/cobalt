// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/inline_items_builder.h"

#include <type_traits>

#include "base/containers/adapters.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/html_area_element.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item_span.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node_data.h"
#include "third_party/blink/renderer/core/layout/inline/offset_mapping_builder.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

namespace blink {
class HTMLAreaElement;

// Returns true if items builder is used for other than offset mapping.
template <typename MappingBuilder>
bool InlineItemsBuilderTemplate<MappingBuilder>::NeedsBoxInfo() {
  return !std::is_same<OffsetMappingBuilder, MappingBuilder>::value;
}

template <typename MappingBuilder>
InlineItemsBuilderTemplate<MappingBuilder>::~InlineItemsBuilderTemplate() {
  DCHECK_EQ(0u, bidi_context_.size());
  DCHECK_EQ(text_.length(), items_->empty() ? 0 : items_->back().EndOffset());
}

template <typename MappingBuilder>
String InlineItemsBuilderTemplate<MappingBuilder>::ToString() {
  return text_.ToString();
}

namespace {
// The spec turned into a discussion that may change. Put this logic on hold
// until CSSWG resolves the issue.
// https://github.com/w3c/csswg-drafts/issues/337
#define SEGMENT_BREAK_TRANSFORMATION_FOR_EAST_ASIAN_WIDTH 0

#if SEGMENT_BREAK_TRANSFORMATION_FOR_EAST_ASIAN_WIDTH
// Determine "Ambiguous" East Asian Width is Wide or Narrow.
// Unicode East Asian Width
// http://unicode.org/reports/tr11/
bool IsAmbiguosEastAsianWidthWide(const ComputedStyle* style) {
  UScriptCode script = style->GetFontDescription().GetScript();
  return script == USCRIPT_KATAKANA_OR_HIRAGANA ||
         script == USCRIPT_SIMPLIFIED_HAN || script == USCRIPT_TRADITIONAL_HAN;
}

// Determine if a character has "Wide" East Asian Width.
bool IsEastAsianWidthWide(UChar32 c, const ComputedStyle* style) {
  UEastAsianWidth eaw = static_cast<UEastAsianWidth>(
      u_getIntPropertyValue(c, UCHAR_EAST_ASIAN_WIDTH));
  return eaw == U_EA_WIDE || eaw == U_EA_FULLWIDTH || eaw == U_EA_HALFWIDTH ||
         (eaw == U_EA_AMBIGUOUS && style &&
          IsAmbiguosEastAsianWidthWide(style));
}
#endif

// Determine whether a newline should be removed or not.
// CSS Text, Segment Break Transformation Rules
// https://drafts.csswg.org/css-text-3/#line-break-transform
bool ShouldRemoveNewlineSlow(const StringBuilder& before,
                             unsigned space_index,
                             const ComputedStyle* before_style,
                             const StringView& after,
                             const ComputedStyle* after_style) {
  // Remove if either before/after the newline is zeroWidthSpaceCharacter.
  UChar32 last = 0;
  DCHECK(space_index == before.length() ||
         (space_index < before.length() && before[space_index] == ' '));
  if (space_index) {
    last = before[space_index - 1];
    if (last == kZeroWidthSpaceCharacter)
      return true;
  }
  UChar32 next = 0;
  if (!after.empty()) {
    next = after[0];
    if (next == kZeroWidthSpaceCharacter)
      return true;
  }

#if SEGMENT_BREAK_TRANSFORMATION_FOR_EAST_ASIAN_WIDTH
  // Logic below this point requires both before and after be 16 bits.
  if (before.Is8Bit() || after.Is8Bit())
    return false;

  // Remove if East Asian Widths of both before/after the newline are Wide, and
  // neither side is Hangul.
  // TODO(layout-dev): Don't remove if any side is Emoji.
  if (U16_IS_TRAIL(last) && space_index >= 2) {
    UChar last_last = before[space_index - 2];
    if (U16_IS_LEAD(last_last))
      last = U16_GET_SUPPLEMENTARY(last_last, last);
  }
  if (!Character::IsHangul(last) && IsEastAsianWidthWide(last, before_style)) {
    if (U16_IS_LEAD(next) && after.length() > 1) {
      UChar next_next = after[1];
      if (U16_IS_TRAIL(next_next))
        next = U16_GET_SUPPLEMENTARY(next, next_next);
    }
    if (!Character::IsHangul(next) && IsEastAsianWidthWide(next, after_style))
      return true;
  }
#endif

  return false;
}

bool ShouldRemoveNewline(const StringBuilder& before,
                         unsigned space_index,
                         const ComputedStyle* before_style,
                         const StringView& after,
                         const ComputedStyle* after_style) {
  // All characters before/after removable newline are 16 bits.
  return (!before.Is8Bit() || !after.Is8Bit()) &&
         ShouldRemoveNewlineSlow(before, space_index, before_style, after,
                                 after_style);
}

inline InlineItem& AppendItem(HeapVector<InlineItem>* items,
                              InlineItem::InlineItemType type,
                              unsigned start,
                              unsigned end,
                              LayoutObject* layout_object) {
  return items->emplace_back(type, start, end, layout_object);
}

inline bool ShouldIgnore(UChar c) {
  // Ignore carriage return and form feed.
  // https://drafts.csswg.org/css-text-3/#white-space-processing
  // https://github.com/w3c/csswg-drafts/issues/855
  //
  // Unicode Default_Ignorable is not included because we need some of them
  // in the line breaker (e.g., SOFT HYPHEN.) HarfBuzz ignores them while
  // shaping.
  return c == kCarriageReturnCharacter || c == kFormFeedCharacter;
}

// Characters needing a separate control item than other text items.
// It makes the line breaker easier to handle.
inline bool IsControlItemCharacter(UChar c) {
  return c == kNewlineCharacter || c == kTabulationCharacter ||
         // Make ZWNJ a control character so that it can prevent kerning.
         c == kZeroWidthNonJoinerCharacter ||
         // Include ignorable character here to avoids shaping/rendering
         // these glyphs, and to help the line breaker to ignore them.
         ShouldIgnore(c);
}

// Find the end of the collapsible spaces.
// Returns whether this space run contains a newline or not, because it changes
// the collapsing behavior.
inline bool MoveToEndOfCollapsibleSpaces(const StringView& string,
                                         unsigned* offset,
                                         UChar* c) {
  DCHECK_EQ(*c, string[*offset]);
  DCHECK(Character::IsCollapsibleSpace(*c));
  bool space_run_has_newline = *c == kNewlineCharacter;
  for ((*offset)++; *offset < string.length(); (*offset)++) {
    *c = string[*offset];
    space_run_has_newline |= *c == kNewlineCharacter;
    if (!Character::IsCollapsibleSpace(*c))
      break;
  }
  return space_run_has_newline;
}

// Find the last item to compute collapsing with. Opaque items such as
// open/close or bidi controls are ignored.
// Returns nullptr if there were no previous items.
InlineItem* LastItemToCollapseWith(HeapVector<InlineItem>* items) {
  for (auto& item : base::Reversed(*items)) {
    if (item.EndCollapseType() != InlineItem::kOpaqueToCollapsing) {
      return &item;
    }
  }
  return nullptr;
}

}  // anonymous namespace

template <typename MappingBuilder>
InlineItemsBuilderTemplate<MappingBuilder>::BoxInfo::BoxInfo(
    unsigned item_index,
    const InlineItem& item)
    : style(*item.Style()),
      item_index(item_index),
      should_create_box_fragment(item.ShouldCreateBoxFragment()),
      text_metrics(style->GetFontHeight()) {
  DCHECK(style);
}

// True if this inline box should create a box fragment when it has |child|.
template <typename MappingBuilder>
bool InlineItemsBuilderTemplate<MappingBuilder>::BoxInfo::
    ShouldCreateBoxFragmentForChild(const BoxInfo& child) const {
  // When a child inline box has margins, the parent has different width/height
  // from the union of children.
  const ComputedStyle& child_style = *child.style;
  if (child_style.MayHaveMargin())
    return true;

  // Because a culled inline box computes its position from its first child,
  // when the first child is shifted vertically, its position will shift too.
  // Note, this is needed only when it's the first child, but checking it need
  // to take collapsed spaces into account. Uncull even when it's not the first
  // child.
  if (child_style.VerticalAlign() != EVerticalAlign::kBaseline)
    return true;

  // Returns true when parent and child boxes have different font metrics, since
  // they may have different heights and/or locations in block direction.
  if (text_metrics != child.text_metrics)
    return true;

  return false;
}

template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::BoxInfo::
    SetShouldCreateBoxFragment(HeapVector<InlineItem>* items) {
  DCHECK(!should_create_box_fragment);
  should_create_box_fragment = true;
  (*items)[item_index].SetShouldCreateBoxFragment();
}

// Append a string as a text item.
template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::AppendTextItem(
    const StringView string,
    LayoutText* layout_object) {
  DCHECK(layout_object);
  AppendTextItem(InlineItem::kText, string, layout_object);
}

template <typename MappingBuilder>
InlineItem& InlineItemsBuilderTemplate<MappingBuilder>::AppendTextItem(
    InlineItem::InlineItemType type,
    const StringView string,
    LayoutText* layout_object) {
  DCHECK(layout_object);
  unsigned start_offset = text_.length();
  text_.Append(string);
  mapping_builder_.AppendIdentityMapping(string.length());
  InlineItem& item =
      AppendItem(items_, type, start_offset, text_.length(), layout_object);
  DCHECK(!item.IsEmptyItem());
  is_block_level_ = false;
  return item;
}

// Empty text items are not needed for the layout purposes, but all LayoutObject
// must be captured in InlineItemsData to maintain states of LayoutObject in
// this inline formatting context.
template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::AppendEmptyTextItem(
    LayoutText* layout_object) {
  DCHECK(layout_object);
  unsigned offset = text_.length();
  InlineItem& item =
      AppendItem(items_, InlineItem::kText, offset, offset, layout_object);
  item.SetEndCollapseType(InlineItem::kOpaqueToCollapsing);
  item.SetIsEmptyItem(true);
  item.SetIsBlockLevel(true);
}

// Same as AppendBreakOpportunity, but mark the item as IsGenerated().
template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::
    AppendGeneratedBreakOpportunity(LayoutObject* layout_object) {
  if (block_flow_->IsSVGText()) {
    return;
  }
  DCHECK(layout_object);
  typename MappingBuilder::SourceNodeScope scope(&mapping_builder_, nullptr);
  InlineItem& item = AppendBreakOpportunity(layout_object);
  item.SetIsGeneratedForLineBreak();
  item.SetEndCollapseType(InlineItem::kOpaqueToCollapsing);
}

template <typename MappingBuilder>
inline void InlineItemsBuilderTemplate<MappingBuilder>::DidAppendForcedBreak() {
  // Bisecting available widths can't handle multiple logical paragraphs, so
  // forced break should disable it. See `ParagraphLineBreaker`.
  is_bisect_line_break_disabled_ = true;
}

template <typename MappingBuilder>
inline void InlineItemsBuilderTemplate<MappingBuilder>::DidAppendTextReusing(
    const InlineItem& item) {
  is_block_level_ &= item.IsBlockLevel();
  if (item.IsForcedLineBreak()) {
    DidAppendForcedBreak();
  }
}

template <typename MappingBuilder>
bool InlineItemsBuilderTemplate<MappingBuilder>::AppendTextReusing(
    const InlineNodeData& original_data,
    LayoutText* layout_text) {
  DCHECK(layout_text);
  const auto& items = layout_text->InlineItems();
  const InlineItem& old_item0 = items.front();
  if (!old_item0.Length())
    return false;

  const String& original_string = original_data.text_content;

  // Don't reuse existing items if they might be affected by whitespace
  // collapsing.
  // TODO(layout-dev): This could likely be optimized further.
  // TODO(layout-dev): Handle cases where the old items are not consecutive.
  const ComputedStyle& new_style = layout_text->StyleRef();
  const bool collapse_spaces = new_style.ShouldCollapseWhiteSpaces();
  const bool preserve_newlines =
      new_style.ShouldPreserveBreaks() && LIKELY(!is_text_combine_);
  if (InlineItem* last_item = LastItemToCollapseWith(items_)) {
    if (collapse_spaces) {
      switch (last_item->EndCollapseType()) {
        case InlineItem::kCollapsible:
          switch (original_string[old_item0.StartOffset()]) {
            case kSpaceCharacter:
              // If the original string starts with a collapsible space, it may
              // be collapsed.
              return false;
            case kNewlineCharacter:
              // Collapsible spaces immediately before a preserved newline
              // should be removed to be consistent with
              // AppendForcedBreakCollapseWhitespace.
              if (preserve_newlines)
                return false;
          }
          // If the last item ended with a collapsible space run with segment
          // breaks, we need to run the full algorithm to apply segment break
          // rules. This may result in removal of the space in the last item.
          if (last_item->IsEndCollapsibleNewline()) {
            const StringView old_item0_view(
                original_string, old_item0.StartOffset(), old_item0.Length());
            if (ShouldRemoveNewline(text_, last_item->EndOffset() - 1,
                                    last_item->Style(), old_item0_view,
                                    &new_style)) {
              return false;
            }
          }
          break;
        case InlineItem::kNotCollapsible: {
          const String& source_text = layout_text->GetText();
          if (source_text.length() &&
              Character::IsCollapsibleSpace(source_text[0])) {
            // If the start of the original string was collapsed, it may be
            // restored.
            if (original_string[old_item0.StartOffset()] != kSpaceCharacter)
              return false;
            // If the start of the original string was not collapsed, and the
            // collapsible space run contains newline, the newline may be
            // removed.
            unsigned offset = 0;
            UChar c = source_text[0];
            bool contains_newline =
                MoveToEndOfCollapsibleSpaces(source_text, &offset, &c);
            if (contains_newline &&
                ShouldRemoveNewline(text_, text_.length(), last_item->Style(),
                                    StringView(source_text, offset),
                                    &new_style)) {
              return false;
            }
          }
          break;
        }
        case InlineItem::kCollapsed:
          RestoreTrailingCollapsibleSpace(last_item);
          return false;
        case InlineItem::kOpaqueToCollapsing:
          NOTREACHED();
          break;
      }
    } else if (last_item->EndCollapseType() == InlineItem::kCollapsed) {
      RestoreTrailingCollapsibleSpace(last_item);
      return false;
    }

    // On nowrap -> wrap boundary, a break opporunity may be inserted.
    DCHECK(last_item->Style());
    if (!last_item->Style()->ShouldWrapLine() && new_style.ShouldWrapLine()) {
      return false;
    }

  } else if (collapse_spaces) {
    // If the original string starts with a collapsible space, it may be
    // collapsed because it is now a leading collapsible space.
    if (original_string[old_item0.StartOffset()] == kSpaceCharacter)
      return false;
  }

  if (preserve_newlines) {
    // We exit and then re-enter all bidi contexts around a forced break. So, We
    // must go through the full pipeline to ensure that we exit and enter the
    // correct bidi contexts the re-layout.
    if (bidi_context_.size() || layout_text->HasBidiControlInlineItems()) {
      if (layout_text->GetText().Contains(kNewlineCharacter))
        return false;
    }
  }

  if (UNLIKELY(old_item0.StartOffset() > 0 &&
               ShouldInsertBreakOpportunityAfterLeadingPreservedSpaces(
                   layout_text->GetText(), new_style))) {
    // e.g. <p>abc xyz</p> => <p> xyz</p> where "abc" and " xyz" are different
    // Text node. |text_| is " \u200Bxyz".
    return false;
  }

  for (const InlineItem& item : items) {
    // Collapsed space item at the start will not be restored, and that not
    // needed to add.
    if (!text_.length() && !item.Length() && collapse_spaces)
      continue;

    // We are reusing items that included 'generated line breaks', inserted to
    // deal with leading preserved space sequences. If we are performing a
    // relayout after removing a <br> (eg. <div>abc<br><span> dfg</span></div>)
    // it may imply that the preserved spaces are not a leading sequence
    // anymore.
    if (item.IsGeneratedForLineBreak()) {
      // We wont restore 'generated line breaks' at the start
      // TODO(jfernandez): How it's possible that we have a generated break at
      // position 0 ?
      if (!text_.length())
        continue;
      int index = text_.length() - 1;
      while (index >= 0 && text_[index] == kSpaceCharacter)
        --index;
      if (index >= 0 && text_[index] != kNewlineCharacter)
        continue;
    }

    unsigned start = text_.length();
    text_.Append(original_string, item.StartOffset(), item.Length());

    // If the item's position within the container remains unchanged the item
    // itself may be reused.
    if (item.StartOffset() == start) {
      items_->push_back(item);
      DidAppendTextReusing(item);
      continue;
    }

    // If the position has shifted the item and the shape result needs to be
    // adjusted to reflect the new start and end offsets.
    unsigned end = start + item.Length();
    scoped_refptr<ShapeResult> adjusted_shape_result;
    if (item.TextShapeResult()) {
      DCHECK_EQ(item.Type(), InlineItem::kText);
      adjusted_shape_result = item.TextShapeResult()->CopyAdjustedOffset(start);
      DCHECK(adjusted_shape_result);
    } else {
      // The following should be true, but some unit tests fail.
      // DCHECK_EQ(item->Type(), InlineItem::kControl);
    }
    InlineItem adjusted_item(item, start, end,
                             std::move(adjusted_shape_result));

#if DCHECK_IS_ON()
    DCHECK_EQ(start, adjusted_item.StartOffset());
    DCHECK_EQ(end, adjusted_item.EndOffset());
    if (adjusted_item.TextShapeResult()) {
      DCHECK_EQ(start, adjusted_item.TextShapeResult()->StartIndex());
      DCHECK_EQ(end, adjusted_item.TextShapeResult()->EndIndex());
    }
    DCHECK_EQ(item.IsEmptyItem(), adjusted_item.IsEmptyItem());
#endif

    items_->push_back(adjusted_item);
    DidAppendTextReusing(adjusted_item);
  }
  return true;
}

template <>
bool InlineItemsBuilderTemplate<OffsetMappingBuilder>::AppendTextReusing(
    const InlineNodeData&,
    LayoutText*) {
  NOTREACHED();
  return false;
}

template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::AppendText(
    LayoutText* layout_text,
    const InlineNodeData* previous_data) {
  // If the LayoutText element hasn't changed, reuse the existing items.
  if (previous_data && layout_text->HasValidInlineItems()) {
    if (AppendTextReusing(*previous_data, layout_text)) {
      return;
    }
  }

  // If not create a new item as needed.
  if (UNLIKELY(layout_text->IsWordBreak())) {
    typename MappingBuilder::SourceNodeScope scope(&mapping_builder_,
                                                   layout_text);
    if (UNLIKELY(is_text_combine_)) {
      // We don't break text runs in text-combine-upright:all.
      // Note: Even if we have overflow-wrap:normal and word-break:keep-all,
      // <wbr> causes line break.
      Append(InlineItem::kText, kZeroWidthSpaceCharacter, layout_text);
      return;
    }
    AppendBreakOpportunity(layout_text);
    return;
  }

  AppendText(layout_text->GetText(), layout_text);
}

template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::AppendText(
    const String& string,
    LayoutText* layout_object) {
  DCHECK(layout_object);

  if (string.empty()) {
    AppendEmptyTextItem(layout_object);
    return;
  }

  const wtf_size_t estimated_length = text_.length() + string.length();
  if (estimated_length > text_.Capacity()) {
    // The reallocations may occur very frequently for large text such as log
    // files. We use a more aggressive expansion strategy, the same as
    // |Vector::ExpandCapacity| does for |Vector|s with inline storage.
    // |ReserveCapacity| reserves only the requested size.
    const wtf_size_t new_capacity =
        std::max(estimated_length, text_.Capacity() * 2);
    if (string.Is8Bit())
      text_.ReserveCapacity(new_capacity);
    else
      text_.Reserve16BitCapacity(new_capacity);
  }

  typename MappingBuilder::SourceNodeScope scope(&mapping_builder_,
                                                 layout_object);

  const ComputedStyle& style = layout_object->StyleRef();
  const bool should_not_preserve_newline =
      (layout_object && layout_object->IsSVGInlineText()) ||
      UNLIKELY(is_text_combine_);

  RestoreTrailingCollapsibleSpaceIfRemoved();

  if (text_chunk_offsets_ && AppendTextChunks(string, *layout_object))
    return;
  if (style.ShouldPreserveWhiteSpaces()) {
    AppendPreserveWhitespace(string, &style, layout_object);
  } else if (style.ShouldPreserveBreaks() && !should_not_preserve_newline) {
    AppendPreserveNewline(string, &style, layout_object);
  } else {
    AppendCollapseWhitespace(string, &style, layout_object);
  }
}

template <typename MappingBuilder>
bool InlineItemsBuilderTemplate<MappingBuilder>::AppendTextChunks(
    const String& string,
    LayoutText& layout_text) {
  auto iter = text_chunk_offsets_->find(&layout_text);
  if (iter == text_chunk_offsets_->end())
    return false;
  const ComputedStyle& style = layout_text.StyleRef();
  const bool should_collapse_space = style.ShouldCollapseWhiteSpaces();
  unsigned start = 0;
  for (unsigned offset : iter->value) {
    DCHECK_LE(offset, string.length());
    if (start < offset) {
      if (!should_collapse_space) {
        AppendPreserveWhitespace(string.Substring(start, offset - start),
                                 &style, &layout_text);
      } else {
        AppendCollapseWhitespace(StringView(string, start, offset - start),
                                 &style, &layout_text);
      }
    }
    ExitAndEnterSvgTextChunk(layout_text);
    start = offset;
  }
  if (start >= string.length())
    return true;
  if (!should_collapse_space) {
    AppendPreserveWhitespace(string.Substring(start), &style, &layout_text);
  } else {
    AppendCollapseWhitespace(StringView(string, start), &style, &layout_text);
  }
  return true;
}

template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::AppendCollapseWhitespace(
    const StringView string,
    const ComputedStyle* style,
    LayoutText* layout_object) {
  DCHECK(!string.empty());

  // This algorithm segments the input string at the collapsible space, and
  // process collapsible space run and non-space run alternately.

  // The first run, regardless it is a collapsible space run or not, is special
  // that it can interact with the last item. Depends on the end of the last
  // item, it may either change collapsing behavior to collapse the leading
  // spaces of this item entirely, or remove the trailing spaces of the last
  // item.

  // Due to this difference, this algorithm process the first run first, then
  // loop through the rest of runs.

  unsigned start_offset;
  InlineItem::NGCollapseType end_collapse = InlineItem::kNotCollapsible;
  unsigned i = 0;
  UChar c = string[i];
  bool space_run_has_newline = false;
  if (Character::IsCollapsibleSpace(c)) {
    // Find the end of the collapsible space run.
    space_run_has_newline = MoveToEndOfCollapsibleSpaces(string, &i, &c);

    // LayoutBR does not set preserve_newline, but should be preserved.
    if (UNLIKELY(space_run_has_newline && string.length() == 1 &&
                 layout_object && layout_object->IsBR())) {
      if (UNLIKELY(is_text_combine_)) {
        AppendTextItem(" ", layout_object);
      } else {
        AppendForcedBreakCollapseWhitespace(layout_object);
      }
      return;
    }

    // Check the last item this space run may be collapsed with.
    bool insert_space;
    if (InlineItem* item = LastItemToCollapseWith(items_)) {
      if (item->EndCollapseType() == InlineItem::kNotCollapsible) {
        // The last item does not end with a collapsible space.
        // Insert a space to represent this space run.
        insert_space = true;
      } else {
        // The last item ends with a collapsible space this run should collapse
        // to. Collapse the entire space run in this item.
        DCHECK(item->EndCollapseType() == InlineItem::kCollapsible);
        insert_space = false;

        // If the space run either in this item or in the last item contains a
        // newline, apply segment break rules. This may result in removal of
        // the space in the last item.
        if ((space_run_has_newline || item->IsEndCollapsibleNewline()) &&
            item->Type() == InlineItem::kText &&
            ShouldRemoveNewline(text_, item->EndOffset() - 1, item->Style(),
                                StringView(string, i), style)) {
          RemoveTrailingCollapsibleSpace(item);
          space_run_has_newline = false;
        } else if (!item->Style()->ShouldWrapLine() &&
                   style->ShouldWrapLine()) {
          // Otherwise, remove the space run entirely, collapsing to the space
          // in the last item.

          // There is a special case to generate a break opportunity though.
          // Spec-wise, collapsed spaces are "zero advance width, invisible,
          // but retains its soft wrap opportunity".
          // https://drafts.csswg.org/css-text-3/#collapse
          // In most cases, this is not needed and that collapsed spaces are
          // removed entirely. However, when the first collapsible space is
          // 'nowrap', and the following collapsed space is 'wrap', the
          // collapsed space needs to create a break opportunity.
          // Note that we don't need to generate a break opportunity right
          // after a forced break.
          if (item->Type() != InlineItem::kControl ||
              text_[item->StartOffset()] != kNewlineCharacter) {
            AppendGeneratedBreakOpportunity(layout_object);
          }
        }
      }
    } else {
      // This space is at the beginning of the paragraph. Remove leading spaces
      // as CSS requires.
      insert_space = false;
    }

    // If this space run contains a newline, apply segment break rules.
    if (space_run_has_newline &&
        ShouldRemoveNewline(text_, text_.length(), style, StringView(string, i),
                            style)) {
      insert_space = space_run_has_newline = false;
    }

    // Done computing the interaction with the last item. Start appending.
    start_offset = text_.length();

    DCHECK(i);
    unsigned collapsed_length = i;
    if (insert_space) {
      text_.Append(kSpaceCharacter);
      mapping_builder_.AppendIdentityMapping(1);
      collapsed_length--;
    }
    if (collapsed_length)
      mapping_builder_.AppendCollapsedMapping(collapsed_length);

    // If this space run is at the end of this item, keep whether the
    // collapsible space run has a newline or not in the item.
    if (i == string.length()) {
      end_collapse = InlineItem::kCollapsible;
    }
  } else {
    // If the last item ended with a collapsible space run with segment breaks,
    // apply segment break rules. This may result in removal of the space in the
    // last item.
    if (InlineItem* item = LastItemToCollapseWith(items_)) {
      if (item->EndCollapseType() == InlineItem::kCollapsible &&
          item->IsEndCollapsibleNewline() &&
          ShouldRemoveNewline(text_, item->EndOffset() - 1, item->Style(),
                              string, style)) {
        RemoveTrailingCollapsibleSpace(item);
      }
    }

    start_offset = text_.length();
  }

  // The first run is done. Loop through the rest of runs.
  if (i < string.length()) {
    while (true) {
      // Append the non-space text until we find a collapsible space.
      // |string[i]| is guaranteed not to be a space.
      DCHECK(!Character::IsCollapsibleSpace(string[i]));
      unsigned start_of_non_space = i;
      for (i++; i < string.length(); i++) {
        c = string[i];
        if (Character::IsCollapsibleSpace(c))
          break;
      }
      text_.Append(string, start_of_non_space, i - start_of_non_space);
      mapping_builder_.AppendIdentityMapping(i - start_of_non_space);

      if (i == string.length()) {
        end_collapse = InlineItem::kNotCollapsible;
        break;
      }

      // Process a collapsible space run. First, find the end of the run.
      DCHECK_EQ(c, string[i]);
      DCHECK(Character::IsCollapsibleSpace(c));
      unsigned start_of_spaces = i;
      space_run_has_newline = MoveToEndOfCollapsibleSpaces(string, &i, &c);

      // Because leading spaces are handled before this loop, no need to check
      // cross-item collapsing.
      DCHECK(start_of_spaces);

      // If this space run contains a newline, apply segment break rules.
      bool remove_newline = space_run_has_newline &&
                            ShouldRemoveNewline(text_, text_.length(), style,
                                                StringView(string, i), style);
      if (UNLIKELY(remove_newline)) {
        // |kNotCollapsible| because the newline is removed, not collapsed.
        end_collapse = InlineItem::kNotCollapsible;
        space_run_has_newline = false;
      } else {
        // If the segment break rules did not remove the run, append a space.
        text_.Append(kSpaceCharacter);
        mapping_builder_.AppendIdentityMapping(1);
        start_of_spaces++;
        end_collapse = InlineItem::kCollapsible;
      }

      if (i != start_of_spaces)
        mapping_builder_.AppendCollapsedMapping(i - start_of_spaces);

      // If this space run is at the end of this item, keep whether the
      // collapsible space run has a newline or not in the item.
      if (i == string.length()) {
        break;
      }
    }
  }

  DCHECK_GE(text_.length(), start_offset);
  if (UNLIKELY(text_.length() == start_offset)) {
    AppendEmptyTextItem(layout_object);
    return;
  }

  InlineItem& item = AppendItem(items_, InlineItem::kText, start_offset,
                                text_.length(), layout_object);
  item.SetEndCollapseType(end_collapse, space_run_has_newline);
  DCHECK(!item.IsEmptyItem());
  is_block_level_ = false;
}

template <typename MappingBuilder>
bool InlineItemsBuilderTemplate<MappingBuilder>::
    ShouldInsertBreakOpportunityAfterLeadingPreservedSpaces(
        const String& string,
        const ComputedStyle& style,
        unsigned index) const {
  DCHECK_LE(index, string.length());
  if (UNLIKELY(is_text_combine_))
    return false;
  // Check if we are at a preserved space character and auto-wrap is enabled.
  if (style.ShouldCollapseWhiteSpaces() || !style.ShouldWrapLine() ||
      !string.length() || index >= string.length() ||
      string[index] != kSpaceCharacter) {
    return false;
  }

  // Preserved leading spaces must be at the beginning of the first line or just
  // after a forced break.
  if (index)
    return string[index - 1] == kNewlineCharacter;
  return text_.empty() || text_[text_.length() - 1] == kNewlineCharacter;
}

template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::
    InsertBreakOpportunityAfterLeadingPreservedSpaces(
        const String& string,
        const ComputedStyle& style,
        LayoutText* layout_object,
        unsigned* start) {
  DCHECK(start);
  if (UNLIKELY(ShouldInsertBreakOpportunityAfterLeadingPreservedSpaces(
          string, style, *start))) {
    wtf_size_t end = *start;
    do {
      ++end;
    } while (end < string.length() && string[end] == kSpaceCharacter);
    AppendTextItem(StringView(string, *start, end - *start), layout_object);
    AppendGeneratedBreakOpportunity(layout_object);
    *start = end;
  }
}

// TODO(yosin): We should remove |style| and |string| parameter because of
// except for testing, we can get them from |LayoutText|.
// Even when without whitespace collapsing, control characters (newlines and
// tabs) are in their own control items to make the line breaker not special.
template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::AppendPreserveWhitespace(
    const String& string,
    const ComputedStyle* style,
    LayoutText* layout_object) {
  DCHECK(style);

  // A soft wrap opportunity exists at the end of the sequence of preserved
  // spaces. https://drafts.csswg.org/css-text-3/#white-space-phase-1
  // Due to our optimization to give opportunities before spaces, the
  // opportunity after leading preserved spaces needs a special code in the line
  // breaker. Generate an opportunity to make it easy.
  unsigned start = 0;
  InsertBreakOpportunityAfterLeadingPreservedSpaces(string, *style,
                                                    layout_object, &start);
  for (; start < string.length();) {
    UChar c = string[start];
    if (IsControlItemCharacter(c)) {
      if (c == kNewlineCharacter) {
        if (UNLIKELY(is_text_combine_)) {
          start++;
          AppendTextItem(" ", layout_object);
          continue;
        }
        AppendForcedBreak(layout_object);
        start++;
        // A forced break is not a collapsible space, but following collapsible
        // spaces are leading spaces and they need a special code in the line
        // breaker. Generate an opportunity to make it easy.
        InsertBreakOpportunityAfterLeadingPreservedSpaces(
            string, *style, layout_object, &start);
        continue;
      }
      if (c == kTabulationCharacter) {
        wtf_size_t end = string.Find(
            [](UChar c) { return c != kTabulationCharacter; }, start + 1);
        if (end == kNotFound)
          end = string.length();
        InlineItem& item = AppendTextItem(
            InlineItem::kControl, StringView(string, start, end - start),
            layout_object);
        item.SetTextType(TextItemType::kFlowControl);
        start = end;
        is_score_line_break_disabled_ = true;
        continue;
      }
      // ZWNJ splits item, but it should be text.
      if (c != kZeroWidthNonJoinerCharacter) {
        InlineItem& item = Append(InlineItem::kControl, c, layout_object);
        item.SetTextType(TextItemType::kFlowControl);
        start++;
        continue;
      }
    }

    wtf_size_t end = string.Find(IsControlItemCharacter, start + 1);
    if (end == kNotFound)
      end = string.length();
    AppendTextItem(StringView(string, start, end - start), layout_object);
    start = end;
  }
}

template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::AppendPreserveNewline(
    const String& string,
    const ComputedStyle* style,
    LayoutText* layout_object) {
  for (unsigned start = 0; start < string.length();) {
    if (string[start] == kNewlineCharacter) {
      AppendForcedBreakCollapseWhitespace(layout_object);
      start++;
      continue;
    }

    wtf_size_t end = string.find(kNewlineCharacter, start + 1);
    if (end == kNotFound)
      end = string.length();
    DCHECK_GE(end, start);
    AppendCollapseWhitespace(StringView(string, start, end - start), style,
                             layout_object);
    start = end;
  }
}

template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::AppendForcedBreak(
    LayoutObject* layout_object) {
  DCHECK(layout_object);
  // Combined text should ignore force line break[1].
  // [1] https://drafts.csswg.org/css-writing-modes-3/#text-combine-layout
  DCHECK(!is_text_combine_);
  // At the forced break, add bidi controls to pop all contexts.
  // https://drafts.csswg.org/css-writing-modes-3/#bidi-embedding-breaks
  if (!bidi_context_.empty()) {
    typename MappingBuilder::SourceNodeScope scope(&mapping_builder_, nullptr);
    // These bidi controls need to be associated with the |layout_object| so
    // that items from a LayoutObject are consecutive.
    for (const auto& bidi : base::Reversed(bidi_context_)) {
      AppendOpaque(InlineItem::kBidiControl, bidi.exit, layout_object);
    }
  }

  InlineItem& item =
      Append(InlineItem::kControl, kNewlineCharacter, layout_object);
  item.SetTextType(TextItemType::kForcedLineBreak);

  // A forced break is not a collapsible space, but following collapsible spaces
  // are leading spaces and that they should be collapsed.
  // Pretend that this item ends with a collapsible space, so that following
  // collapsible spaces can be collapsed.
  item.SetEndCollapseType(InlineItem::kCollapsible, false);

  // Then re-add bidi controls to restore the bidi context.
  if (!bidi_context_.empty()) {
    typename MappingBuilder::SourceNodeScope scope(&mapping_builder_, nullptr);
    for (const auto& bidi : bidi_context_) {
      AppendOpaque(InlineItem::kBidiControl, bidi.enter, layout_object);
    }
  }

  DidAppendForcedBreak();
}

template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::
    AppendForcedBreakCollapseWhitespace(LayoutObject* layout_object) {
  // Remove collapsible spaces immediately before a preserved newline.
  RemoveTrailingCollapsibleSpaceIfExists();

  AppendForcedBreak(layout_object);
}

template <typename MappingBuilder>
InlineItem& InlineItemsBuilderTemplate<MappingBuilder>::AppendBreakOpportunity(
    LayoutObject* layout_object) {
  DCHECK(layout_object);
  InlineItem& item = AppendOpaque(InlineItem::kControl,
                                  kZeroWidthSpaceCharacter, layout_object);
  item.SetTextType(TextItemType::kFlowControl);
  return item;
}

// The logic is similar to AppendForcedBreak().
template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::ExitAndEnterSvgTextChunk(
    LayoutText& layout_text) {
  DCHECK(block_flow_->IsSVGText());
  DCHECK(text_chunk_offsets_);

  if (bidi_context_.empty())
    return;
  typename MappingBuilder::SourceNodeScope scope(&mapping_builder_, nullptr);
  // These bidi controls need to be associated with the |layout_text| so
  // that items from a LayoutObject are consecutive.
  for (const auto& bidi : base::Reversed(bidi_context_))
    AppendOpaque(InlineItem::kBidiControl, bidi.exit, &layout_text);

  // Then re-add bidi controls to restore the bidi context.
  for (const auto& bidi : bidi_context_)
    AppendOpaque(InlineItem::kBidiControl, bidi.enter, &layout_text);
}

template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::EnterSvgTextChunk(
    const ComputedStyle* style) {
  if (LIKELY(!block_flow_->IsSVGText() || !text_chunk_offsets_)) {
    return;
  }
  EnterBidiContext(nullptr, style, kLeftToRightIsolateCharacter,
                   kRightToLeftIsolateCharacter,
                   kPopDirectionalIsolateCharacter);
  // This context is automatically popped by Exit(nullptr) in ExitBlock().
}

template <typename MappingBuilder>
InlineItem& InlineItemsBuilderTemplate<MappingBuilder>::Append(
    InlineItem::InlineItemType type,
    UChar character,
    LayoutObject* layout_object) {
  DCHECK_NE(character, kSpaceCharacter);

  text_.Append(character);
  mapping_builder_.AppendIdentityMapping(1);
  unsigned end_offset = text_.length();
  InlineItem& item =
      AppendItem(items_, type, end_offset - 1, end_offset, layout_object);
  is_block_level_ &= item.IsBlockLevel();
  return item;
}

template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::AppendAtomicInline(
    LayoutObject* layout_object) {
  DCHECK(layout_object);
  typename MappingBuilder::SourceNodeScope scope(&mapping_builder_,
                                                 layout_object);
  RestoreTrailingCollapsibleSpaceIfRemoved();
  Append(InlineItem::kAtomicInline, kObjectReplacementCharacter, layout_object);
  has_ruby_ = has_ruby_ || layout_object->IsRubyColumn();

  // When this atomic inline is inside of an inline box, the height of the
  // inline box can be different from the height of the atomic inline. Ensure
  // the inline box creates a box fragment so that its height is available in
  // the fragment tree.
  if (!boxes_.empty()) {
    BoxInfo* current_box = &boxes_.back();
    if (!current_box->should_create_box_fragment)
      current_box->SetShouldCreateBoxFragment(items_);
  }
}

template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::AppendBlockInInline(
    LayoutObject* layout_object) {
  DCHECK(layout_object);
  // Before a block-in-inline is like after a forced break.
  RemoveTrailingCollapsibleSpaceIfExists();
  InlineItem& item = Append(InlineItem::kBlockInInline,
                            kObjectReplacementCharacter, layout_object);
  // After a block-in-inline is like after a forced break. See
  // |AppendForcedBreak|.
  item.SetEndCollapseType(InlineItem::kCollapsible, false);

  if (ShouldUpdateLayoutObject()) {
    // Prevent the inline box from culling to avoid the need of the special
    // logic when traversing.
    DCHECK(!layout_object->Parent() ||
           IsA<LayoutInline>(layout_object->Parent()));
    if (auto* parent = To<LayoutInline>(layout_object->Parent()))
      parent->SetShouldCreateBoxFragment();
  }

  // Block-in-inline produces 3 logical paragraphs. It requires to bisect
  // block-in-inline, before it and after it separately. See
  // `ParagraphLineBreaker`.
  is_bisect_line_break_disabled_ = true;
}

template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::AppendFloating(
    LayoutObject* layout_object) {
  AppendOpaque(InlineItem::kFloating, kObjectReplacementCharacter,
               layout_object);
  has_floats_ = true;
  // Floats/exclusions require computing line heights, which is currently
  // skipped during the bisect. See `ParagraphLineBreaker`.
  is_bisect_line_break_disabled_ = true;
  // `ScoreLineBreaker` supports "simple" floats. See`LineWidths`.
}

template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::AppendOutOfFlowPositioned(
    LayoutObject* layout_object) {
  AppendOpaque(InlineItem::kOutOfFlowPositioned, kObjectReplacementCharacter,
               layout_object);
}

template <typename MappingBuilder>
InlineItem& InlineItemsBuilderTemplate<MappingBuilder>::AppendOpaque(
    InlineItem::InlineItemType type,
    UChar character,
    LayoutObject* layout_object) {
  text_.Append(character);
  mapping_builder_.AppendIdentityMapping(1);
  unsigned end_offset = text_.length();
  InlineItem& item =
      AppendItem(items_, type, end_offset - 1, end_offset, layout_object);
  item.SetEndCollapseType(InlineItem::kOpaqueToCollapsing);
  is_block_level_ &= item.IsBlockLevel();
  return item;
}

template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::AppendOpaque(
    InlineItem::InlineItemType type,
    LayoutObject* layout_object) {
  unsigned end_offset = text_.length();
  InlineItem& item =
      AppendItem(items_, type, end_offset, end_offset, layout_object);
  item.SetEndCollapseType(InlineItem::kOpaqueToCollapsing);
  is_block_level_ &= item.IsBlockLevel();
}

// Removes the collapsible space at the end of |text_| if exists.
template <typename MappingBuilder>
void InlineItemsBuilderTemplate<
    MappingBuilder>::RemoveTrailingCollapsibleSpaceIfExists() {
  if (InlineItem* item = LastItemToCollapseWith(items_)) {
    if (item->EndCollapseType() == InlineItem::kCollapsible) {
      RemoveTrailingCollapsibleSpace(item);
    }
  }
}

// Removes the collapsible space at the end of the specified item.
template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::RemoveTrailingCollapsibleSpace(
    InlineItem* item) {
  DCHECK(item);
  DCHECK_EQ(item->EndCollapseType(), InlineItem::kCollapsible);
  DCHECK_GT(item->Length(), 0u);

  // A forced break pretends that it's a collapsible space, see
  // |AppendForcedBreak()|. It should not be removed.
  if (item->Type() != InlineItem::kText) {
    DCHECK(item->Type() == InlineItem::kControl ||
           item->Type() == InlineItem::kBlockInInline);
    return;
  }

  DCHECK_GT(item->EndOffset(), item->StartOffset());
  unsigned space_offset = item->EndOffset() - 1;
  DCHECK_EQ(text_[space_offset], kSpaceCharacter);
  text_.erase(space_offset);
  mapping_builder_.CollapseTrailingSpace(space_offset);

  // Keep the item even if the length became zero. This is not needed for
  // the layout purposes, but needed to maintain LayoutObject states. See
  // |AppendEmptyTextItem()|.
  item->SetEndOffset(item->EndOffset() - 1);
  item->SetEndCollapseType(InlineItem::kCollapsed);

  // Trailing spaces can be removed across non-character items.
  // Adjust their offsets if after the removed index.
  for (item++; item != items_->end(); item++) {
    item->SetOffset(item->StartOffset() - 1, item->EndOffset() - 1);
  }
}

// Restore removed collapsible space at the end of items.
template <typename MappingBuilder>
void InlineItemsBuilderTemplate<
    MappingBuilder>::RestoreTrailingCollapsibleSpaceIfRemoved() {
  if (InlineItem* last_item = LastItemToCollapseWith(items_)) {
    if (last_item->EndCollapseType() == InlineItem::kCollapsed) {
      RestoreTrailingCollapsibleSpace(last_item);
    }
  }
}

// Restore removed collapsible space at the end of the specified item.
template <typename MappingBuilder>
void InlineItemsBuilderTemplate<
    MappingBuilder>::RestoreTrailingCollapsibleSpace(InlineItem* item) {
  DCHECK(item);
  DCHECK(item->EndCollapseType() == InlineItem::kCollapsed);

  mapping_builder_.RestoreTrailingCollapsibleSpace(
      To<LayoutText>(*item->GetLayoutObject()), item->EndOffset());

  // TODO(kojii): Implement StringBuilder::insert().
  if (text_.length() == item->EndOffset()) {
    text_.Append(' ');
  } else {
    String current = text_.ToString();
    text_.Clear();
    text_.Append(StringView(current, 0, item->EndOffset()));
    text_.Append(' ');
    text_.Append(StringView(current, item->EndOffset()));
  }

  item->SetEndOffset(item->EndOffset() + 1);
  item->SetEndCollapseType(InlineItem::kCollapsible);

  for (item++; item != items_->end(); item++) {
    item->SetOffset(item->StartOffset() + 1, item->EndOffset() + 1);
  }
}

template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::EnterBidiContext(
    LayoutObject* node,
    UChar enter,
    UChar exit) {
  AppendOpaque(InlineItem::kBidiControl, enter);
  bidi_context_.push_back(BidiContext{node, enter, exit});
  has_bidi_controls_ = true;
}

template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::EnterBidiContext(
    LayoutObject* node,
    const ComputedStyle* style,
    UChar ltr_enter,
    UChar rtl_enter,
    UChar exit) {
  EnterBidiContext(node, IsLtr(style->Direction()) ? ltr_enter : rtl_enter,
                   exit);
}

template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::EnterBlock(
    const ComputedStyle* style) {
  // Handle bidi-override on the block itself.
  if (style->RtlOrdering() == EOrder::kLogical) {
    EnterSvgTextChunk(style);
    switch (style->GetUnicodeBidi()) {
      case UnicodeBidi::kNormal:
      case UnicodeBidi::kEmbed:
      case UnicodeBidi::kIsolate:
        // Isolate and embed values are enforced by default and redundant on the
        // block elements.
        // Direction is handled as the paragraph level by
        // NGBidiParagraph::SetParagraph().
        if (style->Direction() == TextDirection::kRtl)
          has_bidi_controls_ = true;
        break;
      case UnicodeBidi::kBidiOverride:
      case UnicodeBidi::kIsolateOverride:
        EnterBidiContext(nullptr, style, kLeftToRightOverrideCharacter,
                         kRightToLeftOverrideCharacter,
                         kPopDirectionalFormattingCharacter);
        break;
      case UnicodeBidi::kPlaintext:
        // Plaintext is handled as the paragraph level by
        // NGBidiParagraph::SetParagraph().
        has_bidi_controls_ = true;
        // It's not easy to compute which lines will change with `unicode-bidi:
        // plaintext`. Since it is quite uncommon that just disable line cache.
        has_unicode_bidi_plain_text_ = true;
        break;
    }
  } else {
    DCHECK_EQ(style->RtlOrdering(), EOrder::kVisual);
    EnterBidiContext(nullptr, style, kLeftToRightOverrideCharacter,
                     kRightToLeftOverrideCharacter,
                     kPopDirectionalFormattingCharacter);
  }

  if (style->IsDisplayListItem() && style->ListStyleType()) {
    is_block_level_ = false;
  }
}

template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::EnterInline(
    LayoutInline* node) {
  DCHECK(node);

  // https://drafts.csswg.org/css-writing-modes-3/#bidi-control-codes-injection-table
  const ComputedStyle* style = node->Style();
  if (style->RtlOrdering() == EOrder::kLogical) {
    switch (style->GetUnicodeBidi()) {
      case UnicodeBidi::kNormal:
        break;
      case UnicodeBidi::kEmbed:
        EnterBidiContext(node, style, kLeftToRightEmbedCharacter,
                         kRightToLeftEmbedCharacter,
                         kPopDirectionalFormattingCharacter);
        break;
      case UnicodeBidi::kBidiOverride:
        EnterBidiContext(node, style, kLeftToRightOverrideCharacter,
                         kRightToLeftOverrideCharacter,
                         kPopDirectionalFormattingCharacter);
        break;
      case UnicodeBidi::kIsolate:
        EnterBidiContext(node, style, kLeftToRightIsolateCharacter,
                         kRightToLeftIsolateCharacter,
                         kPopDirectionalIsolateCharacter);
        break;
      case UnicodeBidi::kPlaintext:
        has_unicode_bidi_plain_text_ = true;
        EnterBidiContext(node, kFirstStrongIsolateCharacter,
                         kPopDirectionalIsolateCharacter);
        break;
      case UnicodeBidi::kIsolateOverride:
        EnterBidiContext(node, kFirstStrongIsolateCharacter,
                         kPopDirectionalIsolateCharacter);
        EnterBidiContext(node, style, kLeftToRightOverrideCharacter,
                         kRightToLeftOverrideCharacter,
                         kPopDirectionalFormattingCharacter);
        break;
    }
  }

  AppendOpaque(InlineItem::kOpenTag, node);

  if (!NeedsBoxInfo())
    return;

  // Set |ShouldCreateBoxFragment| of the parent box if needed.
  BoxInfo* current_box =
      &boxes_.emplace_back(items_->size() - 1, items_->back());
  if (boxes_.size() > 1) {
    BoxInfo* parent_box = std::prev(current_box);
    if (!parent_box->should_create_box_fragment &&
        parent_box->ShouldCreateBoxFragmentForChild(*current_box)) {
      parent_box->SetShouldCreateBoxFragment(items_);
    }
  }
}

template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::ExitBlock() {
  Exit(nullptr);

  // Segment Break Transformation Rules[1] defines to keep trailing new lines,
  // but it will be removed in Phase II[2]. We prefer not to add trailing new
  // lines and collapsible spaces in Phase I.
  RemoveTrailingCollapsibleSpaceIfExists();
}

template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::ExitInline(
    LayoutObject* node) {
  DCHECK(node);

  if (NeedsBoxInfo()) {
    BoxInfo* current_box = &boxes_.back();
    if (!current_box->should_create_box_fragment) {
      // Set ShouldCreateBoxFragment if this inline box is empty so that we can
      // compute its position/size correctly. Check this by looking for any
      // non-empty items after the last |kOpenTag|.
      const unsigned open_item_index = current_box->item_index;
      DCHECK_GE(items_->size(), open_item_index + 1);
      DCHECK_EQ((*items_)[open_item_index].Type(), InlineItem::kOpenTag);
      for (unsigned i = items_->size() - 1;; --i) {
        InlineItem& item = (*items_)[i];
        if (i == open_item_index) {
          DCHECK_EQ(i, current_box->item_index);
          // TODO(kojii): <area> element fails to hit-test when we don't cull.
          if (!IsA<HTMLAreaElement>(item.GetLayoutObject()->GetNode()))
            item.SetShouldCreateBoxFragment();
          break;
        }
        DCHECK_GT(i, current_box->item_index);
        if (item.IsEmptyItem()) {
          // float, abspos, collapsed space(<div>ab <span> </span>).
          // See editing/caret/empty_inlines.html
          // See also [1] for empty line box.
          // [1] https://drafts.csswg.org/css2/visuren.html#phantom-line-box
          continue;
        }
        if (item.IsCollapsibleSpaceOnly()) {
          // Because we can't collapse trailing spaces until next node, we
          // create box fragment for it: <div>ab<span> </span></div>
          // See editing/selection/mixed-editability-10.html
          continue;
        }
        break;
      }
    }

    boxes_.pop_back();
  }

  AppendOpaque(InlineItem::kCloseTag, node);

  Exit(node);
}

template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::Exit(LayoutObject* node) {
  while (!bidi_context_.empty() && bidi_context_.back().node == node) {
    AppendOpaque(InlineItem::kBidiControl, bidi_context_.back().exit);
    bidi_context_.pop_back();
  }
}

template <typename MappingBuilder>
bool InlineItemsBuilderTemplate<MappingBuilder>::MayBeBidiEnabled() const {
  return !text_.Is8Bit() || HasBidiControls();
}

template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::DidFinishCollectInlines(
    InlineNodeData* data) {
  data->text_content = ToString();

  // Set |is_bidi_enabled_| for all UTF-16 strings for now, because at this
  // point the string may or may not contain RTL characters.
  // |SegmentText()| will analyze the text and reset |is_bidi_enabled_| if it
  // doesn't contain any RTL characters.
  data->is_bidi_enabled_ = MayBeBidiEnabled();
  data->has_floats_ = has_floats_;
  data->has_initial_letter_box_ = has_initial_letter_box_;
  data->has_ruby_ = has_ruby_;
  data->is_block_level_ = IsBlockLevel();
  data->changes_may_affect_earlier_lines_ = HasUnicodeBidiPlainText();
  data->is_bisect_line_break_disabled_ = is_bisect_line_break_disabled_;
  data->is_score_line_break_disabled_ = is_score_line_break_disabled_;

#if DCHECK_IS_ON()
  data->CheckConsistency();
#endif
}

template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::SetHasInititialLetterBox() {
  DCHECK(!items_->empty());
  DCHECK(!has_initial_letter_box_);
  has_initial_letter_box_ = true;
  // Floats/exclusions require computing line heights, which is currently
  // skipped during the bisect. See `ParagraphLineBreaker`.
  is_bisect_line_break_disabled_ = true;
  is_score_line_break_disabled_ = true;
}

template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::SetIsSymbolMarker() {
  DCHECK(!items_->empty());
  items_->back().SetIsSymbolMarker();
}

template <typename MappingBuilder>
bool InlineItemsBuilderTemplate<MappingBuilder>::ShouldUpdateLayoutObject()
    const {
  return true;
}

// Ensure this LayoutObject IsInLayoutNGInlineFormattingContext and does not
// have associated NGPaintFragment.
template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::ClearInlineFragment(
    LayoutObject* object) {
  object->SetIsInLayoutNGInlineFormattingContext(true);
}

template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::ClearNeedsLayout(
    LayoutObject* object) {
  // |CollectInlines()| for the pre-layout does not |ClearNeedsLayout|. It is
  // done during the actual layout because re-layout may not require
  // |CollectInlines()|.
  object->ClearNeedsCollectInlines();
  ClearInlineFragment(object);

  // Reset previous items if they cannot be reused to prevent stale items
  // for subsequent layouts. Items that can be reused have already been
  // added to the builder.
  if (object->IsText())
    To<LayoutText>(object)->ClearInlineItems();
}

template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::UpdateShouldCreateBoxFragment(
    LayoutInline* object) {
  object->UpdateShouldCreateBoxFragment();
}

// |OffsetMappingBuilder| doesn't change states of |LayoutObject|
template <>
bool InlineItemsBuilderTemplate<
    OffsetMappingBuilder>::ShouldUpdateLayoutObject() const {
  return false;
}

// |OffsetMappingBuilder| doesn't change states of |LayoutObject|
template <>
void InlineItemsBuilderTemplate<OffsetMappingBuilder>::ClearNeedsLayout(
    LayoutObject* object) {}

// |OffsetMappingBuilder| doesn't change states of |LayoutObject|
template <>
void InlineItemsBuilderTemplate<OffsetMappingBuilder>::ClearInlineFragment(
    LayoutObject*) {}

// |OffsetMappingBuilder| doesn't change states of |LayoutInline|
template <>
void InlineItemsBuilderTemplate<
    OffsetMappingBuilder>::UpdateShouldCreateBoxFragment(LayoutInline*) {}

template <typename MappingBuilder>
void InlineItemsBuilderTemplate<MappingBuilder>::BidiContext::Trace(
    Visitor* visitor) const {
  visitor->Trace(node);
}

template class CORE_TEMPLATE_EXPORT
    InlineItemsBuilderTemplate<EmptyOffsetMappingBuilder>;
template class CORE_TEMPLATE_EXPORT
    InlineItemsBuilderTemplate<OffsetMappingBuilder>;

}  // namespace blink
