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

#ifndef COBALT_CSS_PARSER_MARGIN_OR_PADDING_SHORTHAND_H_
#define COBALT_CSS_PARSER_MARGIN_OR_PADDING_SHORTHAND_H_

#include <memory>

#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace css_parser {

// A helper used in YYSTYPE to represent the value of shorthand properties such
// as margin and padding.
struct MarginOrPaddingShorthand {
  static std::unique_ptr<MarginOrPaddingShorthand> TryCreate(
      const scoped_refptr<cssom::PropertyValue>& top,
      const scoped_refptr<cssom::PropertyValue>& right,
      const scoped_refptr<cssom::PropertyValue>& bottom,
      const scoped_refptr<cssom::PropertyValue>& left);

  scoped_refptr<cssom::PropertyValue> top;
  scoped_refptr<cssom::PropertyValue> right;
  scoped_refptr<cssom::PropertyValue> bottom;
  scoped_refptr<cssom::PropertyValue> left;
};

}  // namespace css_parser
}  // namespace cobalt

#endif  // COBALT_CSS_PARSER_MARGIN_OR_PADDING_SHORTHAND_H_
