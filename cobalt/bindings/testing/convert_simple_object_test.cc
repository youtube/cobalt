// Copyright 2018 Google Inc. All Rights Reserved.
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
#include "cobalt/bindings/testing/convert_simple_object_interface.h"

using ::testing::ContainsRegex;

namespace cobalt {
namespace bindings {
namespace testing {

class ConvertSimpleObjectTest
    : public InterfaceBindingsTest<ConvertSimpleObjectInterface> {};

TEST_F(ConvertSimpleObjectTest, ValidObjectAllStrings) {
  std::unordered_map<std::string, std::string> expected_result = {
      {"countryCode", "US"}, {"language", "english"}};

  const char script[] = R"(
      const productSpecificData = {countryCode: 'US', language: 'english'};
      test.convertSimpleObjectToMapTest(productSpecificData);
      )";

  EXPECT_TRUE(EvaluateScript(script));
  EXPECT_EQ(test_mock().result(), expected_result);
}

TEST_F(ConvertSimpleObjectTest, ValidObjectWithNumber) {
  std::unordered_map<std::string, std::string> expected_result = {
      {"countryCode", "5"}, {"language", "english"}};

  const char script[] = R"(
      const productSpecificData = {countryCode: 5, language: 'english'};
      test.convertSimpleObjectToMapTest(productSpecificData);
      )";

  EXPECT_TRUE(EvaluateScript(script));
  EXPECT_EQ(test_mock().result(), expected_result);
}

TEST_F(ConvertSimpleObjectTest, ValidObjectWithBool) {
  std::unordered_map<std::string, std::string> expected_result = {
      {"countryCode", "US"}, {"language", "true"}};

  const char script[] = R"(
      const productSpecificData = {countryCode: 'US', language: true};
      test.convertSimpleObjectToMapTest(productSpecificData);
      )";

  EXPECT_TRUE(EvaluateScript(script));
  EXPECT_EQ(test_mock().result(), expected_result);
}

TEST_F(ConvertSimpleObjectTest, InvalidValue) {
  const char script[] = R"(
      const productSpecificData = 'data';
      let error = null;
      try {
        test.convertSimpleObjectToMapTest(productSpecificData);
      } catch(ex) {
        error = ex;
      }
      )";

  EXPECT_TRUE(EvaluateScript(script));
  std::string result;
  EXPECT_TRUE(EvaluateScript("error instanceof Error;", &result));
  EXPECT_STREQ("true", result.c_str());
  EXPECT_TRUE(EvaluateScript("error.toString();", &result));
  EXPECT_STREQ("TypeError: The value must be an object", result.c_str());
}

TEST_F(ConvertSimpleObjectTest, InvalidObjectValues) {
  const std::string invalid_values[] = {"null", "undefined", "{}",
                                        "function(){}", "Symbol('US')"};

  std::string result;
  for (const auto& value : invalid_values) {
    const std::string script = R"(
      var productSpecificData = {countryCode: )" +
                               value + R"(, language: 'english'};
      var error = null;
      try {
        test.convertSimpleObjectToMapTest(productSpecificData);
      } catch(ex) {
        error = ex;
      }
      )";

    EXPECT_TRUE(EvaluateScript(script));
    EXPECT_TRUE(EvaluateScript("error instanceof Error;", &result));
    EXPECT_STREQ("true", result.c_str());
    EXPECT_TRUE(EvaluateScript("error.toString();", &result));
    EXPECT_STREQ(
        "TypeError: Object property values must be a number, string or boolean",
        result.c_str());
  }
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt
