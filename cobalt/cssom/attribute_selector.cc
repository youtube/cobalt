// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/attribute_selector.h"

#include "cobalt/cssom/selector_visitor.h"

namespace cobalt {
namespace cssom {

namespace {

std::string ValueMatchTypeToString(AttributeSelector::ValueMatchType type) {
  // The full string isn't necessary. Simply convert the underlying type to
  // a string.
  return std::to_string(type);
}

// Helper function to convert the parameters to a single value.
std::string GetValue(AttributeSelector::ValueMatchType value_match_type,
                     const std::string& attribute_value) {
  return ValueMatchTypeToString(value_match_type) + "_" + attribute_value;
}

}  // namespace

AttributeSelector::AttributeSelector(const std::string& attribute_name)
    : SimpleSelector(kAttributeSelector, base_token::Token(),
                     base_token::Token(attribute_name)),
      value_match_type_(kNoMatch) {}

AttributeSelector::AttributeSelector(const std::string& attribute_name,
                                     ValueMatchType value_match_type,
                                     const std::string& attribute_value)
    : SimpleSelector(kAttributeSelector, base_token::Token(),
                     base_token::Token(attribute_name),
                     GetValue(value_match_type, attribute_value)),
      value_match_type_(value_match_type),
      value_(attribute_value) {}

void AttributeSelector::Accept(SelectorVisitor* visitor) {
  visitor->VisitAttributeSelector(this);
}

void AttributeSelector::IndexSelectorTreeNode(SelectorTree::Node* parent_node,
                                              SelectorTree::Node* child_node,
                                              CombinatorType combinator) {
  parent_node->AppendSimpleSelector(attribute_name(), kAttributeSelector,
                                    combinator, child_node);
}

}  // namespace cssom
}  // namespace cobalt
