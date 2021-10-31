// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_CSSOM_SELECTOR_VISITOR_H_
#define COBALT_CSSOM_SELECTOR_VISITOR_H_

namespace cobalt {
namespace cssom {

class ActivePseudoClass;
class AfterPseudoElement;
class AttributeSelector;
class BeforePseudoElement;
class ClassSelector;
class ComplexSelector;
class CompoundSelector;
class EmptyPseudoClass;
class FocusPseudoClass;
class HoverPseudoClass;
class IdSelector;
class NotPseudoClass;
class TypeSelector;
class UniversalSelector;

// Type-safe branching on a class hierarchy of CSS selectors,
// implemented after a classical GoF pattern (see
// http://en.wikipedia.org/wiki/Visitor_pattern#Java_example).
class SelectorVisitor {
 public:
  // Simple selectors.

  virtual void VisitUniversalSelector(
      UniversalSelector* universal_selector) = 0;
  virtual void VisitTypeSelector(TypeSelector* type_selector) = 0;
  virtual void VisitAttributeSelector(
      AttributeSelector* attribute_selector) = 0;
  virtual void VisitClassSelector(ClassSelector* class_selector) = 0;
  virtual void VisitIdSelector(IdSelector* id_selector) = 0;

  // Pseudo classes.
  virtual void VisitActivePseudoClass(
      ActivePseudoClass* active_pseudo_class) = 0;
  virtual void VisitEmptyPseudoClass(EmptyPseudoClass* empty_pseudo_class) = 0;
  virtual void VisitFocusPseudoClass(FocusPseudoClass* focus_pseudo_class) = 0;
  virtual void VisitHoverPseudoClass(HoverPseudoClass* hover_pseudo_class) = 0;
  virtual void VisitNotPseudoClass(NotPseudoClass* not_pseudo_class) = 0;

  // Pseudo elements.
  virtual void VisitAfterPseudoElement(
      AfterPseudoElement* after_pseudo_element) = 0;
  virtual void VisitBeforePseudoElement(
      BeforePseudoElement* before_pseudo_element) = 0;

  // Compound selector.

  virtual void VisitCompoundSelector(CompoundSelector* compound_selector) = 0;

  // Complex selector.

  virtual void VisitComplexSelector(ComplexSelector* complex_selector) = 0;

 protected:
  ~SelectorVisitor() {}
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_SELECTOR_VISITOR_H_
