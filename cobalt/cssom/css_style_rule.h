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

#ifndef CSSOM_CSS_STYLE_RULE_H_
#define CSSOM_CSS_STYLE_RULE_H_

#include "base/memory/scoped_vector.h"
#include "cobalt/cssom/css_rule.h"
#include "cobalt/cssom/selector.h"

namespace cobalt {
namespace cssom {

class CSSStyleDeclaration;

// The CSSStyleRule interface represents a style rule.
//   http://dev.w3.org/csswg/cssom/#the-cssstylerule-interface
class CSSStyleRule : public CSSRule {
 public:
  CSSStyleRule(Selectors selectors,
               const scoped_refptr<CSSStyleDeclaration>& style);

  const Selectors& selectors() const { return selectors_; }

  const scoped_refptr<CSSStyleDeclaration>& style();

  DEFINE_WRAPPABLE_TYPE(CSSStyleRule);

 private:
  ~CSSStyleRule() OVERRIDE;

  Selectors selectors_;
  scoped_refptr<CSSStyleDeclaration> style_;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_CSS_STYLE_RULE_H_
