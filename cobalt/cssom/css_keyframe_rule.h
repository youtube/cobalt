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

#ifndef COBALT_CSSOM_CSS_KEYFRAME_RULE_H_
#define COBALT_CSSOM_CSS_KEYFRAME_RULE_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "cobalt/cssom/css_rule.h"
#include "cobalt/cssom/css_rule_style_declaration.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace cssom {

class CSSRuleVisitor;
class CSSStyleSheet;

// The CSSKeyframeRule interface represents the style rule for a single key.
//   https://www.w3.org/TR/2013/WD-css3-animations-20130219/#CSSKeyframeRule-interface
class CSSKeyframeRule : public CSSRule {
 public:
  CSSKeyframeRule(const std::vector<float>& offsets,
                  const scoped_refptr<CSSRuleStyleDeclaration>& style);

  // Web API: CSSRule
  Type type() const override { return kKeyframeRule; }
  std::string css_text(script::ExceptionState* exception_state) const override;
  void set_css_text(const std::string& css_text,
                    script::ExceptionState* exception_state) override;

  // Web API: CSSKeyframeRule
  //
  std::string key_text() const;

  const scoped_refptr<CSSRuleStyleDeclaration>& style() const { return style_; }

  // Custom, not in any spec.
  //
  const std::vector<float>& offsets() const { return offsets_; }

  // From CSSRule.
  void Accept(CSSRuleVisitor* visitor) override;
  void AttachToCSSStyleSheet(CSSStyleSheet* style_sheet) override;

  DEFINE_WRAPPABLE_TYPE(CSSKeyframeRule);

 private:
  ~CSSKeyframeRule() override;

  // The set of offsets that this keyframe's rule should apply to.
  // This is computed from keyText.
  std::vector<float> offsets_;

  // The style that is represented by this keyframe rule.
  scoped_refptr<CSSRuleStyleDeclaration> style_;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CSS_KEYFRAME_RULE_H_
