// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/layout/paragraph.h"

#include "base/i18n/char_iterator.h"
#include "cobalt/base/unicode/character_values.h"

#include "third_party/icu/source/common/unicode/ubidi.h"

namespace cobalt {
namespace layout {

namespace {

int ConvertBaseDirectionToBidiLevel(BaseDirection base_direction) {
  return base_direction == kRightToLeftBaseDirection ? 1 : 0;
}

}  // namespace

Paragraph::Paragraph(
    const icu::Locale& locale, BaseDirection base_direction,
    const DirectionalEmbeddingStack& directional_embedding_stack,
    icu::BreakIterator* line_break_iterator,
    icu::BreakIterator* character_break_iterator)
    : locale_(locale),
      base_direction_(base_direction),
      directional_embedding_stack_(directional_embedding_stack),
      line_break_iterator_(line_break_iterator),
      character_break_iterator_(character_break_iterator),
      is_closed_(false),
      last_retrieved_run_index_(0) {
  DCHECK(line_break_iterator_);
  DCHECK(character_break_iterator_);

  level_runs_.push_back(
      BidiLevelRun(0, ConvertBaseDirectionToBidiLevel(base_direction)));

  // Walk through the passed in directional embedding stack and add each
  // embedding to the text. This allows a paragraph to continue the directional
  // state of a prior paragraph.
  DCHECK(unicode_text_.isEmpty());
  for (size_t i = 0; i < directional_embedding_stack_.size(); ++i) {
    if (directional_embedding_stack_[i] == kRightToLeftDirectionalEmbedding) {
      unicode_text_ += base::unicode::kRightToLeftEmbeddingCharacter;
    } else {
      unicode_text_ += base::unicode::kLeftToRightEmbeddingCharacter;
    }
  }
}

int32 Paragraph::AppendUtf8String(const std::string& utf8_string) {
  return AppendUtf8String(utf8_string, kNoTextTransform);
}

int32 Paragraph::AppendUtf8String(const std::string& utf8_string,
                                  TextTransform transform) {
  int32 start_position = GetTextEndPosition();
  DCHECK(!is_closed_);
  if (!is_closed_) {
    icu::UnicodeString unicode_string =
        icu::UnicodeString::fromUTF8(utf8_string);
    if (transform == kUppercaseTextTransform) {
      unicode_string.toUpper(locale_);
    }

    unicode_text_ += unicode_string;
  }

  return start_position;
}

int32 Paragraph::AppendCodePoint(CodePoint code_point) {
  // TODO: Switch from appending directional embedding characters to
  // using directional isolate characters as soon as we upgrade to the latest
  // version of ICU. Our current version doesn't support them.
  int32 start_position = GetTextEndPosition();
  DCHECK(!is_closed_);
  if (!is_closed_) {
    switch (code_point) {
      case kLeftToRightEmbedCodePoint:
        // If this is a directional embedding that is being added, then add it
        // to the directional embedding stack. This guarantees that a
        // corresponding pop directional formatting will later be added to the
        // text and allows later paragraphs to copy the directional state.
        // http://unicode.org/reports/tr9/#Explicit_Directional_Embeddings
        directional_embedding_stack_.push_back(
            kLeftToRightDirectionalEmbedding);
        unicode_text_ += base::unicode::kLeftToRightEmbeddingCharacter;
        break;
      case kLineFeedCodePoint:
        unicode_text_ += base::unicode::kNewLineCharacter;
        break;
      case kNoBreakSpaceCodePoint:
        unicode_text_ += base::unicode::kNoBreakSpaceCharacter;
        break;
      case kObjectReplacementCharacterCodePoint:
        unicode_text_ += base::unicode::kObjectReplacementCharacter;
        break;
      case kPopDirectionalFormattingCharacterCodePoint:
        directional_embedding_stack_.pop_back();
        unicode_text_ += base::unicode::kPopDirectionalFormattingCharacter;
        break;
      case kRightToLeftEmbedCodePoint:
        // If this is a directional embedding that is being added, then add it
        // to the directional embedding stack. This guarantees that a
        // corresponding pop directional formatting will later be added to the
        // text and allows later paragraphs to to copy the directional state.
        // http://unicode.org/reports/tr9/#Explicit_Directional_Embeddings
        directional_embedding_stack_.push_back(
            kRightToLeftDirectionalEmbedding);
        unicode_text_ += base::unicode::kRightToLeftEmbeddingCharacter;
        break;
    }
  }
  return start_position;
}

bool Paragraph::FindBreakPosition(const scoped_refptr<dom::FontList>& used_font,
                                  int32 start_position, int32 end_position,
                                  LayoutUnit available_width,
                                  bool should_collapse_trailing_white_space,
                                  bool allow_overflow,
                                  Paragraph::BreakPolicy break_policy,
                                  int32* break_position,
                                  LayoutUnit* break_width) {
  DCHECK(is_closed_);

  *break_position = start_position;
  *break_width = LayoutUnit();

  // If overflow isn't allowed and there is no available width, then there is
  // nothing to do. No break position can be found.
  if (!allow_overflow && available_width <= LayoutUnit()) {
    return false;
  }

  // Normal overflow is not allowed when the break policy is break-word, so
  // only attempt to find normal break positions if the break policy is normal
  // or there is width still available.
  // https://www.w3.org/TR/css-text-3/#overflow-wrap
  // NOTE: Normal break positions are still found when the break policy is
  // break-word and overflow has not yet occurred because width calculations are
  // more accurate between word intervals, as these properly take into complex
  // shaping. Due to this, calculating the width using word intervals until
  // reaching the final word that must be broken between grapheme clusters
  // results in less accumulated width calculation error.
  if (break_policy == kBreakPolicyNormal || available_width > LayoutUnit()) {
    // Only allow normal overflow if the policy is |kBreakPolicyNormal|;
    // otherwise, overflowing via normal breaking would be too greedy in what it
    // included and overflow wrapping will be attempted via word breaking.
    bool allow_normal_overflow =
        allow_overflow && break_policy == kBreakPolicyNormal;

    // Find the last available normal break position. |break_position| and
    // |break_width| will be updated with the position of the last available
    // break position.
    FindIteratorBreakPosition(
        used_font, line_break_iterator_, start_position, end_position,
        available_width, should_collapse_trailing_white_space,
        allow_normal_overflow, break_position, break_width);
  }

  // If break word is the break policy, attempt to break unbreakable "words" at
  // an arbitrary point, while still maintaining grapheme clusters as
  // indivisible units.
  // https://www.w3.org/TR/css3-text/#overflow-wrap
  if (break_policy == kBreakPolicyBreakWord) {
    // Only continue allowing overflow if the break position has not moved from
    // start, meaning that no normal break positions were found.
    allow_overflow = allow_overflow && (*break_position == start_position);

    // Find the last available break-word break position. |break_position| and
    // |break_width| will be updated with the position of the last available
    // break position. The search begins at the location of the last normal
    // break position that fit within the available width.
    FindIteratorBreakPosition(
        used_font, character_break_iterator_, *break_position, end_position,
        available_width, false, allow_overflow, break_position, break_width);
  }

  // No usable break position was found if the break position has not moved
  // from the start position.
  return *break_position > start_position;
}

int32 Paragraph::GetNextBreakPosition(int32 position,
                                      BreakPolicy break_policy) {
  icu::BreakIterator* break_iterator = GetBreakIterator(break_policy);
  break_iterator->setText(unicode_text_);
  return break_iterator->following(position);
}

int32 Paragraph::GetPreviousBreakPosition(int32 position,
                                          BreakPolicy break_policy) {
  icu::BreakIterator* break_iterator = GetBreakIterator(break_policy);
  break_iterator->setText(unicode_text_);
  return break_iterator->preceding(position);
}

bool Paragraph::IsBreakPosition(int32 position, BreakPolicy break_policy) {
  icu::BreakIterator* break_iterator = GetBreakIterator(break_policy);
  break_iterator->setText(unicode_text_);
  return break_iterator->isBoundary(position) == TRUE;
}

std::string Paragraph::RetrieveUtf8SubString(int32 start_position,
                                             int32 end_position) const {
  return RetrieveUtf8SubString(start_position, end_position, kLogicalTextOrder);
}

std::string Paragraph::RetrieveUtf8SubString(int32 start_position,
                                             int32 end_position,
                                             TextOrder text_order) const {
  std::string utf8_sub_string;

  if (start_position < end_position) {
    icu::UnicodeString unicode_sub_string =
        unicode_text_.tempSubStringBetween(start_position, end_position);

    // Odd bidi levels signify RTL directionality. If the text is being
    // retrieved in visual order and has RTL directionality, then reverse the
    // text.
    if (text_order == kVisualTextOrder && IsRTL(start_position)) {
      unicode_sub_string.reverse();
    }

    unicode_sub_string.toUTF8String(utf8_sub_string);
  }

  return utf8_sub_string;
}

const char16* Paragraph::GetTextBuffer() const {
  return unicode_text_.getBuffer();
}

const icu::Locale& Paragraph::GetLocale() const { return locale_; }

BaseDirection Paragraph::GetBaseDirection() const { return base_direction_; }

BaseDirection Paragraph::GetDirectionalEmbeddingStackDirection() const {
  size_t stack_size = directional_embedding_stack_.size();
  if (stack_size > 0) {
    if (directional_embedding_stack_[stack_size - 1] ==
        kRightToLeftDirectionalEmbedding) {
      return kRightToLeftBaseDirection;
    } else {
      return kLeftToRightBaseDirection;
    }
  } else {
    return base_direction_;
  }
}

int Paragraph::GetBidiLevel(int32 position) const {
  return level_runs_[GetRunIndex(position)].level_;
}

bool Paragraph::IsRTL(int32 position) const {
  return (GetBidiLevel(position) % 2) == 1;
}

bool Paragraph::IsCollapsibleWhiteSpace(int32 position) const {
  // Only check for the space character. Other collapsible white space
  // characters will have already been converted into the space characters and
  // do not need to be checked against.
  return unicode_text_[position] == base::unicode::kSpaceCharacter;
}

bool Paragraph::GetNextRunPosition(int32 position,
                                   int32* next_run_position) const {
  size_t next_run_index = GetRunIndex(position) + 1;
  if (next_run_index >= level_runs_.size()) {
    return false;
  } else {
    *next_run_position = level_runs_[next_run_index].start_position_;
    return true;
  }
}

int32 Paragraph::GetTextEndPosition() const { return unicode_text_.length(); }

const Paragraph::DirectionalEmbeddingStack&
Paragraph::GetDirectionalEmbeddingStack() const {
  return directional_embedding_stack_;
}

void Paragraph::Close() {
  DCHECK(!is_closed_);
  if (!is_closed_) {
    // Terminate all of the explicit directional embeddings that were previously
    // added. However, do not clear the stack. A subsequent paragraph may need
    // to copy it.
    // http://unicode.org/reports/tr9/#Terminating_Explicit_Directional_Embeddings_and_Overrides
    for (size_t count = directional_embedding_stack_.size(); count > 0;
         --count) {
      unicode_text_ += base::unicode::kPopDirectionalFormattingCharacter;
    }

    is_closed_ = true;
    GenerateBidiLevelRuns();
  }
}

bool Paragraph::IsClosed() const { return is_closed_; }

Paragraph::BreakPolicy Paragraph::GetBreakPolicyFromWrapOpportunityPolicy(
    WrapOpportunityPolicy wrap_opportunity_policy,
    bool does_style_allow_break_word) {
  // Initialize here to prevent a potential compiler warning.
  BreakPolicy break_policy = kBreakPolicyNormal;
  switch (wrap_opportunity_policy) {
    case kWrapOpportunityPolicyNormal:
      break_policy = kBreakPolicyNormal;
      break;
    case kWrapOpportunityPolicyBreakWord:
      break_policy = kBreakPolicyBreakWord;
      break;
    case kWrapOpportunityPolicyBreakWordOrNormal:
      break_policy = does_style_allow_break_word ? kBreakPolicyBreakWord
                                                 : kBreakPolicyNormal;
      break;
  }
  return break_policy;
}

icu::BreakIterator* Paragraph::GetBreakIterator(BreakPolicy break_policy) {
  return break_policy == kBreakPolicyNormal ? line_break_iterator_
                                            : character_break_iterator_;
}

void Paragraph::FindIteratorBreakPosition(
    const scoped_refptr<dom::FontList>& used_font,
    icu::BreakIterator* const break_iterator, int32 start_position,
    int32 end_position, LayoutUnit available_width,
    bool should_collapse_trailing_white_space, bool allow_overflow,
    int32* break_position, LayoutUnit* break_width) {
  // Iterate through break segments, beginning from the passed in start
  // position. Continue until TryIncludeSegmentWithinAvailableWidth() returns
  // false, indicating that no more segments can be included.
  break_iterator->setText(unicode_text_);
  for (int32 segment_end = break_iterator->following(start_position);
       segment_end != icu::BreakIterator::DONE && segment_end < end_position;
       segment_end = break_iterator->next()) {
    if (!TryIncludeSegmentWithinAvailableWidth(
            used_font, *break_position, segment_end, available_width,
            should_collapse_trailing_white_space, &allow_overflow,
            break_position, break_width)) {
      break;
    }
  }
}

bool Paragraph::TryIncludeSegmentWithinAvailableWidth(
    const scoped_refptr<dom::FontList>& used_font, int32 segment_start,
    int32 segment_end, LayoutUnit available_width,
    bool should_collapse_trailing_white_space, bool* allow_overflow,
    int32* break_position, LayoutUnit* break_width) {
  // Add the width of the segment encountered to the total, until reaching one
  // that causes the available width to be exceeded. The previous break position
  // is the last usable one. However, if overflow is allowed and no segment has
  // been found, then the first overflowing segment is accepted.
  LayoutUnit segment_width = LayoutUnit(used_font->GetTextWidth(
      unicode_text_.getBuffer() + segment_start, segment_end - segment_start,
      IsRTL(segment_start), NULL));

  // If trailing white space is being collapsed, then it will not be included
  // when determining if the segment can fit within the available width.
  // However, it is still added to |break_width|, as it will impact the
  // width available to additional segments.
  LayoutUnit collapsible_trailing_white_space_width =
      LayoutUnit(should_collapse_trailing_white_space &&
                         IsCollapsibleWhiteSpace(segment_end - 1)
                     ? used_font->GetSpaceWidth()
                     : 0);

  if (!*allow_overflow &&
      *break_width + segment_width - collapsible_trailing_white_space_width >
          available_width) {
    return false;
  }

  *break_position = segment_end;
  *break_width += segment_width;

  if (*allow_overflow) {
    *allow_overflow = false;
    if (*break_width >= available_width) {
      return false;
    }
  }

  return true;
}

namespace {

// This class guarantees that a UBiDi object is closed when it goes out of
// scope.
class ScopedUBiDiPtr {
 public:
  explicit ScopedUBiDiPtr(UBiDi* ubidi) : ubidi_(ubidi) {}

