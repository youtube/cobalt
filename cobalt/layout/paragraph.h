// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_LAYOUT_PARAGRAPH_H_
#define COBALT_LAYOUT_PARAGRAPH_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "cobalt/dom/font_list.h"
#include "cobalt/layout/base_direction.h"
#include "cobalt/layout/layout_unit.h"
#include "cobalt/layout/line_wrapping.h"
#include "cobalt/render_tree/font.h"

#include "third_party/icu/source/common/unicode/brkiter.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/unistr.h"

namespace cobalt {
namespace layout {

// The paragraph contains all of the text for a single layout paragraph. It
// handles both bidi and line breaking analysis for the text contained within
// it, while also serving as a repository for the text itself. Text boxes
// do not internally store their text, but instead keep a reference to their
// text paragraph along with their start and end indices within that paragraph,
// which allows them to retrieve both their specific text and any accompanying
// textual analysis from the text paragraph.
//
// The text paragraph is initially open and mutable. When the paragraph is
// closed, it becomes immutable and the Unicode bidirectional algorithm is
// applied to its text (http://www.unicode.org/reports/tr9/). The text runs
// generated from this analysis are later used to both split boxes and organize
// boxes within a line according to their bidi levels.
//
// During layout, the the text paragraph determines line breaking locations for
// its text at soft wrap opportunities
// (https://www.w3.org/TR/css-text-3/#soft-wrap-opportunity), according to the
// Unicode line breaking algorithm (http://www.unicode.org/reports/tr14/),
// which can result in text boxes being split.
class Paragraph : public base::RefCounted<Paragraph> {
 public:
  // 'normal': Lines may break only at allowed break points.
  // 'break-word': An unbreakable 'word' may be broken at an an arbitrary
  // point...
  //   https://www.w3.org/TR/css-text-3/#overflow-wrap
  enum BreakPolicy {
    kBreakPolicyNormal,
    kBreakPolicyBreakWord,
  };

  enum CodePoint {
    kLeftToRightIsolateCodePoint,
    kLineFeedCodePoint,
    kNoBreakSpaceCodePoint,
    kObjectReplacementCharacterCodePoint,
    kPopDirectionalIsolateCodePoint,
    kRightToLeftIsolateCodePoint,
  };

  // http://unicode.org/reports/tr9/#Directional_Formatting_Characters
  enum DirectionalFormatting {
    // Setting the "dir" attribute corresponds to using directional isolates.
    //   http://unicode.org/reports/tr9/#Markup_And_Formatting
    kLeftToRightDirectionalIsolate,
    kRightToLeftDirectionalIsolate,
  };

  enum TextOrder {
    kLogicalTextOrder,
    kVisualTextOrder,
  };

  enum TextTransform {
    kNoTextTransform,
    kUppercaseTextTransform,
  };

  typedef std::vector<DirectionalFormatting> DirectionalFormattingStack;

  Paragraph(const icu::Locale& locale, BaseDirection base_direction,
            const DirectionalFormattingStack& directional_formatting_stack,
            icu::BreakIterator* line_break_iterator,
            icu::BreakIterator* character_break_iterator);

  // Append the string and return the position where the string begins.
  int32 AppendUtf8String(const std::string& utf8_string);
  int32 AppendUtf8String(const std::string& utf8_string,
                         TextTransform text_transform);
  int32 AppendCodePoint(CodePoint code_point);

  // Using the specified break policy, finds the last break position that fits
  // within the available width. In the case where overflow is allowed and no
  // break position is found within the available width, the first overflowing
  // break position is used instead.
  //
  // The parameter |break_width| indicates the width of the portion of the
  // substring coming before |break_position|.
  //
  // Returns false if no usable break position was found.
  bool FindBreakPosition(BaseDirection direction, bool should_attempt_to_wrap,
                         const scoped_refptr<dom::FontList>& used_font,
                         int32 start_position, int32 end_position,
                         LayoutUnit available_width,
                         bool should_collapse_trailing_white_space,
                         bool allow_overflow, BreakPolicy break_policy,
                         int32* break_position, LayoutUnit* break_width);
  int32 GetNextBreakPosition(int32 position, BreakPolicy break_policy);
  int32 GetPreviousBreakPosition(int32 position, BreakPolicy break_policy);
  bool IsBreakPosition(int32 position, BreakPolicy break_policy);

  std::string RetrieveUtf8SubString(int32 start_position,
                                    int32 end_position) const;
  std::string RetrieveUtf8SubString(int32 start_position, int32 end_position,
                                    TextOrder text_order) const;
  const base::char16* GetTextBuffer() const;

  const icu::Locale& GetLocale() const;
  BaseDirection base_direction() const { return base_direction_; }

