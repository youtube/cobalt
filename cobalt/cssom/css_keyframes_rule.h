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

#ifndef COBALT_CSSOM_CSS_KEYFRAMES_RULE_H_
#define COBALT_CSSOM_CSS_KEYFRAMES_RULE_H_

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "cobalt/cssom/css_declared_style_data.h"
#include "cobalt/cssom/css_rule.h"
#include "cobalt/cssom/css_rule_list.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace cssom {

class CSSRuleVisitor;
class CSSStyleSheet;

// The CSSKeyframesRule interface represents a complete set of keyframes for a
// single animation.
//   https://www.w3.org/TR/2013/WD-css3-animations-20130219/#CSSKeyframesRule-interface
class CSSKeyframesRule : public CSSRule {
 public:
  typedef std::map<std::string, scoped_refptr<CSSKeyframesRule> > NameMap;

  CSSKeyframesRule(const std::string& name,
                   const scoped_refptr<CSSRuleList>& css_rules);

  // Web API: CSSRule
  Type type() const override { return kKeyframesRule; }
  std::string css_text(script::ExceptionState* exception_state) const override;
  void set_css_text(const std::string& css_text,
                    script::ExceptionState* exception_state) override;

  // Web API: CSSKeyframesRule
  //
  const std::string& name() const;

  const scoped_refptr<CSSRuleList>& css_rules() const;

  // Custom, not in any spec.
  //

  // The KeyframeInfo struct represents the same data as is found in each
  // CSSKeyframeRule object within the css_rules_ list, but the data stored here
  // is simplified and there is only one entry per offset.
  struct KeyframeInfo {
    float offset;
    scoped_refptr<const CSSDeclaredStyleData> style;
  };
  // A list of keyframes sorted by offset.
  const std::vector<KeyframeInfo>& sorted_keyframes() const {
    return sorted_keyframes_;
  }

  // From CSSRule.
  void Accept(CSSRuleVisitor* visitor) override;
  void AttachToCSSStyleSheet(CSSStyleSheet* style_sheet) override;

  DEFINE_WRAPPABLE_TYPE(CSSKeyframesRule);

 private:
  ~CSSKeyframesRule() override;
  void UpdateSortedKeyframes();

  // The unprocessed key text.
  std::string name_;

  // The list of CSSKeyframeRule objects that define the keyframes.
  scoped_refptr<CSSRuleList> css_rules_;

  // A list of keyframes sorted by offset.
  std::vector<KeyframeInfo> sorted_keyframes_;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CSS_KEYFRAMES_RULE_H_
