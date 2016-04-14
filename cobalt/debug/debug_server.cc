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

JSONObject DebugServer::Component::CreateRemoteObject(
    const script::OpaqueHandleHolder* object) {
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

DebugServer::DebugServer(script::GlobalObjectProxy* global_object_proxy,
                         const dom::CspDelegate* csp_delegate)
    : ALLOW_THIS_IN_INITIALIZER_LIST(script_runner_(new DebugScriptRunner(
          global_object_proxy, csp_delegate,
          base::Bind(&DebugServer::OnEvent, base::Unretained(this))))),
      message_loop_(MessageLoop::current()),
      components_deleter_(&components_),
      is_paused_(false),
      // No manual reset, not initially signaled.
      command_added_while_paused_(false, false) {}

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

base::optional<std::string> DebugServer::CreateRemoteObject(
    const script::OpaqueHandleHolder* object, const std::string& params) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return script_runner_->CreateRemoteObject(object, params);
}

void DebugServer::SendCommand(const std::string& method,
                              const std::string& json_params,
                              CommandCallback callback) {
  // Create a closure that will run the command and the response callback.
  // The task is either posted to the debug target (WebModule) thread if
  // that thread is running normally, or added to a queue of debugger tasks
  // being processed while paused.
  CommandCallbackInfo callback_info(callback);
  base::Closure command_and_callback_closure =
      base::Bind(&DebugServer::DispatchCommand, base::Unretained(this), method,
                 json_params, callback_info);

  if (is_paused_) {
    DispatchCommandWhilePaused(command_and_callback_closure);
  } else {
    message_loop_->PostTask(FROM_HERE, command_and_callback_closure);
  }
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

void DebugServer::DispatchCommandWhilePaused(
    const base::Closure& command_and_callback_closure) {
  // We are currently paused, so the debug target (WebModule) thread is
  // blocked and processing debugger commands locally. Add the command closure
  // to the queue of commands pending while paused and signal the blocked
  // thread to let it know there's something to do.
  base::AutoLock auto_lock(command_while_paused_lock_);
  DCHECK(is_paused_);
  commands_pending_while_paused_.push_back(command_and_callback_closure);
  command_added_while_paused_.Signal();
}

void DebugServer::HandlePause() {
  // Pauses JavaScript execution by blocking the debug target (WebModule)
  // thread while processing debugger commands that come in on other threads
  // (e.g. from DebugWebServer).

  // Must be called on the thread of the debug target (WebModule).
  DCHECK(thread_checker_.CalledOnValidThread());

  while (is_paused_) {
    command_added_while_paused_.Wait();

    while (true) {
      base::Closure task;
      {
        base::AutoLock auto_lock(command_while_paused_lock_);
        if (commands_pending_while_paused_.empty()) {
          break;
        }
        task = commands_pending_while_paused_.front();
        commands_pending_while_paused_.pop_front();
      }
      task.Run();
    }
  }
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

void DebugServer::SetPaused(bool is_paused) {
  // Must be called on the thread of the debug target (WebModule).
  DCHECK(thread_checker_.CalledOnValidThread());

  is_paused_ = is_paused;
  if (is_paused) {
    HandlePause();
  }
}

}  // namespace debug
}  // namespace cobalt