  // Return the direction of the top directional formatting in the paragraph's
  // stack or the base direction if the stack is empty.
  BaseDirection GetDirectionalFormattingStackDirection() const;

  int GetBidiLevel(int32 position) const;
  bool IsRTL(int32 position) const;
  bool AreInlineAndScriptDirectionsTheSame(BaseDirection direction,
                                           int32 position) const;
  bool IsCollapsibleWhiteSpace(int32 position) const;
  bool GetNextRunPosition(int32 position, int32* next_run_position) const;
  int32 GetTextEndPosition() const;

  // Return the full directional formatting stack for the paragraph. This allows
  // newly created paragraphs to copy the directional state of a preceding
  // paragraph.
  const DirectionalFormattingStack& GetDirectionalFormattingStack() const;

  // Close the paragraph so that it becomes immutable and generates the text
  // runs.
  void Close();
  bool IsClosed() const;

  static BreakPolicy GetBreakPolicyFromWrapOpportunityPolicy(
      WrapOpportunityPolicy wrap_opportunity_policy,
      bool does_style_allow_break_word);

 private:
  struct BidiLevelRun {
    BidiLevelRun(int32 start_position, int level)
        : start_position_(start_position), level_(level) {}

    int32 start_position_;
    int level_;
  };

  typedef std::vector<BidiLevelRun> BidiLevelRuns;

  icu::BreakIterator* GetBreakIterator(BreakPolicy break_policy);

  // Iterate over text segments as determined by the break iterator's strategy
  // from the starting position, adding the width of each segment and
  // determining the last one that fits within the available width. In the case
  // where |allow_overflow| is true and the first segment overflows the width,
  // that first overflowing segment will be included. The parameter
  // |break_width| indicates the width of the portion of the substring coming
  // before |break_position|.
  void FindIteratorBreakPosition(BaseDirection direction,
                                 bool should_attempt_to_wrap,
                                 const scoped_refptr<dom::FontList>& used_font,
                                 icu::BreakIterator* const break_iterator,
                                 int32 start_position, int32 end_position,
                                 LayoutUnit available_width,
                                 bool should_collapse_trailing_white_space,
                                 bool allow_overflow, int32* break_position,
                                 LayoutUnit* break_width);

  // Attempt to include the specified segment within the available width. If
  // either the segment fits within the width or |allow_overflow| is true, then
  // |break_position| and |break_width| are updated to include the segment.
  // NOTE: When |should_collapse_trailing_white_space| is true, then trailing
  // white space in the segment is not included when determining if the segment
  // can fit within the available width.
  //
  // |allow_overflow| is always set to false after the first segment is added,
  // ensuring that only the first segment can overflow the available width.
  //
  // Returns false if the specified segment exceeded the available width.
  // However, as a result of overflow potentially being allowed, a return value
  // of false does not guarantee that the segment was not included, but simply
  // that no additional segments can be included.
  bool TryIncludeSegmentWithinAvailableWidth(
      BaseDirection direction, bool should_attempt_to_wrap,
      const scoped_refptr<dom::FontList>& used_font, int32 start_position,
      int32 end_position, LayoutUnit available_width,
      bool should_collapse_trailing_white_space, bool* allow_overflow,
      int32* break_position, LayoutUnit* break_width);

  void GenerateBidiLevelRuns();
  size_t GetRunIndex(int32 position) const;

  // A processed text in which:
  //   - collapsible white space has been collapsed and trimmed for both ends;
  //   - segment breaks have been transformed;
  //   - letter case has been transformed.
  icu::UnicodeString unicode_text_;

  // The locale of the paragraph.
  const icu::Locale& locale_;

  // The base direction of the paragraph.
  const BaseDirection base_direction_;

  // The stack tracking all active directional formatting in the paragraph.
  // http://unicode.org/reports/tr9/#Directional_Formatting_Characters
  DirectionalFormattingStack directional_formatting_stack_;

  // The line break iterator to use when splitting the text boxes derived from
  // this text paragraph across multiple lines.
  icu::BreakIterator* const line_break_iterator_;
  icu::BreakIterator* const character_break_iterator_;

  // Whether or not the paragraph is open and modifiable or closed and
  // immutable.
  bool is_closed_;

  // All of the bidi level runs contained within the paragraph.
  BidiLevelRuns level_runs_;

  // |last_retrieved_run_index_| is tracked so that the next retrieval search
  // begins from this index. The vase majority of run retrievals either retrieve
  // the same index as the previous one, or retrieve a neighboring index.
  mutable size_t last_retrieved_run_index_;

  // Allow the reference counting system access to our destructor.
  friend class base::RefCounted<Paragraph>;
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_PARAGRAPH_H_
