// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/debug/script_debugger_component.h"

#include <string>

#include "base/stringprintf.h"
#include "cobalt/debug/json_object.h"

namespace {
constexpr const char* kInspectorDomains[] = {
    "Runtime", "Console",
    // TODO: "Debugger", "Profiler", "HeapProfiler", "Schema",
};
constexpr int kInspectorDomainsCount =
    sizeof(kInspectorDomains) / sizeof(kInspectorDomains[0]);

// JSON attribute names
constexpr char kId[] = "id";
constexpr char kMethod[] = "method";
constexpr char kParams[] = "params";
}  // namespace

namespace cobalt {
namespace debug {

ScriptDebuggerComponent::ScriptDebuggerComponent(
    DebugDispatcher* dispatcher, script::ScriptDebugger* script_debugger)
    : dispatcher_(dispatcher), script_debugger_(script_debugger) {
  for (int i = 0; i < kInspectorDomainsCount; i++) {
    std::string domain(kInspectorDomains[i]);
    if (script_debugger_->CanDispatchProtocolMethod(domain + ".enable")) {
      dispatcher_->AddDomain(domain,
                             base::Bind(&ScriptDebuggerComponent::RunCommand,
                                        base::Unretained(this)));
      registered_domains_.insert(domain);
    }
  }
}

bool ScriptDebuggerComponent::RunCommand(const Command& command) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!script_debugger_->CanDispatchProtocolMethod(command.GetMethod())) {
    return false;
  }

  // Use an internal ID to store the pending command until we get a response.
  int command_id = ++last_command_id_;
  pending_commands_.emplace(command_id, command);

  JSONObject message(new base::DictionaryValue());
  message->SetInteger(kId, command_id);
  message->SetString(kMethod, command.GetMethod());
  JSONObject params = JSONParse(command.GetParams());
  if (params) {
    message->Set(kParams, params.release());
  }
  script_debugger_->DispatchProtocolMessage(JSONStringify(message));
  return true;
}

void ScriptDebuggerComponent::SendCommandResponse(
    const std::string& json_response) {
  JSONObject response = JSONParse(json_response);

  // Strip the internal ID from the response, and get its value.
  int command_id = 0;
  response->GetInteger(kId, &command_id);
  response->Remove(kId, nullptr);

  // Use the stripped ID to lookup the command it's a response for.
  auto iter = pending_commands_.find(command_id);
  if (iter != pending_commands_.end()) {
    iter->second.SendResponse(response);
    pending_commands_.erase(iter);
  } else {
    DLOG(ERROR) << "Spurious debugger response: " << json_response;
  }
}

void ScriptDebuggerComponent::SendEvent(const std::string& json_event) {
  JSONObject event = JSONParse(json_event);

  std::string method;
  event->GetString(kMethod, &method);

  JSONObject params;
  base::Value* value = nullptr;
  base::DictionaryValue* dict_value = nullptr;
  if (event->Get(kParams, &value) && value->GetAsDictionary(&dict_value)) {
    params.reset(dict_value->DeepCopy());
  }

  dispatcher_->SendEvent(method, params);
}

}  // namespace debug
}  // namespace cobalt
