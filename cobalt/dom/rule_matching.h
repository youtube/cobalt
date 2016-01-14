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

#ifndef COBALT_DOM_RULE_MATCHING_H_
#define COBALT_DOM_RULE_MATCHING_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/selector_tree.h"

namespace cobalt {
namespace dom {

class Element;
class HTMLElement;
class NodeList;
class Node;

// Updates the matching rules on an element and its children, using the given
// selector tree.
void UpdateMatchingRulesUsingSelectorTree(
    HTMLElement* dom_root, const cssom::SelectorTree* selector_tree);

// Returns the first element in the subtree that matches the given selector.
scoped_refptr<Element> QuerySelector(Node* node, const std::string& selectors,
                                     cssom::CSSParser* css_parser);

// Returns all the elements in the subtree that match the given selector.
scoped_refptr<NodeList> QuerySelectorAll(Node* node,
                                         const std::string& selectors,
                                         cssom::CSSParser* css_parser);

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_RULE_MATCHING_H_
