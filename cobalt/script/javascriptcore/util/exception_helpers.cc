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
  WTF::String wtf_exception_string = exception.toWTFString(exec_state);
  JSC::JSObject* exception_object = exception.toObject(exec_state);
  int line_number =
      exception_object->get(exec_state, JSC::Identifier(exec_state, "line"))
          .toInt32(exec_state);
  WTF::String source_url =
      exception_object->get(exec_state, JSC::Identifier(exec_state, "sourceURL"))
          .toWTFString(exec_state);
  std::string exception_string =
      base::StringPrintf("%s : %s:%d", wtf_exception_string.utf8().data(),
                            source_url.utf8().data(),
                            line_number);
  exception_string += "\n";
  exception_string += GetStackTrace(exec_state);
  return exception_string;
}

std::string GetStackTrace(JSC::ExecState* exec) {
  std::stringstream backtrace_stream;

  JSC::JSLockHolder lock(exec);

  JSC::CallFrame* call_frame = exec->globalData().topCallFrame;
  if (!call_frame) {
    return std::string();
  }
  int call_frame_count = 0;

  // TODO(***REMOVED***): Initialize |callee()| in bindings.
  if (exec->callee()) {
    if (asObject(exec->callee())->inherits(&JSC::InternalFunction::s_info)) {
      WTF::String function_name =
          asInternalFunction(exec->callee())->name(exec);
      backtrace_stream << "#0 " << function_name.utf8().data() << "() ";
      ++call_frame_count;
    }
  }

  while (true) {
    DCHECK(call_frame);

    int line_number;
    intptr_t source_id;
    WTF::String url_string;
    JSC::JSValue function;
    exec->interpreter()->retrieveLastCaller(call_frame, line_number, source_id,
                                            url_string, function);
    WTF::String function_name =
        function ? JSC::jsCast<JSC::JSFunction*>(function)->name(exec)
                 : "(anonymous function)";

    if (call_frame_count > 0) {
      backtrace_stream << '\n';
    }
    backtrace_stream << function_name.utf8().data() << " @ "
                     << url_string.utf8().data() << ':'
                     << std::max(0, line_number);

    if (!function) {
      break;
    }
    call_frame = call_frame->callerFrame();
    ++call_frame_count;
  }

  return backtrace_stream.str();
}

}  // namespace util
}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt
