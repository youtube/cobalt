/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/script/javascriptcore/jsc_debugger.h"

#include <string>

#include "base/logging.h"
#include "third_party/WebKit/Source/JavaScriptCore/debugger/DebuggerCallFrame.h"
#include "third_party/WebKit/Source/JavaScriptCore/interpreter/CallFrame.h"
#include "third_party/WebKit/Source/JavaScriptCore/parser/SourceProvider.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSGlobalData.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSGlobalObject.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSValue.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

JSCDebugger::~JSCDebugger() {}

void JSCDebugger::attach(JSC::JSGlobalObject* global_object) {
  DLOG(INFO) << "JSCDebugger::attach";
  JSC::Debugger::attach(global_object);
}

void JSCDebugger::detach(JSC::JSGlobalObject* global_object) {
  DLOG(INFO) << "JSCDebugger::detach";
  JSC::Debugger::detach(global_object);
}

void JSCDebugger::sourceParsed(JSC::ExecState* exec_state,
                               JSC::SourceProvider* source_provider,
                               int error_line,
                               const WTF::String& error_message) {
  UNREFERENCED_PARAMETER(exec_state);
  UNREFERENCED_PARAMETER(source_provider);
  DLOG(INFO) << "JSCDebugger::sourceParsed";
  DLOG(INFO) << "error_line: " << error_line;
  if (error_message.isEmpty()) {
    DLOG(INFO) << "no error message";
  } else {
    DLOG(INFO) << "error_message: " << error_message.characters();
  }
}

void JSCDebugger::exception(const JSC::DebuggerCallFrame& call_frame,
                            intptr_t source_id, int line_number,
                            int column_number, bool has_handler) {
  UNREFERENCED_PARAMETER(call_frame);
  DLOG(INFO) << "JSCDebugger::exception";
  DLOG(INFO) << "source_id: " << source_id;
  DLOG(INFO) << "line_number: " << line_number;
  DLOG(INFO) << "column_number: " << column_number;
  DLOG(INFO) << "has_handler: " << has_handler;
}

void JSCDebugger::atStatement(const JSC::DebuggerCallFrame& call_frame,
                              intptr_t source_id, int line_number,
                              int column_number) {
  UNREFERENCED_PARAMETER(call_frame);
  DLOG(INFO) << "JSCDebugger::atStatement";
  DLOG(INFO) << "source_id: " << source_id;
  DLOG(INFO) << "line_number: " << line_number;
  DLOG(INFO) << "column_number: " << column_number;
}

void JSCDebugger::callEvent(const JSC::DebuggerCallFrame& call_frame,
                            intptr_t source_id, int line_number,
                            int column_number) {
  UNREFERENCED_PARAMETER(call_frame);
  DLOG(INFO) << "JSCDebugger::callEvent";
  DLOG(INFO) << "source_id: " << source_id;
  DLOG(INFO) << "line_number: " << line_number;
  DLOG(INFO) << "column_number: " << column_number;
}

void JSCDebugger::returnEvent(const JSC::DebuggerCallFrame& call_frame,
                              intptr_t source_id, int line_number,
                              int column_number) {
  UNREFERENCED_PARAMETER(call_frame);
  DLOG(INFO) << "JSCDebugger::returnEvent";
  DLOG(INFO) << "source_id: " << source_id;
  DLOG(INFO) << "line_number: " << line_number;
  DLOG(INFO) << "column_number: " << column_number;
}

void JSCDebugger::willExecuteProgram(const JSC::DebuggerCallFrame& call_frame,
                                     intptr_t source_id, int line_number,
                                     int column_number) {
  UNREFERENCED_PARAMETER(call_frame);
  DLOG(INFO) << "JSCDebugger::willExecuteProgram";
  DLOG(INFO) << "source_id: " << source_id;
  DLOG(INFO) << "line_number: " << line_number;
  DLOG(INFO) << "column_number: " << column_number;
}

void JSCDebugger::didExecuteProgram(const JSC::DebuggerCallFrame& call_frame,
                                    intptr_t source_id, int line_number,
                                    int column_number) {
  UNREFERENCED_PARAMETER(call_frame);
  DLOG(INFO) << "JSCDebugger::didExecuteProgram";
  DLOG(INFO) << "source_id: " << source_id;
  DLOG(INFO) << "line_number: " << line_number;
  DLOG(INFO) << "column_number: " << column_number;
}

void JSCDebugger::didReachBreakpoint(const JSC::DebuggerCallFrame& call_frame,
                                     intptr_t source_id, int line_number,
                                     int column_number) {
  UNREFERENCED_PARAMETER(call_frame);
  DLOG(INFO) << "JSCDebugger::didReachBreakpoint";
  DLOG(INFO) << "source_id: " << source_id;
  DLOG(INFO) << "line_number: " << line_number;
  DLOG(INFO) << "column_number: " << column_number;
}

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt
