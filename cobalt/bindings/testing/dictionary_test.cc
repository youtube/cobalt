/*
 * Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/dictionary_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::SaveArg;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
typedef InterfaceBindingsTest<DictionaryInterface> DictionaryTest;
}  // namespace

TEST_F(DictionaryTest, NoValuesSet) {
  TestDictionary dictionary;
  EXPECT_CALL(test_mock(), DictionaryOperation(_))
      .WillOnce(SaveArg<0>(&dictionary));
  EXPECT_TRUE(EvaluateScript("test.dictionaryOperation({});", NULL));
  EXPECT_FALSE(dictionary.has_non_default_member());
  EXPECT_TRUE(dictionary.has_member_with_default());
  EXPECT_EQ(5, dictionary.member_with_default());

  EXPECT_CALL(test_mock(), DictionaryOperation(_))
      .WillOnce(SaveArg<0>(&dictionary));
  EXPECT_TRUE(EvaluateScript("test.dictionaryOperation(undefined);", NULL));
  EXPECT_FALSE(dictionary.has_non_default_member());
  EXPECT_TRUE(dictionary.has_member_with_default());
  EXPECT_EQ(5, dictionary.member_with_default());

  EXPECT_CALL(test_mock(), DictionaryOperation(_))
      .WillOnce(SaveArg<0>(&dictionary));
  EXPECT_TRUE(EvaluateScript("test.dictionaryOperation(null);", NULL));
  EXPECT_FALSE(dictionary.has_non_default_member());
  EXPECT_TRUE(dictionary.has_member_with_default());
  EXPECT_EQ(5, dictionary.member_with_default());
}

TEST_F(DictionaryTest, OverrideDefault) {
  TestDictionary dictionary;
  EXPECT_CALL(test_mock(), DictionaryOperation(_))
      .WillOnce(SaveArg<0>(&dictionary));
  EXPECT_TRUE(EvaluateScript(
      "test.dictionaryOperation( {memberWithDefault : 20} );", NULL));
  EXPECT_EQ(20, dictionary.member_with_default());
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt
