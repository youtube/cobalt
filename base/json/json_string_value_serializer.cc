// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_string_value_serializer.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"

JSONStringValueSerializer::~JSONStringValueSerializer() {}

bool JSONStringValueSerializer::Serialize(const Value& root) {
  return SerializeInternal(root, false);
}

bool JSONStringValueSerializer::SerializeAndOmitBinaryValues(
    const Value& root) {
  return SerializeInternal(root, true);
}

bool JSONStringValueSerializer::SerializeInternal(const Value& root,
                                                  bool omit_binary_values) {
  if (!json_string_ || initialized_with_const_string_)
    return false;

  base::JSONWriter::WriteWithOptions(
      &root,
      pretty_print_,
      omit_binary_values ? base::JSONWriter::OPTIONS_OMIT_BINARY_VALUES : 0,
      json_string_);
  return true;
}

Value* JSONStringValueSerializer::Deserialize(int* error_code,
                                              std::string* error_str) {
  if (!json_string_)
    return NULL;

  return base::JSONReader::ReadAndReturnError(*json_string_,
                                              allow_trailing_comma_,
                                              error_code,
                                              error_str);
}

