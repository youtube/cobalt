// Copyright 2016 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_SCRIPT_SCRIPT_DEBUGGER_H_
#define COBALT_SCRIPT_SCRIPT_DEBUGGER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "cobalt/script/call_frame.h"
#include "cobalt/script/source_provider.h"

namespace cobalt {
namespace script {

class GlobalEnvironment;

// Engine-independent pure virtual interface to a JavaScript debugger.
// Used as an opaque interface to the specific debugger implementation,
// e.g. JSCDebugger.
// Only pure virtual or static methods should be added to this class.
// No data members should be added to this class.
class ScriptDebugger {
 public:
  // Ideally, we want the delegate to do as much as possible, as its
  // implementation can be independent of the specific JS engine.
  class Delegate {
   public:
    // Called when the script debugger wants to pause script execution.
    virtual void OnScriptDebuggerPause() = 0;

    // Called when the script debugger wants to resume script execution.
    virtual void OnScriptDebuggerResume() = 0;

    // Called with the response to a previously dispatched protocol message.
    virtual void OnScriptDebuggerResponse(const std::string& response) = 0;

    // Called when a debugging protocol event occurs.
    virtual void OnScriptDebuggerEvent(const std::string& event) = 0;
  };

  // Possible pause on exceptions states.
  enum PauseOnExceptionsState { kAll, kNone, kUncaught };

  // Used to temporarily override the pause on exceptions state, e.g. to
  // disable it when executing devtools backend scripts.
  class ScopedPauseOnExceptionsState {
   public:
    ScopedPauseOnExceptionsState(ScriptDebugger* script_debugger,
                                 PauseOnExceptionsState state)
        : script_debugger_(script_debugger) {
      DCHECK(script_debugger_);
      stored_state_ = script_debugger_->SetPauseOnExceptions(state);
    }

    ~ScopedPauseOnExceptionsState() {
      script_debugger_->SetPauseOnExceptions(stored_state_);
    }

   private:
    ScriptDebugger* script_debugger_;
    PauseOnExceptionsState stored_state_;
  };

  // Factory method to create an engine-specific instance. Implementation to be
  // provided by derived class.
  static scoped_ptr<ScriptDebugger> CreateDebugger(
      GlobalEnvironment* global_environment, Delegate* delegate);

  // Attach/detach the script debugger.
  virtual void Attach() = 0;
  virtual void Detach() = 0;

  // For engines like V8 that directly handle protocol commands.
  virtual bool CanDispatchProtocolMethod(const std::string& method) = 0;
  virtual void DispatchProtocolMessage(const std::string& message) = 0;

  // Code execution control. Implementations may use
  // |Delegate::OnScriptDebuggerPause| to have the delegate handle the
  // actual blocking of the thread in an engine-independent way.
  virtual void Pause() = 0;
  virtual void Resume() = 0;
  virtual void SetBreakpoint(const std::string& script_id, int line_number,
                             int column_number) = 0;
  virtual PauseOnExceptionsState SetPauseOnExceptions(
      PauseOnExceptionsState state) = 0;  // Returns the previous state.
  virtual void StepInto() = 0;
  virtual void StepOut() = 0;
  virtual void StepOver() = 0;

 protected:
  virtual ~ScriptDebugger() {}
  friend class scoped_ptr<ScriptDebugger>;
};

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_SCRIPT_DEBUGGER_H_
