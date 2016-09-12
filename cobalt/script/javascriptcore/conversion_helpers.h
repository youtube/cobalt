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

#ifndef COBALT_SCRIPT_JAVASCRIPTCORE_CONVERSION_HELPERS_H_
#define COBALT_SCRIPT_JAVASCRIPTCORE_CONVERSION_HELPERS_H_

#include <limits>
#include <string>

#include "base/logging.h"
#include "base/optional.h"
#include "cobalt/base/enable_if.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/base/token.h"
#include "cobalt/script/javascriptcore/jsc_callback_function.h"
#include "cobalt/script/javascriptcore/jsc_callback_function_holder.h"
#include "cobalt/script/javascriptcore/jsc_callback_interface.h"
#include "cobalt/script/javascriptcore/jsc_callback_interface_holder.h"
#include "cobalt/script/javascriptcore/jsc_exception_state.h"
#include "cobalt/script/javascriptcore/jsc_global_object.h"
#include "cobalt/script/javascriptcore/jsc_object_handle_holder.h"
#include "cobalt/script/javascriptcore/union_type_conversion_forward.h"
#include "cobalt/script/javascriptcore/wrapper_base.h"
#include "third_party/WebKit/Source/JavaScriptCore/config.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/Error.h"
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
  kConversionFlagNullable = 1 << 1,
  kConversionFlagTreatNullAsEmptyString = 1 << 2,
  kConversionFlagTreatUndefinedAsEmptyString = 1 << 3,

  // Valid conversion flags for numeric values.
  kConversionFlagsNumeric = kConversionFlagRestricted,

  // Valid conversion flags for string types.
  kConversionFlagsString = kConversionFlagTreatNullAsEmptyString |
                           kConversionFlagTreatUndefinedAsEmptyString,

  // Valid conversion flags for callback functions.
  kConversionFlagsCallbackFunction = kConversionFlagNullable,

  // Valid conversion flags for callback interfaces.
  kConversionFlagsCallbackInterface = kConversionFlagNullable,

  // Valid conversion flags for objects.
  kConversionFlagsObject = kConversionFlagNullable,
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

// Convert base::Token in utf8 encoding to a JSValue representing the string.
JSC::JSValue TokenToJSValue(JSC::JSGlobalData* global_data,
                            base::Token utf8_string);

// For a given JSObject* get a pointer to the corresponding Cobalt
// implementation.
template <class T>
inline T* JSObjectToWrappable(JSC::ExecState* exec_state,
                              JSC::JSObject* js_object,
                              ExceptionState* out_exception) {
  // Assumes we've already checked that null values are acceptable.
  if (!js_object) {
    return NULL;
  }

  JSCGlobalObject* global_object =
      JSC::jsCast<JSCGlobalObject*>(exec_state->lexicalGlobalObject());
  Wrappable* wrappable = NULL;
  const JSC::ClassInfo* class_info = NULL;
  if (js_object == global_object) {
    // This is the global object, so get the pointer to the global interface
    // from the JSCGlobalObject.
    wrappable = global_object->global_interface().get();
    class_info = JSCGlobalObject::s_classinfo();
  } else if (global_object->wrapper_factory()->IsWrapper(js_object)) {
    // This is a wrapper object, so get the Wrappable from the appropriate
    // base class.
    if (js_object->isErrorInstance()) {
      wrappable = ExceptionBase::GetWrappable(js_object).get();
    } else {
      wrappable = InterfaceBase::GetWrappable(js_object).get();
    }
    class_info =
        global_object->wrapper_factory()->GetClassInfo(base::GetTypeId<T>());
  } else {
    // This is not a platform object. Return a type error.
    out_exception->SetSimpleException(kDoesNotImplementInterface);
    return NULL;
  }

  // Check that the js_object is the expected class.
  if (js_object->inherits(class_info)) {
    return base::polymorphic_downcast<T*>(wrappable);
  } else {
    out_exception->SetSimpleException(kDoesNotImplementInterface);
    return NULL;
  }
}

// GetWrappableOrSetException functions will set an exception if the value is
// not a Wrapper object that corresponds to the Wrappable type T.
// Additionally, an exception will be set if object is NULL.
template <class T>
T* GetWrappableOrSetException(JSC::ExecState* exec_state,
                              JSC::JSObject* object) {
  if (!object) {
    JSC::throwTypeError(exec_state);
    return NULL;
  }
  JSCGlobalObject* global_object =
      JSC::jsCast<JSCGlobalObject*>(exec_state->lexicalGlobalObject());
  JSCExceptionState exception_state(global_object);
  T* impl = JSObjectToWrappable<T>(exec_state, object, &exception_state);
  if (exception_state.is_exception_set()) {
    JSC::throwError(exec_state, exception_state.exception_object());
    return NULL;
  }
  return impl;
}

template <class T>
T* GetWrappableOrSetException(JSC::ExecState* exec_state, JSC::JSCell* cell) {
  // getObject() returns NULL if the cell is not an object.
  JSC::JSObject* object = cell ? cell->getObject() : NULL;
  return GetWrappableOrSetException<T>(exec_state, object);
}

template <class T>
T* GetWrappableOrSetException(JSC::ExecState* exec_state, JSC::JSValue value) {
  // getObject() returns NULL if the value is not an object.
  JSC::JSObject* object = value.getObject();
  return GetWrappableOrSetException<T>(exec_state, object);
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

// base::Token -> JSValue
inline JSC::JSValue ToJSValue(JSCGlobalObject* global_object,
                              base::Token in_token) {
  return TokenToJSValue(&(global_object->globalData()), in_token);
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

// CallbackInterface -> JSValue
template <class T>
inline JSC::JSValue ToJSValue(JSCGlobalObject* global_object,
                              const ScriptObject<T>* callback_interface) {
  if (!callback_interface) {
    return JSC::jsNull();
  }
  typedef typename CallbackInterfaceTraits<T>::JSCCallbackInterfaceClass
      JSCCallbackInterfaceClass;
  // Downcast to JSCCallbackInterfaceHolder
  const JSCCallbackInterfaceHolder<T>* jsc_callback_interface_holder =
      base::polymorphic_downcast<const JSCCallbackInterfaceHolder<T>*>(
          callback_interface);

  // Shouldn't be NULL. If the callback was NULL then NULL should have been
  // passed as an argument into this function.
  // Downcast to the corresponding JSCCallbackInterface type, from which we can
  // get the implementing object.
  const JSCCallbackInterfaceClass* jsc_callback_interface =
      base::polymorphic_downcast<const JSCCallbackInterfaceClass*>(
          jsc_callback_interface_holder->GetScriptObject());
  DCHECK(jsc_callback_interface);
  // Return the user object implementing this interface.
  return JSC::JSValue(jsc_callback_interface->implementing_object());
}

// OpaqueHandle -> JSValue
inline JSC::JSValue ToJSValue(JSCGlobalObject* global_object,
                              const OpaqueHandleHolder* opaque_handle_holder) {
  if (!opaque_handle_holder) {
    return JSC::jsNull();
  }

  // Downcast to JSCObjectHandleHolder, which has public members that
  // we can use to dig in to get the JS object.
  const JSCObjectHandleHolder* jsc_object_handle_holder =
      base::polymorphic_downcast<const JSCObjectHandleHolder*>(
          opaque_handle_holder);

  JSC::JSObject* js_object = jsc_object_handle_holder->js_object();
  if (js_object) {
    return JSC::JSValue(js_object);
  } else {
    return JSC::jsNull();
  }
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

// JSValue -> signed integers > 4 bytes
template <class T>
inline void FromJSValue(
    JSC::ExecState* exec_state, JSC::JSValue jsvalue, int conversion_flags,
    ExceptionState* out_exception, T* out_number,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 std::numeric_limits<T>::is_integer &&
                                 std::numeric_limits<T>::is_signed &&
                                 (sizeof(T) > 4),
                             T>::type* = NULL) {
  NOTIMPLEMENTED();
}

// JSValue -> unsigned integers > 4 bytes
template <class T>
inline void FromJSValue(
    JSC::ExecState* exec_state, JSC::JSValue jsvalue, int conversion_flags,
    ExceptionState* out_exception, T* out_number,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 std::numeric_limits<T>::is_integer &&
                                 !std::numeric_limits<T>::is_signed &&
                                 (sizeof(T) > 4),
                             T>::type* = NULL) {
  NOTIMPLEMENTED();
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
    out_exception->SetSimpleException(kNotFinite);
    return;
  }
  *out_number = double_value;
}

// JSValue -> std::string
inline void FromJSValue(JSC::ExecState* exec_state, JSC::JSValue jsvalue,
                        int conversion_flags, ExceptionState* out_exception,
                        std::string* out_string) {
  DCHECK_EQ(conversion_flags & ~kConversionFlagsString, 0)
      << "Unexpected conversion flags found: ";
  if (jsvalue.isNull() &&
      conversion_flags & kConversionFlagTreatNullAsEmptyString) {
    *out_string = "";
  } else if (jsvalue.isUndefined() &&
             conversion_flags & kConversionFlagTreatUndefinedAsEmptyString) {
    *out_string = "";
  } else {
    JSValueToString(exec_state, jsvalue, out_string);
  }
}

// JSValue -> object
template <class T>
inline void FromJSValue(JSC::ExecState* exec_state, JSC::JSValue jsvalue,
                        int conversion_flags, ExceptionState* out_exception,
                        scoped_refptr<T>* out_object) {
  DCHECK_EQ(conversion_flags & ~kConversionFlagsObject, 0)
      << "Unexpected conversion flags found.";

  JSC::JSObject* js_object = NULL;
  if (jsvalue.isNull()) {
    if (!(conversion_flags & kConversionFlagNullable)) {
      out_exception->SetSimpleException(kNotNullableType);
      return;
    }
  } else {
    // Returns NULL if jsvalue is not an object.
    js_object = jsvalue.getObject();
    if (!js_object) {
      out_exception->SetSimpleException(kNotObjectType);
      return;
    }
  }
  *out_object = JSObjectToWrappable<T>(exec_state, js_object, out_exception);
}

// JSValue -> optional<T>
template <class T>
inline void FromJSValue(JSC::ExecState* exec_state, JSC::JSValue jsvalue,
                        int conversion_flags, ExceptionState* out_exception,
                        base::optional<T>* out_optional) {
  if (jsvalue.isNull()) {
    *out_optional = base::nullopt;
  } else if (jsvalue.isUndefined()) {
    *out_optional = base::nullopt;
  } else {
    *out_optional = T();
    FromJSValue(exec_state, jsvalue,
                conversion_flags & ~kConversionFlagNullable, out_exception,
                &(out_optional->value()));
  }
}

// JSValue -> optional<T>
template <>
inline void FromJSValue(JSC::ExecState* exec_state, JSC::JSValue jsvalue,
                        int conversion_flags, ExceptionState* out_exception,
                        base::optional<std::string>* out_optional) {
  if (jsvalue.isNull()) {
    *out_optional = base::nullopt;
  } else if (jsvalue.isUndefined() &&
             !(conversion_flags & kConversionFlagTreatUndefinedAsEmptyString)) {
    // If TreatUndefinedAs=EmptyString is set, skip the default conversion
    // of undefined to null.
    *out_optional = base::nullopt;
  } else {
    *out_optional = std::string();
    FromJSValue(exec_state, jsvalue,
                conversion_flags & ~kConversionFlagNullable, out_exception,
                &(out_optional->value()));
  }
}

// JSValue -> CallbackFunction
template <class T>
inline void FromJSValue(JSC::ExecState* exec_state, JSC::JSValue jsvalue,
                        int conversion_flags, ExceptionState* out_exception,
                        JSCCallbackFunctionHolder<T>* out_callback) {
  DCHECK_EQ(conversion_flags & ~kConversionFlagsCallbackFunction, 0)
      << "No conversion flags supported.";
  if (jsvalue.isNull()) {
    if (!(conversion_flags & kConversionFlagNullable)) {
      out_exception->SetSimpleException(kNotNullableType);
    }
    // If it is a nullable type, just return.
    return;
  }

  JSCGlobalObject* global_object =
      JSC::jsCast<JSCGlobalObject*>(exec_state->lexicalGlobalObject());

  // https://www.w3.org/TR/WebIDL/#es-callback-function
  // 1. If V is not a Function object, throw a TypeError
  if (!jsvalue.isFunction()) {
    out_exception->SetSimpleException(kNotFunctionValue);
    return;
  }

  JSC::JSFunction* js_function =
      JSC::jsCast<JSC::JSFunction*>(jsvalue.asCell());
  DCHECK(js_function);
  JSCCallbackFunction<typename T::Signature> callback_function(js_function);
  *out_callback = JSCCallbackFunctionHolder<T>(
      callback_function, global_object->script_object_registry());
}

// JSValue -> CallbackInterface
template <class T>
inline void FromJSValue(JSC::ExecState* exec_state, JSC::JSValue jsvalue,
                        int conversion_flags, ExceptionState* out_exception,
                        JSCCallbackInterfaceHolder<T>* out_callback_interface) {
  typedef typename CallbackInterfaceTraits<T>::JSCCallbackInterfaceClass
      JSCCallbackInterfaceClass;
  DCHECK_EQ(conversion_flags & ~kConversionFlagsCallbackFunction, 0)
      << "No conversion flags supported.";
  if (jsvalue.isNull()) {
    if (!(conversion_flags & kConversionFlagNullable)) {
      out_exception->SetSimpleException(kNotNullableType);
    }
    // If it is a nullable type, just return.
    return;
  }

  JSCGlobalObject* global_object =
      JSC::jsCast<JSCGlobalObject*>(exec_state->lexicalGlobalObject());

  // https://www.w3.org/TR/WebIDL/#es-user-objects
  // Any user object can be considered to implement a user interface. Actually
  // checking if the correct properties exist will happen when the operation
  // on the callback interface is run.

  if (!jsvalue.isFunction() && !jsvalue.isObject()) {
    out_exception->SetSimpleException(kNotObjectOrFunction);
    return;
  }

  JSC::JSObject* js_object = jsvalue.getObject();
  DCHECK(js_object);
  JSCCallbackInterfaceClass callback_interface(js_object);
  *out_callback_interface = JSCCallbackInterfaceHolder<T>(
      callback_interface, global_object->script_object_registry());
}

// JSValue -> OpaqueHandle
inline void FromJSValue(JSC::ExecState* exec_state, JSC::JSValue jsvalue,
                        int conversion_flags, ExceptionState* out_exception,
                        JSCObjectHandleHolder* out_handle) {
  DCHECK_EQ(conversion_flags & ~kConversionFlagsObject, 0)
      << "Unexpected conversion flags found.";
  JSC::JSObject* js_object = NULL;
  if (jsvalue.isNull()) {
    if (!(conversion_flags & kConversionFlagNullable)) {
      out_exception->SetSimpleException(kNotNullableType);
    }
    // Return here whether an exception was set or not.
    return;
  } else {
    // https://www.w3.org/TR/WebIDL/#es-object
    // 1. If Type(V) is not Object, throw a TypeError
    js_object = jsvalue.getObject();
    if (!js_object) {
      out_exception->SetSimpleException(kNotObjectType);
      return;
    }
  }

  // Null cases should be handled above.
  DCHECK(js_object);

  JSCGlobalObject* global_object =
      JSC::jsCast<JSCGlobalObject*>(exec_state->lexicalGlobalObject());
  JSCObjectHandle jsc_object_handle(js_object);
  *out_handle = JSCObjectHandleHolder(jsc_object_handle,
                                      global_object->script_object_registry());
}

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#include "cobalt/script/javascriptcore/jsc_callback_function_impl.h"
// Union type conversion is generated by a pump script.
#include "cobalt/script/javascriptcore/union_type_conversion_impl.h"

#endif  // COBALT_SCRIPT_JAVASCRIPTCORE_CONVERSION_HELPERS_H_
