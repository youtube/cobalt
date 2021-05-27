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

#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/dom_string_test_interface.h"

#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
typedef InterfaceBindingsTest<DOMStringTestInterface> DOMStringBindingsTest;
}  // namespace

TEST_F(DOMStringBindingsTest, GetProperty) {
  EXPECT_CALL(test_mock(), property()).WillOnce(Return("mock_value"));

  std::string result;
  EXPECT_TRUE(EvaluateScript("test.property;", &result));
  EXPECT_STREQ("mock_value", result.c_str());
}

TEST_F(DOMStringBindingsTest, SetProperty) {
  EXPECT_CALL(test_mock(), set_property(std::string("mock_value")));

  std::string result;
  EXPECT_TRUE(EvaluateScript("test.property = \"mock_value\";", &result));
  EXPECT_STREQ("mock_value", result.c_str());
}

TEST_F(DOMStringBindingsTest, SetPropertyWithJapanese) {
  EXPECT_CALL(test_mock(), set_property(std::string("かわいいです")));

  std::string result;
  EXPECT_TRUE(EvaluateScript("test.property = \"かわいいです\";", &result));
  EXPECT_STREQ("かわいいです", result.c_str());
}

TEST_F(DOMStringBindingsTest, SetPropertyWithChinese) {
  EXPECT_CALL(test_mock(), set_property(std::string("的")));

  std::string result;
  EXPECT_TRUE(EvaluateScript("test.property = \"\xe7\x9a\x84\";", &result));
  EXPECT_STREQ("的", result.c_str());
}

TEST_F(DOMStringBindingsTest, GetReadOnlyProperty) {
  EXPECT_CALL(test_mock(), read_only_property()).WillOnce(Return("mock_value"));

  std::string result;
  EXPECT_TRUE(EvaluateScript("test.readOnlyProperty;", &result));
  EXPECT_STREQ("mock_value", result.c_str());
}

TEST_F(DOMStringBindingsTest, GetReadOnlyPropertyWithKorean) {
  EXPECT_CALL(test_mock(), read_only_property()).WillOnce(Return("대박"));

  std::string result;
  EXPECT_TRUE(EvaluateScript("test.readOnlyProperty;", &result));
  EXPECT_STREQ("대박", result.c_str());
}

TEST_F(DOMStringBindingsTest, SetReadOnlyProperty) {
  // Script executes with no error, but interface is not called.
  EXPECT_TRUE(EvaluateScript("test.readOnlyProperty = \"foo\";", NULL));
}

TEST_F(DOMStringBindingsTest, GetReadOnlyTokenProperty) {
  EXPECT_CALL(test_mock(), read_only_token_property())
      .WillOnce(Return(base::Token("mock_value")));

  std::string result;
  EXPECT_TRUE(EvaluateScript("test.readOnlyTokenProperty;", &result));
  EXPECT_STREQ("mock_value", result.c_str());
}

TEST_F(DOMStringBindingsTest, SetNull) {
  EXPECT_CALL(test_mock(), set_property("null"));

  std::string result;
  EXPECT_TRUE(EvaluateScript("test.property = null;", &result));
  EXPECT_STREQ("null", result.c_str());
}

TEST_F(DOMStringBindingsTest, SetUndefined) {
  EXPECT_CALL(test_mock(), set_property("undefined"));

  std::string result;
  EXPECT_TRUE(EvaluateScript("test.property = undefined;", &result));
  EXPECT_STREQ("undefined", result.c_str());
}

TEST_F(DOMStringBindingsTest, SetBoolean) {
  EXPECT_CALL(test_mock(), set_property("true"));

  std::string result;
  EXPECT_TRUE(EvaluateScript("test.property = true;", &result));
  EXPECT_STREQ("true", result.c_str());
}

TEST_F(DOMStringBindingsTest, SetNaN) {
  EXPECT_CALL(test_mock(), set_property("NaN"));

  EXPECT_TRUE(EvaluateScript("test.property = NaN;"));
}

TEST_F(DOMStringBindingsTest, SetPositiveZero) {
  EXPECT_CALL(test_mock(), set_property("0"));

  EXPECT_TRUE(EvaluateScript("test.property = +0;"));
}

TEST_F(DOMStringBindingsTest, SetNegativeZero) {
  EXPECT_CALL(test_mock(), set_property("0"));

  EXPECT_TRUE(EvaluateScript("test.property = -0;"));
}

TEST_F(DOMStringBindingsTest, SetNumber) {
  EXPECT_CALL(test_mock(), set_property("50"));

  std::string result;
  EXPECT_TRUE(EvaluateScript("test.property = 50;", &result));
  EXPECT_STREQ("50", result.c_str());
}

TEST_F(DOMStringBindingsTest, SetInfinity) {
  EXPECT_CALL(test_mock(), set_property("Infinity"));

  EXPECT_TRUE(EvaluateScript("test.property = Infinity;"));
}

