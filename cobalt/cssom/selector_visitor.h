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

#ifndef CSSOM_SELECTOR_VISITOR_H_
#define CSSOM_SELECTOR_VISITOR_H_

namespace cobalt {
namespace cssom {

class ClassSelector;
class IdSelector;
class TypeSelector;

// Type-safe branching on a class hierarchy of CSS selectors,
// implemented after a classical GoF pattern (see
// http://en.wikipedia.org/wiki/Visitor_pattern#Java_example).
class SelectorVisitor {
 public:
  virtual void VisitClassSelector(ClassSelector* class_selector) = 0;
  virtual void VisitIdSelector(IdSelector* id_selector) = 0;
  virtual void VisitTypeSelector(TypeSelector* type_selector) = 0;

 protected:
  ~SelectorVisitor() {}
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_SELECTOR_VISITOR_H_
