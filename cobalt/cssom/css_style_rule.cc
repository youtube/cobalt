// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/css_style_rule.h"

#include "cobalt/cssom/css_rule_style_declaration.h"
#include "cobalt/cssom/css_rule_visitor.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/serializer.h"

namespace cobalt {
namespace cssom {

CSSStyleRule::CSSStyleRule() : added_to_selector_tree_(false) {}

CSSStyleRule::CSSStyleRule(Selectors selectors,
                           const scoped_refptr<CSSRuleStyleDeclaration>& style)
    : selectors_(std::move(selectors)),
      style_(style),
      added_to_selector_tree_(false) {
  if (style_) {
    style_->set_parent_rule(this);
  }
}

std::string CSSStyleRule::selector_text() const {
  std::string output;
  Serializer serializer(&output);
  serializer.SerializeSelectors(selectors_);
  return output;
}

const scoped_refptr<CSSStyleDeclaration> CSSStyleRule::style() const {
  return style_;
}

std::string CSSStyleRule::css_text(
    script::ExceptionState* exception_state) const {
  return style_ ? style_->css_text(exception_state) : "";
}

void CSSStyleRule::set_css_text(const std::string& css_text,
                                script::ExceptionState* exception_state) {
  if (!style_) {
    DCHECK(parent_style_sheet());
    style_ = new CSSRuleStyleDeclaration(parent_style_sheet()->css_parser());
  }
  style_->set_css_text(css_text, exception_state);
  style_->set_parent_rule(this);
}

void CSSStyleRule::Accept(CSSRuleVisitor* visitor) {
  visitor->VisitCSSStyleRule(this);
}

void CSSStyleRule::AttachToCSSStyleSheet(CSSStyleSheet* style_sheet) {
  set_parent_style_sheet(style_sheet);
  if (style_) {
    style_->set_mutation_observer(style_sheet);
  }
}

CSSStyleRule::~CSSStyleRule() {}

}  // namespace cssom
}  // namespace cobalt
