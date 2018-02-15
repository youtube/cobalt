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

#include "base/stringprintf.h"
#include "cobalt/bindings/testing/anonymous_indexed_getter_interface.h"
#include "cobalt/bindings/testing/anonymous_named_getter_interface.h"
#include "cobalt/bindings/testing/anonymous_named_indexed_getter_interface.h"
#include "cobalt/bindings/testing/arbitrary_interface.h"
#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/derived_getter_setter_interface.h"
#include "cobalt/bindings/testing/indexed_getter_interface.h"
#include "cobalt/bindings/testing/named_getter_interface.h"
#include "cobalt/bindings/testing/named_indexed_getter_interface.h"
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

// Use this fixture to create a new MockT object with a BaseClass wrapper, and
// bind the wrapper to the javascript variable "test".
template <class MockT>
class GetterSetterBindingsTestBase : public BindingsTestBase {
 public:
  GetterSetterBindingsTestBase()
      : test_mock_(new ::testing::NiceMock<MockT>()) {
    global_environment_->Bind("test", make_scoped_refptr<MockT>((test_mock_)));
  }

  MockT& test_mock() { return *test_mock_.get(); }

  const scoped_refptr<MockT> test_mock_;
};

typedef GetterSetterBindingsTestBase<ArbitraryInterface>
    GetterSetterBindingsTest;
typedef GetterSetterBindingsTestBase<AnonymousIndexedGetterInterface>
    AnonymousIndexedGetterBindingsTest;
typedef GetterSetterBindingsTestBase<AnonymousNamedIndexedGetterInterface>
    AnonymousNamedIndexedGetterBindingsTest;
typedef GetterSetterBindingsTestBase<AnonymousNamedGetterInterface>
    AnonymousNamedGetterBindingsTest;
typedef GetterSetterBindingsTestBase<DerivedGetterSetterInterface>
    DerivedGetterSetterBindingsTest;
typedef GetterSetterBindingsTestBase<IndexedGetterInterface>
    IndexedGetterBindingsTest;
typedef GetterSetterBindingsTestBase<NamedGetterInterface>
    NamedGetterBindingsTest;
typedef GetterSetterBindingsTestBase<NamedIndexedGetterInterface>
    NamedIndexedGetterBindingsTest;

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

TEST_F(GetterSetterBindingsTest, GetterCanHandleAllValueTypes) {
  const char* script = R"EOF(
      const getter = Object.getOwnPropertyDescriptor(
          ArbitraryInterface.prototype, "arbitraryProperty").get;
      [null, undefined, false, 0, "", {}, Symbol("")]
        .map(value => {
          try { getter.call(value); }
          catch (ex) { return ex.toString().startsWith("TypeError"); }
          return false;
        })
        .every(result => result);
  )EOF";
  std::string result;
  EXPECT_TRUE(EvaluateScript(script, &result));
  EXPECT_STREQ("true", result.c_str());
}

TEST_F(GetterSetterBindingsTest, SetterCanHandleAllValueTypes) {
  const char* script = R"EOF(
      const setter = Object.getOwnPropertyDescriptor(
          ArbitraryInterface.prototype, "arbitraryProperty").set;
      [null, undefined, false, 0, "", {}, Symbol("")]
        .map(value => {
          try { setter.call(value); }
          catch (ex) { return ex.toString().startsWith("TypeError"); }
          return false;
        })
        .every(result => result);
  )EOF";
  std::string result;
  EXPECT_TRUE(EvaluateScript(script, &result));
  EXPECT_STREQ("true", result.c_str());
}

TEST_F(IndexedGetterBindingsTest, IndexedGetter) {
  ON_CALL(test_mock(), length()).WillByDefault(Return(10));
  ON_CALL(test_mock(), IndexedGetter(_)).WillByDefault(ReturnArg<0>());
  InSequence dummy;

  std::string result;
  EXPECT_CALL(test_mock(), IndexedGetter(4)).Times(1);
  EXPECT_TRUE(EvaluateScript("test[4];", &result));
  EXPECT_STREQ("4", result.c_str());

  EXPECT_CALL(test_mock(), IndexedGetter(6)).Times(1);
  EXPECT_TRUE(EvaluateScript("test.indexedGetter(6);", &result));
  EXPECT_STREQ("6", result.c_str());
}

TEST_F(IndexedGetterBindingsTest, IndexIsOwnProperty) {
  ON_CALL(test_mock(), length()).WillByDefault(Return(10));
  InSequence dummy;

  std::string result;
  EXPECT_TRUE(EvaluateScript("test.hasOwnProperty(4);", &result));
  EXPECT_STREQ("true", result.c_str());
}

TEST_F(IndexedGetterBindingsTest, IndexedGetterOutOfRange) {
  ON_CALL(test_mock(), length()).WillByDefault(Return(10));
  ON_CALL(test_mock(), IndexedGetter(_)).WillByDefault(ReturnArg<0>());
  InSequence dummy;

  std::string result;
  EXPECT_CALL(test_mock(), IndexedGetter(_)).Times(0);
  EXPECT_TRUE(EvaluateScript("test[20];", &result));
  EXPECT_STREQ("undefined", result.c_str());

  EXPECT_CALL(test_mock(), IndexedGetter(20)).Times(1);
  EXPECT_TRUE(EvaluateScript("test.indexedGetter(20);", &result));
  EXPECT_STREQ("20", result.c_str());
}

TEST_F(IndexedGetterBindingsTest, IndexedSetter) {
  ON_CALL(test_mock(), length()).WillByDefault(Return(10));
  InSequence dummy;

  EXPECT_CALL(test_mock(), IndexedSetter(4, 100)).Times(1);
  EXPECT_TRUE(EvaluateScript("test[4] = 100;", NULL));

  EXPECT_CALL(test_mock(), IndexedSetter(4, 100)).Times(1);
  EXPECT_TRUE(EvaluateScript("test.indexedSetter(4, 100);", NULL));
}

#if defined(ENGINE_SUPPORTS_INDEXED_DELETERS)
TEST_F(IndexedGetterBindingsTest, IndexedDeleter) {
  ON_CALL(test_mock(), length()).WillByDefault(Return(10));
  EXPECT_CALL(test_mock(), IndexedDeleter(4)).Times(1);
  EXPECT_TRUE(EvaluateScript("delete test[4];", NULL));
}

TEST_F(IndexedGetterBindingsTest, IndexedDeleterOutOfRange) {
  ON_CALL(test_mock(), length()).WillByDefault(Return(1));
  EXPECT_CALL(test_mock(), IndexedDeleter(_)).Times(0);
  EXPECT_TRUE(EvaluateScript("delete test[4];", NULL));
}
#endif

TEST_F(IndexedGetterBindingsTest, IndexedSetterOutOfRange) {
  ON_CALL(test_mock(), length()).WillByDefault(Return(1));

  InSequence dummy;
  EXPECT_CALL(test_mock(), IndexedSetter(_, _)).Times(0);
  EXPECT_TRUE(EvaluateScript("test[4] = 100;", NULL));

  EXPECT_CALL(test_mock(), IndexedSetter(4, 100)).Times(1);
  EXPECT_TRUE(EvaluateScript("test.indexedSetter(4, 100);", NULL));
}

TEST_F(NamedGetterBindingsTest, NamedGetter) {
  ON_CALL(test_mock(), CanQueryNamedProperty(std::string("foo")))
      .WillByDefault(Return(true));
  ON_CALL(test_mock(), NamedGetter(std::string("foo")))
      .WillByDefault(Return(std::string("bar")));
  ON_CALL(test_mock(), NamedGetter(std::string("bar")))
      .WillByDefault(Return(std::string("foo")));
  InSequence dummy;

  std::string result;
  EXPECT_CALL(test_mock(), NamedGetter(std::string("foo"))).Times(1);
  EXPECT_TRUE(EvaluateScript("test[\"foo\"];", &result));
  EXPECT_STREQ("bar", result.c_str());

  EXPECT_CALL(test_mock(), NamedGetter(std::string("bar"))).Times(1);
  EXPECT_TRUE(EvaluateScript("test.namedGetter(\"bar\");", &result));
  EXPECT_STREQ("foo", result.c_str());
}

TEST_F(NamedGetterBindingsTest, NamedPropertyIsOwnProperty) {
  ON_CALL(test_mock(), CanQueryNamedProperty(std::string("foo")))
      .WillByDefault(Return(true));
  InSequence dummy;

  std::string result;
  EXPECT_TRUE(EvaluateScript("test.hasOwnProperty(\"foo\");", &result));
  EXPECT_STREQ("true", result.c_str());
}

TEST_F(NamedGetterBindingsTest, NamedGetterUnsupportedName) {
  ON_CALL(test_mock(), CanQueryNamedProperty(_)).WillByDefault(Return(false));
  InSequence dummy;

  std::string result;
  EXPECT_CALL(test_mock(), NamedGetter(_)).Times(0);
  EXPECT_TRUE(EvaluateScript("test[\"foo\"];", &result));
  EXPECT_STREQ("undefined", result.c_str());
}

TEST_F(NamedGetterBindingsTest, NamedSetter) {
  ON_CALL(test_mock(), CanQueryNamedProperty(std::string("foo")))
      .WillByDefault(Return(true));
  InSequence dummy;

  EXPECT_CALL(test_mock(), NamedSetter(std::string("foo"), std::string("bar")))
      .Times(1);
  EXPECT_TRUE(EvaluateScript("test[\"foo\"] = \"bar\";", NULL));

  EXPECT_CALL(test_mock(), NamedSetter(std::string("foo"), std::string("bar")))
      .Times(1);
  EXPECT_TRUE(EvaluateScript("test.namedSetter(\"foo\", \"bar\");", NULL));
}

TEST_F(NamedGetterBindingsTest, NamedDeleter) {
  ON_CALL(test_mock(), CanQueryNamedProperty(std::string("foo")))
      .WillByDefault(Return(true));
  InSequence dummy;

  EXPECT_CALL(test_mock(), NamedDeleter(std::string("foo"))).Times(1);
  EXPECT_TRUE(EvaluateScript("delete test.foo;", NULL));

  EXPECT_CALL(test_mock(), NamedDeleter(std::string("bar"))).Times(1);
  EXPECT_TRUE(EvaluateScript("test.namedDeleter(\"bar\");", NULL));
}

TEST_F(NamedGetterBindingsTest, NamedDeleterUnsupportedName) {
  ON_CALL(test_mock(), CanQueryNamedProperty(_)).WillByDefault(Return(false));
  InSequence dummy;

  EXPECT_CALL(test_mock(), NamedDeleter(_)).Times(0);
  EXPECT_TRUE(EvaluateScript("delete test.foo;", NULL));

  EXPECT_CALL(test_mock(), NamedDeleter(std::string("bar"))).Times(1);
  EXPECT_TRUE(EvaluateScript("test.namedDeleter(\"bar\");", NULL));
}

TEST_F(NamedGetterBindingsTest, NamedDeleterIndexProperty) {
  ON_CALL(test_mock(), CanQueryNamedProperty(std::string("1")))
      .WillByDefault(Return(true));
  InSequence dummy;

  EXPECT_CALL(test_mock(), NamedDeleter(std::string("1"))).Times(1);
  EXPECT_TRUE(EvaluateScript("delete test[1];", NULL));
}

TEST_F(NamedGetterBindingsTest, IndexConvertsToNamedProperty) {
  ON_CALL(test_mock(), CanQueryNamedProperty(std::string("5")))
      .WillByDefault(Return(true));
  InSequence dummy;

  EXPECT_CALL(test_mock(), NamedSetter(std::string("5"), std::string("bar")));
  EXPECT_TRUE(EvaluateScript("test[5] = \"bar\";", NULL));

  std::string result;
  EXPECT_CALL(test_mock(), CanQueryNamedProperty(std::string("5")))
      .WillOnce(Return(true));
  EXPECT_CALL(test_mock(), NamedGetter(std::string("5")))
      .WillOnce(Return(std::string("bar")));
  EXPECT_TRUE(EvaluateScript("test[5];", &result));
  EXPECT_STREQ("bar", result.c_str());
}

TEST_F(NamedIndexedGetterBindingsTest, NamedGetterDoesNotShadowBuiltIns) {
  InSequence dummy;

  std::string result;
  EXPECT_CALL(test_mock(), NamedGetter(_)).Times(0);
  EXPECT_TRUE(EvaluateScript("test[\"toString\"]();", &result));
  EXPECT_STREQ("[object NamedIndexedGetterInterface]", result.c_str());
}

TEST_F(AnonymousNamedIndexedGetterBindingsTest, EnumeratedPropertiesOrdering) {
  NamedPropertiesEnumerator enumerator(2);
  ON_CALL(test_mock(), length()).WillByDefault(Return(2));
  ON_CALL(test_mock(), EnumerateNamedProperties(_))
      .WillByDefault(Invoke(
          &enumerator, &NamedPropertiesEnumerator::EnumerateNamedProperties));
  ON_CALL(test_mock(), CanQueryNamedProperty(_)).WillByDefault(Return(true));

  std::string result;
  EXPECT_TRUE(
      EvaluateScript("var properties = [];"
                     "for (p in test) { properties.push(p); }"
                     "properties;",
                     &result));
  // Indexed properties should come first, then named properties, and then
  // other "regular" properties that are defined on the interface;
  EXPECT_TRUE(EvaluateScript("properties.length;", &result));
  EXPECT_STREQ("5", result.c_str());
  EXPECT_TRUE(EvaluateScript("properties[0];", &result));
  EXPECT_STREQ("0", result.c_str());
  EXPECT_TRUE(EvaluateScript("properties[1];", &result));
  EXPECT_STREQ("1", result.c_str());
  EXPECT_TRUE(EvaluateScript("properties[2];", &result));
  EXPECT_STREQ("property_a", result.c_str());
  EXPECT_TRUE(EvaluateScript("properties[3];", &result));
  EXPECT_STREQ("property_b", result.c_str());
  EXPECT_TRUE(EvaluateScript("properties[4];", &result));
  EXPECT_STREQ("length", result.c_str());
}

TEST_F(AnonymousIndexedGetterBindingsTest, EnumerateIndexedProperties) {
  ON_CALL(test_mock(), length()).WillByDefault(Return(4));
  std::string result;
  EXPECT_TRUE(
      EvaluateScript("var properties = [];"
                     "for (p in test) { properties.push(p); }"
                     "properties;",
                     &result));

  EXPECT_TRUE(EvaluateScript("properties.length;", &result));
  EXPECT_STREQ("5", result.c_str());
  EXPECT_TRUE(EvaluateScript("properties[0];", &result));
  EXPECT_STREQ("0", result.c_str());
  EXPECT_TRUE(EvaluateScript("properties[1];", &result));
  EXPECT_STREQ("1", result.c_str());
  EXPECT_TRUE(EvaluateScript("properties[2];", &result));
  EXPECT_STREQ("2", result.c_str());
  EXPECT_TRUE(EvaluateScript("properties[3];", &result));
  EXPECT_STREQ("3", result.c_str());
  EXPECT_TRUE(EvaluateScript("properties[4];", &result));
  EXPECT_STREQ("length", result.c_str());
}

TEST_F(AnonymousNamedGetterBindingsTest, EnumerateNamedProperties) {
  NamedPropertiesEnumerator enumerator(4);
  ON_CALL(test_mock(), EnumerateNamedProperties(_))
      .WillByDefault(Invoke(
          &enumerator, &NamedPropertiesEnumerator::EnumerateNamedProperties));
  ON_CALL(test_mock(), CanQueryNamedProperty(_)).WillByDefault(Return(true));

  std::string result;
  EXPECT_TRUE(
      EvaluateScript("var properties = [];"
                     "for (p in test) { properties.push(p); }"));

  EXPECT_TRUE(EvaluateScript("properties.length;", &result));
  EXPECT_STREQ("4", result.c_str());
  EXPECT_TRUE(EvaluateScript("properties[0];", &result));
  EXPECT_STREQ("property_a", result.c_str());
  EXPECT_TRUE(EvaluateScript("properties[1];", &result));
  EXPECT_STREQ("property_b", result.c_str());
  EXPECT_TRUE(EvaluateScript("properties[2];", &result));
  EXPECT_STREQ("property_c", result.c_str());
  EXPECT_TRUE(EvaluateScript("properties[3];", &result));
  EXPECT_STREQ("property_d", result.c_str());
}

TEST_F(AnonymousIndexedGetterBindingsTest, IndexedGetter) {
  ON_CALL(test_mock(), length()).WillByDefault(Return(10));
  ON_CALL(test_mock(), AnonymousIndexedGetter(_)).WillByDefault(ReturnArg<0>());
  InSequence dummy;

  std::string result;
  EXPECT_TRUE(EvaluateScript("test[4];", &result));
  EXPECT_STREQ("4", result.c_str());

  EXPECT_TRUE(EvaluateScript("test[10];", NULL));
}

TEST_F(AnonymousIndexedGetterBindingsTest, IndexedSetter) {
  ON_CALL(test_mock(), length()).WillByDefault(Return(10));

  InSequence dummy;

  EXPECT_CALL(test_mock(), AnonymousIndexedSetter(4, 100));
  EXPECT_TRUE(EvaluateScript("test[4] = 100;", NULL));

  EXPECT_CALL(test_mock(), AnonymousIndexedSetter(_, _)).Times(0);
  EXPECT_TRUE(EvaluateScript("test[10] = 100;", NULL));
}

TEST_F(AnonymousNamedGetterBindingsTest, NamedGetter) {
  InSequence dummy;

  std::string result;
  EXPECT_CALL(test_mock(), CanQueryNamedProperty(std::string("foo")))
      .WillOnce(Return(true));
  EXPECT_CALL(test_mock(), AnonymousNamedGetter(std::string("foo")))
      .WillOnce(Return(std::string("bar")));
  EXPECT_TRUE(EvaluateScript("test[\"foo\"];", &result));
  EXPECT_STREQ("bar", result.c_str());
}

TEST_F(AnonymousNamedGetterBindingsTest, NamedGetterUnsupportedName) {
  InSequence dummy;

  std::string result;
  EXPECT_CALL(test_mock(), CanQueryNamedProperty(std::string("foo")))
      .WillOnce(Return(false));
  EXPECT_TRUE(EvaluateScript("test[\"foo\"];", &result));
  EXPECT_STREQ("undefined", result.c_str());
}

TEST_F(AnonymousNamedGetterBindingsTest, NamedSetter) {
  ON_CALL(test_mock(), CanQueryNamedProperty(std::string("foo")))
      .WillByDefault(Return(true));
  InSequence dummy;

  EXPECT_CALL(test_mock(),
              AnonymousNamedSetter(std::string("foo"), std::string("bar")));
  EXPECT_TRUE(EvaluateScript("test[\"foo\"] = \"bar\";", NULL));
}

TEST_F(DerivedGetterSetterBindingsTest, OverridesGetterAndSetter) {
  ON_CALL(test_mock(), length()).WillByDefault(Return(10));
  InSequence dummy;

  std::string result;
  EXPECT_CALL(test_mock(), DerivedIndexedGetter(4)).WillOnce(Return(100));
  EXPECT_CALL(test_mock(), IndexedGetter(_)).Times(0);
  EXPECT_TRUE(EvaluateScript("test[4] == 100;", &result));
  EXPECT_STREQ("true", result.c_str());

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
  EXPECT_CALL(test_mock(), CanQueryNamedProperty(_))
      .Times(::testing::AtLeast(0));
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
