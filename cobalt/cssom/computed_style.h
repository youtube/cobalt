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

#ifndef COBALT_CSSOM_COMPUTED_STYLE_H_
#define COBALT_CSSOM_COMPUTED_STYLE_H_

#include "base/memory/ref_counted.h"
#include "cobalt/cssom/property_definitions.h"
#include "cobalt/math/size.h"

namespace cobalt {
namespace cssom {

class CSSComputedStyleData;
class MutableCSSComputedStyleData;
class CSSComputedStyleDeclaration;

// The computed value is the result of resolving the specified value,
// generally absolutizing it. The computed value is the value that is
// transferred from parent to child during inheritance.
//   https://www.w3.org/TR/css-cascade-3/#computed

// Converts specified values into computed values in place.
// parent_computed_style_declaration and root_computed_style cannot be NULL.
// property_key_to_base_url_map can be NULL.
void PromoteToComputedStyle(
    const scoped_refptr<MutableCSSComputedStyleData>& specified_style,
    const scoped_refptr<CSSComputedStyleDeclaration>&
        parent_computed_style_declaration,
    const scoped_refptr<const CSSComputedStyleData>& root_computed_style,
    const math::Size& viewport_size,
    GURLMap* const property_key_to_base_url_map);

// Creates the computed style of an anonymous box from the given parent style.
// CSS 2.1 specification provides identical definitions for anonymous block and
// inline boxes. The inheritable properties of anonymous boxes are inherited
// from the enclosing non-anonymous box. Non-inherited properties have their
// initial value.
//   https://www.w3.org/TR/CSS21/visuren.html#anonymous-block-level
//   https://www.w3.org/TR/CSS21/visuren.html#anonymous
scoped_refptr<CSSComputedStyleData> GetComputedStyleOfAnonymousBox(
    const scoped_refptr<CSSComputedStyleDeclaration>&
        parent_computed_style_declaration);

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_COMPUTED_STYLE_H_
