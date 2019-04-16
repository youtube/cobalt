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

#ifndef COBALT_CSS_PARSER_BACKGROUND_SHORTHAND_PROPERTY_PARSE_STRUCTURES_H_
#define COBALT_CSS_PARSER_BACKGROUND_SHORTHAND_PROPERTY_PARSE_STRUCTURES_H_

#include "cobalt/cssom/property_definitions.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace css_parser {

// This helps parsing and verifying syntax of background shorthand property
// values.
struct BackgroundShorthandLayer {
  BackgroundShorthandLayer() : error(false) {}

  bool IsBackgroundPropertyOverlapped(
      const BackgroundShorthandLayer& that) const;
  void IntegrateNonOverlapped(const BackgroundShorthandLayer& that);

  void ReplaceNullWithInitialValues();

  bool error;
  scoped_refptr<cssom::PropertyValue> background_color;
  scoped_refptr<cssom::PropertyValue> background_image;
  scoped_refptr<cssom::PropertyValue> background_position;
  scoped_refptr<cssom::PropertyValue> background_repeat;
  scoped_refptr<cssom::PropertyValue> background_size;
};

}  // namespace css_parser
}  // namespace cobalt

#endif  // COBALT_CSS_PARSER_BACKGROUND_SHORTHAND_PROPERTY_PARSE_STRUCTURES_H_
