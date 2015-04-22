/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef CSSOM_TYPE_SELECTOR_H_
#define CSSOM_TYPE_SELECTOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cobalt/cssom/selector.h"

namespace cobalt {
namespace cssom {

// A type selector represents an instance of the element type in the document
// tree.
//   http://www.w3.org/TR/selectors4/#type-selector
class TypeSelector : public Selector {
 public:
  explicit TypeSelector(const std::string& element_name)
      : element_name_(element_name) {}
  ~TypeSelector() OVERRIDE {}

  Specificity GetSpecificity() const OVERRIDE { return Specificity(0, 0, 1); }

  void Accept(SelectorVisitor* visitor) OVERRIDE;

  const std::string& element_name() const { return element_name_; }

 private:
  const std::string element_name_;

  DISALLOW_COPY_AND_ASSIGN(TypeSelector);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_TYPE_SELECTOR_H_
