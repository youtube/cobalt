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

#include <sstream>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "third_party/WebKit/Source/JavaScriptCore/interpreter/Interpreter.h"
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
  JSC::JSGlobalData* global_data = &exec_state->globalData();

  WTF::String wtf_exception_string = exception.toWTFString(exec_state);
  std::string exception_string =
      exception.toWTFString(exec_state).utf8().data();
  exception_string += "\n";

  JSC::JSObject* exception_object = exception.toObject(exec_state);
  JSC::JSValue stack_value = exception_object->getDirect(
      *global_data, global_data->propertyNames->stack);
  if (stack_value.isString()) {
    exception_string += stack_value.toWTFString(exec_state).utf8().data();
  } else {
    int line_number =
        exception_object->get(exec_state, JSC::Identifier(exec_state, "line"))
            .toInt32(exec_state);
    std::string source_url =
        exception_object->get(exec_state,
                              JSC::Identifier(exec_state, "sourceURL"))
            .toWTFString(exec_state)
            .utf8()
            .data();
    exception_string +=
        base::StringPrintf("%s:%d", source_url.c_str(), line_number);
  }
  return exception_string;
}

std::string GetStackTrace(JSC::ExecState* exec) {
  std::stringstream backtrace_stream;

  JSC::JSLockHolder lock(exec);

  WTF::Vector<JSC::StackFrame> stack_trace;
  exec->interpreter()->getStackTrace(&exec->globalData(), stack_trace);

  for (uint32 i = 0; i < stack_trace.size(); ++i) {
    WTF::String function_name = stack_trace[i].friendlyFunctionName(exec);
    WTF::String source_url = stack_trace[i].friendlySourceURL();
    uint32 line_number = stack_trace[i].friendlyLineNumber();
    if (i > 0) {
      backtrace_stream << std::endl;
    }
    backtrace_stream << function_name.utf8().data();
    if (!source_url.isEmpty()) {
      backtrace_stream << " @ " << source_url.utf8().data() << ':'
                       << line_number;
    }
  }

  return backtrace_stream.str();
}

}  // namespace util
}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt
