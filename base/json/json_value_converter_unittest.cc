// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_value_converter.h"

#include <string>
#include <vector>

#include "base/values.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

// Very simple messages.
struct SimpleMessage {
  enum SimpleEnum {
    FOO, BAR,
  };
  int foo;
  std::string bar;
  bool baz;
  SimpleEnum simple_enum;
  std::vector<int> ints;
  SimpleMessage() : foo(0), baz(false) {}

  static bool ParseSimpleEnum(const StringPiece& value, SimpleEnum* field) {
    if (value == "foo") {
      *field = FOO;
      return true;
    } else if (value == "bar") {
      *field = BAR;
      return true;
    }
    return false;
  }

  static void RegisterJSONConverter(
      base::JSONValueConverter<SimpleMessage>* converter) {
    converter->RegisterIntField("foo", &SimpleMessage::foo);
    converter->RegisterStringField("bar", &SimpleMessage::bar);
    converter->RegisterBoolField("baz", &SimpleMessage::baz);
    converter->RegisterCustomField<SimpleEnum>(
        "simple_enum", &SimpleMessage::simple_enum, &ParseSimpleEnum);
    converter->RegisterRepeatedInt("ints", &SimpleMessage::ints);
  }
};

// For nested messages.
struct NestedMessage {
  double foo;
  SimpleMessage child;
  std::vector<SimpleMessage> children;

  NestedMessage() : foo(0) {}

  static void RegisterJSONConverter(
      base::JSONValueConverter<NestedMessage>* converter) {
    converter->RegisterDoubleField("foo", &NestedMessage::foo);
    converter->RegisterNestedField("child", &NestedMessage::child);
    converter->RegisterRepeatedMessage("children", &NestedMessage::children);
  }
};

}  // namespace

TEST(JSONValueConverterTest, ParseSimpleMessage) {
  const char normal_data[] =
      "{\n"
      "  \"foo\": 1,\n"
      "  \"bar\": \"bar\",\n"
      "  \"baz\": true,\n"
      "  \"simple_enum\": \"foo\","
      "  \"ints\": [1, 2]"
      "}\n";

  scoped_ptr<Value> value(base::JSONReader::Read(normal_data, false));
  SimpleMessage message;
  base::JSONValueConverter<SimpleMessage> converter;
  EXPECT_TRUE(converter.Convert(*value.get(), &message));

  EXPECT_EQ(1, message.foo);
  EXPECT_EQ("bar", message.bar);
  EXPECT_TRUE(message.baz);
  EXPECT_EQ(SimpleMessage::FOO, message.simple_enum);
  EXPECT_EQ(2, static_cast<int>(message.ints.size()));
  EXPECT_EQ(1, message.ints[0]);
  EXPECT_EQ(2, message.ints[1]);
}

TEST(JSONValueConverterTest, ParseNestedMessage) {
  const char normal_data[] =
      "{\n"
      "  \"foo\": 1.0,\n"
      "  \"child\": {\n"
      "    \"foo\": 1,\n"
      "    \"bar\": \"bar\",\n"
      "    \"baz\": true\n"
      "  },\n"
      "  \"children\": [{\n"
      "    \"foo\": 2,\n"
      "    \"bar\": \"foobar\",\n"
      "    \"baz\": true\n"
      "  },\n"
      "  {\n"
      "    \"foo\": 3,\n"
      "    \"bar\": \"barbaz\",\n"
      "    \"baz\": false\n"
      "  }]\n"
      "}\n";

  scoped_ptr<Value> value(base::JSONReader::Read(normal_data, false));
  NestedMessage message;
  base::JSONValueConverter<NestedMessage> converter;
  EXPECT_TRUE(converter.Convert(*value.get(), &message));

  EXPECT_EQ(1.0, message.foo);
  EXPECT_EQ(1, message.child.foo);
  EXPECT_EQ("bar", message.child.bar);
  EXPECT_TRUE(message.child.baz);

  EXPECT_EQ(2, static_cast<int>(message.children.size()));
  const SimpleMessage& first_child = message.children[0];
  EXPECT_EQ(2, first_child.foo);
  EXPECT_EQ("foobar", first_child.bar);
  EXPECT_TRUE(first_child.baz);

  const SimpleMessage& second_child = message.children[1];
  EXPECT_EQ(3, second_child.foo);
  EXPECT_EQ("barbaz", second_child.bar);
  EXPECT_FALSE(second_child.baz);
}

TEST(JSONValueConverterTest, ParseFailures) {
  const char normal_data[] =
      "{\n"
      "  \"foo\": 1,\n"
      "  \"bar\": 2,\n" // "bar" is an integer here.
      "  \"baz\": true,\n"
      "  \"ints\": [1, 2]"
      "}\n";

  scoped_ptr<Value> value(base::JSONReader::Read(normal_data, false));
  SimpleMessage message;
  base::JSONValueConverter<SimpleMessage> converter;
  EXPECT_FALSE(converter.Convert(*value.get(), &message));
  // Do not check the values below.  |message| may be modified during
  // Convert() even it fails.
}

TEST(JSONValueConverterTest, ParseWithMissingFields) {
  const char normal_data[] =
      "{\n"
      "  \"foo\": 1,\n"
      "  \"baz\": true,\n"
      "  \"ints\": [1, 2]"
      "}\n";

  scoped_ptr<Value> value(base::JSONReader::Read(normal_data, false));
  SimpleMessage message;
  base::JSONValueConverter<SimpleMessage> converter;
  // Convert() still succeeds even if the input doesn't have "bar" field.
  EXPECT_TRUE(converter.Convert(*value.get(), &message));

  EXPECT_EQ(1, message.foo);
  EXPECT_TRUE(message.baz);
  EXPECT_EQ(2, static_cast<int>(message.ints.size()));
  EXPECT_EQ(1, message.ints[0]);
  EXPECT_EQ(2, message.ints[1]);
}

TEST(JSONValueConverterTest, EnumParserFails) {
  const char normal_data[] =
      "{\n"
      "  \"foo\": 1,\n"
      "  \"bar\": \"bar\",\n"
      "  \"baz\": true,\n"
      "  \"simple_enum\": \"baz\","
      "  \"ints\": [1, 2]"
      "}\n";

  scoped_ptr<Value> value(base::JSONReader::Read(normal_data, false));
  SimpleMessage message;
  base::JSONValueConverter<SimpleMessage> converter;
  EXPECT_FALSE(converter.Convert(*value.get(), &message));
  // No check the values as mentioned above.
}

}  // namespace base
