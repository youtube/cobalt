// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BINDINGS_TESTING_NUMERIC_TYPES_TEST_INTERFACE_H_
#define COBALT_BINDINGS_TESTING_NUMERIC_TYPES_TEST_INTERFACE_H_

#include <limits>

#include "cobalt/script/wrappable.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cobalt {
namespace bindings {
namespace testing {

class NumericTypesTestInterface : public script::Wrappable {
 public:
  virtual int8_t ByteReturnOperation() { return 0; }
  virtual void ByteArgumentOperation(int8_t value) {}
  virtual int8_t byte_property() { return 0; }
  virtual void set_byte_property(int8_t value) {}
  virtual int8_t byte_clamp_property() { return 0; }
  virtual void set_byte_clamp_property(int8_t value) {}

  virtual uint8_t OctetReturnOperation() { return 0; }
  virtual void OctetArgumentOperation(uint8_t value) {}
  virtual uint8_t octet_property() { return 0; }
  virtual void set_octet_property(uint8_t value) {}
  virtual uint8_t octet_clamp_property() { return 0; }
  virtual void set_octet_clamp_property(uint8_t value) {}

  virtual int16_t ShortReturnOperation() { return 0; }
  virtual void ShortArgumentOperation(int16_t value) {}
  virtual int16_t short_property() { return 0; }
  virtual void set_short_property(int16_t value) {}
  virtual int16_t short_clamp_property() { return 0; }
  virtual void set_short_clamp_property(int16_t value) {}

  virtual uint16_t UnsignedShortReturnOperation() { return 0; }
  virtual void UnsignedShortArgumentOperation(uint16_t value) {}
  virtual uint16_t unsigned_short_property() { return 0; }
  virtual void set_unsigned_short_property(uint16_t value) {}
  virtual uint16_t unsigned_short_clamp_property() { return 0; }
  virtual void set_unsigned_short_clamp_property(uint16_t value) {}

  virtual int32_t LongReturnOperation() { return 0; }
  virtual void LongArgumentOperation(int32_t value) {}
  virtual int32_t long_property() { return 0; }
  virtual void set_long_property(int32_t value) {}
  virtual int32_t long_clamp_property() { return 0; }
  virtual void set_long_clamp_property(int32_t value) {}

  virtual uint32_t UnsignedLongReturnOperation() { return 0; }
  virtual void UnsignedLongArgumentOperation(uint32_t value) {}
  virtual uint32_t unsigned_long_property() { return 0; }
  virtual void set_unsigned_long_property(uint32_t value) {}
  virtual uint32_t unsigned_long_clamp_property() { return 0; }
  virtual void set_unsigned_long_clamp_property(uint32_t value) {}

  virtual int64_t LongLongReturnOperation() { return 0; }
  virtual void LongLongArgumentOperation(int64_t value) {}
  virtual int64_t long_long_property() { return 0; }
  virtual void set_long_long_property(int64_t value) {}
  virtual int64_t long_long_clamp_property() { return 0; }
  virtual void set_long_long_clamp_property(int64_t value) {}

  virtual uint64_t UnsignedLongLongReturnOperation() { return 0; }
  virtual void UnsignedLongLongArgumentOperation(uint64_t value) {}
  virtual uint64_t unsigned_long_long_property() { return 0; }
  virtual void set_unsigned_long_long_property(uint64_t value) {}
  virtual uint64_t unsigned_long_long_clamp_property() { return 0; }
  virtual void set_unsigned_long_long_clamp_property(uint64_t value) {}

  virtual double DoubleReturnOperation() { return 0; }
  virtual void DoubleArgumentOperation(double value) {}
  virtual double double_property() { return 0; }
  virtual void set_double_property(double value) {}

  virtual double UnrestrictedDoubleReturnOperation() { return 0; }
  virtual void UnrestrictedDoubleArgumentOperation(double value) {}
  virtual double unrestricted_double_property() { return 0; }
  virtual void set_unrestricted_double_property(double value) {}

  DEFINE_WRAPPABLE_TYPE(NumericTypesTestInterface);
};

// Inheriting from an instantiation of this class allows for the mocking
// and testing of one type, while returning default values for the ones that
// are not being tested.
// Since the mock function names are the same for every type, expectations can
// be set in TYPED_TEST cases without needing to change the name of the
// mock function.
template <typename T>
class NumericTypesTestInterfaceT : public NumericTypesTestInterface {
 public:
  MOCK_METHOD0_T(MockReturnValueOperation, T());
  MOCK_METHOD1_T(MockArgumentOperation, void(T));
  MOCK_METHOD0_T(mock_get_property, T());
  MOCK_METHOD1_T(mock_set_property, void(T));

  typedef T BaseType;
};

class ByteTypeTest : public NumericTypesTestInterfaceT<int8_t> {
 public:
  int8_t ByteReturnOperation() override { return MockReturnValueOperation(); }
  void ByteArgumentOperation(int8_t value) override {
    MockArgumentOperation(value);
  }
  int8_t byte_property() override { return mock_get_property(); }
  void set_byte_property(int8_t value) override { mock_set_property(value); }
  int8_t byte_clamp_property() override { return mock_get_property(); }
  void set_byte_clamp_property(int8_t value) override {
    mock_set_property(value);
  }

  static const char* type_string() { return "byte"; }
  static int8_t max_value() { return 127; }
  static int8_t min_value() { return -128; }
  static const char* max_value_string() { return "127"; }
  static const char* min_value_string() { return "-128"; }
};

class OctetTypeTest : public NumericTypesTestInterfaceT<uint8_t> {
 public:
  uint8_t OctetReturnOperation() override { return MockReturnValueOperation(); }
  void OctetArgumentOperation(uint8_t value) override {
    MockArgumentOperation(value);
  }
  uint8_t octet_property() override { return mock_get_property(); }
  void set_octet_property(uint8_t value) override { mock_set_property(value); }
  uint8_t octet_clamp_property() override { return mock_get_property(); }
  void set_octet_clamp_property(uint8_t value) override {
    mock_set_property(value);
  }

  static const char* type_string() { return "octet"; }
  static uint8_t max_value() { return 255; }
  static uint8_t min_value() { return 0; }
  static const char* max_value_string() { return "255"; }
  static const char* min_value_string() { return "0"; }
};

class ShortTypeTest : public NumericTypesTestInterfaceT<int16_t> {
 public:
  int16_t ShortReturnOperation() override { return MockReturnValueOperation(); }
  void ShortArgumentOperation(int16_t value) override {
    MockArgumentOperation(value);
  }
  int16_t short_property() override { return mock_get_property(); }
  void set_short_property(int16_t value) override { mock_set_property(value); }
  int16_t short_clamp_property() override { return mock_get_property(); }
  void set_short_clamp_property(int16_t value) override {
    mock_set_property(value);
  }

  static const char* type_string() { return "short"; }
  static int16_t max_value() { return 32767; }
  static int16_t min_value() { return -32768; }
  static const char* max_value_string() { return "32767"; }
  static const char* min_value_string() { return "-32768"; }
};

class UnsignedShortTypeTest : public NumericTypesTestInterfaceT<uint16_t> {
 public:
  uint16_t UnsignedShortReturnOperation() override {
    return MockReturnValueOperation();
  }
  void UnsignedShortArgumentOperation(uint16_t value) override {
    MockArgumentOperation(value);
  }
  uint16_t unsigned_short_property() override { return mock_get_property(); }
  void set_unsigned_short_property(uint16_t value) override {
    mock_set_property(value);
  }
  uint16_t unsigned_short_clamp_property() override {
    return mock_get_property();
  }
  void set_unsigned_short_clamp_property(uint16_t value) override {
    mock_set_property(value);
  }

  static const char* type_string() { return "unsignedShort"; }
  static uint16_t max_value() { return 65535; }
  static uint16_t min_value() { return 0; }
  static const char* max_value_string() { return "65535"; }
  static const char* min_value_string() { return "0"; }
};

class LongTypeTest : public NumericTypesTestInterfaceT<int32_t> {
 public:
  int32_t LongReturnOperation() override { return MockReturnValueOperation(); }
  void LongArgumentOperation(int32_t value) override {
    MockArgumentOperation(value);
  }
  int32_t long_property() override { return mock_get_property(); }
  void set_long_property(int32_t value) override { mock_set_property(value); }
  int32_t long_clamp_property() override { return mock_get_property(); }
  void set_long_clamp_property(int32_t value) override {
    mock_set_property(value);
  }

  static const char* type_string() { return "long"; }
  static int32_t max_value() { return 2147483647; }
  static int32_t min_value() {
    // The minimum integer value cannot be written as -2147483648,
    // see https://msdn.microsoft.com/en-us/library/4kh09110.aspx for details.
    // Express it as a 64-bit integer literal and static_cast to int32_t.
    return static_cast<int32_t>(-2147483648ll);
  }
  static const char* max_value_string() { return "2147483647"; }
  static const char* min_value_string() { return "-2147483648"; }
};

class UnsignedLongTypeTest : public NumericTypesTestInterfaceT<uint32_t> {
 public:
  uint32_t UnsignedLongReturnOperation() override {
    return MockReturnValueOperation();
  }
  void UnsignedLongArgumentOperation(uint32_t value) override {
    MockArgumentOperation(value);
  }
  uint32_t unsigned_long_property() override { return mock_get_property(); }
  void set_unsigned_long_property(uint32_t value) override {
    mock_set_property(value);
  }
  uint32_t unsigned_long_clamp_property() override {
    return mock_get_property();
  }
  void set_unsigned_long_clamp_property(uint32_t value) override {
    mock_set_property(value);
  }

  static const char* type_string() { return "unsignedLong"; }
  static uint32_t max_value() { return 4294967295; }
  static uint32_t min_value() { return 0; }
  static const char* max_value_string() { return "4294967295"; }
  static const char* min_value_string() { return "0"; }
};

#if defined(ENGINE_SUPPORTS_INT64)
class LongLongTypeTest : public NumericTypesTestInterfaceT<int64_t> {
 public:
  int64_t LongLongReturnOperation() override {
    return MockReturnValueOperation();
  }
  void LongLongArgumentOperation(int64_t value) override {
    MockArgumentOperation(value);
  }
  int64_t long_long_property() override { return mock_get_property(); }
  void set_long_long_property(int64_t value) override {
    mock_set_property(value);
  }
  int64_t long_long_clamp_property() override { return mock_get_property(); }
  void set_long_long_clamp_property(int64_t value) override {
    mock_set_property(value);
  }

  static const char* type_string() { return "longLong"; }
  static int64_t max_value() { return 9223372036854775807ll; }
  static int64_t min_value() { return -9223372036854775807ll - 1; }
  // This is what 9223372036854775807 maps to in javascript.
  static const char* max_value_string() { return "9223372036854776000"; }
  static const char* min_value_string() { return "-9223372036854776000"; }
};

class UnsignedLongLongTypeTest : public NumericTypesTestInterfaceT<uint64_t> {
 public:
  uint64_t UnsignedLongLongReturnOperation() override {
    return MockReturnValueOperation();
  }
  void UnsignedLongLongArgumentOperation(uint64_t value) override {
    MockArgumentOperation(value);
  }
  uint64_t unsigned_long_long_property() override {
    return mock_get_property();
  }
  void set_unsigned_long_long_property(uint64_t value) override {
    mock_set_property(value);
  }
  uint64_t unsigned_long_long_clamp_property() override {
    return mock_get_property();
  }
  void set_unsigned_long_long_clamp_property(uint64_t value) override {
    mock_set_property(value);
  }

  static const char* type_string() { return "unsignedLongLong"; }

  static uint64_t max_value() { return 18446744073709551615ull; }
  static uint64_t min_value() { return 0; }
  // This is what the value 18446744073709551615 maps to in javascript.
  static const char* max_value_string() { return "18446744073709552000"; }
  static const char* min_value_string() { return "0"; }
};
#endif  // ENGINE_SUPPORTS_INT64

class DoubleTypeTest : public NumericTypesTestInterfaceT<double> {
 public:
  double DoubleReturnOperation() override { return MockReturnValueOperation(); }
  void DoubleArgumentOperation(double value) override {
    MockArgumentOperation(value);
  }
  double double_property() override { return mock_get_property(); }
  void set_double_property(double value) override { mock_set_property(value); }

  static const char* type_string() { return "double"; }
  static bool is_restricted() { return true; }
  static double positive_infinity() {
    return std::numeric_limits<double>::infinity();
  }
  static double negative_infinity() { return -positive_infinity(); }
};

class UnrestrictedDoubleTypeTest : public NumericTypesTestInterfaceT<double> {
 public:
  double UnrestrictedDoubleReturnOperation() override {
    return MockReturnValueOperation();
  }
  void UnrestrictedDoubleArgumentOperation(double value) override {
    MockArgumentOperation(value);
  }
  double unrestricted_double_property() override { return mock_get_property(); }
  void set_unrestricted_double_property(double value) override {
    mock_set_property(value);
  }

  static const char* type_string() { return "unrestrictedDouble"; }
  static bool is_restricted() { return false; }
  static double positive_infinity() {
    return std::numeric_limits<double>::infinity();
  }
  static double negative_infinity() { return -positive_infinity(); }
};

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // COBALT_BINDINGS_TESTING_NUMERIC_TYPES_TEST_INTERFACE_H_
