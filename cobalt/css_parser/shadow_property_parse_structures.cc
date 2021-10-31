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

#include "cobalt/css_parser/shadow_property_parse_structures.h"

namespace cobalt {
namespace css_parser {

bool ShadowPropertyInfo::IsShadowPropertyValid(ShadowType type) {
  if (error) return false;
  switch (type) {
    case kBoxShadow:
      //  https://www.w3.org/TR/css3-background/#box-shadow
      if (length_vector.size() >= 2 && length_vector.size() <= 4) {
        return true;
      }
      break;
    case kTextShadow:
      //  https://www.w3.org/TR/css-text-decor-3/#text-shadow-property
      if (length_vector.size() >= 2 && length_vector.size() <= 3 &&
          !has_inset) {
        return true;
      }
      break;
    default:
      NOTREACHED();
      break;
  }
  return false;
}

}  // namespace css_parser
}  // namespace cobalt
