// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_split.h"
#include "testing/gtest/include/gtest/gtest.h"

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

}  // namespace base
