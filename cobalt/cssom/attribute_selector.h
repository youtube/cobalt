// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_CSSOM_ATTRIBUTE_SELECTOR_H_
#define COBALT_CSSOM_ATTRIBUTE_SELECTOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cobalt/base/tokens.h"
#include "cobalt/cssom/selector_tree.h"
#include "cobalt/cssom/simple_selector.h"
#include "cobalt/cssom/specificity.h"

namespace cobalt {
namespace cssom {

class SelectorVisitor;

// An attribute selector represents an element that has an attribute that
// matches the attribute represented by the attribute selector.
//   https://www.w3.org/TR/selectors4/#attribute-selector
class AttributeSelector : public SimpleSelector {
 public:
  enum ValueMatchType {
    kNoMatch,
    kEquals,
    kIncludes,
    kDashMatch,
    kBeginsWith,
    kEndsWith,
    kContains,
  };

  explicit AttributeSelector(const std::string& attribute_name)
      : SimpleSelector(kAttributeSelector, base::Token(),
                       base::Token(attribute_name)),
        value_match_type_(kNoMatch) {}
  AttributeSelector(const std::string& attribute_name,
                    ValueMatchType value_match_type,
                    const std::string& attribute_value)
      : SimpleSelector(kAttributeSelector, base::Token(),
                       base::Token(attribute_name)),
        value_match_type_(value_match_type),
        value_(attribute_value) {}
  ~AttributeSelector() override {}

  // From Selector.
  void Accept(SelectorVisitor* visitor) override;
  Specificity GetSpecificity() const override { return Specificity(0, 1, 0); }

  // From SimpleSelector.
  bool AlwaysRequiresRuleMatchingVerificationVisit() const override {
    return true;
  }

  void IndexSelectorTreeNode(SelectorTree::Node* parent_node,
                             SelectorTree::Node* child_node,
                             CombinatorType combinator) override;

  // Rest of public methods.
  base::Token attribute_name() const { return text(); }
  ValueMatchType value_match_type() const { return value_match_type_; }
  const std::string& attribute_value() const { return value_; }

 private:
  ValueMatchType value_match_type_;
  std::string value_;

  DISALLOW_COPY_AND_ASSIGN(AttributeSelector);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_ATTRIBUTE_SELECTOR_H_
