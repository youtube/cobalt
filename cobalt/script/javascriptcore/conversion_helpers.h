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
#include "cobalt/base/enable_if.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSGlobalData.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSValue.h"
#include "third_party/WebKit/Source/WTF/wtf/text/WTFString.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

JSC::JSValue BooleanToJSValue(bool in_boolean);
WTF::String ToWTFString(const std::string& utf8_string);
JSC::JSValue StringToJSValue(JSC::JSGlobalData* global_data,
                             const std::string& utf8_string);
JSC::JSValue JSObjectToJSValue(JSC::JSObject* js_object);

bool JSValueToBoolean(JSC::ExecState* exec_state, JSC::JSValue value);
void JSValueToString(JSC::ExecState* exec_state, JSC::JSValue value,
                     std::string* out_string);
JSC::JSObject* JSValueToJSObject(JSC::ExecState* exec_state,
                                 JSC::JSValue value);

template <typename T>
JSC::JSValue NumberToJSValue(T in_number) {
  JSC::JSValue number_value = JSC::jsNumber(in_number);
  DCHECK(number_value.isNumber());
  return number_value;
}

template <typename T>
typename base::enable_if<std::numeric_limits<T>::is_signed && (sizeof(T) <= 4),
                   T>::type
JSValueToNumber(JSC::ExecState* exec_state, JSC::JSValue value) {
  int32_t int32_value = value.toInt32(exec_state);
  return static_cast<T>(int32_value);
}

template <typename T>
typename base::enable_if<!std::numeric_limits<T>::is_signed && (sizeof(T) <= 4),
                   T>::type
JSValueToNumber(JSC::ExecState* exec_state, JSC::JSValue value) {
  uint32_t uint32_value = value.toUInt32(exec_state);
  return static_cast<T>(uint32_value);
}

template <typename T>
typename base::enable_if<!std::numeric_limits<T>::is_integer, T>::type
JSValueToNumber(JSC::ExecState* exec_state, JSC::JSValue value) {
  // Stubbed out to allow compilation to continue.
  NOTIMPLEMENTED();
  return 0;
}

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#endif  // SCRIPT_JAVASCRIPTCORE_CONVERSION_HELPERS_H_
