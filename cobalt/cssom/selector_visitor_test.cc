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

#include "cobalt/cssom/selector_visitor.h"

#include "cobalt/cssom/active_pseudo_class.h"
#include "cobalt/cssom/after_pseudo_element.h"
#include "cobalt/cssom/attribute_selector.h"
#include "cobalt/cssom/before_pseudo_element.h"
#include "cobalt/cssom/class_selector.h"
#include "cobalt/cssom/complex_selector.h"
#include "cobalt/cssom/compound_selector.h"
#include "cobalt/cssom/empty_pseudo_class.h"
#include "cobalt/cssom/focus_pseudo_class.h"
#include "cobalt/cssom/hover_pseudo_class.h"
#include "cobalt/cssom/id_selector.h"
#include "cobalt/cssom/not_pseudo_class.h"
#include "cobalt/cssom/type_selector.h"
#include "cobalt/cssom/universal_selector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

class MockSelectorVisitor : public SelectorVisitor {
 public:
  MOCK_METHOD1(VisitUniversalSelector,
               void(UniversalSelector* universal_selector));
  MOCK_METHOD1(VisitTypeSelector, void(TypeSelector* type_selector));
  MOCK_METHOD1(VisitAttributeSelector,
               void(AttributeSelector* attribute_selector));
  MOCK_METHOD1(VisitClassSelector, void(ClassSelector* class_selector));
  MOCK_METHOD1(VisitIdSelector, void(IdSelector* id_selector));

  MOCK_METHOD1(VisitActivePseudoClass,
               void(ActivePseudoClass* active_pseudo_class));
  MOCK_METHOD1(VisitEmptyPseudoClass,
               void(EmptyPseudoClass* empty_pseudo_class));
  MOCK_METHOD1(VisitFocusPseudoClass,
               void(FocusPseudoClass* focus_pseudo_class));
  MOCK_METHOD1(VisitHoverPseudoClass,
               void(HoverPseudoClass* hover_pseudo_class));
  MOCK_METHOD1(VisitNotPseudoClass, void(NotPseudoClass* not_pseudo_class));

  MOCK_METHOD1(VisitAfterPseudoElement,
               void(AfterPseudoElement* after_pseudo_element));
  MOCK_METHOD1(VisitBeforePseudoElement,
               void(BeforePseudoElement* before_pseudo_element));

  MOCK_METHOD1(VisitCompoundSelector,
               void(CompoundSelector* compound_selector));
  MOCK_METHOD1(VisitComplexSelector, void(ComplexSelector* complex_selector));
};

TEST(SelectorVisitorTest, VisitsUniversalSelector) {
  UniversalSelector universal_selector;
  MockSelectorVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitUniversalSelector(&universal_selector));
  universal_selector.Accept(&mock_visitor);
}

TEST(SelectorVisitorTest, VisitsTypeSelector) {
  TypeSelector type_selector("div");
  MockSelectorVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitTypeSelector(&type_selector));
  type_selector.Accept(&mock_visitor);
}

TEST(SelectorVisitorTest, VisitsAttributeSelector) {
  AttributeSelector attribute_selector("attr");
  MockSelectorVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitAttributeSelector(&attribute_selector));
  attribute_selector.Accept(&mock_visitor);
}

TEST(SelectorVisitorTest, VisitsClassSelector) {
  ClassSelector class_selector("my-class");
  MockSelectorVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitClassSelector(&class_selector));
  class_selector.Accept(&mock_visitor);
}

TEST(SelectorVisitorTest, VisitsIdSelector) {
  IdSelector id_selector("id");
  MockSelectorVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitIdSelector(&id_selector));
  id_selector.Accept(&mock_visitor);
}

TEST(SelectorVisitorTest, VisitsActivePseudoClass) {
  ActivePseudoClass active_pseudo_class;
  MockSelectorVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitActivePseudoClass(&active_pseudo_class));
  active_pseudo_class.Accept(&mock_visitor);
}

TEST(SelectorVisitorTest, VisitsFocusPseudoClass) {
  FocusPseudoClass focus_pseudo_class;
  MockSelectorVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitFocusPseudoClass(&focus_pseudo_class));
  focus_pseudo_class.Accept(&mock_visitor);
}

TEST(SelectorVisitorTest, VisitsEmptyPseudoClass) {
  EmptyPseudoClass empty_pseudo_class;
  MockSelectorVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitEmptyPseudoClass(&empty_pseudo_class));
  empty_pseudo_class.Accept(&mock_visitor);
}

TEST(SelectorVisitorTest, VisitsHoverPseudoClass) {
  HoverPseudoClass hover_pseudo_class;
  MockSelectorVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitHoverPseudoClass(&hover_pseudo_class));
  hover_pseudo_class.Accept(&mock_visitor);
}

TEST(SelectorVisitorTest, VisitsNotPseudoClass) {
  NotPseudoClass not_pseudo_class;
  MockSelectorVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitNotPseudoClass(&not_pseudo_class));
  not_pseudo_class.Accept(&mock_visitor);
}

TEST(SelectorVisitorTest, VisitsAfterPseudoElement) {
  AfterPseudoElement after_pseudo_element;
  MockSelectorVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitAfterPseudoElement(&after_pseudo_element));
  after_pseudo_element.Accept(&mock_visitor);
}

TEST(SelectorVisitorTest, VisitsBeforePseudoElement) {
  BeforePseudoElement before_pseudo_element;
  MockSelectorVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitBeforePseudoElement(&before_pseudo_element));
  before_pseudo_element.Accept(&mock_visitor);
}

TEST(SelectorVisitorTest, VisitCompoundSelector) {
  CompoundSelector compound_selector;
  MockSelectorVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitCompoundSelector(&compound_selector));
  compound_selector.Accept(&mock_visitor);
}

TEST(SelectorVisitorTest, VisitComplexSelector) {
  ComplexSelector complex_selector;
  MockSelectorVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitComplexSelector(&complex_selector));
  complex_selector.Accept(&mock_visitor);
}

}  // namespace cssom
}  // namespace cobalt
