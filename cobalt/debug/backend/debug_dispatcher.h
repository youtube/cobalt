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
#ifndef COBALT_DEBUG_BACKEND_DEBUG_DISPATCHER_H_
#define COBALT_DEBUG_BACKEND_DEBUG_DISPATCHER_H_

#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "cobalt/debug/backend/debug_script_runner.h"
#include "cobalt/debug/command.h"
#include "cobalt/debug/debug_client.h"
#include "cobalt/debug/json_object.h"
#include "cobalt/dom/csp_delegate.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/script_debugger.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/value_handle.h"

namespace cobalt {
namespace debug {
namespace backend {

// Dispatches debug commands to the agent that implements the particular
// command. This is the core of the debugging system. The overall architecture
// of the debugging system is documented here:
// https://docs.google.com/document/d/1_PV0auxlFBHJbPdMpCnyUvC8IslEyzuTar79qmniO7w/
//
// A single instance of this class is owned by the DebugModule object, which is
// in turn expected to be instanced once by the main WebModule (where the page
// being debugged is loaded). Clients that want to connect to the debug
// dispatcher should construct an instance of a DebugClient, passing in a
// pointer to the DebugDispatcher.
//
// All debugging commands and events are routed through the DebugDispatcher.
// The protocol is defined here:
// https://chromedevtools.github.io/devtools-protocol/1-3
//
// Debugging commands are sent via the |SendCommand| method, which runs
// a command asynchronously on the main WebModule thread, and returns its result
// via a callback on the client's message loop. DebugDispatcher also sends
// events separately from command results to the clients, but events are sent on
// the main WebModule thread, and it is the responsibility of the recipient to
// post it to its own thread to be processed as needed.
//
// This class is not intended to implement any debugging commands directly -
// it is expected that the functionality of the various debugging command
// domains will be provided by agent objects that register the domains they
// implement with this object using the |AddDomain| and |RemoveDomain| methods.
//
// The DebugDispatcher must be created on the same message loop as the WebModule
// it attaches to, so that all debugging can be synchronized with the execution
// of the WebModule.
//
// The class also supports pausing of execution (e.g. for breakpoints) by
// blocking the WebModule thread. While paused, |SendCommand| can still be
// called from another thread (e.g. a web server) and will continue to execute
// commands on the WebModule thread. This functionality is used by calling the
// |SetPaused| function.

class DebugDispatcher {
 public:
  // Move-only set of all attached clients. Allows the set of clients to be
  // transferred through |DebuggerState| to a new instance of |DebugDispatcher|
  // when navigating. Attached clients will be notified when the set is finally
  // destroyed.
  class ClientsSet {
   public:
    ClientsSet() = default;
    ClientsSet(const ClientsSet&) = delete;
    ClientsSet(ClientsSet&&) = default;
    ~ClientsSet();

    ClientsSet& operator=(ClientsSet&) = delete;
    ClientsSet& operator=(ClientsSet&&) = default;

    // Proxy methods just for what we need to interact with the actual set.
    void insert(DebugClient* client) { clients_.insert(client); }
    void erase(DebugClient* client) { clients_.erase(client); }
    std::set<DebugClient*>::size_type size() { return clients_.size(); }
    std::set<DebugClient*>::iterator begin() { return clients_.begin(); }
    std::set<DebugClient*>::iterator end() { return clients_.end(); }

   private:
    std::set<DebugClient*> clients_;
  };

  // A command execution function stored in the domain registry. If the command
  // is supported, ownership of the command parameter should be kept and used to
  // send the response. If the command is not supported, the command should be
  // returned so the dispatcher can try calling a JS fallback implementation.
  typedef base::Callback<base::Optional<Command>(Command command)>
      CommandHandler;

  DebugDispatcher(script::ScriptDebugger* script_debugger,
                  DebugScriptRunner* script_runner);