  ~ScopedUBiDiPtr() { ubidi_close(ubidi_); }

  UBiDi* get() const { return ubidi_; }

 private:
  UBiDi* ubidi_;
};

}  // namespace

// Should only be called by Close().
void Paragraph::GenerateBidiLevelRuns() {
  DCHECK(is_closed_);

  UErrorCode error = U_ZERO_ERROR;

  // Create a scoped ubidi ptr when opening the text. It guarantees that the
  // UBiDi object will be closed when it goes out of scope.
  ScopedUBiDiPtr ubidi(ubidi_openSized(unicode_text_.length(), 0, &error));
  if (U_FAILURE(error)) {
    return;
  }

  ubidi_setPara(ubidi.get(), unicode_text_.getBuffer(), unicode_text_.length(),
                UBiDiLevel(ConvertBaseDirectionToBidiLevel(base_direction_)),
                NULL, &error);
  if (U_FAILURE(error)) {
    return;
  }

  const int runs = ubidi_countRuns(ubidi.get(), &error);
  if (U_FAILURE(error)) {
    return;
  }

  level_runs_.clear();
  level_runs_.reserve(size_t(runs));

  int run_start_position = 0;
  int run_end_position;
  UBiDiLevel run_ubidi_level;

  int32 text_end_position = GetTextEndPosition();
  while (run_start_position < text_end_position) {
    ubidi_getLogicalRun(ubidi.get(), run_start_position, &run_end_position,
                        &run_ubidi_level);
    level_runs_.push_back(
        BidiLevelRun(run_start_position, static_cast<int>(run_ubidi_level)));
    run_start_position = run_end_position;
  }
}

size_t Paragraph::GetRunIndex(int32 position) const {
  DCHECK(is_closed_);

  // Start the search from the last retrieved index. This is tracked because
  // the next retrieved run index is almost always either the same as the last
  // retrieved one or a neighbor of it, so this reduces nearly all calls to only
  // requiring a couple comparisons.

  // First iterate up the level run vector, finding the first element that has
  // a start position larger than the passed in position. The upward iteration
  // is stopped if the last element is reached.
  while (last_retrieved_run_index_ < level_runs_.size() - 1 &&
         level_runs_[last_retrieved_run_index_].start_position_ < position) {
    ++last_retrieved_run_index_;
  }
  // Iterate down the level run vector until a start position smaller than or
  // equal to the passed in position is reached. This run contains the passed in
  // position. The start position of the first run has a value of 0 and serves
  // as a sentinel for the loop.
  while (level_runs_[last_retrieved_run_index_].start_position_ > position) {
    --last_retrieved_run_index_;
  }

  return last_retrieved_run_index_;
}

}  // namespace layout
}  // namespace cobalt
