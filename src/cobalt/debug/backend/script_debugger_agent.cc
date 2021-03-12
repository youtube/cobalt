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

#include "cobalt/debug/backend/script_debugger_agent.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "cobalt/debug/json_object.h"

namespace {
// State keys
constexpr char kScriptDebuggerState[] = "script_debugger";

// JSON attribute names
constexpr char kId[] = "id";
constexpr char kMethod[] = "method";
constexpr char kParams[] = "params";
}  // namespace

namespace cobalt {
namespace debug {
namespace backend {

ScriptDebuggerAgent::ScriptDebuggerAgent(
    DebugDispatcher* dispatcher, script::ScriptDebugger* script_debugger)
    : dispatcher_(dispatcher),
      script_debugger_(script_debugger),
      supported_domains_(script_debugger->SupportedProtocolDomains()) {}

void ScriptDebuggerAgent::Thaw(JSONObject agent_state) {
  for (auto domain : supported_domains_) {
    dispatcher_->AddDomain(domain, base::Bind(&ScriptDebuggerAgent::RunCommand,
                                              base::Unretained(this)));
  }
  std::string script_debugger_state;
  if (agent_state) {
    agent_state->GetString(kScriptDebuggerState, &script_debugger_state);
  }
  script_debugger_->Attach(script_debugger_state);
}

JSONObject ScriptDebuggerAgent::Freeze() {
  for (auto domain : supported_domains_) {
    dispatcher_->RemoveDomain(domain);
  }
  JSONObject agent_state(new base::DictionaryValue());
  std::string script_debugger_state = script_debugger_->Detach();
  agent_state->SetString(kScriptDebuggerState, script_debugger_state);
  return agent_state;
}

base::Optional<Command> ScriptDebuggerAgent::RunCommand(Command command) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Use an internal ID to store the pending command until we get a response.
  int command_id = ++last_command_id_;

  JSONObject message(new base::DictionaryValue());
  message->SetInteger(kId, command_id);
  message->SetString(kMethod, command.GetMethod());
  JSONObject params = JSONParse(command.GetParams());
  if (params) {
    message->Set(kParams, std::move(params));
  }

  // Store the pending command before dispatching it so that we can find it if
  // the script debugger sends a synchronous response before returning.
  std::string method = command.GetMethod();
  pending_commands_.emplace(command_id, std::move(command));
  if (script_debugger_->DispatchProtocolMessage(method,
                                                JSONStringify(message))) {
    // The command has been dispatched; keep ownership of it in the map.
    return base::nullopt;
  }

  // Take the command back out of the map and return it for fallback.
  auto opt_command =
      base::make_optional(std::move(pending_commands_.at(command_id)));
  pending_commands_.erase(command_id);
  return opt_command;
}

void ScriptDebuggerAgent::SendCommandResponse(
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

void ScriptDebuggerAgent::SendEvent(const std::string& json_event) {
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

}  // namespace backend
}  // namespace debug
}  // namespace cobalt
