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

#include "cobalt/dom/dom_string_map.h"

#include <string>

#include "cobalt/dom/document.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/script/testing/mock_exception_state.h"
#include "cobalt/script/testing/mock_property_enumerator.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::StrictMock;

namespace cobalt {
namespace dom {

class DOMStringMapTest : public ::testing::Test {
 protected:
  DOMStringMapTest();
  ~DOMStringMapTest() override {}

  HTMLElementContext html_element_context_;
  scoped_refptr<Document> document_;
  scoped_refptr<Element> element_;
  scoped_refptr<DOMStringMap> dom_string_map_;
  StrictMock<script::testing::MockExceptionState> exception_state_;
};

DOMStringMapTest::DOMStringMapTest()
    : document_(new Document(&html_element_context_)),
      element_(new Element(document_, base::Token("element"))),
      dom_string_map_(new DOMStringMap(element_)) {}

TEST_F(DOMStringMapTest, InvalidPrefix) {
  element_->SetAttribute("data", "Los Angeles");

  StrictMock<script::testing::MockPropertyEnumerator> property_enumerator;
  dom_string_map_->EnumerateNamedProperties(&property_enumerator);

  EXPECT_FALSE(dom_string_map_->CanQueryNamedProperty(""));

  dom_string_map_->AnonymousNamedSetter("", "San Francisco", &exception_state_);
  EXPECT_EQ(std::string("Los Angeles"), element_->GetAttribute("data"));
}

TEST_F(DOMStringMapTest, Empty) {
  element_->SetAttribute("data-", "Los Angeles");

  StrictMock<script::testing::MockPropertyEnumerator> enumerator;
  EXPECT_CALL(enumerator, AddProperty(""));
  dom_string_map_->EnumerateNamedProperties(&enumerator);

  EXPECT_TRUE(dom_string_map_->CanQueryNamedProperty(""));
  EXPECT_EQ(std::string("Los Angeles"),
            dom_string_map_->AnonymousNamedGetter("", &exception_state_));

  dom_string_map_->AnonymousNamedSetter("", "San Francisco", &exception_state_);
  EXPECT_EQ(std::string("San Francisco"), element_->GetAttribute("data-"));
}

TEST_F(DOMStringMapTest, OneWord) {
  element_->SetAttribute("data-city", "Los Angeles");

  StrictMock<script::testing::MockPropertyEnumerator> enumerator;
  EXPECT_CALL(enumerator, AddProperty("city"));
  dom_string_map_->EnumerateNamedProperties(&enumerator);

  EXPECT_TRUE(dom_string_map_->CanQueryNamedProperty("city"));
  EXPECT_EQ(std::string("Los Angeles"),
            dom_string_map_->AnonymousNamedGetter("city", &exception_state_));

  dom_string_map_->AnonymousNamedSetter("city", "San Francisco",
                                        &exception_state_);
  EXPECT_EQ(std::string("San Francisco"), element_->GetAttribute("data-city"));
}

TEST_F(DOMStringMapTest, MultipleWords) {
  element_->SetAttribute("data-city-on-western-coast", "Los Angeles");

  StrictMock<script::testing::MockPropertyEnumerator> enumerator;
  EXPECT_CALL(enumerator, AddProperty("cityOnWesternCoast"));
  dom_string_map_->EnumerateNamedProperties(&enumerator);

  EXPECT_TRUE(dom_string_map_->CanQueryNamedProperty("cityOnWesternCoast"));
  EXPECT_EQ(std::string("Los Angeles"),
            dom_string_map_->AnonymousNamedGetter("cityOnWesternCoast",
                                                  &exception_state_));

  dom_string_map_->AnonymousNamedSetter("cityOnWesternCoast", "San Francisco",
                                        &exception_state_);
  EXPECT_EQ(std::string("San Francisco"),
            element_->GetAttribute("data-city-on-western-coast"));
}

TEST_F(DOMStringMapTest, ContainsNonLetterAfterHyphen) {
  element_->SetAttribute("data-city-on-$western-coast", "Los Angeles");

  StrictMock<script::testing::MockPropertyEnumerator> enumerator;
  EXPECT_CALL(enumerator, AddProperty("cityOn-$westernCoast"));
  dom_string_map_->EnumerateNamedProperties(&enumerator);

  EXPECT_TRUE(dom_string_map_->CanQueryNamedProperty("cityOn-$westernCoast"));
  EXPECT_EQ(std::string("Los Angeles"),
            dom_string_map_->AnonymousNamedGetter("cityOn-$westernCoast",
                                                  &exception_state_));

  dom_string_map_->AnonymousNamedSetter("cityOn-$westernCoast", "San Francisco",
                                        &exception_state_);
  EXPECT_EQ(std::string("San Francisco"),
            element_->GetAttribute("data-city-on-$western-coast"));
}

TEST_F(DOMStringMapTest, EndsWithHyphen) {
  element_->SetAttribute("data-city-", "Los Angeles");

  StrictMock<script::testing::MockPropertyEnumerator> enumerator;
  EXPECT_CALL(enumerator, AddProperty("city-"));
  dom_string_map_->EnumerateNamedProperties(&enumerator);

  EXPECT_TRUE(dom_string_map_->CanQueryNamedProperty("city-"));
  EXPECT_EQ(std::string("Los Angeles"),
            dom_string_map_->AnonymousNamedGetter("city-", &exception_state_));

  dom_string_map_->AnonymousNamedSetter("city-", "San Francisco",
                                        &exception_state_);
  EXPECT_EQ(std::string("San Francisco"), element_->GetAttribute("data-city-"));
}

TEST_F(DOMStringMapTest, ContainsMultipleHyphens) {
  element_->SetAttribute("data-california---city", "Los Angeles");

  StrictMock<script::testing::MockPropertyEnumerator> enumerator;
  EXPECT_CALL(enumerator, AddProperty("california--City"));
  dom_string_map_->EnumerateNamedProperties(&enumerator);

  EXPECT_TRUE(dom_string_map_->CanQueryNamedProperty("california--City"));
  EXPECT_EQ(std::string("Los Angeles"),
            dom_string_map_->AnonymousNamedGetter("california--City",
                                                  &exception_state_));

  dom_string_map_->AnonymousNamedSetter("california--City", "San Francisco",
                                        &exception_state_);
  EXPECT_EQ(std::string("San Francisco"),
            element_->GetAttribute("data-california---city"));
}

TEST_F(DOMStringMapTest, InvalidPropertyName) {
  EXPECT_CALL(exception_state_,
              SetSimpleExceptionVA(script::kSyntaxError, _, _));
  dom_string_map_->AnonymousNamedSetter("hyphen-lowercase", "Los Angeles",
                                        &exception_state_);

  EXPECT_CALL(exception_state_,
              SetSimpleExceptionVA(script::kSyntaxError, _, _));
  dom_string_map_->AnonymousNamedGetter("hyphen-lowercase", &exception_state_);
}

}  // namespace dom
}  // namespace cobalt
