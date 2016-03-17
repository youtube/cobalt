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
#ifndef COBALT_DEBUG_DEBUG_SERVER_H_
#define COBALT_DEBUG_DEBUG_SERVER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/threading/thread_checker.h"
#include "cobalt/debug/debug_script_runner.h"
#include "cobalt/debug/json_object.h"
#include "cobalt/script/global_object_proxy.h"

namespace cobalt {
namespace debug {

// Forward declaration of the client class that can connect to a DebugServer.
class DebugClient;

// The core of the debugging system. The overall architecture of the debugging
// system is documented here:
// https://docs.google.com/document/d/1lZhrBTusQZJsacpt21J3kPgnkj7pyQObhFqYktvm40Y/
//
// A single lazily created instance of this class is owned by the main
// WebModule. Clients that want to connect to the debug server should construct
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
// This class knows nothing about the external modules it may be connecting to.
// The actual functionality is implemented by objects of classes derived from
// the |Component| class.
//
// DebugServer handles synchronization and (de-)serialization of JSON
// parameters. Commands can be executed from any thread: the command itself
// will be posted to the message loop specified in the constructor, which must
// be the message loop of the WebModule containing the debug targets this
// instance will attach to; the command callback will be posted to the message
// loop the command was executed from. Command parameters are specified as
// JSON strings that are parsed to base::DictionaryValue objects before
// passing to the debug targets. Event parameters are received from the debug
// targets as base::DictionaryValue objects and serialized to JSON strings
// that are passed to the appropriate event handlers. The event
// callbacks (onEvent/onDetach) will be run synchronously on the message loop
// of the debug target that sent them - it is the responsibility of the client
// to re-post to its own message loop if necessary.

class DebugServer : public base::SupportsWeakPtr<DebugServer> {
 public:
  // Callback to pass a command response to the client.
  typedef base::Callback<void(const base::optional<std::string>& response)>
      CommandCallback;

  // A command execution function stored in the command registry.
  typedef base::Callback<JSONObject(const JSONObject& params)> Command;

  // Objects of this class are used to provide events and commands.
  class Component {
   public:
    explicit Component(const base::WeakPtr<DebugServer>& server);
    virtual ~Component();

   protected:
    // Adds a command function to the registry of the |DebugServer|
    // referenced by this object.
    void AddCommand(const std::string& method,
                    const DebugServer::Command& callback);

    // Removes a command function from the registry of the |DebugServer|
    // referenced by this object.
    void RemoveCommand(const std::string& method);

    // Runs a JavaScript command with JSON parameters and returns a JSON result.
    JSONObject RunScriptCommand(const std::string& command,
                                const JSONObject& params);

    // Loads JavaScript from file and runs the contents. Used to populate the
    // JavaScript object created by |script_runner_| with commands.
    bool RunScriptFile(const std::string& filename);

    // Sends an event to the |DebugServer| referenced by this object.
    void SendEvent(const std::string& method, const JSONObject& params);

    bool CalledOnValidThread() const {
      return thread_checker_.CalledOnValidThread();
    }

    // Generates an error response that can be returned by any command handler.
    static JSONObject ErrorResponse(const std::string& error_message);

   private:
    base::WeakPtr<DebugServer> server_;
    base::ThreadChecker thread_checker_;
    std::vector<std::string> command_methods_;
  };

  // Adds a debug component to this object. This object takes ownership.
  void AddComponent(scoped_ptr<Component> component);

  // Adds a client to this object. This object does not own the client, but
  // notify it when debugging events occur, or when this object is destroyed.
  void AddClient(DebugClient* client);

  // Removes a client from this object.
  void RemoveClient(DebugClient* client);

  // Called to send a command to this debug server, with a callback for the
  // response. The command is one from the Chrome Remote Debugging Protocol:
  // https://developer.chrome.com/devtools/docs/protocol/1.1/index
  // May be called from any thread - the command will be run on the
  // message loop specified in the constructor and the callback will be run on
  // the message loop of the caller.
  void SendCommand(const std::string& method, const std::string& json_params,
                   CommandCallback callback);

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

  // Constructor should only be called by |DebugServerBuilder|, which will
  // ensure it is created on the specified thread, which must be the message
  // loop of the WebModule this debug server is attached to, so that
  // operations are executed synchronously with the other methods of that
  // WebModule.
  explicit DebugServer(script::GlobalObjectProxy* global_object_proxy);

  // Destructor should only be called by |scoped_ptr<DebugServer>|.
  ~DebugServer();

  // Called by |script_runner_| and |OnEventInternal|. Notifies the clients.
  void OnEvent(const std::string& method,
               const base::optional<std::string>& params);

  // Callback to receive events from the debug components.
  // Serializes the method and params object to a JSON string and
  // calls |OnEvent|.
  void OnEventInternal(const std::string& method, const JSONObject& params);

  // Dispatches a command received via |SendCommand| by looking up the method
  // name in the command registry and running the corresponding function.
  // The command callback will be run on the message loop specified in the info
  // structure with the result as an argument.
  void DispatchCommand(std::string method, std::string json_params,
                       CommandCallbackInfo callback_info);

  // Adds a command to the command registry. This will be called by the
  // objects referenced by |components_|.
  void AddCommand(const std::string& method, const Command& callback);

  // Removes a command from the command registry. This will be called by the
  // objects referenced by |components_|.
  void RemoveCommand(const std::string& method);

  // Calls |command| in |script_runner_| and creates a response object from
  // the result.
  JSONObject RunScriptCommand(const std::string& command,
                              const JSONObject& params);

  // Loads JavaScript from file and executes the contents.
  bool RunScriptFile(const std::string& filename);

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

  // Debug components owned by this object.
  std::vector<Component*> components_;
  STLElementDeleter<std::vector<Component*> > components_deleter_;

  friend class DebugServerBuilder;
  friend class scoped_ptr<DebugServer>;
};

}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_DEBUG_SERVER_H_
