// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_CSSOM_CHILD_COMBINATOR_H_
#define COBALT_CSSOM_CHILD_COMBINATOR_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cobalt/cssom/combinator.h"

namespace cobalt {
namespace cssom {

class CombinatorVisitor;

// Child combinator describes a childhood relationship between two elements.
//   https://www.w3.org/TR/selectors4/#child-combinators
class ChildCombinator : public Combinator {
 public:
  ChildCombinator() {}
  ~ChildCombinator() override {}

  // From Combinator.
  void Accept(CombinatorVisitor* visitor) override;
  CombinatorType GetCombinatorType() override { return kChildCombinator; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChildCombinator);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CHILD_COMBINATOR_H_
