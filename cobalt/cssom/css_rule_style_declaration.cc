// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/css_rule_style_declaration.h"

#include "cobalt/cssom/css_rule.h"

namespace cobalt {
namespace cssom {

CSSRuleStyleDeclaration::CSSRuleStyleDeclaration(CSSParser* css_parser)
    : CSSDeclaredStyleDeclaration(css_parser) {}

CSSRuleStyleDeclaration::CSSRuleStyleDeclaration(
    const scoped_refptr<CSSDeclaredStyleData>& style, CSSParser* css_parser)
    : CSSDeclaredStyleDeclaration(style, css_parser) {}

scoped_refptr<CSSRule> CSSRuleStyleDeclaration::parent_rule() const {
  return parent_rule_.get();
}

void CSSRuleStyleDeclaration::set_parent_rule(CSSRule* parent_rule) {
  parent_rule_ = base::AsWeakPtr(parent_rule);
}

}  // namespace cssom
}  // namespace cobalt
