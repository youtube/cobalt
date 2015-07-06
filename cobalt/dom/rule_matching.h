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

#ifndef DOM_RULE_MATCHING_H_
#define DOM_RULE_MATCHING_H_

#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/selector.h"
#include "cobalt/cssom/style_sheet_list.h"
#include "cobalt/dom/document.h"

namespace cobalt {
namespace dom {

class Element;
class HTMLElement;

// Returns whether a rule matches an element.
bool MatchRuleAndElement(cssom::CSSStyleRule* rule, Element* element);

// Scans one style sheet for rules that match the given element and appends the
// matching rules to |matching_rules|;
void GetMatchingRulesFromStyleSheet(
    const scoped_refptr<cssom::CSSStyleSheet>& style_sheet,
    dom::HTMLElement* element, cssom::RulesWithCascadePriority* matching_rules,
    cssom::Origin origin);

// Scans the user agent style sheet and all style sheets in the given style
// sheet list and updates the matching rules of the root element and its
// descendants by performing rule matching. Only a subset of selectors is
// supported as specified here:
//   http://***REMOVED***cobalt-css#heading=h.s82z8u3l3se
// Those selectors that are supported are implemented after Selectors Level 4.
//   http://www.w3.org/TR/selectors4/
void UpdateMatchingRules(
    const scoped_refptr<dom::Document>& document,
    const scoped_refptr<cssom::CSSStyleSheet>& user_agent_style_sheet);

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_RULE_MATCHING_H_
