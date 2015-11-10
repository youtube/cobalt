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

#include "cobalt/script/javascriptcore/util/exception_helpers.h"

#include "base/stringprintf.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSDestructibleObject.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSScope.h"
#include "third_party/WebKit/Source/WTF/wtf/text/WTFString.h"

namespace cobalt {
namespace script {
namespace javascriptcore {
namespace util {

std::string GetExceptionString(JSC::ExecState* exec_state) {
  return GetExceptionString(exec_state, exec_state->exception());
}

std::string GetExceptionString(JSC::ExecState* exec_state,
                               JSC::JSValue exception) {
  WTF::String exception_string = exception.toWTFString(exec_state);
  JSC::JSObject* exception_object = exception.toObject(exec_state);
  int line_number =
      exception_object->get(exec_state, JSC::Identifier(exec_state, "line"))
          .toInt32(exec_state);
  return base::StringPrintf("%s : line %d", exception_string.utf8().data(),
                            line_number);
}

}  // namespace util
}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt
