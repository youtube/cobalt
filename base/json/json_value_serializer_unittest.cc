// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_value_serializer.h"

#include <string>

#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// Some proper JSON to test with:
const char kProperJSON[] =
    "{\n"
    "   \"compound\": {\n"
    "      \"a\": 1,\n"
    "      \"b\": 2\n"
    "   },\n"
    "   \"some_String\": \"1337\",\n"
    "   \"some_int\": 42,\n"
    "   \"the_list\": [ \"val1\", \"val2\" ]\n"
    "}\n";

// Some proper JSON with trailing commas:
const char kProperJSONWithCommas[] =
    "{\n"
    "\t\"some_int\": 42,\n"
    "\t\"some_String\": \"1337\",\n"
    "\t\"the_list\": [\"val1\", \"val2\", ],\n"
    "\t\"compound\": { \"a\": 1, \"b\": 2, },\n"
    "}\n";

const char kWinLineEnds[] = "\r\n";
const char kLinuxLineEnds[] = "\n";

// Verifies the generated JSON against the expected output.
void CheckJSONIsStillTheSame(Value& value) {
  // Serialize back the output.
  std::string serialized_json;
  JSONStringValueSerializer str_serializer(&serialized_json);
  str_serializer.set_pretty_print(true);
  ASSERT_TRUE(str_serializer.Serialize(value));
  // Unify line endings between platforms.
  ReplaceSubstringsAfterOffset(&serialized_json, 0,
                               kWinLineEnds, kLinuxLineEnds);
  // Now compare the input with the output.
  ASSERT_EQ(kProperJSON, serialized_json);
}

// Test proper JSON [de]serialization from string is working.
TEST(JSONValueSerializerTest, ReadProperJSONFromString) {
  // Try to deserialize it through the serializer.
  std::string proper_json(kProperJSON);
  JSONStringValueSerializer str_deserializer(proper_json);

  int error_code = 0;
  std::string error_message;
  scoped_ptr<Value> value(
      str_deserializer.Deserialize(&error_code, &error_message));
  ASSERT_TRUE(value.get());
  ASSERT_EQ(0, error_code);
  ASSERT_TRUE(error_message.empty());
  // Verify if the same JSON is still there.
  CheckJSONIsStillTheSame(*value);
}

// Test that trialing commas are only properly deserialized from string when
// the proper flag for that is set.
TEST(JSONValueSerializerTest, ReadJSONWithTrailingCommasFromString) {
  // Try to deserialize it through the serializer.
  std::string proper_json(kProperJSONWithCommas);
  JSONStringValueSerializer str_deserializer(proper_json);

  int error_code = 0;
  std::string error_message;
  scoped_ptr<Value> value(
      str_deserializer.Deserialize(&error_code, &error_message));
  ASSERT_FALSE(value.get());
  ASSERT_NE(0, error_code);
  ASSERT_FALSE(error_message.empty());
  // Now the flag is set and it must pass.
  str_deserializer.set_allow_trailing_comma(true);
  value.reset(str_deserializer.Deserialize(&error_code, &error_message));
  ASSERT_TRUE(value.get());
  ASSERT_EQ(JSONReader::JSON_TRAILING_COMMA, error_code);
  // Verify if the same JSON is still there.
  CheckJSONIsStillTheSame(*value);
}

// Test proper JSON [de]serialization from file is working.
TEST(JSONValueSerializerTest, ReadProperJSONFromFile) {
  ScopedTempDir tempdir;
  ASSERT_TRUE(tempdir.CreateUniqueTempDir());
  // Write it down in the file.
  FilePath temp_file(tempdir.path().AppendASCII("test.json"));
  ASSERT_EQ(static_cast<int>(strlen(kProperJSON)),
            file_util::WriteFile(temp_file, kProperJSON, strlen(kProperJSON)));

  // Try to deserialize it through the serializer.
  JSONFileValueSerializer file_deserializer(temp_file);

  int error_code = 0;
  std::string error_message;
  scoped_ptr<Value> value(
      file_deserializer.Deserialize(&error_code, &error_message));
  ASSERT_TRUE(value.get());
  ASSERT_EQ(0, error_code);
  ASSERT_TRUE(error_message.empty());
  // Verify if the same JSON is still there.
  CheckJSONIsStillTheSame(*value);
}

// Test that trialing commas are only properly deserialized from file when
// the proper flag for that is set.
TEST(JSONValueSerializerTest, ReadJSONWithCommasFromFile) {
  ScopedTempDir tempdir;
  ASSERT_TRUE(tempdir.CreateUniqueTempDir());
  // Write it down in the file.
  FilePath temp_file(tempdir.path().AppendASCII("test.json"));
  ASSERT_EQ(static_cast<int>(strlen(kProperJSONWithCommas)),
            file_util::WriteFile(temp_file,
                                 kProperJSONWithCommas,
                                 strlen(kProperJSONWithCommas)));

  // Try to deserialize it through the serializer.
  JSONFileValueSerializer file_deserializer(temp_file);
  // This must fail without the proper flag.
  int error_code = 0;
  std::string error_message;
  scoped_ptr<Value> value(
      file_deserializer.Deserialize(&error_code, &error_message));
  ASSERT_FALSE(value.get());
  ASSERT_NE(0, error_code);
  ASSERT_FALSE(error_message.empty());
  // Now the flag is set and it must pass.
  file_deserializer.set_allow_trailing_comma(true);
  value.reset(file_deserializer.Deserialize(&error_code, &error_message));
  ASSERT_TRUE(value.get());
  ASSERT_EQ(JSONReader::JSON_TRAILING_COMMA, error_code);
  // Verify if the same JSON is still there.
  CheckJSONIsStillTheSame(*value);
}

}  // namespace

}  // namespace base

