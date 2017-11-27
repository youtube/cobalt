// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_CSSOM_CSS_RULE_STYLE_DECLARATION_H_
#define COBALT_CSSOM_CSS_RULE_STYLE_DECLARATION_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/cssom/css_declared_style_data.h"
#include "cobalt/cssom/css_declared_style_declaration.h"
#include "cobalt/script/exception_state.h"

namespace cobalt {
namespace cssom {

class CSSParser;
class CSSRule;

// A CSS declaration block is an ordered collection of CSS properties with their
// associated values, also named CSS declarations. In the DOM a CSS declaration
// block is a CSSStyleDeclaration object.
//   https://www.w3.org/TR/2013/WD-cssom-20131205/#css-declaration-blocks
// The CSSDeclaredStyleDeclaration implements a CSS Declaration block
// for declared styles, such as css style rules and inline styles.
class CSSRuleStyleDeclaration : public CSSDeclaredStyleDeclaration {
 public:
  explicit CSSRuleStyleDeclaration(CSSParser* css_parser);

  CSSRuleStyleDeclaration(const scoped_refptr<CSSDeclaredStyleData>& style,
                          CSSParser* css_parser);

  scoped_refptr<CSSRule> parent_rule() const override;
  void set_parent_rule(CSSRule* parent_rule);

 private:
  base::WeakPtr<CSSRule> parent_rule_;

  DISALLOW_COPY_AND_ASSIGN(CSSRuleStyleDeclaration);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CSS_RULE_STYLE_DECLARATION_H_
