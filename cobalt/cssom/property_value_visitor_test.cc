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

#include "cobalt/cssom/property_value_visitor.h"

#include "cobalt/cssom/color_value.h"
#include "cobalt/cssom/inherited_value.h"
#include "cobalt/cssom/initial_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/string_value.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

class MockPropertyValueVisitor : public PropertyValueVisitor {
 public:
  MOCK_METHOD1(VisitColor, void(RGBAColorValue* color_value));
  MOCK_METHOD1(VisitInherited, void(InheritedValue* inherited_value));
  MOCK_METHOD1(VisitInitial, void(InitialValue* initial_value));
  MOCK_METHOD1(VisitLength, void(LengthValue* length_value));
  MOCK_METHOD1(VisitString, void(StringValue* string_value));
};

TEST(PropertyValueVisitorTest, VisitsColorValue) {
  RGBAColorValue color_value(0x0047abff);
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitColor(&color_value));
  color_value.Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsInheritedValue) {
  InheritedValue inherited_value;
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitInherited(&inherited_value));
  inherited_value.Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsInitialValue) {
  InitialValue initial_value;
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitInitial(&initial_value));
  initial_value.Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsLengthValue) {
  LengthValue length_value(10, kPixelsUnit);
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitLength(&length_value));
  length_value.Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsStringValue) {
  StringValue string_value("sans-serif");
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitString(&string_value));
  string_value.Accept(&mock_visitor);
}

}  // namespace cssom
}  // namespace cobalt
