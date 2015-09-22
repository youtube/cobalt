/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "base/stringprintf.h"
#include "cobalt/bindings/testing/anonymous_getter_setter_interface.h"
#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/derived_getter_setter_interface.h"
#include "cobalt/bindings/testing/getter_setter_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Invoke;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::ReturnArg;
using ::testing::_;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
typedef InterfaceBindingsTest<GetterSetterInterface> GetterSetterBindingsTest;
typedef InterfaceBindingsTest<AnonymousGetterSetterInterface>
    AnonymousGetterSetterBindingsTest;
typedef InterfaceBindingsTest<DerivedGetterSetterInterface>
    DerivedGetterSetterBindingsTest;

class NamedPropertiesEnumerator {
 public:
  explicit NamedPropertiesEnumerator(int num_properties)
      : num_properties_(num_properties) {}
  void EnumerateNamedProperties(script::PropertyEnumerator* enumerator) {
    for (int i = 0; i < num_properties_; ++i) {
      char letter = 'a' + i;
      enumerator->AddProperty(StringPrintf("property_%c", letter));
    }
  }

 private:
  int num_properties_;
};
}  // namespace

TEST_F(GetterSetterBindingsTest, IndexedGetter) {
  InSequence dummy;

  std::string result;
  EXPECT_CALL(test_mock(), length()).WillOnce(Return(10));
  EXPECT_CALL(test_mock(), IndexedGetter(_)).WillOnce(ReturnArg<0>());
  EXPECT_TRUE(EvaluateScript("test[4];", &result));
  EXPECT_STREQ("4", result.c_str());

  EXPECT_CALL(test_mock(), IndexedGetter(_)).WillOnce(ReturnArg<0>());
  EXPECT_TRUE(EvaluateScript("test.indexedGetter(6);", &result));
  EXPECT_STREQ("6", result.c_str());
}

TEST_F(GetterSetterBindingsTest, IndexedGetterOutOfRange) {
  InSequence dummy;

  std::string result;
  EXPECT_CALL(test_mock(), length()).WillOnce(Return(10));
  EXPECT_TRUE(EvaluateScript("test[20];", &result));
  EXPECT_STREQ("undefined", result.c_str());

  EXPECT_CALL(test_mock(), IndexedGetter(_)).WillOnce(ReturnArg<0>());
  EXPECT_TRUE(EvaluateScript("test.indexedGetter(20);", &result));
  EXPECT_STREQ("20", result.c_str());
}

TEST_F(GetterSetterBindingsTest, IndexedSetter) {
  InSequence dummy;

  EXPECT_CALL(test_mock(), length()).WillOnce(Return(10));
  EXPECT_CALL(test_mock(), IndexedSetter(4, 100));
  EXPECT_TRUE(EvaluateScript("test[4] = 100;", NULL));

  EXPECT_CALL(test_mock(), IndexedSetter(4, 100));
  EXPECT_TRUE(EvaluateScript("test.indexedSetter(4, 100);", NULL));
}

TEST_F(GetterSetterBindingsTest, IndexedSetterOutOfRange) {
  InSequence dummy;

  EXPECT_CALL(test_mock(), length()).WillOnce(Return(1));
  EXPECT_CALL(test_mock(), IndexedSetter(_, _)).Times(0);
  EXPECT_TRUE(EvaluateScript("test[4] = 100;", NULL));

  EXPECT_CALL(test_mock(), IndexedSetter(4, 100));
  EXPECT_TRUE(EvaluateScript("test.indexedSetter(4, 100);", NULL));
}

TEST_F(GetterSetterBindingsTest, NamedGetter) {
  InSequence dummy;

  std::string result;
  EXPECT_CALL(test_mock(), CanQueryNamedProperty(std::string("foo")))
      .WillOnce(Return(true));
  EXPECT_CALL(test_mock(), NamedGetter(std::string("foo")))
      .WillOnce(Return(std::string("bar")));
  EXPECT_TRUE(EvaluateScript("test[\"foo\"];", &result));
  EXPECT_STREQ("bar", result.c_str());

  EXPECT_CALL(test_mock(), NamedGetter(std::string("bar")))
      .WillOnce(Return(std::string("foo")));
  EXPECT_TRUE(EvaluateScript("test.namedGetter(\"bar\");", &result));
  EXPECT_STREQ("foo", result.c_str());
}

