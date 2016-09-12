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
#include "base/stringprintf.h"
#include "cobalt/base/enable_if.h"
#include "cobalt/base/token.h"
#include "cobalt/script/mozjs/mozjs_callback_interface_holder.h"
#include "cobalt/script/mozjs/mozjs_exception_state.h"
#include "cobalt/script/mozjs/mozjs_global_object_proxy.h"
#include "cobalt/script/mozjs/mozjs_object_handle.h"
#include "cobalt/script/mozjs/mozjs_user_object_holder.h"
#include "cobalt/script/mozjs/type_traits.h"
#include "cobalt/script/mozjs/union_type_conversion_forward.h"
#include "third_party/mozjs/js/src/jsapi.h"
#include "third_party/mozjs/js/src/jsproxy.h"
#include "third_party/mozjs/js/src/jsstr.h"

namespace cobalt {
namespace script {
namespace mozjs {

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

  // Valid conversion flags for callback functions.
  kConversionFlagsCallbackFunction = kConversionFlagNullable,

  // Valid conversion flags for callback interfaces.
  kConversionFlagsCallbackInterface = kConversionFlagNullable,
};

// std::string -> JSValue
inline void ToJSValue(JSContext* context, const std::string& in_string,
                      JS::MutableHandleValue out_value) {
  size_t length = in_string.length();
  jschar* inflated_buffer =
      js::InflateUTF8String(context, in_string.c_str(), &length);
  DCHECK(inflated_buffer);

  if (!inflated_buffer) {
    LOG(ERROR) << "Failed to inflate UTF8 string.";
    out_value.setNull();
    return;
  }

  JS::RootedString rooted_string(
      context, JS_NewUCStringCopyN(context, inflated_buffer, length));
  js_free(inflated_buffer);

  out_value.set(JS::StringValue(rooted_string));
}

// JSValue -> std::string
void FromJSValue(JSContext* context, JS::HandleValue value,
                 int conversion_flags, ExceptionState* exception_state,
                 std::string* out_string);

// base::Token -> JSValue
inline void ToJSValue(JSContext* context, const base::Token& token,
                      JS::MutableHandleValue out_value) {
  ToJSValue(context, std::string(token.c_str()), out_value);
}

// bool -> JSValue
inline void ToJSValue(JSContext* context, bool in_boolean,
                      JS::MutableHandleValue out_value) {
  out_value.set(JS::BooleanValue(in_boolean));
}

// JSValue -> bool
inline void FromJSValue(JSContext* context, JS::HandleValue value,
                        int conversion_flags, ExceptionState* exception_state,
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
    JSContext* context, T in_number, JS::MutableHandleValue out_value,
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
    ExceptionState* exception_state, T* out_number,
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

// JSValue -> signed integers > 4 bytes
template <typename T>
inline void FromJSValue(
    JSContext* context, JS::HandleValue value, int conversion_flags,
    ExceptionState* exception_state, T* out_number,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 std::numeric_limits<T>::is_integer &&
                                 std::numeric_limits<T>::is_signed &&
                                 (sizeof(T) > 4),
                             T>::type* = NULL) {
  double to_number;
  JS::ToNumber(context, value, &to_number);

  std::string value_str;
  FromJSValue(context, value, conversion_flags, exception_state, &value_str);
  DCHECK_EQ(conversion_flags, kNoConversionFlags)
      << "No conversion flags supported.";
  DCHECK(out_number);
  int64_t out;
  // This produces an IDL long long.
  JSBool success = JS_ValueToInt64(context, value, &out);
  DCHECK(success);
  if (!success) {
    exception_state->SetSimpleException(kNotInt64Type);
    return;
  }
  *out_number = static_cast<T>(out);
}

// signed integers > 4 bytes -> JSValue
template <typename T>
inline void ToJSValue(
    JSContext* context, T in_number, JS::MutableHandleValue out_value,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 std::numeric_limits<T>::is_integer &&
                                 std::numeric_limits<T>::is_signed &&
                                 (sizeof(T) > 4),
                             T>::type* = NULL) {
  out_value.set(JS_NumberValue(in_number));
}

// unsigned integers <= 4 bytes -> JSValue
template <typename T>
inline void ToJSValue(
    JSContext* context, T in_number, JS::MutableHandleValue out_value,
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
    ExceptionState* exception_state, T* out_number,
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

// JSValue -> unsigned integers > 4 bytes
template <typename T>
inline void FromJSValue(
    JSContext* context, JS::HandleValue value, int conversion_flags,
    ExceptionState* exception_state, T* out_number,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 std::numeric_limits<T>::is_integer &&
                                 !std::numeric_limits<T>::is_signed &&
                                 (sizeof(T) > 4),
                             T>::type* = NULL) {
  DCHECK_EQ(conversion_flags, kNoConversionFlags)
      << "No conversion flags supported.";
  DCHECK(out_number);

  uint64_t out;
  // This produces and IDL unsigned long long.
  JSBool success = JS_ValueToUint64(context, value, &out);
  DCHECK(success);
  if (!success) {
    exception_state->SetSimpleException(kNotUint64Type);
    return;
  }
  *out_number = static_cast<T>(out);
}

// unsigned integers > 4 bytes -> JSValue
template <typename T>
inline void ToJSValue(
    JSContext* context, T in_number, JS::MutableHandleValue out_value,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 std::numeric_limits<T>::is_integer &&
                                 !std::numeric_limits<T>::is_signed &&
                                 (sizeof(T) > 4),
                             T>::type* = NULL) {
  out_value.set(JS_NumberValue(in_number));
}

// double -> JSValue
template <typename T>
inline void ToJSValue(
    JSContext* context, T in_number, JS::MutableHandleValue out_value,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 !std::numeric_limits<T>::is_integer,
                             T>::type* = NULL) {
  out_value.set(DOUBLE_TO_JSVAL(in_number));
}

// JSValue -> double
template <typename T>
inline void FromJSValue(
    JSContext* context, JS::HandleValue value, int conversion_flags,
    ExceptionState* exception_state, T* out_number,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 !std::numeric_limits<T>::is_integer,
                             T>::type* = NULL) {
  DCHECK_EQ(conversion_flags & ~kConversionFlagsNumeric, 0)
      << "Unexpected conversion flags found.";
  DCHECK(out_number);
  double double_value;
  if (!JS::ToNumber(context, value, &double_value)) {
    exception_state->SetSimpleException(kNotNumberType);
    return;
  }

  if (!mozilla::IsFinite(double_value) &&
      (conversion_flags & kConversionFlagRestricted)) {
    exception_state->SetSimpleException(kNotFinite);
    return;
  }

  *out_number = double_value;
}

// optional<T> -> JSValue
template <typename T>
inline void ToJSValue(JSContext* context, const base::optional<T>& in_optional,
                      JS::MutableHandleValue out_value) {
  if (!in_optional) {
    out_value.setNull();
    return;
  }
  ToJSValue(context, in_optional.value(), out_value);
}

// JSValue -> optional<T>
template <typename T>
inline void FromJSValue(JSContext* context, JS::HandleValue value,
                        int conversion_flags, ExceptionState* exception_state,
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

// JSValue -> optional<std::string>
template <>
inline void FromJSValue(JSContext* context, JS::HandleValue value,
                        int conversion_flags, ExceptionState* exception_state,
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
               JS::MutableHandleValue out_value);

// JSValue -> OpaqueHandle
void FromJSValue(JSContext* context, JS::HandleValue value,
                 int conversion_flags, ExceptionState* exception_state,
                 MozjsObjectHandleHolder* out_holder);

// object -> JSValue
template <typename T>
inline void ToJSValue(JSContext* context, const scoped_refptr<T>& in_object,
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
template <typename T>
inline void FromJSValue(JSContext* context, JS::HandleValue value,
                        int conversion_flags, ExceptionState* exception_state,
                        scoped_refptr<T>* out_object) {
  DCHECK_EQ(conversion_flags & ~kConversionFlagsObject, 0)
      << "Unexpected conversion flags found.";
  JS::RootedObject js_object(context);
  if (value.isNull() || value.isUndefined()) {
    if (!(conversion_flags & kConversionFlagNullable)) {
      exception_state->SetSimpleException(kNotNullableType);
    }
    return;
  }
  if (!JS_ValueToObject(context, value, js_object.address())) {
    exception_state->SetSimpleException(kNotObjectType);
    return;
  }
  DCHECK(js_object);
  if (js::IsProxy(js_object)) {
    JS::RootedObject wrapper(context, js::GetProxyTargetObject(js_object));
    MozjsGlobalObjectProxy* global_object_proxy =
        static_cast<MozjsGlobalObjectProxy*>(JS_GetContextPrivate(context));
    const WrapperFactory* wrapper_factory =
        global_object_proxy->wrapper_factory();
    if (wrapper_factory->IsWrapper(wrapper)) {
      bool object_implements_interface =
          wrapper_factory->DoesObjectImplementInterface(js_object,
                                                        base::GetTypeId<T>());
      if (!object_implements_interface) {
        exception_state->SetSimpleException(kDoesNotImplementInterface);
        return;
      }
      WrapperPrivate* wrapper_private =
          WrapperPrivate::GetFromWrapperObject(wrapper);
      *out_object = wrapper_private->wrappable<T>();
      return;
    }
  }
  // This is not a platform object. Return a type error.
  exception_state->SetSimpleException(kDoesNotImplementInterface);
}

// CallbackInterface -> JSValue
template <typename T>
inline void ToJSValue(JSContext* context,
                      const ScriptObject<T>* callback_interface,
                      JS::MutableHandleValue out_value) {
  if (!callback_interface) {
    out_value.set(JS::NullValue());
    return;
  }
  typedef typename CallbackInterfaceTraits<T>::MozjsCallbackInterfaceClass
      MozjsCallbackInterfaceClass;
  // Downcast to MozjsUserObjectHolder<T> so we can get the underlying JSObject.
  typedef MozjsUserObjectHolder<MozjsCallbackInterfaceClass>
      MozjsUserObjectHolderClass;
  const MozjsUserObjectHolderClass* user_object_holder =
      base::polymorphic_downcast<const MozjsUserObjectHolderClass*>(
          callback_interface);

  // Shouldn't be NULL. If the callback was NULL then NULL should have been
  // passed as an argument into this function.
  // Downcast to the corresponding MozjsCallbackInterface type, from which we
  // can get the implementing object.
  const MozjsCallbackInterfaceClass* mozjs_callback_interface =
      base::polymorphic_downcast<const MozjsCallbackInterfaceClass*>(
          user_object_holder->GetScriptObject());
  DCHECK(mozjs_callback_interface);
  out_value.set(OBJECT_TO_JSVAL(mozjs_callback_interface->handle()));
}

// JSValue -> CallbackInterface
template <typename T>
inline void FromJSValue(
    JSContext* context, JS::HandleValue value, int conversion_flags,
    ExceptionState* out_exception,
    MozjsCallbackInterfaceHolder<T>* out_callback_interface) {
  typedef T MozjsCallbackInterfaceClass;
  DCHECK_EQ(conversion_flags & ~kConversionFlagsCallbackFunction, 0)
      << "No conversion flags supported.";
  if (value.isNull()) {
    if (!(conversion_flags & kConversionFlagNullable)) {
      out_exception->SetSimpleException(kNotNullableType);
    }
    // If it is a nullable type, just return.
    return;
  }

  // https://www.w3.org/TR/WebIDL/#es-user-objects
  // Any user object can be considered to implement a user interface. Actually
  // checking if the correct properties exist will happen when the operation
  // on the callback interface is run.
  if (!value.isObject()) {
    out_exception->SetSimpleException(kNotObjectType);
    return;
  }

  MozjsGlobalObjectProxy* global_object_proxy =
      static_cast<MozjsGlobalObjectProxy*>(JS_GetContextPrivate(context));

  JS::RootedObject implementing_object(context, JSVAL_TO_OBJECT(value));
  DCHECK(implementing_object);
  *out_callback_interface = MozjsCallbackInterfaceHolder<T>(
      implementing_object, context, global_object_proxy->wrapper_factory());
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

// Union type conversion is generated by a pump script.
#include "cobalt/script/mozjs/union_type_conversion_impl.h"

#endif  // COBALT_SCRIPT_MOZJS_CONVERSION_HELPERS_H_
