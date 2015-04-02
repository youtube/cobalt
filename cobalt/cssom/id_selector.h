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

#ifndef CSSOM_ID_SELECTOR_H_
#define CSSOM_ID_SELECTOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cobalt/cssom/selector.h"

namespace cobalt {
namespace cssom {

// An ID selector represents an element instance that has an identifier that
// matches the identifier in the ID selector.
//   http://www.w3.org/TR/selectors4/#id-selector
class IdSelector : public Selector {
 public:
  explicit IdSelector(const std::string& id) : id_(id) {}
  ~IdSelector() OVERRIDE {}

  void Accept(SelectorVisitor* visitor) OVERRIDE;

  const std::string& id() const { return id_; }

 private:
  const std::string id_;

  DISALLOW_COPY_AND_ASSIGN(IdSelector);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_ID_SELECTOR_H_
