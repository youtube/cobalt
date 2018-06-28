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

#ifndef COBALT_CSSOM_CSS_RULE_VISITOR_H_
#define COBALT_CSSOM_CSS_RULE_VISITOR_H_

namespace cobalt {
namespace cssom {

class CSSStyleRule;
class CSSMediaRule;
class CSSFontFaceRule;
class CSSKeyframeRule;
class CSSKeyframesRule;

// Type-safe branching on a class hierarchy of CSS selectors,
// implemented after a classical GoF pattern (see
// http://en.wikipedia.org/wiki/Visitor_pattern#Java_example).
class CSSRuleVisitor {
 public:
  // Simple selectors.
  virtual void VisitCSSStyleRule(CSSStyleRule* css_style_rule) = 0;
  virtual void VisitCSSFontFaceRule(CSSFontFaceRule* css_font_face_rule) = 0;
  virtual void VisitCSSMediaRule(CSSMediaRule* css_media_rule) = 0;
  virtual void VisitCSSKeyframeRule(CSSKeyframeRule* css_keyframe_rule) = 0;
  virtual void VisitCSSKeyframesRule(CSSKeyframesRule* css_keyframes_rule) = 0;

 protected:
  ~CSSRuleVisitor() {}
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CSS_RULE_VISITOR_H_
