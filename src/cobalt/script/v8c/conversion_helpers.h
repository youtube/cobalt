// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_V8C_CONVERSION_HELPERS_H_
#define COBALT_SCRIPT_V8C_CONVERSION_HELPERS_H_

#include <limits>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/optional.h"
#include "base/stringprintf.h"
#include "cobalt/base/compiler.h"
#include "cobalt/base/enable_if.h"
#include "cobalt/base/token.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/v8c/type_traits.h"
#include "cobalt/script/v8c/union_type_conversion_forward.h"
#include "cobalt/script/v8c/v8c_callback_interface_holder.h"
#include "cobalt/script/v8c/v8c_global_environment.h"
#include "cobalt/script/v8c/v8c_user_object_holder.h"
#include "cobalt/script/v8c/v8c_value_handle.h"
#include "cobalt/script/value_handle.h"
#include "nb/memory_scope.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace script {
namespace v8c {

// Flags that can be used as a bitmask for special conversion behaviour.
enum ConversionFlags {
  kNoConversionFlags = 0,
  kConversionFlagRestricted = 1 << 0,
  kConversionFlagNullable = 1 << 1,
  kConversionFlagTreatNullAsEmptyString = 1 << 2,
  kConversionFlagTreatUndefinedAsEmptyString = 1 << 3,
  kConversionFlagClamped = 1 << 4,
  kConversionFlagObjectOnly = 1 << 5,

  // Valid conversion flags for numeric values.
  kConversionFlagsNumeric = kConversionFlagRestricted | kConversionFlagClamped,

  // Valid conversion flags for string types.
  kConversionFlagsString = kConversionFlagTreatNullAsEmptyString |
                           kConversionFlagTreatUndefinedAsEmptyString,

  // Valid conversion flags for objects.
  kConversionFlagsObject = kConversionFlagNullable,

  // Valid conversion flags for ValueHandles.
  kConversionFlagsValueHandle =
      kConversionFlagObjectOnly | kConversionFlagNullable,

  // Valid conversion flags for callback functions.
  kConversionFlagsCallbackFunction = kConversionFlagNullable,

  // Valid conversion flags for callback interfaces.
  kConversionFlagsCallbackInterface = kConversionFlagNullable,
};

// std::string -> JSValue
inline void ToJSValue(v8::Isolate* isolate, const std::string& in_string,
                      v8::Local<v8::Value>* out_value) {
  NOTIMPLEMENTED();
}

// JSValue -> std::string
void FromJSValue(v8::Isolate* isolate, v8::Local<v8::Value> value,
                 int conversion_flags, ExceptionState* exception_state,
                 std::string* out_string);

// std::vector<uint8_t> -> JSValue
// Note that this conversion is specifically for the Web IDL type ByteString.
// Ideally, conversion requests would be explicit in what type they wanted to
// convert to, however it is currently solely based on input type.
inline void ToJSValue(v8::Isolate* isolate, const std::vector<uint8_t>& in_data,
                      v8::Local<v8::Value>* out_value) {
  NOTIMPLEMENTED();
}

// base::Token -> JSValue
inline void ToJSValue(v8::Isolate* isolate, const base::Token& token,
                      v8::Local<v8::Value>* out_value) {
  NOTIMPLEMENTED();
}

// bool -> JSValue
inline void ToJSValue(v8::Isolate* isolate, bool in_boolean,
                      v8::Local<v8::Value>* out_value) {
  NOTIMPLEMENTED();
}

// JSValue -> bool
inline void FromJSValue(v8::Isolate* isolate, v8::Local<v8::Value> value,
                        int conversion_flags, ExceptionState* exception_state,
                        bool* out_boolean) {
  NOTIMPLEMENTED();
}

// signed integers <= 4 bytes -> JSValue
template <typename T>
inline void ToJSValue(
    v8::Isolate* isolate, T in_number, v8::Local<v8::Value>* out_value,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 std::numeric_limits<T>::is_integer &&
                                 std::numeric_limits<T>::is_signed &&
                                 (sizeof(T) <= 4),
                             T>::type* = NULL) {
  NOTIMPLEMENTED();
}

template <typename T>
inline const double UpperBound() {
  return std::numeric_limits<T>::max();
}

template <typename T>
inline const double LowerBound() {
  return std::numeric_limits<T>::min();
}

// The below specializations of UpperBound<T> and LowerBound<T> for 64
// bit integers use the (2^(53) - 1) and similar bounds specified in
// step 1 of ConvertToInt, see:
// https://heycam.github.io/webidl/#abstract-opdef-converttoint
template <>
inline const double UpperBound<int64_t>() {
  const double kInt64UpperBound = static_cast<double>((1ll << 53) - 1);
  return kInt64UpperBound;
}

template <>
inline const double LowerBound<int64_t>() {
  const double kInt64LowerBound = static_cast<double>(-(1ll << 53) + 1);
  return kInt64LowerBound;
}

template <>
inline const double UpperBound<uint64_t>() {
  const double kUInt64UpperBound = static_cast<double>((1ll << 53) - 1);
  return kUInt64UpperBound;
}

template <typename T>
void ClampedValue(v8::Isolate* isolate, v8::Local<v8::Value> value,
                  v8::Local<v8::Value>* clamped_value) {
  NOTIMPLEMENTED();
}

// JSValue -> signed integers <= 4 bytes
template <typename T>
inline void FromJSValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, int conversion_flags,
    ExceptionState* exception_state, T* out_number,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 std::numeric_limits<T>::is_integer &&
                                 std::numeric_limits<T>::is_signed &&
                                 (sizeof(T) <= 4),
                             T>::type* = NULL) {
  NOTIMPLEMENTED();
}

