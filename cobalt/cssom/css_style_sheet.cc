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

#include "cobalt/cssom/css_style_sheet.h"

#include "cobalt/cssom/css_condition_rule.h"
#include "cobalt/cssom/css_font_face_rule.h"
#include "cobalt/cssom/css_grouping_rule.h"
#include "cobalt/cssom/css_media_rule.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_rule_list.h"
#include "cobalt/cssom/css_rule_visitor.h"
#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/style_sheet_list.h"
#include "cobalt/dom/dom_exception.h"

namespace cobalt {
namespace cssom {
namespace {

//////////////////////////////////////////////////////////////////////////
// MediaRuleUpdater
//////////////////////////////////////////////////////////////////////////

class MediaRuleUpdater : public CSSRuleVisitor {
 public:
  explicit MediaRuleUpdater(const math::Size& viewport_size)
      : any_condition_value_changed_(false), viewport_size_(viewport_size) {}

  void VisitCSSStyleRule(CSSStyleRule* /*css_style_rule*/) override {}

  void VisitCSSFontFaceRule(CSSFontFaceRule* /*css_font_face_rule*/) override {}

  void VisitCSSMediaRule(CSSMediaRule* css_media_rule) override {
    bool condition_value_changed =
        css_media_rule->EvaluateConditionValueAndReturnIfChanged(
            viewport_size_);
    any_condition_value_changed_ |= condition_value_changed;
  }

  void VisitCSSKeyframeRule(CSSKeyframeRule* /*css_keyframe_rule*/) override {}
  void VisitCSSKeyframesRule(
      CSSKeyframesRule* /*css_keyframes_rule*/) override {}

  bool AnyConditionValueChanged() { return any_condition_value_changed_; }

 private:
  bool any_condition_value_changed_;
  math::Size viewport_size_;

  DISALLOW_COPY_AND_ASSIGN(MediaRuleUpdater);
};

}  // namespace

//////////////////////////////////////////////////////////////////////////
// CSSStyleSheet
//////////////////////////////////////////////////////////////////////////

CSSStyleSheet::CSSStyleSheet()
    : parent_style_sheet_list_(NULL),
      css_parser_(NULL),
      media_rules_changed_(false),
      origin_(kNormalAuthor),
      origin_clean_(false) {}

CSSStyleSheet::CSSStyleSheet(CSSParser* css_parser)
    : parent_style_sheet_list_(NULL),
      css_parser_(css_parser),
      media_rules_changed_(false),
      origin_(kNormalAuthor),
      origin_clean_(false) {}

// https://drafts.csswg.org/cssom/#dom-cssstylesheet-cssrules
const scoped_refptr<CSSRuleList>& CSSStyleSheet::css_rules(
    script::ExceptionState* exception_state) {
  if (!origin_clean_) {
    dom::DOMException::Raise(
        dom::DOMException::kSecurityErr,
        "Website trys to access css rules from a CSSStyleSheet "
        "fetched from another origin.",
        exception_state);
    DCHECK(!null_css_rule_list_);
    return null_css_rule_list_;
  }
  return css_rules_same_origin();
}

const scoped_refptr<CSSRuleList>& CSSStyleSheet::css_rules_same_origin() {
  if (!css_rule_list_) {
    set_css_rules(new CSSRuleList());
  }
  DCHECK(css_rule_list_);
  return css_rule_list_;
}

// https://drafts.csswg.org/cssom/#dom-cssstylesheet-insertrule
unsigned int CSSStyleSheet::InsertRule(
    const std::string& rule, unsigned int index,
    script::ExceptionState* exception_state) {
  if (!origin_clean_) {
    dom::DOMException::Raise(
        dom::DOMException::kSecurityErr,
        "Website trys to insert css rule to a CSSStyleSheet fetched"
        "from another origin.",
        exception_state);
    return 0;
  }
  return css_rules_same_origin()->InsertRule(rule, index);
}

unsigned int CSSStyleSheet::InsertRuleSameOrigin(const std::string& rule,
                                                 unsigned int index) {
  return css_rules_same_origin()->InsertRule(rule, index);
}

void CSSStyleSheet::OnCSSMutation() {
  if (parent_style_sheet_list_) {
    parent_style_sheet_list_->OnCSSMutation();
  }
}

void CSSStyleSheet::AttachToStyleSheetList(StyleSheetList* style_sheet_list) {
  parent_style_sheet_list_ = style_sheet_list;
}

void CSSStyleSheet::DetachFromStyleSheetList() {
  parent_style_sheet_list_ = NULL;
}

void CSSStyleSheet::set_css_rules(
    const scoped_refptr<CSSRuleList>& css_rule_list) {
  DCHECK(css_rule_list);
  if (css_rule_list == css_rule_list_) {
    return;
  }
  css_rule_list->AttachToCSSStyleSheet(this);
  bool rules_possibly_added_or_changed_or_removed =
      (css_rule_list->length() > 0) ||
      (css_rule_list_ && css_rule_list_->length() > 0);
  css_rule_list_ = css_rule_list;
  if (rules_possibly_added_or_changed_or_removed) {
    OnCSSMutation();
  }
}

void CSSStyleSheet::EvaluateMediaRules(const math::Size& viewport_size) {
  // If the media rules change, we have to do an update.
  bool should_update_media_rules = media_rules_changed_;
  media_rules_changed_ = false;

  if (!css_rule_list_) {
    return;
  }

  // If the media parameters change, we have to do an update.
  if (!should_update_media_rules) {
    if (!previous_media_viewport_size_ ||
        viewport_size != *previous_media_viewport_size_) {
      should_update_media_rules = true;
    }
  }

  if (should_update_media_rules) {
    // Evaluate the media rule conditions, and signal a css mutation if any
    // condition values have changed.
    MediaRuleUpdater media_rule_updater(viewport_size);
    css_rule_list_->Accept(&media_rule_updater);
    if (media_rule_updater.AnyConditionValueChanged()) {
      OnCSSMutation();
    }
    previous_media_viewport_size_ = viewport_size;
  }
}

CSSStyleSheet::~CSSStyleSheet() {}

}  // namespace cssom
}  // namespace cobalt
