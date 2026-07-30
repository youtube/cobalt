// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/script_message_value_util.h"

#import <Foundation/Foundation.h>

#import <memory>

#import "base/values.h"
#import "ios/web/public/js_messaging/script_message_value.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace web {

using ScriptMessageValueUtilTest = PlatformTest;

// Tests whether the CreateScriptMessageValue function can create a
// ScriptMessageValue object from a NSString.
TEST_F(ScriptMessageValueUtilTest, ConvertNSStringToScriptMessageValue) {
  NSString* string_element = @"test_string";

  ScriptMessageValue value = CreateScriptMessageValue(string_element);

  ASSERT_EQ(base::Value::Type::STRING, value.type());
  EXPECT_EQ("test_string", value.GetValue().GetString());
}

// Tests whether the CreateScriptMessageValue function can create a
// ScriptMessageValue object from an integer.
TEST_F(ScriptMessageValueUtilTest, ConvertIntegerToScriptMessageValue) {
  NSNumber* integer_element = @(3);

  ScriptMessageValue value = CreateScriptMessageValue(integer_element);

  ASSERT_EQ(base::Value::Type::INTEGER, value.type());
  EXPECT_EQ(3, value.GetValue().GetInt());
}

// Tests whether the CreateScriptMessageValue function can create a
// ScriptMessageValue object from a double.
TEST_F(ScriptMessageValueUtilTest, ConvertDoubleToScriptMessageValue) {
  NSNumber* double_element = @3.14;

  ScriptMessageValue value = CreateScriptMessageValue(double_element);

  ASSERT_EQ(base::Value::Type::DOUBLE, value.type());
  EXPECT_EQ(3.14, value.GetValue().GetDouble());
}

// Tests whether the CreateScriptMessageValue function can create a
// ScriptMessageValue object from a Boolean.
TEST_F(ScriptMessageValueUtilTest, ConvertBooleanToScriptMessageValue) {
  NSNumber* bool_element = @YES;

  ScriptMessageValue value = CreateScriptMessageValue(bool_element);

  ASSERT_EQ(base::Value::Type::BOOLEAN, value.type());
  EXPECT_TRUE(value.GetValue().GetBool());
}

// Tests whether the CreateScriptMessageValue function can create a
// ScriptMessageValue object from a NSDictionary.
TEST_F(ScriptMessageValueUtilTest, ConvertNSDictionaryToScriptMessageValue) {
  NSDictionary* dict_element = @{@"key" : @"value"};

  ScriptMessageValue value = CreateScriptMessageValue(dict_element);

  ASSERT_EQ(base::Value::Type::DICT, value.type());
  EXPECT_EQ(value.GetDict().size(), 1u);
}

// Tests whether the CreateScriptMessageValue function can create a
// ScriptMessageValue object from a NSArray.
TEST_F(ScriptMessageValueUtilTest, ConvertNSArrayToScriptMessageValue) {
  NSArray* array_element = @[ @"item1", @"item2" ];

  ScriptMessageValue value = CreateScriptMessageValue(array_element);

  ASSERT_EQ(base::Value::Type::LIST, value.type());
  EXPECT_EQ(value.GetList().size(), 2u);
}

}  // namespace web
