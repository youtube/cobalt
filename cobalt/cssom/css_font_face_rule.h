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

#ifndef COBALT_CSSOM_CSS_FONT_FACE_RULE_H_
#define COBALT_CSSOM_CSS_FONT_FACE_RULE_H_

#include <string>

#include "base/compiler_specific.h"
#include "cobalt/cssom/css_font_face_declaration_data.h"
#include "cobalt/cssom/css_rule.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace cssom {

class CSSRuleVisitor;
class CSSStyleSheet;

// The CSSFontFaceRule represents an @font-face at-rule.
//   https://www.w3.org/TR/css3-fonts/#font-face-rule
class CSSFontFaceRule : public CSSRule {
 public:
  CSSFontFaceRule();
  explicit CSSFontFaceRule(
      const scoped_refptr<CSSFontFaceDeclarationData>& data);

  // Web API: CSSRule
  Type type() const override { return kFontFaceRule; }
  std::string css_text(script::ExceptionState* exception_state) const override;
  void set_css_text(const std::string& css_text,
                    script::ExceptionState* exception_state) override;

  // Web API: CSSFontFaceRule
  //
  std::string family() const;
  void set_family(const std::string& family);

  std::string src() const;
  void set_src(const std::string& src);

  std::string style() const;
  void set_style(const std::string& style);

  std::string weight() const;
  void set_weight(const std::string& weight);

  std::string unicode_range() const;
  void set_unicode_range(const std::string& unicode_range);

  // Custom, not in any spec.
  //

  // From CSSRule.
  void Accept(CSSRuleVisitor* visitor) override;
  void AttachToCSSStyleSheet(CSSStyleSheet* style_sheet) override;

  // Rest of public methods.

  scoped_refptr<const CSSFontFaceDeclarationData> data() const { return data_; }

  std::string GetPropertyValue(PropertyKey key);
  void SetPropertyValue(const std::string& property_name,
                        const std::string& property_value);

  bool IsValid() const;

  DEFINE_WRAPPABLE_TYPE(CSSFontFaceRule);

 private:
  ~CSSFontFaceRule() override;

  void RecordMutation();

  scoped_refptr<CSSFontFaceDeclarationData> data_;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CSS_FONT_FACE_RULE_H_
