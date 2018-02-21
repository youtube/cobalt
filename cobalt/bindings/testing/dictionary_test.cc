// Copyright 2017 Google Inc. All Rights Reserved.
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

#include <limits>

#include "base/stringprintf.h"
#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/derived_dictionary.h"
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
  ASSERT_TRUE(dictionary.has_member_with_default());
  EXPECT_EQ(5, dictionary.member_with_default());

  EXPECT_CALL(test_mock(), DictionaryOperation(_))
      .WillOnce(SaveArg<0>(&dictionary));
  EXPECT_TRUE(EvaluateScript("test.dictionaryOperation(undefined);", NULL));
  EXPECT_FALSE(dictionary.has_non_default_member());
  ASSERT_TRUE(dictionary.has_member_with_default());
  EXPECT_EQ(5, dictionary.member_with_default());

  EXPECT_CALL(test_mock(), DictionaryOperation(_))
      .WillOnce(SaveArg<0>(&dictionary));
  EXPECT_TRUE(EvaluateScript("test.dictionaryOperation(null);", NULL));
  EXPECT_FALSE(dictionary.has_non_default_member());
  ASSERT_TRUE(dictionary.has_member_with_default());
  EXPECT_EQ(5, dictionary.member_with_default());
}

TEST_F(DictionaryTest, BooleanMember) {
  TestDictionary dictionary;
  EXPECT_FALSE(dictionary.has_boolean_member());
  EXPECT_CALL(test_mock(), DictionaryOperation(_))
      .WillOnce(SaveArg<0>(&dictionary));
  EXPECT_TRUE(EvaluateScript(
      "test.dictionaryOperation( {booleanMember : true} );", NULL));
  EXPECT_TRUE(dictionary.has_boolean_member());
  EXPECT_TRUE(dictionary.boolean_member());

  EXPECT_CALL(test_mock(), DictionaryOperation(_))
      .WillOnce(SaveArg<0>(&dictionary));
  EXPECT_TRUE(EvaluateScript(
      "test.dictionaryOperation( {booleanMember : false} );", NULL));
  ASSERT_TRUE(dictionary.has_boolean_member());
  EXPECT_FALSE(dictionary.boolean_member());
}

TEST_F(DictionaryTest, ClampMember) {
  TestDictionary dictionary;
  EXPECT_FALSE(dictionary.has_short_clamp_member());
  EXPECT_CALL(test_mock(), DictionaryOperation(_))
      .WillOnce(SaveArg<0>(&dictionary));

  EXPECT_TRUE(EvaluateScript(
      StringPrintf("test.dictionaryOperation( {shortClampMember : %d } );",
                   std::numeric_limits<int32_t>::max()),
      NULL));
  ASSERT_TRUE(dictionary.has_short_clamp_member());
  EXPECT_EQ(std::numeric_limits<int16_t>::max(),
            dictionary.short_clamp_member());
}

TEST_F(DictionaryTest, LongMember) {
  TestDictionary dictionary;
  EXPECT_FALSE(dictionary.has_long_member());
  EXPECT_CALL(test_mock(), DictionaryOperation(_))
      .WillOnce(SaveArg<0>(&dictionary));
  EXPECT_TRUE(EvaluateScript(
      "test.dictionaryOperation( {longMember : 123456} );", NULL));
  ASSERT_TRUE(dictionary.has_long_member());
  EXPECT_EQ(123456, dictionary.long_member());
}

TEST_F(DictionaryTest, DoubleMember) {
  TestDictionary dictionary;
  EXPECT_FALSE(dictionary.has_double_member());
  EXPECT_CALL(test_mock(), DictionaryOperation(_))
      .WillOnce(SaveArg<0>(&dictionary));
  EXPECT_TRUE(EvaluateScript(
      "test.dictionaryOperation( {doubleMember : 123.456} );", NULL));
  ASSERT_TRUE(dictionary.has_double_member());
  EXPECT_EQ(123.456, dictionary.double_member());
}