  // Support moving the clients through |DebuggerState| to a new instance.
  ClientsSet ReleaseClients();
  void RestoreClients(ClientsSet clients);

  // Adds a client to this object. This object does not own the client, but
  // notifies it when debugging events occur, or when this object is destroyed.
  void AddClient(DebugClient* client);

  // Removes a client from this object.
  void RemoveClient(DebugClient* client);

  // Adds a domain to the domain registry. This will be called by agents
  // providing the protocol command implementations.
  void AddDomain(const std::string& domain, const CommandHandler& handler);

  // Removes a domain from the domain registry. This will be called by
  // agents providing the protocol command implementations.
  void RemoveDomain(const std::string& domain);

  // Sends a protocol event to the frontend.
  void SendEvent(const std::string& method,
                 const JSONObject& params = JSONObject());

  // Sends a protocol event to the frontend.
  void SendEvent(const std::string& method, const std::string& params);

  // Calls |method| in |script_runner_| and creates a response object from
  // the result.
  JSONObject RunScriptCommand(const std::string& method,
                              const std::string& json_params);

  // Loads JavaScript from file and executes the contents.
  bool RunScriptFile(const std::string& filename);

  // Called to send a protocol command through this debug dispatcher. May be
  // called from any thread - the command will be run on the dispatcher's
  // message loop, and the response will be sent to the callback and message
  // loop held in the command object.
  void SendCommand(Command command);

  // Sets or unsets the paused state and calls |HandlePause| if set.
  // Must be called on the debug target (WebModule) thread.
  void SetPaused(bool is_paused);

  // Returns the engine-specific script debugger.
  script::ScriptDebugger* script_debugger() const { return script_debugger_; }

 private:
  // A registry of commands, mapping method names from the protocol
  // to command handlers implemented by the debug agents.
  typedef std::map<std::string, CommandHandler> DomainRegistry;

  // Queue of command/response closures. Used to process debugger commands
  // received while script execution is paused.
  typedef std::deque<base::Closure> PausedTaskQueue;

  // Destructor should only be called by |std::unique_ptr<DebugDispatcher>|.
  ~DebugDispatcher();

  // Dispatches a command received via |SendCommand| by looking up the method
  // name in the command registry and running the corresponding function.
  // The response callback will be run on the message loop specified in the
  // info structure with the result as an argument.
  void DispatchCommand(Command command);

  // Called by |SendCommand| if a debugger command is received while script
  // execution is paused.
  void DispatchCommandWhilePaused(const base::Closure& command_and_response);

  // Called by |SetPaused| to pause script execution in the debug target
  // (WebModule) by blocking its thread while continuing to process debugger
  // commands from other threads. Must be called on the WebModule thread.
  void HandlePause();

  // Engine-specific debugger implementation.
  script::ScriptDebugger* script_debugger_;

  // Runs backend JavaScript.
  DebugScriptRunner* script_runner_;

  // Clients connected to this dispatcher.
  ClientsSet clients_;

  // Map of commands, indexed by method name.
  DomainRegistry domain_registry_;

  // Message loop of the web module this debug dispatcher is attached to, and
  // thread checker to check access.
  base::SingleThreadTaskRunner* task_runner_;
  THREAD_CHECKER(thread_checker_);

  // Whether the debug target (WebModule) is currently paused.
  // See description of |SetPaused| and |HandlePause| above.
  bool is_paused_;

  // Used to signal the debug target (WebModule) thread that there are
  // debugger commands to process while paused.
  base::WaitableEvent command_added_while_paused_;

  // Queue of pending debugger commands received while paused.
  PausedTaskQueue commands_pending_while_paused_;

  // Lock to synchronize access to |commands_pending_while_paused|.
  base::Lock command_while_paused_lock_;

  friend std::unique_ptr<DebugDispatcher>::deleter_type;
};

}  // namespace backend
}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_BACKEND_DEBUG_DISPATCHER_H_
