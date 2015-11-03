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
#ifndef SCRIPT_JAVASCRIPTCORE_JSC_DEBUGGER_H_
#define SCRIPT_JAVASCRIPTCORE_JSC_DEBUGGER_H_

#include "base/compiler_specific.h"

#include "third_party/WebKit/Source/JavaScriptCore/config.h"
#include "third_party/WebKit/Source/JavaScriptCore/debugger/Debugger.h"
#include "third_party/WebKit/Source/WTF/wtf/text/WTFString.h"

namespace JSC {
class DebuggerCallFrame;
class ExecState;
class JSGlobalData;
class JSGlobalObject;
class JSValue;
class SourceProvider;
}

namespace cobalt {
namespace script {
namespace javascriptcore {

// Our implementation of the Debugger interface.
class JSCDebugger : protected JSC::Debugger {
 public:
  ~JSCDebugger() OVERRIDE;
  // Hides a non-virtual base class method with the same name.
  void attach(JSC::JSGlobalObject* global_object);
  void detach(JSC::JSGlobalObject* global_object) OVERRIDE;

 protected:
  void sourceParsed(JSC::ExecState* exec_state,
                    JSC::SourceProvider* source_provider, int error_line,
                    const WTF::String& error_message) OVERRIDE;

  void exception(const JSC::DebuggerCallFrame& call_frame, intptr_t source_id,
                 int line_number, int column_number, bool has_handler) OVERRIDE;

  void atStatement(const JSC::DebuggerCallFrame& call_frame, intptr_t source_id,
                   int line_number, int column_number) OVERRIDE;

  void callEvent(const JSC::DebuggerCallFrame& call_frame, intptr_t source_id,
                 int line_number, int column_number) OVERRIDE;

  void returnEvent(const JSC::DebuggerCallFrame& call_frame, intptr_t source_id,
                   int line_number, int column_number) OVERRIDE;

  void willExecuteProgram(const JSC::DebuggerCallFrame& call_frame,
                          intptr_t source_id, int line_number,
                          int column_number) OVERRIDE;

  void didExecuteProgram(const JSC::DebuggerCallFrame& call_frame,
                         intptr_t source_id, int line_number,
                         int column_number) OVERRIDE;

  void didReachBreakpoint(const JSC::DebuggerCallFrame& call_frame,
                          intptr_t source_id, int line_number,
                          int column_number) OVERRIDE;
};

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#endif  // SCRIPT_JAVASCRIPTCORE_JSC_DEBUGGER_H_
