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

#ifndef CSSOM_CSS_MEDIA_RULE_H_
#define CSSOM_CSS_MEDIA_RULE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "cobalt/cssom/css_condition_rule.h"
#include "cobalt/cssom/css_rule.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace cssom {

class MediaList;
class PropertyValue;
class CSSRuleList;
class CSSRuleVisitor;
class CSSStyleSheet;

// The CSSMediaRule interface represents an @media at-rule.
//   http://www.w3.org/TR/cssom/#the-cssmediarule-interface
class CSSMediaRule : public CSSConditionRule {
 public:
  CSSMediaRule();
  CSSMediaRule(const scoped_refptr<MediaList>& media_list,
               const scoped_refptr<CSSRuleList>& css_rule_list);

  // Web API: CSSMediaRule
  //

  // Returns a read-only, live object representing the CSS rules.
  const scoped_refptr<MediaList>& media() const;

  // Web API: CSSRule
  //

  Type type() const OVERRIDE { return kMediaRule; }
  std::string css_text() const OVERRIDE;
  void set_css_text(const std::string& css_text) OVERRIDE;

  // Web API: CSSConditionRule
  //

  std::string condition_text() OVERRIDE;
  void set_condition_text(const std::string& condition) OVERRIDE;

  // Custom, not in any spec.
  //

  // Evaluates the condition expression and caches the resulting condition
  // value. Returns true if the cached condition value has changed.
  bool EvaluateConditionValueAndReturnIfChanged(
      const scoped_refptr<PropertyValue>& width,
      const scoped_refptr<PropertyValue>& height);

  // From CSSRule.
  void Accept(CSSRuleVisitor* visitor) OVERRIDE;

  // This method can be used to setup the parent style sheet.
  void AttachToCSSStyleSheet(CSSStyleSheet* style_sheet) OVERRIDE;

  DEFINE_WRAPPABLE_TYPE(CSSMediaRule);

 private:
  scoped_refptr<MediaList> media_list_;

  ~CSSMediaRule() OVERRIDE;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_CSS_MEDIA_RULE_H_
