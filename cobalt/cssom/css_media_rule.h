// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_CSSOM_CSS_MEDIA_RULE_H_
#define COBALT_CSSOM_CSS_MEDIA_RULE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "cobalt/cssom/css_condition_rule.h"
#include "cobalt/math/size.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace cssom {

class CSSRuleList;
class CSSRuleVisitor;
class CSSStyleSheet;
class MediaList;
class PropertyValue;

// The CSSMediaRule interface represents an @media at-rule.
//   https://www.w3.org/TR/cssom/#the-cssmediarule-interface
class CSSMediaRule : public CSSConditionRule {
 public:
  CSSMediaRule();
  CSSMediaRule(const scoped_refptr<MediaList>& media_list,
               const scoped_refptr<CSSRuleList>& css_rule_list);

  // Web API: CSSRule
  Type type() const override { return kMediaRule; }
  std::string css_text(script::ExceptionState* exception_state) const override;
  void set_css_text(const std::string& css_text,
                    script::ExceptionState* exception_state) override;

  // Web API: CSSConditionRule
  std::string condition_text() override;
  void set_condition_text(const std::string& condition) override;

  // Web API: CSSMediaRule
  //
  // Returns a read-only, live object representing the CSS rules.
  const scoped_refptr<MediaList>& media() const;

  // Custom, not in any spec.
  //
  // From CSSRule.
  void Accept(CSSRuleVisitor* visitor) override;

  // Evaluates the condition expression and caches the resulting condition
  // value. Returns true if the cached condition value has changed.
  bool EvaluateConditionValueAndReturnIfChanged(
      const math::Size& viewport_size);

  // This method can be used to setup the parent style sheet.
  void AttachToCSSStyleSheet(CSSStyleSheet* style_sheet) override;

  CSSMediaRule* AsCSSMediaRule() override { return this; }

  DEFINE_WRAPPABLE_TYPE(CSSMediaRule);

 private:
  ~CSSMediaRule() override;

  scoped_refptr<MediaList> media_list_;

  DISALLOW_COPY_AND_ASSIGN(CSSMediaRule);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CSS_MEDIA_RULE_H_
