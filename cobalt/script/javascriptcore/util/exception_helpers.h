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

#ifndef SCRIPT_JAVASCRIPTCORE_UTIL_EXCEPTION_HELPERS_H_
#define SCRIPT_JAVASCRIPTCORE_UTIL_EXCEPTION_HELPERS_H_

#include <string>

#include "third_party/WebKit/Source/JavaScriptCore/config.h"
#include "third_party/WebKit/Source/JavaScriptCore/interpreter/CallFrame.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSValue.h"

namespace cobalt {
namespace script {
namespace javascriptcore {
namespace util {

std::string GetExceptionString(JSC::ExecState* exec_state);

std::string GetExceptionString(JSC::ExecState* exec_state,
                               JSC::JSValue exception);

std::string GetStackTrace(JSC::ExecState* exec_state);

}  // namespace util
}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#endif  // SCRIPT_JAVASCRIPTCORE_UTIL_EXCEPTION_HELPERS_H_
