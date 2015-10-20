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

#ifndef CSSOM_CLASS_SELECTOR_H_
#define CSSOM_CLASS_SELECTOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cobalt/cssom/selector_tree.h"
#include "cobalt/cssom/simple_selector.h"
#include "cobalt/cssom/specificity.h"

namespace cobalt {
namespace cssom {

class SelectorVisitor;

// The class selector represents an element belonging to the class identified by
// the identifier.
//   http://www.w3.org/TR/selectors4/#class-selector
class ClassSelector : public SimpleSelector {
 public:
  explicit ClassSelector(const std::string& class_name)
      : class_name_(class_name) {}
  ~ClassSelector() OVERRIDE {}

  // From Selector.
  void Accept(SelectorVisitor* visitor) OVERRIDE;
  Specificity GetSpecificity() const OVERRIDE { return Specificity(0, 1, 0); }

  // From SimpleSelector.
  ClassSelector* AsClassSelector() OVERRIDE { return this; }
  int GetRank() const OVERRIDE { return kClassSelectorRank; }
  std::string GetSelectorText() const OVERRIDE { return "." + class_name_; }
  void IndexSelectorTreeNode(SelectorTree::Node* parent_node,
                             SelectorTree::Node* child_node,
                             CombinatorType combinator) OVERRIDE;

  // Rest of public methods.

  const std::string& class_name() const { return class_name_; }

 private:
  const std::string class_name_;

  DISALLOW_COPY_AND_ASSIGN(ClassSelector);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_CLASS_SELECTOR_H_
