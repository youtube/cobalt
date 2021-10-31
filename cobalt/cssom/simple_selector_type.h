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

#ifndef COBALT_CSSOM_SIMPLE_SELECTOR_TYPE_H_
#define COBALT_CSSOM_SIMPLE_SELECTOR_TYPE_H_

namespace cobalt {
namespace cssom {

// Define the following enum here instead of inside simple_selector.h to avoid
// circular dependency between header files.
// It is also used as the order to normalize simple selectors in a compound
// selector.
enum SimpleSelectorType {
  kUniversalSelector,
  kTypeSelector,
  kAttributeSelector,
  kClassSelector,
  kIdSelector,
  kPseudoClass,
  kPseudoElement,
  kSimpleSelectorTypeCount,
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_SIMPLE_SELECTOR_TYPE_H_