// JSValue -> signed integers > 4 bytes
template <typename T>
inline void FromJSValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, int conversion_flags,
    ExceptionState* exception_state, T* out_number,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 std::numeric_limits<T>::is_integer &&
                                 std::numeric_limits<T>::is_signed &&
                                 (sizeof(T) > 4),
                             T>::type* = NULL) {
  NOTIMPLEMENTED();
}

// signed integers > 4 bytes -> JSValue
template <typename T>
inline void ToJSValue(
    v8::Isolate* isolate, T in_number, v8::Local<v8::Value>* out_value,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 std::numeric_limits<T>::is_integer &&
                                 std::numeric_limits<T>::is_signed &&
                                 (sizeof(T) > 4),
                             T>::type* = NULL) {
  NOTIMPLEMENTED();
}

// unsigned integers <= 4 bytes -> JSValue
template <typename T>
inline void ToJSValue(
    v8::Isolate* isolate, T in_number, v8::Local<v8::Value>* out_value,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 std::numeric_limits<T>::is_integer &&
                                 !std::numeric_limits<T>::is_signed &&
                                 (sizeof(T) <= 4),
                             T>::type* = NULL) {
  NOTIMPLEMENTED();
}

// JSValue -> unsigned integers <= 4 bytes
template <typename T>
inline void FromJSValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, int conversion_flags,
    ExceptionState* exception_state, T* out_number,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 std::numeric_limits<T>::is_integer &&
                                 !std::numeric_limits<T>::is_signed &&
                                 (sizeof(T) <= 4),
                             T>::type* = NULL) {
  NOTIMPLEMENTED();
}

// JSValue -> unsigned integers > 4 bytes
template <typename T>
inline void FromJSValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, int conversion_flags,
    ExceptionState* exception_state, T* out_number,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 std::numeric_limits<T>::is_integer &&
                                 !std::numeric_limits<T>::is_signed &&
                                 (sizeof(T) > 4),
                             T>::type* = NULL) {
  NOTIMPLEMENTED();
}

// unsigned integers > 4 bytes -> JSValue
template <typename T>
inline void ToJSValue(
    v8::Isolate* isolate, T in_number, v8::Local<v8::Value>* out_value,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 std::numeric_limits<T>::is_integer &&
                                 !std::numeric_limits<T>::is_signed &&
                                 (sizeof(T) > 4),
                             T>::type* = NULL) {
  NOTIMPLEMENTED();
}

// double -> JSValue
template <typename T>
inline void ToJSValue(
    v8::Isolate* isolate, T in_number, v8::Local<v8::Value>* out_value,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 !std::numeric_limits<T>::is_integer,
                             T>::type* = NULL) {
  NOTIMPLEMENTED();
}

