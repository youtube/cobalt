// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/debug/backend/debug_dispatcher.h"

#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/values.h"
#include "cobalt/debug/debug_client.h"

namespace cobalt {
namespace debug {
namespace backend {

DebugDispatcher::DebugDispatcher(script::GlobalEnvironment* global_environment,
                                 const dom::CspDelegate* csp_delegate,
                                 script::ScriptDebugger* script_debugger)
    : script_debugger_(script_debugger),
      ALLOW_THIS_IN_INITIALIZER_LIST(script_runner_(
          new DebugScriptRunner(global_environment, csp_delegate,
                                base::Bind(&DebugDispatcher::SendEventInternal,
                                           base::Unretained(this))))),
      message_loop_(MessageLoop::current()),
      is_paused_(false),
      // No manual reset, not initially signaled.
      command_added_while_paused_(false, false) {}

DebugDispatcher::~DebugDispatcher() {
  // Notify all clients.
  // |detach_reason| argument from set here:
  // https://developer.chrome.com/extensions/debugger#type-DetachReason
  const std::string detach_reason = "target_closed";
  for (std::set<DebugClient*>::iterator it = clients_.begin();
       it != clients_.end(); ++it) {
    (*it)->OnDetach(detach_reason);
  }
  for (DomainRegistry::iterator it = domain_registry_.begin();
       it != domain_registry_.end(); ++it) {
    RemoveDomain(it->first);
  }
}

void DebugDispatcher::AddClient(DebugClient* client) {
  clients_.insert(client);
}

void DebugDispatcher::RemoveClient(DebugClient* client) {
  clients_.erase(client);
}

JSONObject DebugDispatcher::CreateRemoteObject(
    const script::ValueHandleHolder* object) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Use default values for the parameters in the JS createRemoteObjectCallback.
  static const char* kDefaultParams = "{}";

  // This will execute a JavaScript function to create a Runtime.Remote object
  // that describes the opaque JavaScript object.
  base::optional<std::string> json_result =
      script_runner_->CreateRemoteObject(object, kDefaultParams);

  // Parse the serialized JSON result.
  if (json_result) {
    return JSONParse(json_result.value());
  } else {
    DLOG(WARNING) << "Could not create Runtime.RemoteObject";
    return JSONObject(new base::DictionaryValue());
  }
}

void DebugDispatcher::SendCommand(const Command& command) {
  // Create a closure that will run the command and the response callback.
  // The task is either posted to the debug target (WebModule) thread if
  // that thread is running normally, or added to a queue of debugger tasks
  // being processed while paused.
  base::Closure command_closure = base::Bind(&DebugDispatcher::DispatchCommand,
                                             base::Unretained(this), command);

  if (is_paused_) {
    DispatchCommandWhilePaused(command_closure);
  } else {
    message_loop_->PostTask(FROM_HERE, command_closure);
  }
}

void DebugDispatcher::DispatchCommand(Command command) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DomainRegistry::iterator iter = domain_registry_.find(command.GetDomain());
  if (iter != domain_registry_.end() && iter->second.Run(command)) {
    // The component command implementation ran and sends its own response.
    return;
  }

  // The component didn't have a native implementation. Try to run a
  // JavaScript implementation (which the component would have loaded at the
  // same time as it registered its domain command handler).
  JSONObject response =
      RunScriptCommand(command.GetMethod(), command.GetParams());
  if (response) {
    command.SendResponse(response);
  } else {
    DLOG(WARNING) << "Command not implemented: " << command.GetMethod();
    command.SendErrorResponse("Command not implemented");
  }
}

void DebugDispatcher::DispatchCommandWhilePaused(
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

void DebugDispatcher::HandlePause() {
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

void DebugDispatcher::SendEvent(const std::string& method,
                                const JSONObject& params) {
  base::optional<std::string> json_params;
  if (params) json_params = JSONStringify(params);
  SendEventInternal(method, json_params);
}

void DebugDispatcher::SendScriptEvent(const std::string& event,
                                      const std::string& method,
                                      const std::string& json_params) {
  script::ScriptDebugger::ScopedPauseOnExceptionsState no_pause(
      script_debugger_, script::ScriptDebugger::kNone);

  std::string json_result;
  bool success = script_runner_->RunCommand(method, json_params, &json_result);

  if (!success) {
    DLOG(ERROR) << "Script event failed (" << method << "): " << json_result;
  } else if (json_result.empty()) {
    DLOG(ERROR) << "Script event method not defined: " << method;
  } else {
    SendEventInternal(event, json_result);
  }
}

void DebugDispatcher::SendEventInternal(
    const std::string& method, const base::optional<std::string>& json_params) {
  for (std::set<DebugClient*>::iterator it = clients_.begin();
       it != clients_.end(); ++it) {
    (*it)->OnEvent(method, json_params);
  }
}

void DebugDispatcher::AddDomain(const std::string& domain,
                                const CommandHandler& handler) {
  DCHECK_EQ(domain_registry_.count(domain), 0);
  domain_registry_[domain] = handler;
}

void DebugDispatcher::RemoveDomain(const std::string& domain) {
  DCHECK_EQ(domain_registry_.count(domain), 1);
  domain_registry_.erase(domain);
}

JSONObject DebugDispatcher::RunScriptCommand(const std::string& method,
                                             const std::string& json_params) {
  script::ScriptDebugger::ScopedPauseOnExceptionsState no_pause(
      script_debugger_, script::ScriptDebugger::kNone);

  std::string json_result;
  bool success = script_runner_->RunCommand(method, json_params, &json_result);

  JSONObject response(new base::DictionaryValue());
  if (json_result.empty()) {
    // An empty result means the method isn't implemented so return no response.
    response.reset();
  } else if (!success) {
    // On error, |json_result| is the error message.
    response->SetString("error.message", json_result);
  } else {
    JSONObject result = JSONParse(json_result);
    if (result) {
      response->Set("result", result.release());
    }
  }
  return response.Pass();
}

bool DebugDispatcher::RunScriptFile(const std::string& filename) {
  script::ScriptDebugger::ScopedPauseOnExceptionsState no_pause(
      script_debugger_, script::ScriptDebugger::kNone);
  return script_runner_->RunScriptFile(filename);
}

void DebugDispatcher::SetPaused(bool is_paused) {
  // Must be called on the thread of the debug target (WebModule).
  DCHECK(thread_checker_.CalledOnValidThread());

  is_paused_ = is_paused;
  if (is_paused) {
    HandlePause();
  }
}

}  // namespace backend
}  // namespace debug
}  // namespace cobalt
