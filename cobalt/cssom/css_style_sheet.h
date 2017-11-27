// Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef COBALT_CSSOM_CSS_STYLE_SHEET_H_
#define COBALT_CSSOM_CSS_STYLE_SHEET_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "cobalt/cssom/cascade_precedence.h"
#include "cobalt/cssom/mutation_observer.h"
#include "cobalt/cssom/style_sheet.h"
#include "cobalt/math/size.h"
#include "cobalt/script/exception_state.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace cssom {

class CSSParser;
class CSSRuleList;
class CSSStyleRule;
class PropertyValue;
class StyleSheetList;

// The CSSStyleSheet interface represents a CSS style sheet.
//   https://www.w3.org/TR/2013/WD-cssom-20131205/#the-cssstylesheet-interface
class CSSStyleSheet : public StyleSheet, public MutationObserver {
 public:
  CSSStyleSheet();
  explicit CSSStyleSheet(CSSParser* css_parser);

  // Web API: CSSStyleSheet
  //
  // Returns a read-only, live object representing the CSS rules.
  const scoped_refptr<CSSRuleList>& css_rules(
      cobalt::script::ExceptionState* exception_state);

  // Bypass same origin policy to get css rules. This assumes that the request
  // comes from same origin and should not be accessible to javascript code.
  const scoped_refptr<CSSRuleList>& css_rules_same_origin();

  // Inserts a new rule into the current style sheet. This Web API takes a
  // string as input and parses it into a rule.
  unsigned int InsertRule(const std::string& rule, unsigned int index,
                          cobalt::script::ExceptionState* exception_state);

  // Insert css rules without disallowing cross-origin access. This should be
  // used internally by Cobalt.
  unsigned int InsertRuleSameOrigin(const std::string& rule,
                                    unsigned int index);

  // Custom, not in any spec.
  //
  // From StyleSheet.
  void AttachToStyleSheetList(StyleSheetList* style_sheet_list) override;
  void DetachFromStyleSheetList() override;
  StyleSheetList* ParentStyleSheetList() const override {
    return parent_style_sheet_list_;
  }
  void SetLocationUrl(const GURL& url) override { location_url_ = url; }
  const GURL& LocationUrl() const override { return location_url_; }
  scoped_refptr<CSSStyleSheet> AsCSSStyleSheet() override { return this; }

  // From MutationObserver.
  void OnCSSMutation() override;

  CSSParser* css_parser() const { return css_parser_; }
  void set_css_rules(const scoped_refptr<CSSRuleList>& css_rule_list);

  Origin origin() const { return origin_; }
  void set_origin(Origin origin) { origin_ = origin; }

  // This performs a recalculation of the media rule expressions, if needed.
  void EvaluateMediaRules(const math::Size& viewport_size);

  // Should be called when a media rule is added or modified. It sets a flag
  // that is reset in EvaluateMediaRules().
  void OnMediaRuleMutation() { media_rules_changed_ = true; }

  void SetOriginClean(bool origin_clean) { origin_clean_ = origin_clean; }

  DEFINE_WRAPPABLE_TYPE(CSSStyleSheet);

 private:
  ~CSSStyleSheet() override;

  scoped_refptr<CSSRuleList> css_rule_list_;

  // Null scoped_refptr used when access to css rules should be blocked.
  scoped_refptr<CSSRuleList> null_css_rule_list_;

  StyleSheetList* parent_style_sheet_list_;
  CSSParser* const css_parser_;
  GURL location_url_;

  // When true, this means a media rule has been added or modified, and a
  // re-evaluation of the media at-rules is needed.
  bool media_rules_changed_;

  // This stores the most recent media parameters, used to detect when they
  // change, which will require a re-evaluation of the media rule expressions.
  base::optional<math::Size> previous_media_viewport_size_;

  // Origin of this style sheet.
  Origin origin_;

  // https://drafts.csswg.org/cssom/#concept-css-style-sheet-origin-clean-flag
  // It is used to block cross-origin website's access to the fetched style
  // sheet. It is only possible to be set false when creating a HTMLLinkElement
  bool origin_clean_;

  // Since CSSRuleList is merely a proxy, it needs access to CSS rules stored
  // in the stylesheet.
  friend class CSSRuleList;

  DISALLOW_COPY_AND_ASSIGN(CSSStyleSheet);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CSS_STYLE_SHEET_H_
