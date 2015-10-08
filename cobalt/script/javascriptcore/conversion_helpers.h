/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef SCRIPT_JAVASCRIPTCORE_CONVERSION_HELPERS_H_
#define SCRIPT_JAVASCRIPTCORE_CONVERSION_HELPERS_H_

#include <limits>
#include <string>

#include "config.h"

#include "base/logging.h"
#include "base/optional.h"
#include "cobalt/base/enable_if.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/script_event_listener.h"
#include "cobalt/script/javascriptcore/jsc_callback_function.h"
#include "cobalt/script/javascriptcore/jsc_callback_function_holder.h"
#include "cobalt/script/javascriptcore/jsc_event_listener_callable.h"
#include "cobalt/script/javascriptcore/jsc_exception_state.h"
#include "cobalt/script/javascriptcore/jsc_global_object.h"
#include "cobalt/script/javascriptcore/union_type_conversion_forward.h"
#include "cobalt/script/javascriptcore/wrapper_base.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSFunction.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSGlobalData.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSValue.h"
#include "third_party/WebKit/Source/WTF/wtf/text/WTFString.h"


namespace cobalt {
namespace script {
namespace javascriptcore {

// Flags that can be used as a bitmask for special conversion behaviour.
enum ConversionFlags {
  kNoConversionFlags = 0,
  kConversionFlagRestricted = 1 << 0,
  kConversionFlagsNumeric = kConversionFlagRestricted,
};

// Convert std::string in utf8 encoding to WTFString.
WTF::String ToWTFString(const std::string& utf8_string);

// Convert WTFString to std::string in utf8.
std::string FromWTFString(const WTF::String& wtf_string);

// Convert std::string in utf8 encoding to a JSValue representing the string.
JSC::JSValue StringToJSValue(JSC::JSGlobalData* global_data,
                             const std::string& utf8_string);

// Convert a JSValue to a std::string in utf8 encoding.
void JSValueToString(JSC::ExecState* exec_state, JSC::JSValue value,
                     std::string* out_string);

// For a given JSObject* get a pointer to the corresponding Cobalt
// implementation.
template <class T>
inline T* JSObjectToWrappable(JSC::ExecState* exec_state,
                              JSC::JSObject* js_object) {
  if (!js_object) {
    return NULL;
  }
  JSCGlobalObject* global_object =
      JSC::jsCast<JSCGlobalObject*>(exec_state->lexicalGlobalObject());
  Wrappable* wrappable = NULL;
  // If the js_object is in fact the global object, then get the pointer to the
  // global interface from the JSCGlobalObject. Otherwise, it is a regular
  // interface and the JSObject inherits from WrapperBase which holds a
  // reference
  // to the Wrappable.
  if (js_object == global_object) {
    wrappable = global_object->global_interface().get();
  } else if (js_object->isErrorInstance()) {
    wrappable = ExceptionBase::GetWrappable(js_object).get();
  } else {
    wrappable = InterfaceBase::GetWrappable(js_object).get();
  }
  return base::polymorphic_downcast<T*>(wrappable);
}

// Overloads of ToJSValue convert different Cobalt types to JSValue.
//
// bool -> JSValue
inline JSC::JSValue ToJSValue(JSCGlobalObject*, bool in_boolean) {
  return JSC::jsBoolean(in_boolean);
}

// numeric types -> JSValue
template <class T>
inline JSC::JSValue ToJSValue(
    JSCGlobalObject*, T in_number,
    typename base::enable_if<std::numeric_limits<T>::is_specialized>::type* =
        NULL) {
  JSC::JSValue number_value = JSC::jsNumber(in_number);
  DCHECK(number_value.isNumber());
  return number_value;
}

// std::string -> JSValue
inline JSC::JSValue ToJSValue(JSCGlobalObject* global_object,
                              const std::string& in_string) {
  return StringToJSValue(&(global_object->globalData()), in_string);
}

// object -> JSValue
template <class T>
inline JSC::JSValue ToJSValue(JSCGlobalObject* global_object,
                              const scoped_refptr<T>& in_object) {
  if (!in_object) {
    return JSC::jsNull();
  }
  JSC::JSObject* wrapper =
      global_object->wrapper_factory()->GetWrapper(global_object, in_object);
  DCHECK(wrapper);
  return JSC::JSValue(wrapper);
}

// optional<T> -> JSValue
template <class T>
inline JSC::JSValue ToJSValue(JSCGlobalObject* global_object,
                              base::optional<T> in_optional) {
  if (!in_optional) {
    return JSC::jsNull();
  }
  return ToJSValue(global_object, in_optional.value());
}

// CallbackFunction -> JSValue
template <class T>
inline JSC::JSValue ToJSValue(
    JSCGlobalObject* global_object,
    const ScriptObject<CallbackFunction<T> >* callback_function) {
  if (!callback_function) {
    return JSC::jsNull();
  }

  // Downcast to JSCCallbackFunctionHolder, which has public members that
  // we can use to dig in to get the JS object.
  const JSCCallbackFunctionHolder<CallbackFunction<T> >*
      jsc_callback_function_holder = base::polymorphic_downcast<
          const JSCCallbackFunctionHolder<CallbackFunction<T> >*>(
          callback_function);
  // Get the base CallbackFunction<T> class from the JSCCallbackFunctionHolder.
  const CallbackFunction<T>* callback =
      jsc_callback_function_holder->GetScriptObject();
  // Shouldn't be NULL. If the callback was NULL then NULL should have been
  // passed as an argument into this function.
  DCHECK(callback);
  // Downcast to the JSCCallbackFunction type, from which we can get the
  // JSFunction.
  const JSCCallbackFunction<T>* jsc_callback =
      base::polymorphic_downcast<const JSCCallbackFunction<T>*>(callback);
  return JSC::JSValue(jsc_callback->callable());
}

// Overloads of FromJSValue retrieve the Cobalt value from a JSValue.
//
// JSValue -> bool
inline void FromJSValue(JSC::ExecState* exec_state, JSC::JSValue jsvalue,
                        int conversion_flags, ExceptionState* out_exception,
                        bool* out_bool) {
  DCHECK_EQ(conversion_flags, kNoConversionFlags)
      << "No conversion flags supported.";
  *out_bool = jsvalue.toBoolean(exec_state);
}

// JSValue -> signed integers <= 4 bytes
template <class T>
inline void FromJSValue(
    JSC::ExecState* exec_state, JSC::JSValue jsvalue, int conversion_flags,
    ExceptionState* out_exception, T* out_number,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 std::numeric_limits<T>::is_integer &&
                                 std::numeric_limits<T>::is_signed &&
                                 (sizeof(T) <= 4),
                             T>::type* = NULL) {
  DCHECK_EQ(conversion_flags, kNoConversionFlags)
      << "No conversion flags supported.";
  int32_t int32_value = jsvalue.toInt32(exec_state);
  *out_number = static_cast<T>(int32_value);
}

// JSValue -> unsigned integers <= 4 bytes
template <class T>
inline void FromJSValue(
    JSC::ExecState* exec_state, JSC::JSValue jsvalue, int conversion_flags,
    ExceptionState* out_exception, T* out_number,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 std::numeric_limits<T>::is_integer &&
                                 !std::numeric_limits<T>::is_signed &&
                                 (sizeof(T) <= 4),
                             T>::type* = NULL) {
  DCHECK_EQ(conversion_flags, kNoConversionFlags)
      << "No conversion flags supported.";
  uint32_t uint32_value = jsvalue.toUInt32(exec_state);
  *out_number = static_cast<T>(uint32_value);
}

// JSValue -> double
template <class T>
inline void FromJSValue(
    JSC::ExecState* exec_state, JSC::JSValue jsvalue, int conversion_flags,
    ExceptionState* out_exception, T* out_number,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 !std::numeric_limits<T>::is_integer,
                             T>::type* = NULL) {
  DCHECK_EQ(conversion_flags & ~kConversionFlagsNumeric, 0)
      << "Unexpected conversion flags found.";
  double double_value = jsvalue.toNumber(exec_state);
  if (!isfinite(double_value) &&
      (conversion_flags & kConversionFlagRestricted)) {
    out_exception->SetSimpleException(ExceptionState::kTypeError,
                                      "Non-finite floating-point value.");
    return;
  }
  *out_number = double_value;
}

// JSValue -> std::string
inline void FromJSValue(JSC::ExecState* exec_state, JSC::JSValue jsvalue,
                        int conversion_flags, ExceptionState* out_exception,
                        std::string* out_string) {
  DCHECK_EQ(conversion_flags, kNoConversionFlags)
      << "No conversion flags supported.";
  JSValueToString(exec_state, jsvalue, out_string);
}

// JSValue -> object
template <class T>
inline void FromJSValue(JSC::ExecState* exec_state, JSC::JSValue jsvalue,
                        int conversion_flags, ExceptionState* out_exception,
                        scoped_refptr<T>* out_object) {
  DCHECK_EQ(conversion_flags, kNoConversionFlags)
      << "No conversion flags supported.";
  if (jsvalue.isNull()) {
    *out_object = NULL;
    return;
  }
  JSC::JSObject* js_object = jsvalue.toObject(exec_state);
  *out_object = JSObjectToWrappable<T>(exec_state, js_object);
}

// JSValue -> optional<T>
template <class T>
inline void FromJSValue(JSC::ExecState* exec_state, JSC::JSValue jsvalue,
                        int conversion_flags, ExceptionState* out_exception,
                        base::optional<T>* out_optional) {
  DCHECK_EQ(conversion_flags, kNoConversionFlags)
      << "No conversion flags supported.";
  if (jsvalue.isNull()) {
    *out_optional = base::nullopt;
  } else {
    *out_optional = T();
    FromJSValue(exec_state, jsvalue, conversion_flags, out_exception,
                &(out_optional->value()));
  }
}

// JSValue -> CallbackFunction
template <class T>
inline void FromJSValue(JSC::ExecState* exec_state, JSC::JSValue jsvalue,
                        int conversion_flags, ExceptionState* out_exception,
                        JSCCallbackFunctionHolder<T>* out_callback) {
  DCHECK_EQ(conversion_flags, kNoConversionFlags)
      << "No conversion flags supported.";
  if (jsvalue.isNull()) {
    // TODO(***REMOVED***): Throw TypeError if callback is not nullable.
    return;
  }

  JSCGlobalObject* global_object =
      JSC::jsCast<JSCGlobalObject*>(exec_state->lexicalGlobalObject());

  // http://www.w3.org/TR/WebIDL/#es-callback-function
  // 1. If V is not a Function object, throw a TypeError
  if (!jsvalue.isFunction()) {
    // TODO(***REMOVED***): Throw TypeError.
    NOTREACHED();
    return;
  }

  JSC::JSFunction* js_function =
      JSC::jsCast<JSC::JSFunction*>(jsvalue.asCell());
  DCHECK(js_function);
  JSCCallbackFunction<typename T::Signature> callback_function(js_function);
  *out_callback = JSCCallbackFunctionHolder<T>(
      callback_function, global_object->script_object_registry());
}

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

// Union type conversion is generated by a pump script.
#include "cobalt/script/javascriptcore/union_type_conversion_impl.h"

#endif  // SCRIPT_JAVASCRIPTCORE_CONVERSION_HELPERS_H_
