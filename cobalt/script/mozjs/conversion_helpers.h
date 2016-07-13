/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_SCRIPT_MOZJS_CONVERSION_HELPERS_H_
#define COBALT_SCRIPT_MOZJS_CONVERSION_HELPERS_H_

#include <limits>
#include <string>

#include "base/logging.h"
#include "base/optional.h"
#include "cobalt/base/enable_if.h"
#include "cobalt/script/mozjs/mozjs_exception_state.h"
#include "third_party/mozjs/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

// bool -> JSValue
inline void ToJSValue(bool in_boolean, MozjsExceptionState* exception_state,
                      JS::MutableHandleValue out_value) {
  out_value.set(JS::BooleanValue(in_boolean));
}

// JSValue -> bool
inline void FromJSValue(JSContext* context, JS::HandleValue value,
                        MozjsExceptionState* exception_state,
                        bool* out_boolean) {
  DCHECK(out_boolean);
  // ToBoolean implements the ECMAScript ToBoolean operation.
  *out_boolean = JS::ToBoolean(value);
}

// signed integers <= 4 bytes -> JSValue
template <typename T>
inline void ToJSValue(
    T in_number, MozjsExceptionState* exception_state,
    JS::MutableHandleValue out_value,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 std::numeric_limits<T>::is_integer &&
                                 std::numeric_limits<T>::is_signed &&
                                 (sizeof(T) <= 4),
                             T>::type* = NULL) {
  out_value.set(INT_TO_JSVAL(in_number));
}

// JSValue -> signed integers <= 4 bytes
template <typename T>
inline void FromJSValue(
    JSContext* context, JS::HandleValue value,
    MozjsExceptionState* exception_state, T* out_number,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 std::numeric_limits<T>::is_integer &&
                                 std::numeric_limits<T>::is_signed &&
                                 (sizeof(T) <= 4),
                             T>::type* = NULL) {
  DCHECK(out_number);

  int32_t out;
  // Convert a JavaScript value to an integer type as specified by the
  // ECMAScript standard.
  JSBool success = JS_ValueToECMAInt32(context, value, &out);
  DCHECK(success);

  *out_number = static_cast<T>(out);
}

// unsigned integers <= 4 bytes -> JSValue
template <typename T>
inline void ToJSValue(
    T in_number, MozjsExceptionState* exception_state,
    JS::MutableHandleValue out_value,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 std::numeric_limits<T>::is_integer &&
                                 !std::numeric_limits<T>::is_signed &&
                                 (sizeof(T) <= 4),
                             T>::type* = NULL) {
  out_value.set(UINT_TO_JSVAL(in_number));
}

// JSValue -> unsigned integers <= 4 bytes
template <typename T>
inline void FromJSValue(
    JSContext* context, JS::HandleValue value,
    MozjsExceptionState* exception_state, T* out_number,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 std::numeric_limits<T>::is_integer &&
                                 !std::numeric_limits<T>::is_signed &&
                                 (sizeof(T) <= 4),
                             T>::type* = NULL) {
  DCHECK(out_number);

  uint32_t out;
  // Convert a JavaScript value to an integer type as specified by the
  // ECMAScript standard.
  JSBool success = JS_ValueToECMAUint32(context, value, &out);
  DCHECK(success);

  *out_number = static_cast<T>(out);
}

// double -> JSValue
template <typename T>
inline void ToJSValue(
    T in_number, MozjsExceptionState* exception_state,
    JS::MutableHandleValue out_value,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 !std::numeric_limits<T>::is_integer,
                             T>::type* = NULL) {
  out_value.set(DOUBLE_TO_JSVAL(in_number));
}

// JSValue -> double
template <typename T>
inline void FromJSValue(
    JSContext* context, JS::HandleValue value,
    MozjsExceptionState* exception_state, T* out_number,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 !std::numeric_limits<T>::is_integer,
                             T>::type* = NULL) {
  DCHECK(out_number);
  JS::ToNumber(context, value, out_number);
}

// TODO: These will be removed once conversion for all types is implemented.
template <typename T>
void ToJSValue(
    const T& unimplemented, MozjsExceptionState* exception_state,
    JS::MutableHandleValue out_value,
    typename base::enable_if<!std::numeric_limits<T>::is_specialized>::type* =
        NULL) {
  NOTIMPLEMENTED();
}

template <typename T>
void FromJSValue(
    JSContext* context, JS::HandleValue value,
    MozjsExceptionState* exception_state, T* out_unimplemented,
    typename base::enable_if<!std::numeric_limits<T>::is_specialized>::type* =
        NULL) {
  NOTIMPLEMENTED();
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_CONVERSION_HELPERS_H_