TEST_F(DOMStringBindingsTest, SetNegativeInfinity) {
  EXPECT_CALL(test_mock(), set_property("-Infinity"));

  EXPECT_TRUE(EvaluateScript("test.property = -Infinity;"));
}

TEST_F(DOMStringBindingsTest, SetNullAsEmptyString) {
  EXPECT_CALL(test_mock(), set_null_is_empty_property(""));
  EXPECT_TRUE(EvaluateScript("test.nullIsEmptyProperty = null;", NULL));
}

TEST_F(DOMStringBindingsTest, SetUndefinedAsEmptyString) {
  EXPECT_CALL(test_mock(), set_undefined_is_empty_property(""));
  EXPECT_TRUE(
      EvaluateScript("test.undefinedIsEmptyProperty = undefined;", NULL));

  EXPECT_CALL(test_mock(), set_nullable_undefined_is_empty_property(
                               base::Optional<std::string>("")));
  EXPECT_TRUE(EvaluateScript(
      "test.nullableUndefinedIsEmptyProperty = undefined;", NULL));
}

TEST_F(DOMStringBindingsTest, StringContainingNull) {
  EXPECT_CALL(test_mock(), set_property(std::string("mock\0_value", 11)));

  std::string result;
  std::string input("test.property = \"mock\0_value\";", 30);
  EXPECT_TRUE(EvaluateScript(input, &result));
  EXPECT_STREQ("mock\0_value", result.c_str());
}

TEST_F(DOMStringBindingsTest, ForeignStringContainingNull) {
  EXPECT_CALL(test_mock(), set_property(std::string("かわいい\0です", 19)));

  std::string result;
  std::string input("test.property = \"かわいい\0です\";", 38);
  EXPECT_TRUE(EvaluateScript(input, &result));
  EXPECT_STREQ("かわいい\0です", result.c_str());
}

// Invalid UTF-16 surrogates with UTF-8 encoding
// \uFFDD = Replacement char (utf-8) = \xEF\xBF\xBD

// Sanity check
TEST_F(DOMStringBindingsTest, ReplaceUT8SanityCheck) {
  EXPECT_CALL(test_mock(), set_property(std::string("abc123")));
  std::string result;
  std::string input("test.property = \"abc123\";");
  EXPECT_TRUE(EvaluateScript(input, &result));
  EXPECT_STREQ("abc123", result.c_str());
}

// Surrogate half (low)
TEST_F(DOMStringBindingsTest, ReplaceUT8SurrogateLow) {
  EXPECT_CALL(test_mock(), set_property(std::string("\xEF\xBF\xBD")));
  std::string result;
  std::string input("test.property = \"\\uD800\";");
  EXPECT_TRUE(EvaluateScript(input, &result));
  EXPECT_STREQ("\xEF\xBF\xBD", result.c_str());
}

// Surrogate half (high)
TEST_F(DOMStringBindingsTest, ReplaceUT8SurrogateHigh) {
  EXPECT_CALL(test_mock(), set_property(std::string("\xEF\xBF\xBD")));
  std::string result;
  std::string input("test.property = \"\\uDC00\";");
  EXPECT_TRUE(EvaluateScript(input, &result));
  EXPECT_STREQ("\xEF\xBF\xBD", result.c_str());
}

// Surrogate half (low), in a string
TEST_F(DOMStringBindingsTest, ReplaceUT8SurrogateLowInString) {
  EXPECT_CALL(test_mock(), set_property(std::string("abc\xEF\xBF\xBD"
                                                    "123")));
  std::string result;
  std::string input("test.property = \"abc\\uD800123\";");
  EXPECT_TRUE(EvaluateScript(input, &result));
  EXPECT_STREQ(
      "abc\xEF\xBF\xBD"
      "123",
      result.c_str());
}

// Surrogate half (high), in a string
TEST_F(DOMStringBindingsTest, ReplaceUT8SurrogateHighInString) {
  EXPECT_CALL(test_mock(), set_property(std::string("abc\xEF\xBF\xBD"
                                                    "123")));
  std::string result;
  std::string input("test.property = \"abc\\uDC00123\";");
  EXPECT_TRUE(EvaluateScript(input, &result));
  EXPECT_STREQ(
      "abc\xEF\xBF\xBD"
      "123",
      result.c_str());
}

// Surrogates in wrong order
TEST_F(DOMStringBindingsTest, ReplaceUT8WrongOrder) {
  EXPECT_CALL(test_mock(),
              set_property(std::string("\xEF\xBF\xBD\xEF\xBF\xBD")));
  std::string result;
  std::string input("test.property = \"\\uDC00\\uD800\";");
  EXPECT_TRUE(EvaluateScript(input, &result));
  EXPECT_STREQ("\xEF\xBF\xBD\xEF\xBF\xBD", result.c_str());
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt
