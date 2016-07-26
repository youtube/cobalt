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
#include "cobalt/script/mozjs/mozjs_global_object_proxy.h"
#include "cobalt/script/mozjs/mozjs_object_handle.h"
#include "cobalt/script/mozjs/mozjs_user_object_holder.h"
#include "third_party/mozjs/js/src/jsapi.h"
#include "third_party/mozjs/js/src/jsproxy.h"

namespace cobalt {
namespace script {
namespace mozjs {

const char kNotNullableType[] = "Value is null but type is not nullable.";
const char kNotObjectType[] = "Value is not an object.";
const char kDoesNotImplementInterface[] =
    "Value does not implement the interface type.";

// Flags that can be used as a bitmask for special conversion behaviour.
enum ConversionFlags {
  kNoConversionFlags = 0,
  kConversionFlagRestricted = 1 << 0,
  kConversionFlagNullable = 1 << 1,
  kConversionFlagTreatNullAsEmptyString = 1 << 2,
  kConversionFlagTreatUndefinedAsEmptyString = 1 << 3,

  // Valid conversion flags for numeric values.
  kConversionFlagsNumeric = kConversionFlagRestricted,

  // Valid conversion flags for string types.
  kConversionFlagsString = kConversionFlagTreatNullAsEmptyString |
                           kConversionFlagTreatUndefinedAsEmptyString,

  // Valid conversion flags for objects.
  kConversionFlagsObject = kConversionFlagNullable,
};

// bool -> JSValue
inline void ToJSValue(JSContext* context, bool in_boolean,
                      MozjsExceptionState* exception_state,
                      JS::MutableHandleValue out_value) {
  out_value.set(JS::BooleanValue(in_boolean));
}

// JSValue -> bool
inline void FromJSValue(JSContext* context, JS::HandleValue value,
                        int conversion_flags,
                        MozjsExceptionState* exception_state,
                        bool* out_boolean) {
  DCHECK_EQ(conversion_flags, kNoConversionFlags)
      << "No conversion flags supported.";
  DCHECK(out_boolean);
  // ToBoolean implements the ECMAScript ToBoolean operation.
  *out_boolean = JS::ToBoolean(value);
}

// signed integers <= 4 bytes -> JSValue
template <typename T>
inline void ToJSValue(
    JSContext* context, T in_number, MozjsExceptionState* exception_state,
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
    JSContext* context, JS::HandleValue value, int conversion_flags,
    MozjsExceptionState* exception_state, T* out_number,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 std::numeric_limits<T>::is_integer &&
                                 std::numeric_limits<T>::is_signed &&
                                 (sizeof(T) <= 4),
                             T>::type* = NULL) {
  DCHECK_EQ(conversion_flags, kNoConversionFlags)
      << "No conversion flags supported.";
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
    JSContext* context, T in_number, MozjsExceptionState* exception_state,
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
    JSContext* context, JS::HandleValue value, int conversion_flags,
    MozjsExceptionState* exception_state, T* out_number,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 std::numeric_limits<T>::is_integer &&
                                 !std::numeric_limits<T>::is_signed &&
                                 (sizeof(T) <= 4),
                             T>::type* = NULL) {
  DCHECK_EQ(conversion_flags, kNoConversionFlags)
      << "No conversion flags supported.";
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
    JSContext* context, T in_number, MozjsExceptionState* exception_state,
    JS::MutableHandleValue out_value,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 !std::numeric_limits<T>::is_integer,
                             T>::type* = NULL) {
  out_value.set(DOUBLE_TO_JSVAL(in_number));
}

// JSValue -> double
template <typename T>
inline void FromJSValue(
    JSContext* context, JS::HandleValue value, int conversion_flags,
    MozjsExceptionState* exception_state, T* out_number,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 !std::numeric_limits<T>::is_integer,
                             T>::type* = NULL) {
  DCHECK_EQ(conversion_flags & ~kConversionFlagsNumeric, 0)
      << "Unexpected conversion flags found.";
  DCHECK(out_number);
  double double_value;
  if (!JS::ToNumber(context, value, &double_value)) {
    exception_state->SetSimpleException(
        ExceptionState::kError,
        "Cannot convert a JavaScript value to a number.");
    return;
  }

  if (!mozilla::IsFinite(double_value) &&
      (conversion_flags & kConversionFlagRestricted)) {
    exception_state->SetSimpleException(ExceptionState::kTypeError,
                                        "Non-finite floating-point value.");
    return;
  }

  *out_number = double_value;
}

// std::string -> JSValue
inline void ToJSValue(JSContext* context, const std::string& in_string,
                      MozjsExceptionState* exception_state,
                      JS::MutableHandleValue out_value) {
  out_value.set(JS::StringValue(
      JS_NewStringCopyN(context, in_string.c_str(), in_string.length())));
}

// optional<T> -> JSValue
template <typename T>
inline void ToJSValue(JSContext* context, const base::optional<T>& in_optional,
                      MozjsExceptionState* exception_state,
                      JS::MutableHandleValue out_value) {
  if (!in_optional) {
    out_value.setNull();
    return;
  }
  ToJSValue(context, in_optional.value(), exception_state, out_value);
}

// JSValue -> optional<T>
template <typename T>
inline void FromJSValue(JSContext* context, JS::HandleValue value,
                        int conversion_flags,
                        MozjsExceptionState* exception_state,
                        base::optional<T>* out_optional) {
  if (value.isNull()) {
    *out_optional = base::nullopt;
  } else if (value.isUndefined()) {
    *out_optional = base::nullopt;
  } else {
    *out_optional = T();
    FromJSValue(context, value, conversion_flags & ~kConversionFlagNullable,
                exception_state, &(out_optional->value()));
  }
}

// JSValue -> std::string
void FromJSValue(JSContext* context, JS::HandleValue value,
                 int conversion_flags, MozjsExceptionState* exception_state,
                 std::string* out_string);

// JSValue -> optional<T>
template <>
inline void FromJSValue(JSContext* context, JS::HandleValue value,
                        int conversion_flags,
                        MozjsExceptionState* exception_state,
                        base::optional<std::string>* out_optional) {
  if (value.isNull()) {
    *out_optional = base::nullopt;
  } else if (value.isUndefined() &&
             !(conversion_flags & kConversionFlagTreatUndefinedAsEmptyString)) {
    // If TreatUndefinedAs=EmptyString is set, skip the default conversion
    // of undefined to null.
    *out_optional = base::nullopt;
  } else {
    *out_optional = std::string();
    FromJSValue(context, value, conversion_flags & ~kConversionFlagNullable,
                exception_state, &(out_optional->value()));
  }
}

// OpaqueHandle -> JSValue
void ToJSValue(JSContext* context,
               const OpaqueHandleHolder* opaque_handle_holder,
               MozjsExceptionState* exception_state,
               JS::MutableHandleValue out_value);

// JSValue -> OpaqueHandle
void FromJSValue(JSContext* context, JS::HandleValue value,
                 int conversion_flags, MozjsExceptionState* exception_state,
                 MozjsObjectHandle::HolderType* out_holder);

// object -> JSValue
template <class T>
inline void ToJSValue(JSContext* context, const scoped_refptr<T>& in_object,
                      MozjsExceptionState* exception_state,
                      JS::MutableHandleValue out_value) {
  if (!in_object) {
    out_value.setNull();
    return;
  }

  MozjsGlobalObjectProxy* global_object_proxy =
      static_cast<MozjsGlobalObjectProxy*>(JS_GetContextPrivate(context));
  JS::RootedObject object(
      context,
      global_object_proxy->wrapper_factory()->GetWrapperProxy(in_object));
  DCHECK(object);

  out_value.set(OBJECT_TO_JSVAL(object));
}

// JSValue -> object
template <class T>
inline void FromJSValue(JSContext* context, JS::HandleValue value,
                        int conversion_flags,
                        MozjsExceptionState* exception_state,
                        scoped_refptr<T>* out_object) {
  DCHECK_EQ(conversion_flags & ~kConversionFlagsObject, 0)
      << "Unexpected conversion flags found.";

  JS::RootedObject js_object(context);
  if (value.isNull() && !(conversion_flags & kConversionFlagNullable)) {
    exception_state->SetSimpleException(ExceptionState::kTypeError,
                                        kNotNullableType);
    return;
  }

  if (!JS_ValueToObject(context, value, js_object.address())) {
    exception_state->SetSimpleException(
        ExceptionState::kTypeError,
        "Cannot convert a JavaScript value to an object.");
    return;
  }

  DCHECK(js_object);
  if (js::IsProxy(js_object)) {
    JS::RootedObject wrapper(context, js::GetProxyTargetObject(js_object));
    MozjsGlobalObjectProxy* global_object_proxy =
        static_cast<MozjsGlobalObjectProxy*>(JS_GetContextPrivate(context));
    if (global_object_proxy->wrapper_factory()->IsWrapper(wrapper)) {
      WrapperPrivate* wrapper_private =
          WrapperPrivate::GetFromWrapperObject(wrapper);
      *out_object = wrapper_private->wrappable<T>();
      return;
    }
  }
  // This is not a platform object. Return a type error.
  exception_state->SetSimpleException(ExceptionState::kTypeError,
                                      kDoesNotImplementInterface);
}

// TODO: These will be removed once conversion for all types is implemented.
template <typename T>
void ToJSValue(
    JSContext* context, const T& unimplemented,
    MozjsExceptionState* exception_state, JS::MutableHandleValue out_value,
    typename base::enable_if<!std::numeric_limits<T>::is_specialized>::type* =
        NULL) {
  NOTIMPLEMENTED();
}

template <typename T>
void FromJSValue(
    JSContext* context, JS::HandleValue value, int conversion_flags,
    MozjsExceptionState* exception_state, T* out_unimplemented,
    typename base::enable_if<!std::numeric_limits<T>::is_specialized>::type* =
        NULL) {
  NOTIMPLEMENTED();
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_CONVERSION_HELPERS_H_
