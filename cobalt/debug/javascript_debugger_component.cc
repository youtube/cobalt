/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/debug/javascript_debugger_component.h"

#include "base/bind.h"
#include "base/values.h"

namespace cobalt {
namespace debug {

namespace {
// Command "methods" (names) from the set specified here:
// https://developer.chrome.com/devtools/docs/protocol/1.1/debugger
const char kDisable[] = "Debugger.disable";
const char kEnable[] = "Debugger.enable";
const char kGetScriptSource[] = "Debugger.getScriptSource";
const char kPause[] = "Debugger.pause";
const char kResume[] = "Debugger.resume";
const char kStepInto[] = "Debugger.stepInto";
const char kStepOut[] = "Debugger.stepOut";
const char kStepOver[] = "Debugger.stepOver";
}  // namespace

JavaScriptDebuggerComponent::JavaScriptDebuggerComponent(
    const base::WeakPtr<DebugServer>& server,
    script::GlobalObjectProxy* global_object_proxy)
    : DebugServer::Component(server),
      global_object_proxy_(global_object_proxy) {
  AddCommand(kDisable, base::Bind(&JavaScriptDebuggerComponent::Disable,
                                  base::Unretained(this)));
  AddCommand(kEnable, base::Bind(&JavaScriptDebuggerComponent::Enable,
                                 base::Unretained(this)));
  AddCommand(kGetScriptSource,
             base::Bind(&JavaScriptDebuggerComponent::GetScriptSource,
                        base::Unretained(this)));
  AddCommand(kPause, base::Bind(&JavaScriptDebuggerComponent::Pause,
                                base::Unretained(this)));
  AddCommand(kResume, base::Bind(&JavaScriptDebuggerComponent::Resume,
                                 base::Unretained(this)));
  AddCommand(kStepInto, base::Bind(&JavaScriptDebuggerComponent::StepInto,
                                   base::Unretained(this)));
  AddCommand(kStepOut, base::Bind(&JavaScriptDebuggerComponent::StepOut,
                                  base::Unretained(this)));
  AddCommand(kStepOver, base::Bind(&JavaScriptDebuggerComponent::StepOver,
                                   base::Unretained(this)));
}

JSONObject JavaScriptDebuggerComponent::Enable(const JSONObject& params) {
  UNREFERENCED_PARAMETER(params);

  // Reset the debugger first, to detach the current connection, if any.
  javascript_debugger_.reset(NULL);

  javascript_debugger_ = script::JavaScriptDebuggerInterface::CreateDebugger(
      global_object_proxy_, this);
  DCHECK(javascript_debugger_);

  if (javascript_debugger_) {
    return JSONObject(new base::DictionaryValue());
  } else {
    return ErrorResponse("Cannot create JavaScript debugger.");
  }
}

JSONObject JavaScriptDebuggerComponent::Disable(const JSONObject& params) {
  UNREFERENCED_PARAMETER(params);
  javascript_debugger_.reset(NULL);
  return JSONObject(new base::DictionaryValue());
}

JSONObject JavaScriptDebuggerComponent::GetScriptSource(
    const JSONObject& params) {
  if (javascript_debugger_) {
    return javascript_debugger_->GetScriptSource(params);
  } else {
    return ErrorResponse("JavaScript debugger is not enabled.");
  }
}

JSONObject JavaScriptDebuggerComponent::Pause(const JSONObject& params) {
  UNREFERENCED_PARAMETER(params);

  if (!javascript_debugger_) {
    return ErrorResponse("JavaScript debugger is not enabled.");
  }

  // Tell the script debugger to pause on the next statement. When it reaches
  // the next statement, it will call |OnScriptDebuggerPause| below.
  javascript_debugger_->Pause();

  // Empty response.
  return JSONObject(new base::DictionaryValue());
}

JSONObject JavaScriptDebuggerComponent::Resume(const JSONObject& params) {
  UNREFERENCED_PARAMETER(params);

  // Tell the script debugger to resume normal execution.
  if (!javascript_debugger_) {
    return ErrorResponse("JavaScript debugger is not enabled.");
  }
  javascript_debugger_->Resume();

  // Tell the debug server to unblock this thread.
  if (server()) {
    server()->SetPaused(false);
  }

  // Notify the debug clients.
  std::string event_method = "Debugger.resumed";
  JSONObject event_params(new base::DictionaryValue());
  SendEvent(event_method, event_params);

  // Empty response.
  return JSONObject(new base::DictionaryValue());
}

JSONObject JavaScriptDebuggerComponent::StepInto(const JSONObject& params) {
  UNREFERENCED_PARAMETER(params);

  // Tell the script debugger to pause on the next statement.
  if (!javascript_debugger_) {
    return ErrorResponse("JavaScript debugger is not enabled.");
  }
  javascript_debugger_->StepInto();

  // Tell the debug server to unblock this thread.
  if (server()) {
    server()->SetPaused(false);
  }

  // Empty response.
  return JSONObject(new base::DictionaryValue());
}

JSONObject JavaScriptDebuggerComponent::StepOut(const JSONObject& params) {
  UNREFERENCED_PARAMETER(params);

  // Tell the script debugger to pause when it returns from the current
  // function (if any).
  if (!javascript_debugger_) {
    return ErrorResponse("JavaScript debugger is not enabled.");
  }
  javascript_debugger_->StepOut();

  // Tell the debug server to unblock this thread.
  if (server()) {
    server()->SetPaused(false);
  }

  // Empty response.
  return JSONObject(new base::DictionaryValue());
}

JSONObject JavaScriptDebuggerComponent::StepOver(const JSONObject& params) {
  UNREFERENCED_PARAMETER(params);

  // Tell the script debugger to pause on the next statement in the current
  // function.
  if (!javascript_debugger_) {
    return ErrorResponse("JavaScript debugger is not enabled.");
  }
  javascript_debugger_->StepOver();

  // Tell the debug server to unblock this thread.
  if (server()) {
    server()->SetPaused(false);
  }

  // Empty response.
  return JSONObject(new base::DictionaryValue());
}

void JavaScriptDebuggerComponent::OnScriptDebuggerDetach(
    const std::string& reason) {
  DLOG(INFO) << "JavaScript debugger detached: " << reason;
  NOTIMPLEMENTED();
}

void JavaScriptDebuggerComponent::OnScriptDebuggerEvent(
    const std::string& method, const JSONObject& params) {
  SendEvent(method, params);
}

void JavaScriptDebuggerComponent::OnScriptDebuggerPause() {
  // Tell the debug server to enter paused state - block this thread.
  if (server()) {
    server()->SetPaused(true);
  }
}

void JavaScriptDebuggerComponent::OnScriptDebuggerResume() {
  // Tell the debug server to unblock this thread.
  if (server()) {
    server()->SetPaused(false);
  }
}

}  // namespace debug
}  // namespace cobalt
