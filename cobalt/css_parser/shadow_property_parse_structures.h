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

#ifndef COBALT_CSS_PARSER_SHADOW_PROPERTY_PARSE_STRUCTURES_H_
#define COBALT_CSS_PARSER_SHADOW_PROPERTY_PARSE_STRUCTURES_H_

#include <vector>

#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/rgba_color_value.h"

namespace cobalt {
namespace css_parser {

// This helps parsing and verifying syntax of box shadow and text shadow
// property values.

enum ShadowType {
  kBoxShadow,
  kTextShadow,
};

struct ShadowPropertyInfo {
  ShadowPropertyInfo() : error(false), has_inset(false) {}

  bool IsShadowPropertyValid(ShadowType type);

  bool error;
  std::vector<scoped_refptr<cssom::LengthValue> > length_vector;
  scoped_refptr<cssom::RGBAColorValue> color;
  bool has_inset;
};

}  // namespace css_parser
}  // namespace cobalt

#endif  // COBALT_CSS_PARSER_SHADOW_PROPERTY_PARSE_STRUCTURES_H_
