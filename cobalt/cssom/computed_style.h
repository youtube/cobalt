/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef CSSOM_COMPUTED_STYLE_H_
#define CSSOM_COMPUTED_STYLE_H_

#include "base/hash_tables.h"
#include "cobalt/cssom/css_style_declaration_data.h"
#include "cobalt/loader/image_cache.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace cssom {

typedef base::hash_map<const char*, GURL> GURLMap;

// The computed value is the result of resolving the specified value,
// generally absolutizing it. The computed value is the value that is
// transferred from parent to child during inheritance.
//   http://www.w3.org/TR/css-cascade-3/#computed

// Converts specified values into computed values in place.
// If the element has no parent, |parent_computed_style| can be NULL
void PromoteToComputedStyle(
    const scoped_refptr<CSSStyleDeclarationData>& specified_style,
    const scoped_refptr<const CSSStyleDeclarationData>& parent_computed_style,
    GURLMap* const property_name_to_base_url_map);

// Creates the computed style of an anonymous box from the given parent style.
// CSS 2.1 specification provides identical definitions for anonymous block and
// inline boxes. The inheritable properties of anonymous boxes are inherited
// from the enclosing non-anonymous box. Non-inherited properties have their
// initial value.
//   http://www.w3.org/TR/CSS21/visuren.html#anonymous-block-level
//   http://www.w3.org/TR/CSS21/visuren.html#anonymous
scoped_refptr<CSSStyleDeclarationData> GetComputedStyleOfAnonymousBox(
    const scoped_refptr<const CSSStyleDeclarationData>& parent_computed_style);

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_COMPUTED_STYLE_H_
