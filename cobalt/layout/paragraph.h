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

#ifndef LAYOUT_PARAGRAPH_H_
#define LAYOUT_PARAGRAPH_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "cobalt/render_tree/font.h"
#include "third_party/icu/public/common/unicode/brkiter.h"

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
// (http://www.w3.org/TR/css-text-3/#soft-wrap-opportunity), according to the
// Unicode line breaking algorithm (http://www.unicode.org/reports/tr14/),
// which can result in text boxes being split.
class Paragraph : public base::RefCounted<Paragraph> {
 public:
  enum BaseDirection {
    kUnknownBaseDirection,
    kRightToLeftBaseDirection,
    kLeftToRightBaseDirection,
  };

  Paragraph(icu::BreakIterator* line_break_iterator,
            BaseDirection base_direction);

  // Append the text and return the position where the text begins.
  int32 AppendText(const std::string& text);

  // Iterate over breakable segments in the text from the starting position,
  // adding up the width of each segment and determining the last one that fits
  // within the available width. The parameter |break_width| indicates the width
  // of the portion of the substring coming before |break_position|.
  //
  // Returns false if no usable break position was found.
  bool CalculateBreakPosition(const scoped_refptr<render_tree::Font>& used_font,
                              int32 start_position, int32 end_position,
                              float available_width, bool allow_overflow,
                              int32* break_position, float* break_width);
  float CalculateSubStringWidth(
      const scoped_refptr<render_tree::Font>& used_font, int32 start_position,
      int32 end_position) const;
  std::string RetrieveSubString(int32 start_position, int32 end_position) const;

  BaseDirection GetBaseDirection() const;
  int GetBidiLevel(int32 position) const;
  bool IsWhiteSpace(int32 position) const;
  bool GetNextRunPosition(int32 position, int32* next_run_position) const;
  int32 GetTextEndPosition() const;

  // Close the paragraph so that it becomes immutable and generates the text
  // runs.
  void Close();
  bool IsClosed() const;

 private:
  struct BidiLevelRun {
    BidiLevelRun(int32 start_position, int level)
        : start_position_(start_position), level_(level) {}

    int32 start_position_;
    int level_;
  };

  typedef std::vector<BidiLevelRun> BidiLevelRuns;

  void GenerateBidiLevelRuns();
  size_t GetRunIndex(int32 position) const;

  // A processed text in which:
  //   - collapsible white space has been collapsed and trimmed for both ends;
  //   - segment breaks have been transformed;
  //   - letter case has been transformed.
  icu::UnicodeString text_;

  // The line break iterator to use when splitting the text boxes derived from
  // this text paragraph across multiple lines.
  icu::BreakIterator* const line_break_iterator_;

  // The base text direction of the paragraph.
  BaseDirection base_direction_;

  // Whether or not the paragraph is open and modifiable or closed and
  // immutable.
  bool is_closed_;

  // All of the bidi level runs contained within the paragraph.
  // TODO(jgray): Add in splitting of runs around scripts using the chromium
  // logic found in RenderTextHarfBuzz::ItemizeTextToRuns
  BidiLevelRuns level_runs_;

  // |last_retrieved_run_index_| is tracked so that the next retrieval search
  // begins from this index. The vase majority of run retrievals either retrieve
  // the same index as the previous one, or retrieve a neighboring index.
  mutable size_t last_retrieved_run_index_;

  friend class base::RefCounted<Paragraph>;
};

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_PARAGRAPH_H_
