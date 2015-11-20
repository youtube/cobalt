/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef CSSOM_CSS_KEYFRAMES_RULE_H_
#define CSSOM_CSS_KEYFRAMES_RULE_H_

#include <string>

#include "base/compiler_specific.h"
#include "cobalt/cssom/css_rule.h"
#include "cobalt/cssom/css_rule_list.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace cssom {

class CSSRuleVisitor;
class CSSStyleSheet;

// The CSSKeyframesRule interface represents a complete set of keyframes for a
// single animation.
//   http://www.w3.org/TR/2013/WD-css3-animations-20130219/#CSSKeyframesRule-interface
class CSSKeyframesRule : public CSSRule {
 public:
  CSSKeyframesRule(const std::string& name,
                   const scoped_refptr<CSSRuleList>& css_rules);

  // Web API: CSSRule
  Type type() const OVERRIDE { return kKeyframesRule; }
  std::string css_text() const OVERRIDE;
  void set_css_text(const std::string& css_text) OVERRIDE;

  // Web API: CSSKeyframesRule
  //
  const std::string& name() const;

  const scoped_refptr<CSSRuleList>& css_rules() const;

  // Custom, not in any spec.
  //

  // From CSSRule.
  void Accept(CSSRuleVisitor* visitor) OVERRIDE;
  void AttachToCSSStyleSheet(CSSStyleSheet* style_sheet) OVERRIDE;

  DEFINE_WRAPPABLE_TYPE(CSSKeyframesRule);

 private:
  ~CSSKeyframesRule() OVERRIDE;

  // The unprocessed key text.
  std::string name_;

  // The list of CSSKeyframeRule objects that define the keyframes.
  scoped_refptr<CSSRuleList> css_rules_;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_CSS_KEYFRAMES_RULE_H_
