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

#include "cobalt/layout/white_space_processing.h"

#include "cobalt/cssom/character_classification.h"

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

void TrimWhiteSpace(std::string* text, bool* has_leading_white_space,
                    bool* has_trailing_white_space) {
  if (text->empty()) {
    *has_leading_white_space = false;
    *has_trailing_white_space = false;
    return;
  }

  *has_leading_white_space = cssom::IsWhiteSpace(*text->begin());
  *has_trailing_white_space = cssom::IsWhiteSpace(*text->rbegin());

  if (*has_leading_white_space) {
    text->erase(text->begin());
  }
  if (!text->empty() && *has_trailing_white_space) {
    text->erase(text->end() - 1);
  }
}

}  // namespace layout
}  // namespace cobalt
