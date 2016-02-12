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

namespace cobalt {
namespace debug {

namespace {
// Error response message field.
const char kError[] = "error.message";
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

void DebugServer::Component::RemoveCommand(const std::string& method) {
  if (server_) {
    server_->RemoveCommand(method);
  }
}

void DebugServer::Component::SendNotification(const std::string& method,
                                              const JSONObject& params) {
  if (server_) {
    server_->OnNotification(method, params);
  }
}

// static
JSONObject DebugServer::Component::ErrorResponse(
    const std::string& error_message) {
  JSONObject error_response(new base::DictionaryValue());
  error_response->SetString(kError, error_message);
  return error_response.Pass();
}

DebugServer::DebugServer(
    const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy,
    const OnEventCallback& on_event_callback,
    const OnDetachCallback& on_detach_callback)
    : message_loop_proxy_(message_loop_proxy),
      on_event_callback_(on_event_callback),
      on_detach_callback_(on_detach_callback),
      components_deleter_(&components_) {}

DebugServer::~DebugServer() {
  // Invalidate weak pointers explicitly, as the weak pointer factory won't be
  // destroyed until after the debug components that reference this object.
  InvalidateWeakPtrs();
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
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&DebugServer::DispatchCommand, base::Unretained(this), method,
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
  callback_info.message_loop_proxy->PostTask(
      FROM_HERE, base::Bind(callback_info.callback, json_response));
}

void DebugServer::OnNotification(const std::string& method,
                                 const JSONObject& params) {
  // Serialize the event parameters and pass to the external callback.
  const std::string json_params = JSONStringify(params);
  on_event_callback_.Run(method, json_params);
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

}  // namespace debug
}  // namespace cobalt
