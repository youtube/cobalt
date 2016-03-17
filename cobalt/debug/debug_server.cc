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

#include "cobalt/debug/debug_server.h"

#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/values.h"
#include "cobalt/debug/debug_client.h"

namespace cobalt {
namespace debug {

namespace {
// Error response message field.
const char kErrorMessage[] = "error.message";
const char kResult[] = "result";
}  // namespace

DebugServer::Component::Component(const base::WeakPtr<DebugServer>& server)
    : server_(server) {}

DebugServer::Component::~Component() {
  // Remove all the commands added by this component.
  for (std::vector<std::string>::const_iterator it = command_methods_.begin();
       it != command_methods_.end(); ++it) {
    RemoveCommand(*it);
  }
}

void DebugServer::Component::AddCommand(const std::string& method,
                                        const Command& callback) {
  if (server_) {
    server_->AddCommand(method, callback);

    // Store the command methods added by this component, so we can remove on
    // destruction.
    command_methods_.push_back(method);
  }
}

void DebugServer::AddClient(DebugClient* client) { clients_.insert(client); }

void DebugServer::RemoveClient(DebugClient* client) { clients_.erase(client); }

void DebugServer::Component::RemoveCommand(const std::string& method) {
  if (server_) {
    server_->RemoveCommand(method);
  }
}

JSONObject DebugServer::Component::RunScriptCommand(const std::string& command,
                                                    const JSONObject& params) {
  if (server_) {
    return server_->RunScriptCommand(command, params);
  } else {
    return ErrorResponse("Not attached to debug server");
  }
}

bool DebugServer::Component::RunScriptFile(const std::string& filename) {
  if (server_) {
    return server_->RunScriptFile(filename);
  } else {
    return ErrorResponse("Not attached to debug server");
  }
}

void DebugServer::Component::SendEvent(const std::string& method,
                                       const JSONObject& params) {
  if (server_) {
    server_->OnEventInternal(method, params);
  }
}

// static
JSONObject DebugServer::Component::ErrorResponse(
    const std::string& error_message) {
  JSONObject error_response(new base::DictionaryValue());
  error_response->SetString(kErrorMessage, error_message);
  return error_response.Pass();
}

DebugServer::DebugServer(script::GlobalObjectProxy* global_object_proxy)
    : ALLOW_THIS_IN_INITIALIZER_LIST(script_runner_(new DebugScriptRunner(
          global_object_proxy,
          base::Bind(&DebugServer::OnEvent, base::Unretained(this))))),
      message_loop_(MessageLoop::current()),
      components_deleter_(&components_) {}

DebugServer::~DebugServer() {
  // Invalidate weak pointers explicitly, as the weak pointer factory won't be
  // destroyed until after the debug components that reference this object.
  InvalidateWeakPtrs();

  // Notify all clients.
  // |detach_reason| argument from set here:
  // https://developer.chrome.com/extensions/debugger#type-DetachReason
  const std::string detach_reason = "target_closed";
  for (std::set<DebugClient*>::iterator it = clients_.begin();
       it != clients_.end(); ++it) {
    (*it)->OnDetach(detach_reason);
  }
}

void DebugServer::AddComponent(scoped_ptr<DebugServer::Component> component) {
  // This object takes ownership of the component here. The objects referenced
  // in |components_| will be deleted on destruction.
  components_.push_back(component.release());
}

void DebugServer::SendCommand(const std::string& method,
                              const std::string& json_params,
                              CommandCallback callback) {
  CommandCallbackInfo callback_info(callback);
  message_loop_->PostTask(FROM_HERE, base::Bind(&DebugServer::DispatchCommand,
                                                base::Unretained(this), method,
                                                json_params, callback_info));
}

void DebugServer::DispatchCommand(std::string method, std::string json_params,
                                  CommandCallbackInfo callback_info) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Find the command in the registry and parse the JSON parameter string.
  // The |params| may be NULL, if the command takes no parameters.
  CommandRegistry::iterator iter = command_registry_.find(method);
  JSONObject params = JSONParse(json_params);
  JSONObject response;

  if (iter != command_registry_.end()) {
    // Everything is looking good so far - run the command function and take
    // ownership of the response object.
    response.reset(iter->second.Run(params).release());
  } else {
    DLOG(WARNING) << "Unknown command: " << method << ": " << json_params;
  }

  if (!response) {
    // Something went wrong, generate an error response.
    response.reset(new base::DictionaryValue());
    response->SetString("error.message", "Could not execute request.");
  }

  // Serialize the response object and run the callback.
  DCHECK(response);
  std::string json_response = JSONStringify(response);
  callback_info.message_loop->PostTask(
      FROM_HERE, base::Bind(callback_info.callback, json_response));
}

void DebugServer::OnEvent(const std::string& method,
                          const base::optional<std::string>& json_params) {
  for (std::set<DebugClient*>::iterator it = clients_.begin();
       it != clients_.end(); ++it) {
    (*it)->OnEvent(method, json_params);
  }
}

void DebugServer::OnEventInternal(const std::string& method,
                                  const JSONObject& params) {
  OnEvent(method, JSONStringify(params));
}

void DebugServer::AddCommand(const std::string& method,
                             const Command& callback) {
  DCHECK_EQ(command_registry_.count(method), 0);
  command_registry_[method] = callback;
}

void DebugServer::RemoveCommand(const std::string& method) {
  DCHECK_EQ(command_registry_.count(method), 1);
  command_registry_.erase(method);
}

JSONObject DebugServer::RunScriptCommand(const std::string& command,
                                         const JSONObject& params) {
  std::string json_params = params ? JSONStringify(params) : "";
  std::string json_result;
  bool success = script_runner_->RunCommand(command, json_params, &json_result);

  JSONObject response(new base::DictionaryValue());
  if (success) {
    JSONObject result = JSONParse(json_result);
    if (result) {
      response->Set(kResult, result.release());
    }
  } else {
    response->SetString(kErrorMessage, json_result);
  }
  return response.Pass();
}

bool DebugServer::RunScriptFile(const std::string& filename) {
  return script_runner_->RunScriptFile(filename);
}

}  // namespace debug
}  // namespace cobalt
