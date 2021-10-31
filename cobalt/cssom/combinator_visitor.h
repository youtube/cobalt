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

#ifndef COBALT_CSSOM_COMBINATOR_VISITOR_H_
#define COBALT_CSSOM_COMBINATOR_VISITOR_H_

namespace cobalt {
namespace cssom {

class ChildCombinator;
class DescendantCombinator;
class FollowingSiblingCombinator;
class NextSiblingCombinator;

class CombinatorVisitor {
 public:
  virtual void VisitChildCombinator(ChildCombinator* child_combinator) = 0;
  virtual void VisitNextSiblingCombinator(
      NextSiblingCombinator* next_sibling_combinator) = 0;
  virtual void VisitDescendantCombinator(
      DescendantCombinator* descendant_combinator) = 0;
  virtual void VisitFollowingSiblingCombinator(
      FollowingSiblingCombinator* following_sibling_combinator) = 0;

 protected:
  ~CombinatorVisitor() {}
};
}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_COMBINATOR_VISITOR_H_