// JSValue -> double
template <typename T>
inline void FromJSValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, int conversion_flags,
    ExceptionState* exception_state, T* out_number,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 !std::numeric_limits<T>::is_integer,
                             T>::type* = NULL) {
  NOTIMPLEMENTED();
}

// optional<T> -> JSValue
template <typename T>
inline void ToJSValue(v8::Isolate* isolate,
                      const base::optional<T>& in_optional,
                      v8::Local<v8::Value>* out_value) {
  NOTIMPLEMENTED();
}

// JSValue -> optional<T>
template <typename T>
inline void FromJSValue(v8::Isolate* isolate, v8::Local<v8::Value> value,
                        int conversion_flags, ExceptionState* exception_state,
                        base::optional<T>* out_optional) {
  NOTIMPLEMENTED();
}

// JSValue -> optional<std::string>
template <>
inline void FromJSValue(v8::Isolate* isolate, v8::Local<v8::Value> value,
                        int conversion_flags, ExceptionState* exception_state,
                        base::optional<std::string>* out_optional) {
  NOTIMPLEMENTED();
}

// ValueHandle -> JSValue
void ToJSValue(v8::Isolate* isolate,
               const ValueHandleHolder* value_handle_holder,
               v8::Local<v8::Value>* out_value);

// JSValue -> ValueHandle
void FromJSValue(v8::Isolate* isolate, v8::Local<v8::Value> value,
                 int conversion_flags, ExceptionState* exception_state,
                 V8cValueHandleHolder* out_holder);

// object -> JSValue
template <typename T>
inline void ToJSValue(v8::Isolate* isolate, const scoped_refptr<T>& in_object,
                      v8::Local<v8::Value>* out_value) {
  NOTIMPLEMENTED();
}

// raw object pointer -> JSValue
template <typename T>
inline void ToJSValue(v8::Isolate* isolate, T* in_object,
                      v8::Local<v8::Value>* out_value) {
  ToJSValue(isolate, scoped_refptr<T>(in_object), out_value);
}

// JSValue -> object
template <typename T>
inline void FromJSValue(v8::Isolate* isolate, v8::Local<v8::Value> value,
                        int conversion_flags, ExceptionState* exception_state,
                        scoped_refptr<T>* out_object) {
  DCHECK_EQ(conversion_flags & ~kConversionFlagsObject, 0)
      << "Unexpected conversion flags found.";

  if (!value->IsObject()) {
    NOTIMPLEMENTED();
    return;
  }

  // TODO: Check if object implements interface and throw TypeError otherwise.
  v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);
  DCHECK(object->InternalFieldCount() == 1);
  v8::Local<v8::External> external =
      v8::Local<v8::External>::Cast(object->GetInternalField(0));

  *out_object = scoped_refptr<T>(static_cast<T*>(
      (static_cast<WrapperPrivate*>(external->Value()))->wrappable<T>().get()));
}

// CallbackInterface -> JSValue
template <typename T>
inline void ToJSValue(v8::Isolate* isolate,
                      const ScriptValue<T>* callback_interface,
                      v8::Local<v8::Value>* out_value) {
  NOTIMPLEMENTED();
}

// JSValue -> CallbackInterface
template <typename T>
inline void FromJSValue(v8::Isolate* isolate, v8::Local<v8::Value> value,
                        int conversion_flags, ExceptionState* out_exception,
                        V8cCallbackInterfaceHolder<T>* out_callback_interface) {
  NOTIMPLEMENTED();
}

template <typename T>
void ToJSValue(v8::Isolate* isolate, const script::Sequence<T>& sequence,
               v8::Local<v8::Value>* out_value) {
  NOTIMPLEMENTED();
}

template <typename T>
void FromJSValue(v8::Isolate* isolate, v8::Local<v8::Value> value,
                 int conversion_flags, ExceptionState* exception_state,
                 script::Sequence<T>* out_sequence) {
  NOTIMPLEMENTED();
}

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

// Union type conversion is generated by a pump script.
#include "cobalt/script/v8c/union_type_conversion_impl.h"

#endif  // COBALT_SCRIPT_V8C_CONVERSION_HELPERS_H_
