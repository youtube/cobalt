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

namespace web {

using ScriptMessageListValueTest = PlatformTest;

// Tests that `empty()` returns true for an empty list.
TEST_F(ScriptMessageListValueTest, ListValueIsEmptyGivenEmptyArray) {
  NSArray* empty_ns_array = @[];
  ScriptMessageListValue empty_list(empty_ns_array);

  EXPECT_TRUE(empty_list.empty());
}

// Tests that `size()` returns 0 for an empty list.
TEST_F(ScriptMessageListValueTest, SizeIsZeroGivenEmptyArray) {
  NSArray* empty_ns_array = @[];
  ScriptMessageListValue empty_list(empty_ns_array);

  EXPECT_EQ(0u, empty_list.size());
}

// Tests that `empty()` returns false for non-empty list.
TEST_F(ScriptMessageListValueTest, ListValueIsNotEmptyGivenNonEmptyArray) {
  NSArray* non_empty_ns_array = @[ @1, @2 ];
  ScriptMessageListValue list(non_empty_ns_array);

  EXPECT_FALSE(list.empty());
}

// Tests that `size()` is equivalent to the number of elements in a non-empty
// list.
TEST_F(ScriptMessageListValueTest, SizeIsTwoGivenNonEmptyArray) {
  NSArray* array_with_two_elements = @[ @1, @2 ];
  ScriptMessageListValue list(array_with_two_elements);

  EXPECT_EQ(list.size(), 2u);
}

// Tests whether the `front()` function correctly returns `std::nullopt` if the
// list is empty.
TEST_F(ScriptMessageListValueTest, NoFrontElementIfArrayIsEmpty) {
  NSArray* array_with_dict_elements = @[];
  ScriptMessageListValue list(array_with_dict_elements);

  std::optional<ScriptMessageValue> front = list.front();

  ASSERT_FALSE(front.has_value());
}

// Tests whether the `front()` function correctly returns the first element in
// the list.
TEST_F(ScriptMessageListValueTest, FrontElementIsEquivalentToTheFirstElement) {
  NSArray* array_with_dict_elements =
      @[ @{@"name" : @"item1"}, @{@"name" : @"item2"} ];
  ScriptMessageListValue list(array_with_dict_elements);

  std::optional<ScriptMessageValue> front = list.front();

  ASSERT_TRUE(front.has_value());
  ASSERT_EQ(base::Value::Type::DICT, front->type());
  EXPECT_EQ("item1", front->GetDict().FindString("name").value_or(""));
}

// Tests whether the `front()` function correctly returns the first element in
// the list containing NSNumbers.
TEST_F(ScriptMessageListValueTest,
       FrontElementIsEquivalentToTheFirstElementInNumberArray) {
  NSArray* array_with_nsnumber_elements =
      @[ @1, @2, @3, @4, @5, @6, @7, @8, @9, @10 ];
  ScriptMessageListValue list(array_with_nsnumber_elements);

  std::optional<ScriptMessageValue> front = list.front();

  ASSERT_TRUE(front.has_value());
  ASSERT_EQ(base::Value::Type::INTEGER, front->type());
  EXPECT_EQ(1, front->GetValue().GetInt());
}

// Tests whether the `front()` function correctly returns the first element in
// the list containing strings.
TEST_F(ScriptMessageListValueTest,
       FrontElementIsEquivalentToTheFirstElementInStringArray) {
  NSArray* array_with_string_elements = @[ @"a", @"b", @"c", @"d", @"e" ];
  ScriptMessageListValue list(array_with_string_elements);

  std::optional<ScriptMessageValue> front = list.front();

  ASSERT_TRUE(front.has_value());
  ASSERT_EQ(base::Value::Type::STRING, front->type());
  EXPECT_EQ("a", front->GetValue().GetString());
}

// Tests whether the `back()` function correctly returns `std::nullopt` if the
// list is empty.
TEST_F(ScriptMessageListValueTest, NoBackElementIfArrayIsEmpty) {
  NSArray* array_with_dict_elements = @[];
  ScriptMessageListValue list(array_with_dict_elements);

  std::optional<ScriptMessageValue> back = list.back();

  ASSERT_FALSE(back.has_value());
}

// Tests whether the `back()` function correctly returns the last element in
// the list.
TEST_F(ScriptMessageListValueTest, BackElementIsEquivalentToTheLastElement) {
  NSArray* array_with_dict_elements =
      @[ @{@"name" : @"item1"}, @{@"name" : @"item2"} ];
  ScriptMessageListValue list(array_with_dict_elements);

  std::optional<ScriptMessageValue> back = list.back();

  ASSERT_TRUE(back.has_value());
  ASSERT_EQ(base::Value::Type::DICT, back->type());
  EXPECT_EQ("item2", back->GetDict().FindString("name").value_or(""));
}

// Tests whether the `back()` function correctly returns the last element in
// the list containing NSNumbers.
TEST_F(ScriptMessageListValueTest,
       BackElementIsEquivalentToTheLastElementInNumberArray) {
  NSArray* array_with_nsnumber_elements =
      @[ @1, @2, @3, @4, @5, @6, @7, @8, @9, @10 ];
  ScriptMessageListValue list(array_with_nsnumber_elements);

  std::optional<ScriptMessageValue> back = list.back();

  ASSERT_TRUE(back.has_value());
  ASSERT_EQ(base::Value::Type::INTEGER, back->type());
  EXPECT_EQ(10, back->GetValue().GetInt());
}

// Tests whether the `back()` function correctly returns the last element in
// the list containing Strings.
TEST_F(ScriptMessageListValueTest,
       BackElementIsEquivalentToTheLastElementInStringArray) {
  NSArray* array_with_string_elements = @[ @"a", @"b", @"c", @"d", @"e" ];
  ScriptMessageListValue list(array_with_string_elements);

  std::optional<ScriptMessageValue> back = list.back();

  ASSERT_TRUE(back.has_value());
  ASSERT_EQ(base::Value::Type::STRING, back->type());
  EXPECT_EQ("e", back->GetValue().GetString());
}

// Tests move construction and assignment.
TEST_F(ScriptMessageListValueTest, ListValueSupportsMoveConstruction) {
  NSArray* array_with_two_elements = @[ @1, @2 ];
  ScriptMessageListValue original(array_with_two_elements);

  ScriptMessageListValue moved_constructed(std::move(original));

  EXPECT_EQ(2u, moved_constructed.size());
}

// Tests move assignment.
TEST_F(ScriptMessageListValueTest, ListValueSupportsMoveAssignment) {
  NSArray* array_with_two_elements = @[ @1, @2 ];
  ScriptMessageListValue original(array_with_two_elements);
  ScriptMessageListValue moved_assigned(@[]);
  ScriptMessageListValue moved_constructed(std::move(original));

  moved_assigned = std::move(moved_constructed);

  EXPECT_EQ(2u, moved_assigned.size());
}

// Tests forward iteration using loop by size.
TEST_F(ScriptMessageListValueTest, ListValueSupportsForwardIterators) {
  NSArray* array_of_nums = @[ @0, @1, @2, @3, @4, @5, @6, @7, @8, @9 ];
  ScriptMessageListValue list(array_of_nums);

  ScriptMessageListValue::iterator iter = list.begin();
  for (size_t index = 0; index < list.size(); ++index) {
    ASSERT_EQ(base::Value::Type::INTEGER, (*iter).type());
    EXPECT_NSEQ(@((*iter).GetValue().GetInt()), array_of_nums[index]);
    iter++;
  }
}

// Tests if ScriptMessageListValue can be iterated over using range-based loop
// syntax.
TEST_F(ScriptMessageListValueTest, ListValueSupportsRangeBasedLoopSyntax) {
  NSArray* array_of_nums = @[ @0, @1, @2, @3, @4, @5, @6, @7, @8, @9 ];
  ScriptMessageListValue list(array_of_nums);

  size_t index = 0;
  for (ScriptMessageValue value : list) {
    ASSERT_EQ(base::Value::Type::INTEGER, value.type());
    EXPECT_NSEQ(@(value.GetValue().GetInt()), array_of_nums[index]);
    index++;
  }

  EXPECT_EQ(10u, index);
}

}  // namespace web
