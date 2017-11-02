// Copyright 2015 Google Inc. All Rights Reserved.
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

void DebugServer::AddClient(DebugClient* client) { clients_.insert(client); }

void DebugServer::RemoveClient(DebugClient* client) { clients_.erase(client); }

DebugServer::DebugServer(script::GlobalEnvironment* global_environment,
                         const dom::CspDelegate* csp_delegate)
    : ALLOW_THIS_IN_INITIALIZER_LIST(script_runner_(new DebugScriptRunner(
          global_environment, csp_delegate,
          base::Bind(&DebugServer::OnEventInternal, base::Unretained(this))))),
      message_loop_(MessageLoop::current()),
      is_paused_(false),
      // No manual reset, not initially signaled.
      command_added_while_paused_(false, false) {}

DebugServer::~DebugServer() {
  // Notify all clients.
  // |detach_reason| argument from set here:
  // https://developer.chrome.com/extensions/debugger#type-DetachReason
  const std::string detach_reason = "target_closed";
  for (std::set<DebugClient*>::iterator it = clients_.begin();
       it != clients_.end(); ++it) {
    (*it)->OnDetach(detach_reason);
  }
}

base::optional<std::string> DebugServer::CreateRemoteObject(
    const script::ValueHandleHolder* object, const std::string& params) {
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

void DebugServer::OnEvent(const std::string& method, const JSONObject& params) {
  OnEventInternal(method, JSONStringify(params));
}

void DebugServer::OnEventInternal(
    const std::string& method, const base::optional<std::string>& json_params) {
  for (std::set<DebugClient*>::iterator it = clients_.begin();
       it != clients_.end(); ++it) {
    (*it)->OnEvent(method, json_params);
  }
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
