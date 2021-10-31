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

#include "cobalt/css_parser/position_parse_structures.h"

#include "cobalt/cssom/keyword_value.h"

namespace cobalt {
namespace css_parser {

namespace {

enum Direction {
  kHorizontal,
  kVertical,
  kCenter,
  kNone,
};

Direction ConvertToDirection(
    const scoped_refptr<cssom::PropertyValue>& element) {
  if (element == cssom::KeywordValue::GetLeft() ||
      element == cssom::KeywordValue::GetRight()) {
    return kHorizontal;
  } else if (element == cssom::KeywordValue::GetTop() ||
             element == cssom::KeywordValue::GetBottom()) {
    return kVertical;
  } else if (element == cssom::KeywordValue::GetCenter()) {
    return kCenter;
  } else {
    return kNone;
  }
}

}  // namespace

PositionParseStructure::PositionParseStructure()
    : is_horizontal_specified_(false),
      is_vertical_specified_(false),
      previous_is_non_center_keyword_(false),
      number_of_center_keyword_(0) {
  position_builder_.reserve(4);
}

bool PositionParseStructure::PushBackElement(
    const scoped_refptr<cssom::PropertyValue>& element) {
  Direction direction = ConvertToDirection(element);

  switch (direction) {
    case kHorizontal: {
      if (is_horizontal_specified_) {
        // Horizontal value has been specified.
        return false;
      }

      is_horizontal_specified_ = true;
      previous_is_non_center_keyword_ = true;
      break;
    }
    case kVertical: {
      if (is_vertical_specified_) {
        // Vertical value has been specified.
        return false;
      }

      is_vertical_specified_ = true;
      previous_is_non_center_keyword_ = true;
      break;
    }
    case kCenter: {
      if (number_of_center_keyword_ >= 2) {
        // More than 2 center keywords.
        return false;
      }

      ++number_of_center_keyword_;
      previous_is_non_center_keyword_ = false;
      break;
    }
    case kNone: {
      switch (position_builder_.size()) {
        case 0: {
          // A length or percentage as the first value represents the horizontal
          // position (or offset).
          is_horizontal_specified_ = true;
          break;
        }
        case 1: {
          if (!previous_is_non_center_keyword_) {
            // A length or percentage as the second value represents the
            // vertical position (or offset).
            is_vertical_specified_ = true;
          }

          previous_is_non_center_keyword_ = false;
          break;
        }
        case 2:  // fall-through
        case 3: {
          if (!previous_is_non_center_keyword_) {
            // The offset should be specified for a non center keyword.
            return false;
          }

          previous_is_non_center_keyword_ = false;
          break;
        }
        default:
          // Position value couldn't take more than 4 elements.
          return false;
      }
      break;
    }
    default:
      NOTREACHED();
      return false;
  }

  position_builder_.push_back(element);
  return true;
}

bool PositionParseStructure::IsPositionValidOnSizeTwo() {
  // Check for the special case that a pair of keywords can be reordered while
  // a combination of keyword and length or percentage cannot. eg. 'top center'
  // is valid while 'top 50%' is not.
  DCHECK_EQ(size_t(2), position_builder_.size());

  Direction first_direction = ConvertToDirection(position_builder_[0]);
  Direction second_direction = ConvertToDirection(position_builder_[1]);

  // If two values have one center keyword, the other value can be any
  // keyword, length or percentage.
  if (first_direction == kCenter || second_direction == kCenter) {
    return true;
  }

  if (first_direction == kVertical && !is_horizontal_specified_) {
    return false;
  }

  if (second_direction == kHorizontal && !is_vertical_specified_) {
    return false;
  }

  return true;
}

}  // namespace css_parser
}  // namespace cobalt
