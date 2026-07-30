// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "base/values.h"
#import "ios/web/public/js_messaging/script_message_dict_value.h"
#import "ios/web/public/js_messaging/script_message_list_value.h"
#import "ios/web/public/js_messaging/script_message_value.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {
// Creates a populated dict for use in tests.
NSDictionary* test_dict() {
  return @{
    @"boolKey" : @YES,
    @"intKey" : @42,
    @"doubleKey" : @3.14,
    @"stringKey" : @"hello",
  };
}
}  // namespace

namespace web {

using ScriptMessageDictValueTest = PlatformTest;

// Tests whether the `empty()` function returns true when invoked on a
// dictionary with no elements.
TEST_F(ScriptMessageDictValueTest, EmptyFunctionIsTrueWhenDictIsEmpty) {
  NSDictionary* empty_ns_dict = @{};
  ScriptMessageDictValue empty_dict(empty_ns_dict);
  EXPECT_TRUE(empty_dict.empty());
}

// Tests whether the `size()` function returns zero when invoked on an empty
// dictionary.
TEST_F(ScriptMessageDictValueTest, DictSizeIsZeroWhenDictIsEmpty) {
  NSDictionary* empty_ns_dict = @{};
  ScriptMessageDictValue empty_dict(empty_ns_dict);
  EXPECT_EQ(0u, empty_dict.size());
}

// Tests whether the `empty()` function returns false when a dictionary does
// contain elements.
TEST_F(ScriptMessageDictValueTest, EmptyFunctionIsFalseWhenDictIsNonempty) {
  ScriptMessageDictValue dict(test_dict());
  EXPECT_FALSE(dict.empty());
}

// Tests whether the `size()` function returns the correct number of elements
// in the dictionary.
TEST_F(ScriptMessageDictValueTest, DictSizeIsNonzeroWhenDictIsNonempty) {
  ScriptMessageDictValue dict(test_dict());
  EXPECT_EQ(4u, dict.size());
}

// Tests whether the `contains()` function returns true when given a key that
// does exist within the dictionary.
TEST_F(ScriptMessageDictValueTest, ContainsReturnsTrueWhenAPresentKeyIsGiven) {
  ScriptMessageDictValue dict(test_dict());
  EXPECT_TRUE(dict.contains("boolKey"));
}

// Tests whether the `contains()` function returns false if given a key that
// does not exist within the dictionary.
TEST_F(ScriptMessageDictValueTest,
       ContainsReturnsFalseWhenANonExistentKeyIsGiven) {
  ScriptMessageDictValue dict(test_dict());
  EXPECT_FALSE(dict.contains("nonExistentKey"));
}

// Tests that the `Find*()` family of functions returns the correct value.
TEST_F(ScriptMessageDictValueTest, FindFunctionsSuccessfullyReturnValue) {
  ScriptMessageDictValue dict(test_dict());
  EXPECT_EQ(true, dict.FindBool("boolKey").value_or(false));
  EXPECT_EQ(42, dict.FindInt("intKey").value_or(0));
  EXPECT_EQ(3.14, dict.FindDouble("doubleKey").value_or(0.0));
  // Double getter should work for int as well
  EXPECT_EQ(42.0, dict.FindDouble("intKey").value_or(0.0));
  EXPECT_EQ("hello", dict.FindString("stringKey").value_or(""));
}

// Tests whether the `Find*()` family of functions returns `std::nullopt` when
// given a key that does not exist in the dictionary.
TEST_F(ScriptMessageDictValueTest,
       FindFunctionsReturnEmptyOptionalForNonexistentKeys) {
  ScriptMessageDictValue dict(test_dict());
  EXPECT_FALSE(dict.FindBool("missing").has_value());
  EXPECT_FALSE(dict.FindInt("missing").has_value());
  EXPECT_FALSE(dict.FindDouble("missing").has_value());
  EXPECT_FALSE(dict.FindString("missing").has_value());
}

// Tests extraction of nested collections (Dict).
TEST_F(ScriptMessageDictValueTest, DictShouldSupportNestedDicts) {
  NSDictionary* ns_dict = @{
    @"dictKey" : @{@"innerKey" : @"innerVal"},
    @"stringKey" : @"notACollection",
  };
  ScriptMessageDictValue dict(ns_dict);

  std::optional<ScriptMessageDictValue> inner_dict = dict.FindDict("dictKey");
  ASSERT_TRUE(inner_dict);
  EXPECT_EQ("innerVal", inner_dict->FindString("innerKey").value_or(""));
}

// Tests that a dictionary containing a list can be iterated through.
TEST_F(ScriptMessageDictValueTest, DictShouldSupportNestedLists) {
  NSDictionary* ns_dict = @{
    @"listKey" : @[ @0.0, @1.0 ],
  };
  ScriptMessageDictValue dict(ns_dict);

  std::optional<ScriptMessageListValue> inner_list = dict.FindList("listKey");
  ASSERT_TRUE(inner_list);
  size_t index = 0;
  for (const ScriptMessageValue value : *inner_list) {
    EXPECT_EQ(base::Value::Type::DOUBLE, value.type());
    EXPECT_NSEQ(@(value.GetValue().GetDouble()), ns_dict[@"listKey"][index]);
    index++;
  }
}

// Tests generic base::Value conversion via Find().
TEST_F(ScriptMessageDictValueTest, GenericFindShouldReturnBaseValue) {
  NSDictionary* ns_dict = @{
    @"stringKey" : @"hello",
  };
  ScriptMessageDictValue dict(ns_dict);

  const std::optional<std::string> val = dict.FindString("stringKey");
  ASSERT_TRUE(val);
  EXPECT_EQ("hello", val.value());
}

// Tests move operations.
TEST_F(ScriptMessageDictValueTest, DictShouldSupportMoveOperations) {
  NSDictionary* ns_dict = @{@"key" : @"value"};
  ScriptMessageDictValue original(ns_dict);

  // Move construction
  ScriptMessageDictValue moved_constructed(std::move(original));
  EXPECT_EQ("value", moved_constructed.FindString("key").value_or(""));

  // Move assignment
  ScriptMessageDictValue moved_assigned(nullptr);
  moved_assigned = std::move(moved_constructed);
  EXPECT_EQ("value", moved_assigned.FindString("key").value_or(""));
}

}  // namespace web