TEST_F(GetterSetterBindingsTest, NamedGetterUnsupportedName) {
  InSequence dummy;

  std::string result;
  EXPECT_CALL(test_mock(), CanQueryNamedProperty(std::string("foo")))
      .WillOnce(Return(false));
  EXPECT_TRUE(EvaluateScript("test[\"foo\"];", &result));
  EXPECT_STREQ("undefined", result.c_str());
}

TEST_F(GetterSetterBindingsTest, NamedGetterDoesNotShadowBuiltIns) {
  InSequence dummy;

  std::string result;
  EXPECT_TRUE(EvaluateScript("test[\"toString\"]();", &result));
  EXPECT_STREQ("[object GetterSetterInterface]", result.c_str());
}

TEST_F(GetterSetterBindingsTest, NamedSetter) {
  InSequence dummy;

  EXPECT_CALL(test_mock(), NamedSetter(std::string("foo"), std::string("bar")));
  EXPECT_TRUE(EvaluateScript("test[\"foo\"] = \"bar\";", NULL));

  EXPECT_CALL(test_mock(), NamedSetter(std::string("foo"), std::string("bar")));
  EXPECT_TRUE(EvaluateScript("test.namedSetter(\"foo\", \"bar\");", NULL));
}

TEST_F(AnonymousGetterSetterBindingsTest, EnumerateIndexedProperties) {
  std::string result;
  EXPECT_CALL(test_mock(), length()).Times(5).WillRepeatedly(Return(4));
  EXPECT_CALL(test_mock(), EnumerateNamedProperties(_));
  EXPECT_TRUE(
      EvaluateScript("var properties = [];"
                     "for (p in test) { properties.push(p); }"
                     "properties;",
                     &result));

  EXPECT_STREQ("0,1,2,3,length", result.c_str());
}

TEST_F(AnonymousGetterSetterBindingsTest, EnumerateNamedProperties) {
  NamedPropertiesEnumerator enumerator(4);

  std::string result;
  EXPECT_CALL(test_mock(), length()).WillOnce(Return(0));
  EXPECT_CALL(test_mock(), EnumerateNamedProperties(_))
      .WillOnce(Invoke(&enumerator,
                       &NamedPropertiesEnumerator::EnumerateNamedProperties));
  EXPECT_TRUE(
      EvaluateScript("var properties = [];"
                     "for (p in test) { properties.push(p); }"
                     "properties;",
                     &result));

  EXPECT_STREQ("property_a,property_b,property_c,property_d,length",
               result.c_str());
}

TEST_F(AnonymousGetterSetterBindingsTest, EnumeratedPropertiesOrdering) {
  NamedPropertiesEnumerator enumerator(2);

  std::string result;
  EXPECT_CALL(test_mock(), length()).Times(3).WillRepeatedly(Return(2));
  EXPECT_CALL(test_mock(), EnumerateNamedProperties(_))
      .WillOnce(Invoke(&enumerator,
                       &NamedPropertiesEnumerator::EnumerateNamedProperties));
  EXPECT_TRUE(
      EvaluateScript("var properties = [];"
                     "for (p in test) { properties.push(p); }"
                     "properties;",
                     &result));

  // Indexed properties should come first, then named properties, and then
  // other "regular" properties that are defined on the interface;
  EXPECT_STREQ("0,1,property_a,property_b,length", result.c_str());
}

TEST_F(AnonymousGetterSetterBindingsTest, IndexedGetter) {
  InSequence dummy;

  std::string result;
  EXPECT_CALL(test_mock(), length()).WillOnce(Return(10));
  EXPECT_CALL(test_mock(), AnonymousIndexedGetter(_)).WillOnce(ReturnArg<0>());
  EXPECT_TRUE(EvaluateScript("test[4];", &result));
  EXPECT_STREQ("4", result.c_str());

  EXPECT_CALL(test_mock(), length()).WillOnce(Return(10));
  EXPECT_CALL(test_mock(), AnonymousIndexedGetter(_)).Times(0);
  EXPECT_TRUE(EvaluateScript("test[10];", NULL));
}

