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

JSC::JSValue StringToJSValue(JSC::JSGlobalData* global_data,
                             const std::string& utf8_string) {
  WTF::String wtf_string = ToWTFString(utf8_string);
  JSC::JSString* js_string = JSC::jsString(global_data, wtf_string);
  JSC::JSValue string_value(js_string);
  DCHECK(string_value.isString());
  return string_value;
}

void JSValueToString(JSC::ExecState* exec_state, JSC::JSValue value,
                     std::string* out_string) {
  // By default null and undefined evaluate to the strings "null" and
  // "undefined" respectively.
  // TODO: Optimize this.
  JSC::JSString* js_string = value.toString(exec_state);
  const WTF::String& wtf_string = js_string->value(exec_state);
  *out_string = FromWTFString(wtf_string);
}

JSC::JSValue TokenToJSValue(JSC::JSGlobalData* global_data,
                            base::Token utf8_string) {
  WTF::String wtf_string = ToWTFString(utf8_string.c_str());
  JSC::JSString* js_string = JSC::jsString(global_data, wtf_string);
  JSC::JSValue string_value(js_string);
  DCHECK(string_value.isString());
  return string_value;
}

WTF::String ToWTFString(const std::string& utf8_string) {
  WTF::String wtf_string;
  // If the string is all ASCII, we can convert it using half the memory of
  // the generalized conversion function. This is significant as  uses
  // some huge ASCII-only source files (> 10M chars).
  if (IsStringASCII(utf8_string)) {
    wtf_string = WTF::String(utf8_string.c_str(), utf8_string.length());
  }

  wtf_string = WTF::String::fromUTF8(utf8_string.c_str());
  if (wtf_string.isNull()) {
    if (!IsStringUTF8(utf8_string)) {
      DLOG(ERROR) << "Non-UTF8 characters in input string";
    } else {
      DLOG(ERROR) << "Unknown error converting to WTFString";
    }
    wtf_string = WTF::String::fromUTF8("");
  }
  return wtf_string;
}

std::string FromWTFString(const WTF::String& wtf_string) {
  WTF::CString utf8_string = wtf_string.utf8();
  return std::string(utf8_string.data(), utf8_string.length());
}

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt
