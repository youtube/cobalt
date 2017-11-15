// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/debug/component_connector.h"

#include "base/logging.h"
#include "base/values.h"

namespace cobalt {
namespace debug {

namespace {
// Error response message field.
const char kErrorMessage[] = "error.message";
}  // namespace

ComponentConnector::ComponentConnector(DebugServer* server,
                                       script::ScriptDebugger* script_debugger)
    : server_(server), script_debugger_(script_debugger) {}

ComponentConnector::~ComponentConnector() {
  // Remove all the commands added by this component.
  for (std::vector<std::string>::const_iterator it = command_methods_.begin();
       it != command_methods_.end(); ++it) {
    RemoveCommand(*it);
  }
}

void ComponentConnector::AddCommand(const std::string& method,
                                    const DebugServer::Command& callback) {
  if (server_) {
    server_->AddCommand(method, callback);

    // Store the command methods added by this component, so we can remove on
    // destruction.
    command_methods_.push_back(method);
  }
}

void ComponentConnector::RemoveCommand(const std::string& method) {
  if (server_) {
    server_->RemoveCommand(method);
  }
}

JSONObject ComponentConnector::RunScriptCommand(const std::string& command,
                                                const JSONObject& params) {
  if (server_) {
    script::ScriptDebugger::ScopedPauseOnExceptionsState no_pause(
        script_debugger_, script::ScriptDebugger::kNone);
    return server_->RunScriptCommand(command, params);
  } else {
    return ErrorResponse("Not attached to debug server");
  }
}

bool ComponentConnector::RunScriptFile(const std::string& filename) {
  if (server_) {
    script::ScriptDebugger::ScopedPauseOnExceptionsState no_pause(
        script_debugger_, script::ScriptDebugger::kNone);
    return server_->RunScriptFile(filename);
  } else {
    return ErrorResponse("Not attached to debug server");
  }
}

JSONObject ComponentConnector::CreateRemoteObject(
    const script::ValueHandleHolder* object) {
  // Parameter object for the JavaScript function call uses default values.
  JSONObject params(new base::DictionaryValue());

  // This will execute a JavaScript function to create a Runtime.Remote object
  // that describes the opaque JavaScript object.
  DCHECK(server_);
  base::optional<std::string> json_result =
      server()->CreateRemoteObject(object, JSONStringify(params));

  // Parse the serialized JSON result.
  if (json_result) {
    return JSONParse(json_result.value());
  } else {
    DLOG(WARNING) << "Could not create Runtime.RemoteObject";
    return JSONObject(new base::DictionaryValue());
  }
}

void ComponentConnector::SendEvent(const std::string& method,
                                   const JSONObject& params) {
  if (server_) {
    server_->OnEvent(method, params);
  }
}

// static
JSONObject ComponentConnector::ErrorResponse(const std::string& error_message) {
  JSONObject error_response(new base::DictionaryValue());
  error_response->SetString(kErrorMessage, error_message);
  return error_response.Pass();
}

}  // namespace debug
}  // namespace cobalt
