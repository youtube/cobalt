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
#ifndef COBALT_DEBUG_DEBUG_SERVER_H_
#define COBALT_DEBUG_DEBUG_SERVER_H_

#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "cobalt/debug/debug_script_runner.h"
#include "cobalt/debug/json_object.h"
#include "cobalt/dom/csp_delegate.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/value_handle.h"

namespace cobalt {
namespace debug {

// Forward declaration of the client class that can connect to a DebugServer.
class DebugClient;

// The core of the debugging system. The overall architecture of the debugging
// system is documented here:
// https://docs.google.com/document/d/1lZhrBTusQZJsacpt21J3kPgnkj7pyQObhFqYktvm40Y/
//
// A single instance of this class is owned by the DebugServerModule object,
// which is in turn expected to be instanced once by the main WebModule.
// Clients that want to connect to the debug server should construct
// an instance of a DebugClient, passing in a pointer to the DebugServer.
//
// All client debugging commands and events are routed through an object of
// this class. The protocol is defined here:
// https://developer.chrome.com/devtools/docs/protocol/1.1/index
// Debugging commands are sent via the |SendCommand| method, which runs
// a command asynchronously and returns its result via a callback.
// DebugServer will also send events separately from command results, using
// the callbacks specified in the constructor.
//
// This class is not intended to implement any debugging commands directly -
// it is expected that the functionality of the various debugging command
// domains will be provided by other objects that add commands to this object
// using the |AddCommand| and |RemoveCommand| methods.
//
// The DebugServer must be created on the same message loop as the WebModule it
// attaches to, so that all debugging can be synchronized with the execution of
// the WebModule.
// DebugServer handles this synchronization and (de-)serialization of JSON
// parameters. |SendCommand| may be called from any thread: the command itself
// will be executed on the message loop this object was constructed on, which
// must be the message loop of the WebModule containing the debug targets this
// instance will attach to; the command callback will be posted to the message
// loop |SendCommand| was called on. Command parameters are specified as
// JSON strings that are parsed to base::DictionaryValue objects before
// passing to the debug targets. Event parameters are received from the debug
// targets as base::DictionaryValue objects and serialized to JSON strings
// that are passed to the appropriate event handlers. The event
// callbacks (onEvent/onDetach) will be run synchronously on the message loop
// of the debug target that sent them - it is the responsibility of the client
// to re-post to its own message loop if necessary.
//
// The class also supports pausing of execution by blocking the WebModule
// thread. While paused, |SendCommand| can still be called from another thread
// (e.g. a web server) and will continue to execute commands on the WebModule
// thread. This functionality is used by calling the |SetPaused| function.

class DebugServer {
 public:
  // Callback to pass a command response to the client.
  typedef base::Callback<void(const base::optional<std::string>& response)>
      CommandCallback;

  // A command execution function stored in the command registry.
  typedef base::Callback<JSONObject(const JSONObject& params)> Command;

  DebugServer(script::GlobalEnvironment* global_environment,
              const dom::CspDelegate* csp_delegate);

  // Adds a client to this object. This object does not own the client, but
  // notify it when debugging events occur, or when this object is destroyed.
  void AddClient(DebugClient* client);

  // Adds a command to the command registry. This will be called by objects
  // providing the debug command implementations.
  void AddCommand(const std::string& method, const Command& callback);

  // Creates a Runtime.RemoteObject corresponding to an opaque JS object.
  base::optional<std::string> CreateRemoteObject(
      const script::ValueHandleHolder* object, const std::string& params);

  // Called by the debug components when an event occurs.
  // Serializes the method and params object to a JSON string and
  // calls |OnEventInternal|.
  void OnEvent(const std::string& method, const JSONObject& params);

  // Removes a client from this object.
  void RemoveClient(DebugClient* client);

  // Removes a command from the command registry. This will be called by objects
  // providing the debug command implementations.
  void RemoveCommand(const std::string& method);

  // Calls |command| in |script_runner_| and creates a response object from
  // the result.
  JSONObject RunScriptCommand(const std::string& command,
                              const JSONObject& params);

  // Loads JavaScript from file and executes the contents.
  bool RunScriptFile(const std::string& filename);

  // Called to send a command to this debug server, with a callback for the
  // response. The command is one from the Chrome Remote Debugging Protocol:
  // https://developer.chrome.com/devtools/docs/protocol/1.1/index
  // May be called from any thread - the command will be run on |message_loop_|
  // and the callback will be run on the message loop of the caller.
  void SendCommand(const std::string& method, const std::string& json_params,
                   CommandCallback callback);

  // Sets or unsets the paused state and calls |HandlePause| if set.
  // Must be called on the debug target (WebModule) thread.
  void SetPaused(bool is_paused);

 private:
  // A registry of commands, mapping method names from the protocol
  // to command callbacks implemented by the debug components.
  typedef std::map<std::string, Command> CommandRegistry;

  // Type to store a command callback and the current message loop.
  struct CommandCallbackInfo {
    explicit CommandCallbackInfo(const CommandCallback& command_callback)
        : callback(command_callback), message_loop(MessageLoop::current()) {}

    CommandCallback callback;
    MessageLoop* message_loop;
  };

  // Queue of command/response closures. Used to process debugger commands
  // received while script execution is paused.
  typedef std::deque<base::Closure> PausedTaskQueue;

  // Destructor should only be called by |scoped_ptr<DebugServer>|.
  ~DebugServer();

  // Called by |script_runner_| and |OnEvent|. Notifies the clients.
  void OnEventInternal(const std::string& method,
                       const base::optional<std::string>& params);

  // Dispatches a command received via |SendCommand| by looking up the method
  // name in the command registry and running the corresponding function.
  // The command callback will be run on the message loop specified in the info
  // structure with the result as an argument.
  void DispatchCommand(std::string method, std::string json_params,
                       CommandCallbackInfo callback_info);

  // Called by |SendCommand| if a debugger command is received while script
  // execution is paused.
  void DispatchCommandWhilePaused(const base::Closure& command_and_response);

  // Called by |SetPaused| to pause script execution in the debug target
  // (WebModule) by blocking its thread while continuing to process debugger
  // commands from other threads. Must be called on the WebModule thread.
  void HandlePause();

  // Used to run JavaScript commands with persistent state and receive events
  // from JS.
  scoped_refptr<DebugScriptRunner> script_runner_;

  // Clients connected to this server.
  std::set<DebugClient*> clients_;

  // Map of commands, indexed by method name.
  CommandRegistry command_registry_;

  // Message loop of the web module this debug server is attached to, and
  // thread checker to check access.
  MessageLoop* message_loop_;
  base::ThreadChecker thread_checker_;

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

  friend class scoped_ptr<DebugServer>;
};

}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_DEBUG_SERVER_H_
