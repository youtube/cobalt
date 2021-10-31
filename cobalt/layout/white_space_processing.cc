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

#include "cobalt/layout/white_space_processing.h"

#include "cobalt/cssom/character_classification.h"
#include "cobalt/cssom/keyword_value.h"

namespace cobalt {
namespace layout {

namespace {

// Returns true if skipped at least one white space character.
bool SkipWhiteSpaceAndAdvance(
    std::string::const_iterator* input_iterator,
    const std::string::const_iterator& input_end_iterator) {
  bool skipped_at_least_one = false;

  for (; *input_iterator != input_end_iterator; ++*input_iterator) {
    if (!cssom::IsWhiteSpace(**input_iterator)) {
      break;
    }

    skipped_at_least_one = true;
  }

  return skipped_at_least_one;
}

// Returns true if copied at least one character that is not a white space.
bool CopyNonWhiteSpaceAndAdvance(
    std::string::const_iterator* input_iterator,
    const std::string::const_iterator& input_end_iterator,
    std::string::iterator* output_iterator) {
  bool copied_at_least_one = false;

  for (; *input_iterator != input_end_iterator; ++*input_iterator) {
    char character = **input_iterator;
    if (cssom::IsWhiteSpace(character)) {
      break;
    }

    copied_at_least_one = true;
    **output_iterator = character;
    ++*output_iterator;
  }

  return copied_at_least_one;
}

}  // namespace

// "white-space" property values "pre", "pre-line", and "pre-wrap" preserve
// segment breaks, while other values collapse it.
// https://www.w3.org/TR/css-text-3/#white-space
bool DoesCollapseSegmentBreaks(
    const scoped_refptr<cssom::PropertyValue>& value) {
  return value != cssom::KeywordValue::GetPre() &&
         value != cssom::KeywordValue::GetPreLine() &&
         value != cssom::KeywordValue::GetPreWrap();
}

// "white-space" property values "pre", and "pre-wrap" preserve whitespace,
// while other values collapse it.
// https://www.w3.org/TR/css-text-3/#white-space
bool DoesCollapseWhiteSpace(const scoped_refptr<cssom::PropertyValue>& value) {
  return value != cssom::KeywordValue::GetPre() &&
         value != cssom::KeywordValue::GetPreWrap();
}

// "white-space" property values "pre", and "nowrap" prevent wrapping, while
// other values allow it.
// https://www.w3.org/TR/css-text-3/#white-space
bool DoesAllowTextWrapping(const scoped_refptr<cssom::PropertyValue>& value) {
  return value != cssom::KeywordValue::GetPre() &&
         value != cssom::KeywordValue::GetNowrap();
}

void CollapseWhiteSpace(std::string* text) {
  std::string::const_iterator input_iterator = text->begin();
  std::string::const_iterator input_end_iterator = text->end();
  std::string::iterator output_iterator = text->begin();

  // Per the specification, any space immediately following another
  // collapsible space is collapsed to have zero advance width. We approximate
  // this by replacing adjacent spaces with a single space.
  if (SkipWhiteSpaceAndAdvance(&input_iterator, input_end_iterator)) {
    *output_iterator++ = ' ';
  }
  while (CopyNonWhiteSpaceAndAdvance(&input_iterator, input_end_iterator,
                                     &output_iterator) &&
         SkipWhiteSpaceAndAdvance(&input_iterator, input_end_iterator)) {
    *output_iterator++ = ' ';
  }

  text->erase(output_iterator, text->end());
}

bool FindNextNewlineSequence(const std::string& utf8_text, size_t index,
                             size_t* sequence_start, size_t* sequence_length) {
  *sequence_start = utf8_text.size();
  *sequence_length = 0;

  // For CSS processing... CRLF sequence (U+000D U+000A), carriage return
  // (U+000D), and line feed (U+000A) in the text is treated as a segment break.
  //   https://www.w3.org/TR/css3-text/#white-space-processing
  for (; index < utf8_text.size(); ++index) {
    char character = utf8_text[index];
    if (character == '\r') {
      *sequence_start = index++;
      if (index < utf8_text.size() && utf8_text[index] == '\n') {
        *sequence_length = 2;
      } else {
        *sequence_length = 1;
      }
      break;
    } else if (character == '\n') {
      *sequence_start = index;
      *sequence_length = 1;
      break;
    }
  }

  return *sequence_length > 0;
}

}  // namespace layout
}  // namespace cobalt
