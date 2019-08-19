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

#ifndef COBALT_DOM_RULE_MATCHING_H_
#define COBALT_DOM_RULE_MATCHING_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_style_rule.h"

namespace cobalt {
namespace dom {

class Element;
class HTMLElement;
class NodeList;
class Node;

// Only a subset of selectors is supported.
// TODO: Add markdown file with set of supported selectors.
//
// Those selectors that are supported are implemented after Selectors Level 4.
//   https://www.w3.org/TR/selectors4/

// Updates the matching rules on an element. The parent and previous sibling
// need to have their matching rules updated before, since the current element's
// rule matching will depend on their rule matching states.
void UpdateElementMatchingRules(HTMLElement* current_element);

// Returns the first element in the subtree that matches the given selector.
scoped_refptr<Element> QuerySelector(Node* node, const std::string& selectors,
                                     cssom::CSSParser* css_parser);

// Returns all the elements in the subtree that match the given selector.
scoped_refptr<NodeList> QuerySelectorAll(Node* node,
                                         const std::string& selectors,
                                         cssom::CSSParser* css_parser);

// Returns true  if any of the selectors in the rule matches the given element.
bool MatchRuleAndElement(cssom::CSSStyleRule* rule, Element* element);

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_RULE_MATCHING_H_