TEST_F(AnonymousGetterSetterBindingsTest, IndexedSetter) {
  InSequence dummy;

  EXPECT_CALL(test_mock(), length()).WillOnce(Return(10));
  EXPECT_CALL(test_mock(), AnonymousIndexedSetter(4, 100));
  EXPECT_TRUE(EvaluateScript("test[4] = 100;", NULL));

  EXPECT_CALL(test_mock(), length()).WillOnce(Return(10));
  EXPECT_CALL(test_mock(), AnonymousIndexedSetter(_, _)).Times(0);
  EXPECT_TRUE(EvaluateScript("test[10] = 100;", NULL));
}

TEST_F(AnonymousGetterSetterBindingsTest, NamedGetter) {
  InSequence dummy;

  std::string result;
  EXPECT_CALL(test_mock(), CanQueryNamedProperty(std::string("foo")))
      .WillOnce(Return(true));
  EXPECT_CALL(test_mock(), AnonymousNamedGetter(std::string("foo")))
      .WillOnce(Return(std::string("bar")));
  EXPECT_TRUE(EvaluateScript("test[\"foo\"];", &result));
  EXPECT_STREQ("bar", result.c_str());
}

TEST_F(AnonymousGetterSetterBindingsTest, NamedGetterUnsupportedName) {
  InSequence dummy;

  std::string result;
  EXPECT_CALL(test_mock(), CanQueryNamedProperty(std::string("foo")))
      .WillOnce(Return(false));
  EXPECT_TRUE(EvaluateScript("test[\"foo\"];", &result));
  EXPECT_STREQ("undefined", result.c_str());
}

TEST_F(AnonymousGetterSetterBindingsTest, NamedSetter) {
  InSequence dummy;

  EXPECT_CALL(test_mock(),
              AnonymousNamedSetter(std::string("foo"), std::string("bar")));
  EXPECT_TRUE(EvaluateScript("test[\"foo\"] = \"bar\";", NULL));
}

TEST_F(DerivedGetterSetterBindingsTest, OverridesGetterAndSetter) {
  InSequence dummy;

  std::string result;
  EXPECT_CALL(test_mock(), length()).WillOnce(Return(10));
  EXPECT_CALL(test_mock(), DerivedIndexedGetter(4)).WillOnce(Return(100));
  EXPECT_CALL(test_mock(), IndexedGetter(_)).Times(0);
  EXPECT_TRUE(EvaluateScript("test[4] == 100;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), length()).WillOnce(Return(10));
  EXPECT_CALL(test_mock(), DerivedIndexedSetter(4, 100));
  EXPECT_CALL(test_mock(), IndexedSetter(_, _)).Times(0);
  EXPECT_TRUE(EvaluateScript("test[4] = 100;", NULL));
}

TEST_F(DerivedGetterSetterBindingsTest, NamedGetterDoesNotShadowProperties) {
  InSequence dummy;

  std::string result;
  EXPECT_CALL(test_mock(), property_on_derived_class()).WillOnce(Return(true));
  EXPECT_TRUE(EvaluateScript("test[\"propertyOnDerivedClass\"];", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), OperationOnDerivedClass());
  EXPECT_TRUE(EvaluateScript("test[\"operationOnDerivedClass\"]();", &result));

  EXPECT_CALL(test_mock(), property_on_base_class()).WillOnce(Return(true));
  EXPECT_TRUE(EvaluateScript("test[\"propertyOnBaseClass\"];", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), OperationOnBaseClass());
  EXPECT_TRUE(EvaluateScript("test[\"operationOnBaseClass\"]();", &result));
}

TEST_F(DerivedGetterSetterBindingsTest, NamedSetterDoesNotShadowProperties) {
  InSequence dummy;

  std::string result;
  EXPECT_CALL(test_mock(), set_property_on_derived_class(true));
  EXPECT_TRUE(
      EvaluateScript("test[\"propertyOnDerivedClass\"] = true;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), set_property_on_base_class(true));
  EXPECT_TRUE(EvaluateScript("test[\"propertyOnBaseClass\"] = true;", &result));
  EXPECT_STREQ("true", result.c_str());
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt
