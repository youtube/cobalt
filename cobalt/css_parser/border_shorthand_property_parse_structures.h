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

#ifndef COBALT_CSS_PARSER_BORDER_SHORTHAND_PROPERTY_PARSE_STRUCTURES_H_
#define COBALT_CSS_PARSER_BORDER_SHORTHAND_PROPERTY_PARSE_STRUCTURES_H_

#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace css_parser {

// This helps parsing and verifying syntax of border shorthand property values.
struct BorderOrOutlineShorthand {
  BorderOrOutlineShorthand() : error(false) {}

  bool error;
  scoped_refptr<cssom::PropertyValue> color;
  scoped_refptr<cssom::PropertyValue> style;
  scoped_refptr<cssom::PropertyValue> width;
};

// This helps parsing and verifying syntax of border color/style/width shorthand
// property values.
struct BorderShorthandToLonghand {
  void Assign4BordersBasedOnPropertyList(
      const scoped_refptr<cssom::PropertyValue>& property_list);

  scoped_refptr<cssom::PropertyValue> border_top;
  scoped_refptr<cssom::PropertyValue> border_right;
  scoped_refptr<cssom::PropertyValue> border_bottom;
  scoped_refptr<cssom::PropertyValue> border_left;
};

}  // namespace css_parser
}  // namespace cobalt

#endif  // COBALT_CSS_PARSER_BORDER_SHORTHAND_PROPERTY_PARSE_STRUCTURES_H_
