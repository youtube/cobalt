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

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/values.h"
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

    // Called when the script debugger wants to resume script execution, both
    // after a pause and to start running after the devtools frontend connects.
    virtual void OnScriptDebuggerResume() = 0;

    // Called with the response to a previously dispatched protocol message.
    virtual void OnScriptDebuggerResponse(const std::string& response) = 0;

    // Called when a debugging protocol event occurs.
    virtual void OnScriptDebuggerEvent(const std::string& event) = 0;
  };

  // Receives trace events from the JS engine in the JSON Trace Event Format.
  // https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU
  class TraceDelegate {
   public:
    virtual ~TraceDelegate() {}
    virtual void AppendTraceEvent(const std::string& trace_event_json) = 0;
    virtual void FlushTraceEvents() = 0;
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
  static std::unique_ptr<ScriptDebugger> CreateDebugger(
      GlobalEnvironment* global_environment, Delegate* delegate);

  // Attach/detach the script debugger. Saved state can be passed between
  // instances as an opaque string..
  virtual void Attach(const std::string& state) = 0;
  virtual std::string Detach() = 0;

  // Evaluate JavaScript code that is part of the debugger implementation, such
  // that it does not get reported as debuggable source. Returns true on
  // success, false if there is an exception. If out_result_utf8 is non-NULL, it
  // will be set to hold the result of the script evaluation if the script
  // succeeds, or an exception message if it fails.
  virtual bool EvaluateDebuggerScript(const std::string& js_code,
                                      std::string* out_result_utf8) = 0;

  // For engines like V8 that directly handle protocol commands.
  virtual std::set<std::string> SupportedProtocolDomains() = 0;
  virtual bool DispatchProtocolMessage(const std::string& method,
                                       const std::string& message) = 0;

  // Creates a JSON representation of an object.
  // https://chromedevtools.github.io/devtools-protocol/1-3/Runtime#type-RemoteObject
  virtual std::string CreateRemoteObject(const ValueHandleHolder& object,
                                         const std::string& group) = 0;

  // Lookup the object ID that was in the JSON from |CreateRemoteObject| and
  // return the JavaScript object that it refers to.
  // https://chromedevtools.github.io/devtools-protocol/1-3/Runtime#type-RemoteObject
  virtual const script::ValueHandleHolder* LookupRemoteObjectId(
      const std::string& object_id) = 0;

  // For performance tracing of JavaScript methods.
  virtual void StartTracing(const std::vector<std::string>& categories,
                            TraceDelegate* trace_delegate) = 0;
  virtual void StopTracing() = 0;

  virtual PauseOnExceptionsState SetPauseOnExceptions(
      PauseOnExceptionsState state) = 0;  // Returns the previous state.

  // Record the JavaScript stack on the WebModule thread at the point a task is
  // initiated that will run at a later time (on the same thread), allowing it
  // to be seen as the originator when breaking in the asynchronous task.
  virtual void AsyncTaskScheduled(const void* task, const std::string& name,
                                  bool recurring) = 0;

  // A scheduled task is starting to run.
  virtual void AsyncTaskStarted(const void* task) = 0;

  // A scheduled task has finished running.
  virtual void AsyncTaskFinished(const void* task) = 0;

  // A scheduled task will no longer be run, and resources associated with it
  // may be released.
  virtual void AsyncTaskCanceled(const void* task) = 0;

  // All scheduled tasks will no longer be run, and resources associated with
  // them may be released.
  virtual void AllAsyncTasksCanceled() = 0;

 protected:
  virtual ~ScriptDebugger() {}
  friend std::unique_ptr<ScriptDebugger>::deleter_type;
};

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_SCRIPT_DEBUGGER_H_
