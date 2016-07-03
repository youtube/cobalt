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
#ifndef COBALT_SCRIPT_JAVASCRIPTCORE_JSC_DEBUGGER_H_
#define COBALT_SCRIPT_JAVASCRIPTCORE_JSC_DEBUGGER_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/threading/thread_checker.h"
#include "cobalt/script/javascriptcore/jsc_global_object_proxy.h"
#include "cobalt/script/script_debugger.h"
#include "cobalt/script/source_provider.h"

#include "third_party/WebKit/Source/JavaScriptCore/config.h"
#include "third_party/WebKit/Source/JavaScriptCore/debugger/Debugger.h"
#include "third_party/WebKit/Source/JavaScriptCore/debugger/DebuggerCallFrame.h"
#include "third_party/WebKit/Source/WTF/wtf/HashSet.h"
#include "third_party/WebKit/Source/WTF/wtf/text/WTFString.h"

namespace JSC {
class ExecState;
class JSGlobalData;
class JSGlobalObject;
class JSScope;
class JSValue;
class SourceProvider;
}

namespace cobalt {
namespace script {
namespace javascriptcore {

// JavaScriptCore-specific implementation of a JavaScript debugger.
// Uses multiple inheritance in accordance with the C++ style guide to extend
// JSC::Debugger and implement ScriptDebugger.
// https://engdoc.***REMOVED***/eng/doc/devguide/cpp/styleguide.shtml?cl=head#Multiple_Inheritance
// Only the ScriptDebugger is publicly exposed.
// This class is not designed to be thread-safe - it is assumed that all
// public methods will be run on the same message loop as the JavaScript
// global object to which this debugger connects.
class JSCDebugger : protected JSC::Debugger, public ScriptDebugger {
 public:
  JSCDebugger(GlobalObjectProxy* global_object_proxy, Delegate* delegate);
  ~JSCDebugger() OVERRIDE;

  // Implementation of ScriptDebugger.
  void Attach() OVERRIDE;
  void Detach() OVERRIDE;
  void Pause() OVERRIDE;
  void Resume() OVERRIDE;
  void SetBreakpoint(const std::string& script_id, int line_number,
                     int column_number) OVERRIDE;
  PauseOnExceptionsState SetPauseOnExceptions(
      PauseOnExceptionsState state) OVERRIDE;
  void StepInto() OVERRIDE;
  void StepOut() OVERRIDE;
  void StepOver() OVERRIDE;

 protected:
  // Hides a non-virtual JSC::Debugger method with the same name.
  void attach(JSC::JSGlobalObject* global_object);

  // The following methods are overrides of pure virtual methods in
  // JSC::Debugger, hence the non-standard names.
  void detach(JSC::JSGlobalObject* global_object) OVERRIDE;

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

 private:
  // Physical breakpoint corresponding to a specific source location.
  // TODO(***REMOVED***): Include other attributes, e.g. condition.
  struct Breakpoint {
    Breakpoint(intptr_t source_id, int line_number, int column_number)
        : source_id(source_id),
          line_number(line_number),
          column_number(column_number) {}
    intptr_t source_id;
    int line_number;
    int column_number;
  };

  typedef std::vector<Breakpoint> BreakpointVector;

  // Sets the |is_paused_| member of the debugger while in scope, unsets it
  // on destruction.
  class ScopedPausedState {
   public:
    explicit ScopedPausedState(JSCDebugger* debugger) : debugger(debugger) {
      debugger->is_paused_ = true;
    }
    ~ScopedPausedState() { debugger->is_paused_ = false; }

   private:
    JSCDebugger* debugger;
  };

  // Convenience function to get the global object pointer from the proxy.
  JSCGlobalObject* GetGlobalObject() const;

  // Update functions called by the overridden methods from JSC:Debugger above
  // (e.g. |atStatement|) as script is executed.
  void UpdateAndPauseIfNeeded(const JSC::DebuggerCallFrame& call_frame,
                              intptr_t source_id, int line_number,
                              int column_number);
  void UpdateSourceLocation(intptr_t source_id, int line_number,
                            int column_number);
  void UpdateCallFrame(const JSC::DebuggerCallFrame& call_frame);
  void PauseIfNeeded(const JSC::DebuggerCallFrame& call_frame);

  // Sends event notifications via |delegate_|.
  void SendPausedEvent(const JSC::DebuggerCallFrame& call_frame);
  void SendResumedEvent();

  // Whether any of the currently active breakpoints matches the current
  // source location.
  bool IsBreakpointAtCurrentLocation() const;

  base::ThreadChecker thread_checker_;
  GlobalObjectProxy* global_object_proxy_;

  // Lifetime managed by the caller of this object's constructor.
  Delegate* delegate_;

  // Whether script execution pauses on exceptions.
  PauseOnExceptionsState pause_on_exceptions_;

  // Code execution control flags. Script execution can pause on the next
  // statement, or on a specific call frame.
  bool pause_on_next_statement_;
  JSC::CallFrame* pause_on_call_frame_;

  // Current call frame.
  JSC::DebuggerCallFrame current_call_frame_;

  // Current source location.
  intptr_t current_source_id_;
  int current_line_number_;
  int current_column_number_;

  // Whether script execution is currently paused.
  bool is_paused_;

  // Currently active breakpoints.
  BreakpointVector breakpoints_;
};

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_JAVASCRIPTCORE_JSC_DEBUGGER_H_
