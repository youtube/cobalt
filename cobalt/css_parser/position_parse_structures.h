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

#ifndef COBALT_CSS_PARSER_POSITION_PARSE_STRUCTURES_H_
#define COBALT_CSS_PARSER_POSITION_PARSE_STRUCTURES_H_

#include "cobalt/cssom/property_list_value.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace css_parser {

// This helps parsing and verifying syntax of background position and
// transform origin property values.
class PositionParseStructure {
 public:
  PositionParseStructure();

  bool PushBackElement(const scoped_refptr<cssom::PropertyValue>& element);

  // A special case that a combination of keyword and length or percentage
  // cannot be reordered.
  bool IsPositionValidOnSizeTwo();

  const cssom::PropertyListValue::Builder& position_builder() const {
    return position_builder_;
  }

 private:
  bool is_horizontal_specified_;
  bool is_vertical_specified_;
  bool previous_is_non_center_keyword_;
  int number_of_center_keyword_;

  cssom::PropertyListValue::Builder position_builder_;
};

}  // namespace css_parser
}  // namespace cobalt

#endif  // COBALT_CSS_PARSER_POSITION_PARSE_STRUCTURES_H_
