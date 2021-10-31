// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_CSSOM_CASCADED_STYLE_H_
#define COBALT_CSSOM_CASCADED_STYLE_H_

#include "base/memory/ref_counted.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/property_definitions.h"

namespace cobalt {
namespace cssom {

class MutableCSSComputedStyleData;
class CSSDeclaredStyleData;

// The cascaded value represents the result of the cascade: it is the declared
// value that wins the cascade (is sorted first in the output of the cascade).
// If the output of the cascade is an empty list, there is no cascaded value.
//   https://www.w3.org/TR/css-cascade-3/#cascaded

// Given list of rules that match the element, sorts them by specificity and
// applies the declared values contained in the rules on top of the inline style
// according to the cascading algorithm.
//   https://www.w3.org/TR/css-cascade-3/#cascading
scoped_refptr<MutableCSSComputedStyleData> PromoteToCascadedStyle(
    const scoped_refptr<const CSSDeclaredStyleData>& inline_style,
    RulesWithCascadePrecedence* matching_rules,
    GURLMap* property_key_to_base_url_map);

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CASCADED_STYLE_H_
