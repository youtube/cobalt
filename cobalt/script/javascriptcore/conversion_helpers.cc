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

#include "cobalt/script/javascriptcore/conversion_helpers.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSObject.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSScope.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSString.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

JSC::JSValue BooleanToJSValue(bool in_boolean) {
  JSC::JSValue boolean_value = JSC::jsBoolean(in_boolean);
  DCHECK(boolean_value.isBoolean());
  return boolean_value;
}

JSC::JSValue StringToJSValue(JSC::JSGlobalData* global_data,
                             const std::string& utf8_string) {
  WTF::String wtf_string = ToWTFString(utf8_string);
  JSC::JSString* js_string = JSC::jsString(global_data, wtf_string);
  JSC::JSValue string_value(js_string);
  DCHECK(string_value.isString());
  return string_value;
}

JSC::JSValue JSObjectToJSValue(JSC::JSObject* js_object) {
  if (js_object == NULL) {
    return JSC::jsNull();
  }
  JSC::JSValue object_value(js_object);
  DCHECK(object_value.isObject());
  return object_value;
}

bool JSValueToBoolean(JSC::ExecState* exec_state, JSC::JSValue value) {
  return value.toBoolean(exec_state);
}

void JSValueToString(JSC::ExecState* exec_state, JSC::JSValue value,
                     std::string* out_string) {
  // By default null and undefined evaluate to the strings "null" and
  // "undefined" respectively.
  // TODO(***REMOVED***): Optimize this.
  JSC::JSString* js_string = value.toString(exec_state);
  const WTF::String& wtf_string = js_string->value(exec_state);
  WTF::CString utf8_string = wtf_string.utf8();
  *out_string = std::string(utf8_string.data(), utf8_string.length());
}

WTF::String ToWTFString(const std::string& utf8_string) {
  DCHECK(IsStringUTF8(utf8_string));
  return WTF::String::fromUTF8(utf8_string.c_str());
}

JSC::JSObject* JSValueToJSObject(JSC::ExecState* exec_state,
                                 JSC::JSValue value) {
  if (value.isUndefined()) {
    NOTIMPLEMENTED() << "Converting from undefined to object type unsupported.";
    return NULL;
  } else if (value.isNull()) {
    return NULL;
  } else if (!value.isObject()) {
    // TODO(***REMOVED***): Error handling when value is not the expected type.
    NOTIMPLEMENTED() << "Conversion from non-object types unsupported.";
    return NULL;
  }
  return value.toObject(exec_state);
}

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt
