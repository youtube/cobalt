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

#include "cobalt/cssom/css_rule.h"

#include "cobalt/cssom/cascade_precedence.h"
#include "cobalt/cssom/css_style_sheet.h"

namespace cobalt {
namespace cssom {

scoped_refptr<CSSRule> CSSRule::parent_rule() const {
  return parent_rule_.get();
}

scoped_refptr<CSSStyleSheet> CSSRule::parent_style_sheet() const {
  return parent_style_sheet_.get();
}

void CSSRule::set_parent_rule(CSSRule* parent_rule) {
  parent_rule_ = base::AsWeakPtr(parent_rule);
}
void CSSRule::set_parent_style_sheet(CSSStyleSheet* parent_style_sheet) {
  parent_style_sheet_ = base::AsWeakPtr(parent_style_sheet);
}

CSSRule::CSSRule() : index_(Appearance::kUnattached) {}

}  // namespace cssom
}  // namespace cobalt
