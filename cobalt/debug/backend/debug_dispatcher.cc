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

DebugDispatcher::DebugDispatcher(script::ScriptDebugger* script_debugger,
                                 DebugScriptRunner* script_runner)
    : script_debugger_(script_debugger),
      script_runner_(script_runner),
      task_runner_(base::MessageLoop::current()->task_runner()),
      is_paused_(false),
      // No manual reset, not initially signaled.
      command_added_while_paused_(
          base::WaitableEvent::ResetPolicy::AUTOMATIC,
          base::WaitableEvent::InitialState::NOT_SIGNALED) {}

DebugDispatcher::~DebugDispatcher() {
  DCHECK(domain_registry_.empty())
      << domain_registry_.begin()->first << " domain still registered.";
  for (DomainRegistry::iterator it = domain_registry_.begin();
       it != domain_registry_.end(); ++it) {
    RemoveDomain(it->first);
  }
}

DebugDispatcher::ClientsSet::~ClientsSet() {
  // Notify all clients.
  // |detach_reason| argument from set here:
  // https://developer.chrome.com/extensions/debugger#type-DetachReason
  const std::string detach_reason = "target_closed";
  for (auto* client : clients_) {
    client->OnDetach(detach_reason);
  }
}

DebugDispatcher::ClientsSet DebugDispatcher::ReleaseClients() {
  for (auto* client : clients_) {
    client->SetDispatcher(nullptr);
  }
  return std::move(clients_);
}

void DebugDispatcher::RestoreClients(ClientsSet clients) {
  clients_ = std::move(clients);
  for (auto* client : clients_) {
    client->SetDispatcher(this);
  }
}

void DebugDispatcher::AddClient(DebugClient* client) {
  clients_.insert(client);
}

void DebugDispatcher::RemoveClient(DebugClient* client) {
  clients_.erase(client);
}

void DebugDispatcher::SendCommand(Command command) {
  // Create a closure that will run the command and the response callback.
  // The task is either posted to the debug target (WebModule) thread if
  // that thread is running normally, or added to a queue of debugger tasks
  // being processed while paused.
  base::Closure command_closure =
      base::Bind(&DebugDispatcher::DispatchCommand, base::Unretained(this),
                 base::Passed(std::move(command)));

  if (is_paused_) {
    DispatchCommandWhilePaused(command_closure);
  } else {
    task_runner_->PostTask(FROM_HERE, command_closure);
  }
}

void DebugDispatcher::DispatchCommand(Command command) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // This workaround allows both the overlay console and remote DevTools to
  // connect at the same time. Each time a client sends the "Runtime.enable"
  // command, we first inject a "Runtime.disable" command so that the V8
  // Inspector will send the "Runtime.executionContextCreated" event for every
  // "Runtime.enable" command rather than just for the first one.
  if (command.GetMethod() == "Runtime.enable") {
    DispatchCommand(Command::IgnoreResponse("Runtime.disable"));
  }

  DomainRegistry::iterator iter = domain_registry_.find(command.GetDomain());
  if (iter == domain_registry_.end()) {
    // If the domain isn't even registered, return an error without even trying
    // to run a C++ or JS command implementation. This helps avoid problems when
    // commands are received during navigation before agents are ready.
    std::string err = command.GetDomain() + " domain not supported";
    DLOG(WARNING) << err << " (" << command.GetMethod() << ")";
    command.SendErrorResponse(Command::kMethodNotFound, err);
    return;
  } else {
    auto opt_command = iter->second.Run(std::move(command));
    // The agent command implementation kept the command to send the response.
    if (!opt_command) return;
    command = std::move(*opt_command);
  }

  // The agent didn't have a native implementation. Try to run a
  // JavaScript implementation (which the agent would have loaded at the
  // same time as it registered its domain command handler).
  JSONObject response =
      RunScriptCommand(command.GetMethod(), command.GetParams());
  if (response) {
    command.SendResponse(response);
  } else {
    DLOG(WARNING) << "Command not implemented: " << command.GetMethod();
    command.SendErrorResponse(Command::kMethodNotFound,
                              "Command not implemented");
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  LOG(INFO) << "JavaScript execution paused.";
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
  LOG(INFO) << "JavaScript execution resumed.";
}

void DebugDispatcher::SendEvent(const std::string& method,
                                const JSONObject& params) {
  SendEvent(method, JSONStringify(params));
}

void DebugDispatcher::SendEvent(const std::string& method,
                                const std::string& json_params) {
  DCHECK(!json_params.empty());
  DCHECK_EQ(json_params.front(), '{');
  DCHECK_EQ(json_params.back(), '}');
  for (auto* client : clients_) {
    client->OnEvent(method, json_params);
  }
}

void DebugDispatcher::AddDomain(const std::string& domain,
                                const CommandHandler& handler) {
  DCHECK_EQ(domain_registry_.count(domain), size_t(0));
  domain_registry_[domain] = handler;
}

void DebugDispatcher::RemoveDomain(const std::string& domain) {
  DCHECK_EQ(domain_registry_.count(domain), size_t(1));
  domain_registry_.erase(domain);
}

JSONObject DebugDispatcher::RunScriptCommand(const std::string& method,
                                             const std::string& json_params) {
  script::ScriptDebugger::ScopedPauseOnExceptionsState no_pause(
      script_debugger_, script::ScriptDebugger::kNone);

  std::string json_result;
  bool success = script_runner_->RunCommand(method, json_params, &json_result);

  JSONObject response(new base::DictionaryValue());
  if (success) {
    JSONObject result = JSONParse(json_result);
    if (result) {
      response->Set("result", std::unique_ptr<base::Value>(result.release()));
    }
  } else if (json_result.empty()) {
    // Unimplemented commands aren't successful, and |json_result| is empty.
    response.reset();
  } else {
    // On error, |json_result| is the error message.
    response->SetString("error.message", json_result);
  }
  return response;
}

bool DebugDispatcher::RunScriptFile(const std::string& filename) {
  script::ScriptDebugger::ScopedPauseOnExceptionsState no_pause(
      script_debugger_, script::ScriptDebugger::kNone);
  return script_runner_->RunScriptFile(filename);
}

void DebugDispatcher::SetPaused(bool is_paused) {
  // Must be called on the thread of the debug target (WebModule).
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  is_paused_ = is_paused;
  if (is_paused) {
    HandlePause();
  }
}

}  // namespace backend
}  // namespace debug
}  // namespace cobalt
