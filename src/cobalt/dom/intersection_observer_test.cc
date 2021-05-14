// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/percentage_value.h"
#include "cobalt/cssom/testing/mock_css_parser.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_stat_tracker.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/testing/fake_exception_state.h"
#include "cobalt/dom/testing/mock_layout_boxes.h"
#include "cobalt/dom/testing/stub_environment_settings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using cobalt::cssom::PercentageValue;
using cobalt::cssom::PropertyListValue;
using cobalt::dom::testing::MockLayoutBoxes;
using ::testing::NiceMock;
using ::testing::_;
using ::testing::Return;

namespace cobalt {
namespace dom {

class MockDocumentObserver : public DocumentObserver {
 public:
  MOCK_METHOD0(OnLoad, void());

  MOCK_METHOD0(OnMutation, void());

  MOCK_METHOD0(OnFocusChanged, void());
};

class IntersectionCallbackMock {
 public:
  MOCK_METHOD2(
      NativeIntersectionObserverCallback,
      void(const IntersectionObserver::IntersectionObserverEntrySequence&,
           const scoped_refptr<IntersectionObserver>&));
};

class IntersectionObserverTest : public ::testing::Test {
 protected:
  IntersectionObserverTest()
      : dom_stat_tracker_(new DomStatTracker("IntersectionObserverTest")),
        html_element_context_(&environment_settings_, NULL, NULL, &css_parser_,
                              NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                              NULL, NULL, NULL, NULL, dom_stat_tracker_.get(),
                              "", base::kApplicationStateStarted, NULL, NULL),
        document_(new Document(&html_element_context_)) {}

  scoped_refptr<Document> document() { return document_; }
  scoped_refptr<IntersectionObserver> CreateIntersectionObserver();
  void CreateDocumentWithTwoChildrenAndMockLayoutBoxes();

 private:
  std::unique_ptr<DomStatTracker> dom_stat_tracker_;
  testing::StubEnvironmentSettings environment_settings_;
  NiceMock<cssom::testing::MockCSSParser> css_parser_;
  testing::FakeExceptionState exception_state_;
  HTMLElementContext html_element_context_;
  scoped_refptr<Document> document_;
  IntersectionCallbackMock callback_mock_;
};

scoped_refptr<IntersectionObserver>
IntersectionObserverTest::CreateIntersectionObserver() {
  // The intersection observer constructor will use the mock css parser to parse
  // the root margin property value. Because "ParsePropertyValue()"" returns a
  // "PropertyValue", which is not a built-in type, we must set a default
  // action and return type here.
  std::unique_ptr<PropertyListValue::Builder> builder(
      new PropertyListValue::Builder());
  builder->push_back(new PercentageValue(0.0f));
  scoped_refptr<PropertyListValue> property_list_value =
      new PropertyListValue(std::move(builder));
  ON_CALL(css_parser_, ParsePropertyValue(_, _, _))
      .WillByDefault(Return(property_list_value));

  return new IntersectionObserver(
      document_, &css_parser_,
      base::Bind(&IntersectionCallbackMock::NativeIntersectionObserverCallback,
                 base::Unretained(&callback_mock_)),
      &exception_state_);
}

void IntersectionObserverTest::
    CreateDocumentWithTwoChildrenAndMockLayoutBoxes() {
  scoped_refptr<HTMLElement> first_child_element_ =
      document_->CreateElement("div")->AsHTMLElement();
  document_->AppendChild(first_child_element_);

  scoped_refptr<HTMLElement> second_child_element_ =
      document_->CreateElement("div")->AsHTMLElement();
  first_child_element_->AppendChild(second_child_element_);

  std::unique_ptr<MockLayoutBoxes> layout_boxes(new MockLayoutBoxes);
  document_->document_element()->AsHTMLElement()->set_layout_boxes(
      std::unique_ptr<LayoutBoxes>(layout_boxes.release()));
}

TEST_F(IntersectionObserverTest, RegisterIntersectionObserverForTarget) {
  CreateDocumentWithTwoChildrenAndMockLayoutBoxes();

  NiceMock<MockDocumentObserver> document_observer;
  document()->AddObserver(&document_observer);

  std::unique_ptr<ElementIntersectionObserverModule>
      element_intersection_observer_module =
          std::make_unique<ElementIntersectionObserverModule>(
              document()->last_element_child());

  // Treat the second child element as an intersection observer target
  // (the first child element is the same as the document element).
  // When the observer is registered, it should record a mutation on the
  // document and invalidate the layout boxes up to the document element.
  EXPECT_NE(document()->document_element()->AsHTMLElement()->layout_boxes(),
            nullptr);
  EXPECT_CALL(document_observer, OnMutation()).Times(1);
  scoped_refptr<IntersectionObserver> intersection_observer =
      CreateIntersectionObserver();
  element_intersection_observer_module->RegisterIntersectionObserverForTarget(
      intersection_observer);
  EXPECT_EQ(document()->document_element()->AsHTMLElement()->layout_boxes(),
            nullptr);
}

}  // namespace dom
}  // namespace cobalt