TEST_F(DictionaryTest, StringMember) {
  TestDictionary dictionary;
  EXPECT_FALSE(dictionary.has_string_member());
  EXPECT_CALL(test_mock(), DictionaryOperation(_))
      .WillOnce(SaveArg<0>(&dictionary));
  EXPECT_TRUE(EvaluateScript(
      "test.dictionaryOperation( {stringMember : 'Cobalt'} );", NULL));
  ASSERT_TRUE(dictionary.has_string_member());
  EXPECT_EQ("Cobalt", dictionary.string_member());
}

TEST_F(DictionaryTest, ArbitraryInterfaceMember) {
  TestDictionary dictionary;
  EXPECT_FALSE(dictionary.has_interface_member());
  EXPECT_CALL(test_mock(), DictionaryOperation(_))
      .WillOnce(SaveArg<0>(&dictionary));
  EXPECT_TRUE(
      EvaluateScript("test.dictionaryOperation( {interfaceMember : new "
                     "ArbitraryInterface()} );",
                     NULL));
  ASSERT_TRUE(dictionary.has_interface_member());
  EXPECT_TRUE(dictionary.interface_member() != NULL);
}

TEST_F(DictionaryTest, OverrideDefault) {
  TestDictionary dictionary;
  EXPECT_CALL(test_mock(), DictionaryOperation(_))
      .WillOnce(SaveArg<0>(&dictionary));
  EXPECT_TRUE(EvaluateScript(
      "test.dictionaryOperation( {memberWithDefault : 20} );", NULL));
  EXPECT_EQ(20, dictionary.member_with_default());
}

TEST_F(DictionaryTest, AnyMember) {
  TestDictionary dictionary;
  EXPECT_FALSE(dictionary.has_any_member());
  EXPECT_CALL(test_mock(), DictionaryOperation(_))
      .WillOnce(SaveArg<0>(&dictionary));
  EXPECT_TRUE(EvaluateScript(
      "test.dictionaryOperation( {anyMember : new ArbitraryInterface()} );",
      NULL));
  ASSERT_TRUE(dictionary.has_any_member());
  EXPECT_TRUE(dictionary.any_member() != NULL);

  EXPECT_CALL(test_mock(), DictionaryOperation(_))
      .WillOnce(SaveArg<0>(&dictionary));
  EXPECT_TRUE(EvaluateScript(
      "test.dictionaryOperation( {anyMember : {'foo':'bar'}} );", NULL));
  ASSERT_TRUE(dictionary.has_any_member());
  EXPECT_TRUE(dictionary.any_member() != NULL);
}

TEST_F(DictionaryTest, OverrideAnyMemberWithDefault) {
  TestDictionary dictionary;
  EXPECT_TRUE(dictionary.has_any_member_with_default());
  EXPECT_TRUE(dictionary.any_member_with_default() == NULL);
  EXPECT_CALL(test_mock(), DictionaryOperation(_))
      .WillOnce(SaveArg<0>(&dictionary));
  EXPECT_TRUE(
      EvaluateScript("test.dictionaryOperation( {anyMemberWithDefault : new "
                     "ArbitraryInterface()} );",
                     NULL));
  ASSERT_TRUE(dictionary.has_any_member_with_default());
  EXPECT_TRUE(dictionary.any_member_with_default() != NULL);
}

TEST_F(DictionaryTest, DerivedDictionaryMember) {
  DerivedDictionary dictionary;
  EXPECT_CALL(test_mock(), DerivedDictionaryOperation(_))
      .WillOnce(SaveArg<0>(&dictionary));
  EXPECT_TRUE(EvaluateScript(
      "test.derivedDictionaryOperation( {additionalMember : true} );", NULL));
  EXPECT_TRUE(dictionary.additional_member());
}

TEST_F(DictionaryTest, BaseDictionaryMemberOfDerivedDictionary) {
  DerivedDictionary dictionary;
  EXPECT_CALL(test_mock(), DerivedDictionaryOperation(_))
      .WillOnce(SaveArg<0>(&dictionary));
  EXPECT_TRUE(EvaluateScript(
      "test.derivedDictionaryOperation( {memberWithDefault : 25} );", NULL));
  EXPECT_EQ(25, dictionary.member_with_default());
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt
