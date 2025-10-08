/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACE_PROCESSOR_TYPES_VARIADIC_H_
#define SRC_TRACE_PROCESSOR_TYPES_VARIADIC_H_

<<<<<<< HEAD
#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>

#include "src/trace_processor/containers/string_pool.h"

namespace perfetto::trace_processor {
=======
#include "src/trace_processor/containers/string_pool.h"

namespace perfetto {
namespace trace_processor {
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

// Variadic type representing value of different possible types.
struct Variadic {
  enum Type : size_t {
    kInt,
    kUint,
    kString,
    kReal,
    kPointer,
    kBool,
    kJson,
    kNull,
    kMaxType = kNull,
  };
<<<<<<< HEAD
  static constexpr const char* const kTypeNames[] = {
      "int", "uint", "string", "real", "pointer", "bool", "json", "null",
  };

  static constexpr Variadic Integer(int64_t int_value) {
    Variadic variadic(Type::kInt);
=======

  static constexpr const char* const kTypeNames[] = {
      "int", "uint", "string", "real", "pointer", "bool", "json", "null"};

  static Variadic Integer(int64_t int_value) {
    Variadic variadic;
    variadic.type = Type::kInt;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    variadic.int_value = int_value;
    return variadic;
  }

  // BEWARE: Unsigned 64-bit integers will be handled as signed integers by
  // SQLite for built-in SQL operators. This variadic type is used to
  // distinguish between int64 and uint64 for correct JSON export of TrackEvent
  // arguments.
<<<<<<< HEAD
  static constexpr Variadic UnsignedInteger(uint64_t uint_value) {
    Variadic variadic(Type::kUint);
=======
  static Variadic UnsignedInteger(uint64_t uint_value) {
    Variadic variadic;
    variadic.type = Type::kUint;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    variadic.uint_value = uint_value;
    return variadic;
  }

<<<<<<< HEAD
  static constexpr Variadic String(StringPool::Id string_id) {
    Variadic variadic(Type::kString);
=======
  static Variadic String(StringPool::Id string_id) {
    Variadic variadic;
    variadic.type = Type::kString;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    variadic.string_value = string_id;
    return variadic;
  }

<<<<<<< HEAD
  static constexpr Variadic Real(double real_value) {
    Variadic variadic(Type::kReal);
=======
  static Variadic Real(double real_value) {
    Variadic variadic;
    variadic.type = Type::kReal;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    variadic.real_value = real_value;
    return variadic;
  }

  // This variadic type is used to distinguish between integers and pointer
  // values for correct JSON export of TrackEvent arguments.
<<<<<<< HEAD
  static constexpr Variadic Pointer(uint64_t pointer_value) {
    Variadic variadic(Type::kPointer);
=======
  static Variadic Pointer(uint64_t pointer_value) {
    Variadic variadic;
    variadic.type = Type::kPointer;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    variadic.pointer_value = pointer_value;
    return variadic;
  }

<<<<<<< HEAD
  static constexpr Variadic Boolean(bool bool_value) {
    Variadic variadic(Type::kBool);
=======
  static Variadic Boolean(bool bool_value) {
    Variadic variadic;
    variadic.type = Type::kBool;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    variadic.bool_value = bool_value;
    return variadic;
  }

  // This variadic type is used to distinguish between regular string and JSON
  // string values for correct JSON export of TrackEvent arguments.
<<<<<<< HEAD
  static constexpr Variadic Json(StringPool::Id json_value) {
    Variadic variadic(Type::kJson);
=======
  static Variadic Json(StringPool::Id json_value) {
    Variadic variadic;
    variadic.type = Type::kJson;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    variadic.json_value = json_value;
    return variadic;
  }

<<<<<<< HEAD
  static constexpr Variadic Null() { return Variadic(Type::kNull); }
=======
  static Variadic Null() {
    Variadic variadic;
    variadic.type = Type::kNull;
    return variadic;
  }
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

  // Used in tests.
  bool operator==(const Variadic& other) const {
    if (type == other.type) {
      switch (type) {
        case kInt:
          return int_value == other.int_value;
        case kUint:
          return uint_value == other.uint_value;
        case kString:
          return string_value == other.string_value;
        case kReal:
          return std::equal_to<double>()(real_value, other.real_value);
        case kPointer:
          return pointer_value == other.pointer_value;
        case kBool:
          return bool_value == other.bool_value;
        case kJson:
          return json_value == other.json_value;
        case kNull:
          return true;
      }
    }
    return false;
  }

  Type type;
  union {
    int64_t int_value;
    uint64_t uint_value;
    StringPool::Id string_value;
    double real_value;
    uint64_t pointer_value;
    bool bool_value;
    StringPool::Id json_value;
  };
<<<<<<< HEAD

 private:
  constexpr explicit Variadic(Type t) : type(t), int_value(0) {}
};

}  // namespace perfetto::trace_processor
=======
};

}  // namespace trace_processor
}  // namespace perfetto
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

#endif  // SRC_TRACE_PROCESSOR_TYPES_VARIADIC_H_
