// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/types/processed_value.h"

namespace segmentation_platform::processing {

ProcessedValue::ProcessedValue(bool val) : type(Type::BOOL), bool_val(val) {}
ProcessedValue::ProcessedValue(int val) : type(Type::INT), int_val(val) {}
ProcessedValue::ProcessedValue(float val) : type(Type::FLOAT), float_val(val) {}
ProcessedValue::ProcessedValue(double val)
    : type(Type::DOUBLE), double_val(val) {}
ProcessedValue::ProcessedValue(const std::string& val)
    : type(Type::STRING), str_val(val) {}
ProcessedValue::ProcessedValue(base::Time val)
    : type(Type::TIME), time_val(val) {}
ProcessedValue::ProcessedValue(int64_t val)
    : type(Type::INT64), int64_val(val) {}
ProcessedValue::ProcessedValue(const GURL& val)
    : type(Type::URL), url(std::make_unique<GURL>(val)) {}

ProcessedValue::ProcessedValue(const ProcessedValue& other) {
  *this = other;
}
ProcessedValue::ProcessedValue(ProcessedValue&& other) = default;

ProcessedValue& ProcessedValue::operator=(const ProcessedValue& other) {
  type = other.type;
  switch (type) {
    case Type::UNKNOWN:
      return *this;
    case Type::BOOL:
      bool_val = other.bool_val;
      return *this;
    case Type::INT:
      int_val = other.int_val;
      return *this;
    case Type::FLOAT:
      float_val = other.float_val;
      return *this;
    case Type::DOUBLE:
      double_val = other.double_val;
      return *this;
    case Type::STRING:
      str_val = other.str_val;
      return *this;
    case Type::TIME:
      time_val = other.time_val;
      return *this;
    case Type::INT64:
      int64_val = other.int64_val;
      return *this;
    case Type::URL:
      url = std::make_unique<GURL>(*other.url);
      return *this;
  }
}

ProcessedValue::~ProcessedValue() = default;

bool ProcessedValue::operator==(const ProcessedValue& rhs) const {
  if (type != rhs.type)
    return false;
  switch (type) {
    case Type::UNKNOWN:
      return false;
    case Type::BOOL:
      return bool_val == rhs.bool_val;
    case Type::INT:
      return int_val == rhs.int_val;
    case Type::FLOAT:
      return float_val == rhs.float_val;
    case Type::DOUBLE:
      return double_val == rhs.double_val;
    case Type::STRING:
      return str_val == rhs.str_val;
    case Type::TIME:
      return time_val == rhs.time_val;
    case Type::INT64:
      return int64_val == rhs.int64_val;
    case Type::URL:
      return *url == *rhs.url;
  }
}

}  // namespace segmentation_platform::processing
