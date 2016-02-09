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

namespace cobalt {
namespace debug {

namespace {
// Command "methods" (names) from the set specified here:
// https://developer.chrome.com/devtools/docs/protocol/1.1/debugger
const char kGetScriptSource[] = "Debugger.getScriptSource";
}  // namespace

JavaScriptDebuggerComponent::JavaScriptDebuggerComponent(
    const base::WeakPtr<DebugServer>& server,
    script::GlobalObjectProxy* global_object_proxy)
    : DebugServer::Component(server) {
  script::JavaScriptDebuggerInterface::OnEventCallback on_event_callback =
      base::Bind(&JavaScriptDebuggerComponent::OnNotification,
                 base::Unretained(this));

  script::JavaScriptDebuggerInterface::OnDetachCallback on_detach_callback;

  javascript_debugger_ = script::JavaScriptDebuggerInterface::CreateDebugger(
      global_object_proxy, on_event_callback, on_detach_callback);

  AddCommand(kGetScriptSource,
             base::Bind(&JavaScriptDebuggerComponent::GetScriptSource,
                        base::Unretained(this)));
}

void JavaScriptDebuggerComponent::OnNotification(const std::string& method,
                                                 const JSONObject& params) {
  SendNotification(method, params);
}

JSONObject JavaScriptDebuggerComponent::GetScriptSource(
    const JSONObject& params) {
  return javascript_debugger_->GetScriptSource(params);
}

}  // namespace debug
}  // namespace cobalt
