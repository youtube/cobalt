/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/layout/paragraph.h"
#include "base/i18n/bidi_line_iterator.h"
#include "base/i18n/char_iterator.h"
#include "base/i18n/rtl.h"
#include "base/string16.h"

namespace cobalt {
namespace layout {

namespace {

base::i18n::TextDirection ConvertBaseDirectionToTextDirection(
    Paragraph::BaseDirection base_direction) {
  base::i18n::TextDirection text_direction;

  switch (base_direction) {
    case Paragraph::kRightToLeftBaseDirection:
      text_direction = base::i18n::RIGHT_TO_LEFT;
      break;
    case Paragraph::kLeftToRightBaseDirection:
      text_direction = base::i18n::LEFT_TO_RIGHT;
      break;
    case Paragraph::kUnknownBaseDirection:
    default:
      text_direction = base::i18n::UNKNOWN_DIRECTION;
  }

  return text_direction;
}

int ConvertBaseDirectionToBidiLevel(Paragraph::BaseDirection base_direction) {
  return base_direction == Paragraph::kRightToLeftBaseDirection ? 1 : 0;
}

}  // namespace

Paragraph::Paragraph(icu::BreakIterator* line_break_iterator,
                     Paragraph::BaseDirection base_direction)
    : line_break_iterator_(line_break_iterator),
      base_direction_(base_direction),
      is_closed_(false),
      last_retrieved_run_index_(0) {
  DCHECK(line_break_iterator_);

  level_runs_.push_back(
      BidiLevelRun(0, ConvertBaseDirectionToBidiLevel(base_direction)));
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
      // TODO(***REMOVED***): Pass in locale
      unicode_string.toUpper();
    }

    unicode_text_ += unicode_string;
  }
  return start_position;
}

int32 Paragraph::AppendCodePoint(CodePoint code_point) {
  int32 start_position = GetTextEndPosition();
  DCHECK(!is_closed_);
  if (!is_closed_) {
    switch (code_point) {
      case kLineFeedCodePoint:
        unicode_text_ += UChar(0x0a);
        break;
      case kObjectReplacementCharacterCodePoint:
        unicode_text_ += UChar(0xfffc);
        break;
    }
  }
  return start_position;
}

bool Paragraph::CalculateBreakPosition(
    const scoped_refptr<render_tree::Font>& used_font, int32 start_position,
    int32 end_position, float available_width, bool allow_overflow,
    OverflowWrap overflow_wrap, int32* break_position, float* break_width) {
  DCHECK(is_closed_);

  if (!allow_overflow && available_width <= 0) {
    return false;
  }

  if (end_position == -1) {
    end_position = GetTextEndPosition();
  }

  // Always initially try to find a soft wrap break position. However, only
  // allow soft wrap overflow if overflow wrap is |kSoftWrapOverflowWrap|;
  // otherwise, overflowing via soft wrap would be too greedy in what it
  // included.
  bool allow_soft_wrap_overflow =
      allow_overflow && overflow_wrap == kSoftWrapOverflowWrap;

  CalculateSoftWrapBreakPosition(used_font, start_position, end_position,
                                 available_width, allow_soft_wrap_overflow,
                                 break_position, break_width);

  // Check to see if an acceptable soft wrap break position was found. If not,
  // |kBreakWordOverflowWrap| allows for breaking an unbreakable "word" at an
  // arbitrary point when there are no otherwise-acceptable break points in the
  // line. (http://www.w3.org/TR/css3-text/#overflow-wrap)
  if (*break_position == start_position && allow_overflow &&
      overflow_wrap == kBreakWordOverflowWrap) {
    CalculateCharacterBreakPosition(used_font, start_position, end_position,
                                    available_width, allow_overflow,
                                    break_position, break_width);
  }

  // Verify that a usable break position was found between the start and end
  // position.
  return *break_position > start_position;
}

float Paragraph::CalculateSubStringWidth(
    const scoped_refptr<render_tree::Font>& used_font, int32 start_position,
    int32 end_position) const {
  std::string utf8_sub_string =
      RetrieveUtf8SubString(start_position, end_position);
  return used_font->GetBounds(utf8_sub_string).width();
}

std::string Paragraph::RetrieveUtf8SubString(int32 start_position,
                                             int32 end_position) const {
  std::string utf8_sub_string;

  if (start_position < end_position) {
    icu::UnicodeString unicode_sub_string =
        unicode_text_.tempSubStringBetween(start_position, end_position);
    unicode_sub_string.toUTF8String(utf8_sub_string);
  }

  return utf8_sub_string;
}

Paragraph::BaseDirection Paragraph::GetBaseDirection() const {
  return base_direction_;
}

int Paragraph::GetBidiLevel(int32 position) const {
  return level_runs_[GetRunIndex(position)].level_;
}

bool Paragraph::IsSpace(int32 position) const {
  return unicode_text_[position] == ' ';
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

void Paragraph::Close() {
  DCHECK(!is_closed_);
  if (!is_closed_) {
    is_closed_ = true;
    GenerateBidiLevelRuns();
  }
}

bool Paragraph::IsClosed() const { return is_closed_; }

void Paragraph::CalculateCharacterBreakPosition(
    const scoped_refptr<render_tree::Font>& used_font, int32 start_position,
    int32 end_position, float available_width, bool allow_overflow,
    int32* break_position, float* break_width) {
  // Iterate through characters, beginning from the passed in start position.
  // Continue until TryIncludeSegmentWithinAvailableWidth() returns false,
  // indicating that no more segments can be included.
  base::i18n::UTF16CharIterator iter(unicode_text_.getBuffer() + start_position,
                                     size_t(end_position - start_position));
  int32 segment_start = start_position;
  while (iter.Advance()) {
    int32 segment_end = start_position + iter.array_pos();

    if (!TryIncludeSegmentWithinAvailableWidth(
            used_font, segment_start, segment_end, available_width,
            &allow_overflow, break_position, break_width)) {
      break;
    }

    segment_start = segment_end;
  }
}

void Paragraph::CalculateSoftWrapBreakPosition(
    const scoped_refptr<render_tree::Font>& used_font, int32 start_position,
    int32 end_position, float available_width, bool allow_overflow,
    int32* break_position, float* break_width) {
  // Iterate through soft wrap locations, beginning from the passed in start
  // position. Continue until TryIncludeSegmentWithinAvailableWidth() returns
  // false, indicating that no more segments can be included.
  line_break_iterator_->setText(unicode_text_);
  for (int32 segment_end = line_break_iterator_->following(start_position);
       segment_end != icu::BreakIterator::DONE && segment_end < end_position;
       segment_end = line_break_iterator_->next()) {
    if (!TryIncludeSegmentWithinAvailableWidth(
            used_font, *break_position, segment_end, available_width,
            &allow_overflow, break_position, break_width)) {
      break;
    }
  }
}

bool Paragraph::TryIncludeSegmentWithinAvailableWidth(
    const scoped_refptr<render_tree::Font>& used_font, int32 segment_start,
    int32 segment_end, float available_width, bool* allow_overflow,
    int32* break_position, float* break_width) {
  // Add the width of the segment encountered to the total, until reaching one
  // that causes the available width to be exceeded. The previous break position
  // is the last usable one. However, if overflow is allowed and no segment has
  // been found, then the first overflowing segment is accepted.
  std::string utf8_sub_string =
      RetrieveUtf8SubString(segment_start, segment_end);
  float segment_width = used_font->GetBounds(utf8_sub_string).width();

  if (!*allow_overflow && *break_width + segment_width > available_width) {
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

// Should only be called by Close().
void Paragraph::GenerateBidiLevelRuns() {
  DCHECK(is_closed_);

  // TODO(***REMOVED***): Change logic so that the extra copy of the data isn't
  // required.
  string16 bidi_string(unicode_text_.getBuffer(),
                       static_cast<size_t>(unicode_text_.length()));

  base::i18n::BiDiLineIterator bidi_iterator;
  if (bidi_iterator.Open(
          bidi_string, ConvertBaseDirectionToTextDirection(base_direction_))) {
    level_runs_.clear();
    level_runs_.reserve(size_t(bidi_iterator.CountRuns()));

    int run_start_position = 0;
    int run_end_position;
    UBiDiLevel run_ubidi_level;

    int32 text_end_position = GetTextEndPosition();
    while (run_start_position < text_end_position) {
      bidi_iterator.GetLogicalRun(run_start_position, &run_end_position,
                                  &run_ubidi_level);
      level_runs_.push_back(
          BidiLevelRun(run_start_position, static_cast<int>(run_ubidi_level)));
      run_start_position = run_end_position;
    }
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
