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

#ifndef COBALT_CSSOM_COMBINATOR_H_
#define COBALT_CSSOM_COMBINATOR_H_

#include <memory>

#include "base/basictypes.h"

namespace cobalt {
namespace cssom {

class ChildCombinator;
class CombinatorVisitor;
class CompoundSelector;
class DescendantCombinator;
class FollowingSiblingCombinator;
class NextSiblingCombinator;

enum CombinatorType {
  kChildCombinator,
  kDescendantCombinator,
  kNextSiblingCombinator,
  kFollowingSiblingCombinator,
  kCombinatorTypeCount,
};

// Avoids the clang error: arithmetic between different enumeration types.
constexpr size_t kCombinatorCount = static_cast<size_t>(kCombinatorTypeCount);

// A combinator is punctuation that represents a particular kind of relationship
// between the selectors on either side.
//   https://www.w3.org/TR/selectors4/#combinator
class Combinator {
 public:
  Combinator();
  virtual ~Combinator();

  virtual void Accept(CombinatorVisitor* visitor) = 0;

  virtual CombinatorType GetCombinatorType() = 0;

  CompoundSelector* left_selector() { return left_selector_; }
  void set_left_selector(CompoundSelector* left_selector) {
    left_selector_ = left_selector;
  }

  CompoundSelector* right_selector() { return right_selector_.get(); }
  void set_right_selector(std::unique_ptr<CompoundSelector> right_selector);

 private:
  CompoundSelector* left_selector_;
  std::unique_ptr<CompoundSelector> right_selector_;

  DISALLOW_COPY_AND_ASSIGN(Combinator);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_COMBINATOR_H_
