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

// Need to include this first, so we can redefine LOG
#include "config.h"
#undef LOG  // Defined by WTF, redefined in chromium.

#include "base/logging.h"
#include "base/optional.h"
#include "cobalt/base/enable_if.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/script_event_listener.h"
#include "cobalt/script/javascriptcore/jsc_callback_function.h"
#include "cobalt/script/javascriptcore/jsc_event_listener_callable.h"
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

// Convert std::string in utf8 encoding to WTFString.
WTF::String ToWTFString(const std::string& utf8_string);

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
  } else {
    ASSERT_GC_OBJECT_INHERITS(js_object, &WrapperBase::s_info);
    WrapperBase* wrapper_base = JSC::jsCast<WrapperBase*>(js_object);
    wrappable = wrapper_base->wrappable().get();
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
    const scoped_refptr<CallbackFunction<T> >& callback_function) {
  if (!callback_function) {
    return JSC::jsNull();
  }
  // Downcast to JSCCallbackFunction<> concrete class.
  JSCCallbackFunction<typename CallbackFunction<T>::Signature>*
      jsc_callback_function = base::polymorphic_downcast<
          JSCCallbackFunction<typename CallbackFunction<T>::Signature>*>(
          callback_function.get());
  // Retrieve the JSFunction* from the JSCObjectOwner.
  JSCObjectOwner* callable = jsc_callback_function->callable();
  JSC::WriteBarrier<JSC::JSObject> callable_object = callable->js_object();
  ASSERT_GC_OBJECT_INHERITS(callable_object.get(), &JSC::JSFunction::s_info);
  return JSC::JSValue(callable_object.get());
}

// Overloads of FromJSValue retrieve the Cobalt value from a JSValue.
//
// JSValue -> bool
inline void FromJSValue(JSC::ExecState* exec_state, JSC::JSValue jsvalue,
                        bool* out_bool) {
  *out_bool = jsvalue.toBoolean(exec_state);
}

// JSValue -> signed integers <= 4 bytes
template <class T>
inline void FromJSValue(
    JSC::ExecState* exec_state, JSC::JSValue jsvalue, T* out_number,
    typename base::enable_if<
        std::numeric_limits<T>::is_integer &&
            std::numeric_limits<T>::is_signed && (sizeof(T) <= 4),
        T>::type* = NULL) {
  int32_t int32_value = jsvalue.toInt32(exec_state);
  *out_number = static_cast<T>(int32_value);
}

// JSValue -> unsigned integers <= 4 bytes
template <class T>
inline void FromJSValue(
    JSC::ExecState* exec_state, JSC::JSValue jsvalue, T* out_number,
    typename base::enable_if<std::numeric_limits<T>::is_integer &&
                                 !std::numeric_limits<T>::is_signed &&
                                 (sizeof(T) <= 4),
                             T>::type* = NULL) {
  uint32_t uint32_value = jsvalue.toUInt32(exec_state);
  *out_number = static_cast<T>(uint32_value);
}

// JSValue -> double
template <class T>
inline void FromJSValue(
    JSC::ExecState* exec_state, JSC::JSValue jsvalue, T* out_number,
    typename base::enable_if<!std::numeric_limits<T>::is_integer, T>::type* =
        NULL) {
  double double_value = jsvalue.toNumber(exec_state);
  // For non-unrestricted doubles/floats, NaN and +/-Infinity should throw a
  // TypeError
  DCHECK(isfinite(double_value))
      << "unrestricted doubles/floats are not yet supported.";
  *out_number = double_value;
}

// JSValue -> std::string
inline void FromJSValue(JSC::ExecState* exec_state, JSC::JSValue jsvalue,
                        std::string* out_string) {
  JSValueToString(exec_state, jsvalue, out_string);
}

// JSValue -> object
template <class T>
inline void FromJSValue(JSC::ExecState* exec_state, JSC::JSValue jsvalue,
                        scoped_refptr<T>* out_object) {
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
                        base::optional<T>* out_optional) {
  if (jsvalue.isNull()) {
    *out_optional = base::nullopt;
  } else {
    *out_optional = T();
    FromJSValue(exec_state, jsvalue, &(out_optional->value()));
  }
}

// JSValue -> CallbackFunction
template <class T>
inline void FromJSValue(JSC::ExecState* exec_state, JSC::JSValue jsvalue,
                        scoped_refptr<CallbackFunction<T> >* out_callback) {
  if (jsvalue.isNull()) {
    // TODO(***REMOVED***): Throw TypeError if callback is not nullable.
    *out_callback = NULL;
    return;
  }

  JSCGlobalObject* global_object =
      JSC::jsCast<JSCGlobalObject*>(exec_state->lexicalGlobalObject());

  // http://www.w3.org/TR/WebIDL/#es-callback-function
  // 1. If V is not a Function object, throw a TypeError
  if (!jsvalue.isFunction()) {
    // TODO(***REMOVED***): Throw TypeError.
    NOTREACHED();
    *out_callback = NULL;
    return;
  }
  JSC::JSFunction* js_function =
      JSC::jsCast<JSC::JSFunction*>(jsvalue.asCell());
  DCHECK(js_function);
  scoped_refptr<JSCObjectOwner> object_owner =
      global_object->RegisterObjectOwner(js_function);

  // JSCCallbackFunction keeps a handle to object_owner. As long as a reference
  // to the JSCCallbackFunction exists, therefore there is a reference to the
  // JSCObjectOwner, and thus the js_function will not be garbage collected.
  *out_callback = new JSCCallbackFunction<
      typename CallbackFunction<T>::Signature>(object_owner);
}

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

// Union type conversion is generated by a pump script.
#include "cobalt/script/javascriptcore/union_type_conversion_impl.h"

#endif  // SCRIPT_JAVASCRIPTCORE_CONVERSION_HELPERS_H_
