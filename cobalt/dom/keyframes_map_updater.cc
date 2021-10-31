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

#include "cobalt/dom/keyframes_map_updater.h"

#include <string>

#include "cobalt/cssom/css_keyframes_rule.h"
#include "cobalt/cssom/css_rule_list.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/property_list_value.h"
#include "cobalt/cssom/property_value_visitor.h"
#include "cobalt/cssom/style_sheet_list.h"

namespace cobalt {
namespace dom {

KeyframesMapUpdater::KeyframesMapUpdater(
    cssom::CSSKeyframesRule::NameMap* keyframes_map)
    : keyframes_map_(keyframes_map) {
  keyframes_map_->clear();
}

void KeyframesMapUpdater::ProcessCSSStyleSheet(
    const scoped_refptr<cssom::CSSStyleSheet>& style_sheet) {
  // Iterate through each rule in the stylesheet and add any CSSKeyframesRule
  // objects to our keyframes map.
  if (style_sheet && style_sheet->css_rules_same_origin()) {
    style_sheet->css_rules_same_origin()->Accept(this);
  }
}

void KeyframesMapUpdater::ProcessStyleSheetList(
    const scoped_refptr<cssom::StyleSheetList>& style_sheet_list) {
  // Iterate through each style sheet and apply ProcessCSSStyleSheet() to it.
  for (unsigned int style_sheet_index = 0;
       style_sheet_index < style_sheet_list->length(); ++style_sheet_index) {
    scoped_refptr<cssom::CSSStyleSheet> style_sheet =
        style_sheet_list->Item(style_sheet_index)->AsCSSStyleSheet();
    if (style_sheet) {
      ProcessCSSStyleSheet(style_sheet);
    }
  }
}

void KeyframesMapUpdater::VisitCSSKeyframesRule(
    cssom::CSSKeyframesRule* css_keyframes_rule) {
  (*keyframes_map_)[css_keyframes_rule->name()] = css_keyframes_rule;
}

}  // namespace dom
}  // namespace cobalt
