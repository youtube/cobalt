// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_CSSOM_SERIALIZER_H_
#define COBALT_CSSOM_SERIALIZER_H_

#include <string>

#include "cobalt/base/token.h"
#include "cobalt/cssom/combinator_visitor.h"
#include "cobalt/cssom/selector.h"
#include "cobalt/cssom/selector_visitor.h"

namespace cobalt {
namespace cssom {

class PseudoClass;
class PseudoElement;

// Serializes various types in the CSSOM to a string. Currently only implements
// selector serialization; declarations, rules, and whole sheets aren't yet
// implemented.
class Serializer : public SelectorVisitor, public CombinatorVisitor {
 public:
  Serializer(const Serializer& other) = delete;
  Serializer& operator=(const Serializer& other) = delete;

  explicit Serializer(std::string* output);

  void SerializeIdentifier(base::Token identifier);
  void SerializeString(const std::string& string);
  void SerializeSelectors(const Selectors& selectors);
  void SerializeSelector(const Selector& selector);

 private:
  // Decode a UTF-8 character pointed to by |p| into |out_c|.
  // Returns the number of bytes consumed.
  // TODO: Move this to a place it can be shared, or use another shared impl.
  int DecodeUTF8(const uint8_t* p, uint32_t* out_c);

  // Escape a Unicode code point with its hex value.
  void EscapeCodePoint(uint32_t c);

  // Escape an ASCII character with a backslash.
  void EscapeCharacter(uint32_t c);

  // Simple selectors - Overrides from SelectorVisitor.
  void VisitUniversalSelector(UniversalSelector* universal_selector) override;
  void VisitTypeSelector(TypeSelector* type_selector) override;
  void VisitAttributeSelector(AttributeSelector* attribute_selector) override;
  void VisitClassSelector(ClassSelector* class_selector) override;
  void VisitIdSelector(IdSelector* id_selector) override;

  // Pseudo classes - Overrides from SelectorVisitor.
  void VisitPseudoClass(PseudoClass* pseudo_class);
  void VisitActivePseudoClass(ActivePseudoClass* active_pseudo_class) override;
  void VisitEmptyPseudoClass(EmptyPseudoClass* empty_pseudo_class) override;
  void VisitFocusPseudoClass(FocusPseudoClass* focus_pseudo_class) override;
  void VisitHoverPseudoClass(HoverPseudoClass* hover_pseudo_class) override;
  void VisitNotPseudoClass(NotPseudoClass* not_pseudo_class) override;

  // Pseudo elements - Overrides from SelectorVisitor.
  void VisitPseudoElement(PseudoElement* pseudo_element);
  void VisitAfterPseudoElement(
      AfterPseudoElement* after_pseudo_element) override;
  void VisitBeforePseudoElement(
      BeforePseudoElement* before_pseudo_element) override;

  // Compound selector - Overrides from SelectorVisitor.
  void VisitCompoundSelector(CompoundSelector* compound_selector) override;

  // Complex selector - Overrides from SelectorVisitor.
  void VisitComplexSelector(ComplexSelector* complex_selector) override;

  // Combinators - Overrides from CombinatorVisitor.
  void VisitChildCombinator(ChildCombinator* child_combinator) override;
  void VisitNextSiblingCombinator(
      NextSiblingCombinator* next_sibling_combinator) override;
  void VisitDescendantCombinator(
      DescendantCombinator* descendant_combinator) override;
  void VisitFollowingSiblingCombinator(
      FollowingSiblingCombinator* following_sibling_combinator) override;

  // Pointer to the output string provided by the user.
  std::string* output_;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_SERIALIZER_H_
