// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_split.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

namespace base {

class SplitStringIntoKeyValuesTest : public testing::Test {
 protected:
  std::string key;
  std::vector<std::string> values;
};

TEST_F(SplitStringIntoKeyValuesTest, EmptyInputMultipleValues) {
  EXPECT_FALSE(SplitStringIntoKeyValues("",     // Empty input
                                        '\t',   // Key separators
                                        &key, &values));
  EXPECT_TRUE(key.empty());
  EXPECT_TRUE(values.empty());
}

TEST_F(SplitStringIntoKeyValuesTest, EmptyValueInputMultipleValues) {
  EXPECT_FALSE(SplitStringIntoKeyValues("key_with_no_value\t",
                                        '\t',  // Key separators
                                        &key, &values));
  EXPECT_EQ("key_with_no_value", key);
  EXPECT_TRUE(values.empty());
}

TEST_F(SplitStringIntoKeyValuesTest, EmptyKeyInputMultipleValues) {
  EXPECT_TRUE(SplitStringIntoKeyValues("\tvalue for empty key",
                                       '\t',  // Key separators
                                       &key, &values));
  EXPECT_TRUE(key.empty());
  ASSERT_EQ(1U, values.size());
}

TEST_F(SplitStringIntoKeyValuesTest, KeyWithMultipleValues) {
  EXPECT_TRUE(SplitStringIntoKeyValues("key1\tvalue1,   value2   value3",
                                       '\t',  // Key separators
                                       &key, &values));
  EXPECT_EQ("key1", key);
  ASSERT_EQ(1U, values.size());
  EXPECT_EQ("value1,   value2   value3", values[0]);
}

TEST_F(SplitStringIntoKeyValuesTest, EmptyInputSingleValue) {
  EXPECT_FALSE(SplitStringIntoKeyValues("",     // Empty input
                                        '\t',   // Key separators
                                        &key, &values));
  EXPECT_TRUE(key.empty());
  EXPECT_TRUE(values.empty());
}

TEST_F(SplitStringIntoKeyValuesTest, EmptyValueInputSingleValue) {
  EXPECT_FALSE(SplitStringIntoKeyValues("key_with_no_value\t",
                                        '\t',  // Key separators
                                        &key, &values));
  EXPECT_EQ("key_with_no_value", key);
  EXPECT_TRUE(values.empty());
}

TEST_F(SplitStringIntoKeyValuesTest, EmptyKeyInputSingleValue) {
  EXPECT_TRUE(SplitStringIntoKeyValues("\tvalue for empty key",
                                       '\t',  // Key separators
                                       &key, &values));
  EXPECT_TRUE(key.empty());
  ASSERT_EQ(1U, values.size());
  EXPECT_EQ("value for empty key", values[0]);
}

TEST_F(SplitStringIntoKeyValuesTest, KeyWithSingleValue) {
  EXPECT_TRUE(SplitStringIntoKeyValues("key1\tvalue1,   value2   value3",
                                       '\t',  // Key separators
                                       &key, &values));
  EXPECT_EQ("key1", key);
  ASSERT_EQ(1U, values.size());
  EXPECT_EQ("value1,   value2   value3", values[0]);
}

class SplitStringIntoKeyValuePairsTest : public testing::Test {
 protected:
  std::vector<std::pair<std::string, std::string> > kv_pairs;
};

TEST_F(SplitStringIntoKeyValuePairsTest, EmptyString) {
  EXPECT_TRUE(SplitStringIntoKeyValuePairs("",
                                           ':',   // Key-value delimiters
                                           ',',   // Key-value pair delims
                                           &kv_pairs));
  EXPECT_TRUE(kv_pairs.empty());
}

TEST_F(SplitStringIntoKeyValuePairsTest, EmptySecondPair) {
  EXPECT_TRUE(SplitStringIntoKeyValuePairs("key1:value1,,key3:value3",
                                           ':',   // Key-value delimiters
                                           ',',   // Key-value pair delims
                                           &kv_pairs));
  ASSERT_EQ(2U, kv_pairs.size());
  EXPECT_EQ("key1", kv_pairs[0].first);
  EXPECT_EQ("value1", kv_pairs[0].second);
  EXPECT_EQ("key3", kv_pairs[1].first);
  EXPECT_EQ("value3", kv_pairs[1].second);
}

TEST_F(SplitStringIntoKeyValuePairsTest, EmptySecondValue) {
  EXPECT_FALSE(SplitStringIntoKeyValuePairs("key1:value1 , key2:",
                                            ':',   // Key-value delimiters
                                            ',',   // Key-value pair delims
                                            &kv_pairs));
  ASSERT_EQ(2U, kv_pairs.size());
  EXPECT_EQ("key1", kv_pairs[0].first);
  EXPECT_EQ("value1", kv_pairs[0].second);
  EXPECT_EQ("key2", kv_pairs[1].first);
  EXPECT_EQ("", kv_pairs[1].second);
}

TEST_F(SplitStringIntoKeyValuePairsTest, DelimiterInValue) {
  EXPECT_TRUE(SplitStringIntoKeyValuePairs("key1:va:ue1 , key2:value2",
                                           ':',   // Key-value delimiters
                                           ',',   // Key-value pair delims
                                           &kv_pairs));
  ASSERT_EQ(2U, kv_pairs.size());
  EXPECT_EQ("key1", kv_pairs[0].first);
  EXPECT_EQ("va:ue1", kv_pairs[0].second);
  EXPECT_EQ("key2", kv_pairs[1].first);
  EXPECT_EQ("value2", kv_pairs[1].second);
}

TEST(SplitStringUsingSubstrTest, EmptyString) {
  std::vector<std::string> results;
  SplitStringUsingSubstr("", "DELIMITER", &results);
  ASSERT_EQ(1u, results.size());
  EXPECT_THAT(results, ElementsAre(""));
}

TEST(SplitStringUsingSubstrTest, StringWithNoDelimiter) {
  std::vector<std::string> results;
  SplitStringUsingSubstr("alongwordwithnodelimiter", "DELIMITER", &results);
  ASSERT_EQ(1u, results.size());
  EXPECT_THAT(results, ElementsAre("alongwordwithnodelimiter"));
}

TEST(SplitStringUsingSubstrTest, LeadingDelimitersSkipped) {
  std::vector<std::string> results;
  SplitStringUsingSubstr(
      "DELIMITERDELIMITERDELIMITERoneDELIMITERtwoDELIMITERthree",
      "DELIMITER",
      &results);
  ASSERT_EQ(6u, results.size());
  EXPECT_THAT(results, ElementsAre("", "", "", "one", "two", "three"));
}

TEST(SplitStringUsingSubstrTest, ConsecutiveDelimitersSkipped) {
  std::vector<std::string> results;
  SplitStringUsingSubstr(
      "unoDELIMITERDELIMITERDELIMITERdosDELIMITERtresDELIMITERDELIMITERcuatro",
      "DELIMITER",
      &results);
  ASSERT_EQ(7u, results.size());
  EXPECT_THAT(results, ElementsAre("uno", "", "", "dos", "tres", "", "cuatro"));
}

TEST(SplitStringUsingSubstrTest, TrailingDelimitersSkipped) {
  std::vector<std::string> results;
  SplitStringUsingSubstr(
      "unDELIMITERdeuxDELIMITERtroisDELIMITERquatreDELIMITERDELIMITERDELIMITER",
      "DELIMITER",
      &results);
  ASSERT_EQ(7u, results.size());
  EXPECT_THAT(
      results, ElementsAre("un", "deux", "trois", "quatre", "", "", ""));
}

TEST(StringSplitTest, StringSplitDontTrim) {
  std::vector<std::wstring> r;

  SplitStringDontTrim(L"\t\ta\t", L'\t', &r);
  ASSERT_EQ(4U, r.size());
  EXPECT_EQ(r[0], L"");
  EXPECT_EQ(r[1], L"");
  EXPECT_EQ(r[2], L"a");
  EXPECT_EQ(r[3], L"");
  r.clear();

  SplitStringDontTrim(L"\ta\t\nb\tcc", L'\n', &r);
  ASSERT_EQ(2U, r.size());
  EXPECT_EQ(r[0], L"\ta\t");
  EXPECT_EQ(r[1], L"b\tcc");
  r.clear();
}

}  // namespace base
