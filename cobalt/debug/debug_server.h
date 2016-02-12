/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/threading/thread_checker.h"
#include "cobalt/debug/json_object.h"

namespace cobalt {
namespace debug {

// The core of the debugging system. The overall architecture of the debugging
// system is documented here:
// https://docs.google.com/document/d/1lZhrBTusQZJsacpt21J3kPgnkj7pyQObhFqYktvm40Y/
//
// When a client (either local or remote) wants to connect to the debugger, it
// should create an instance of this class.
//
// All client debugging commands and event notifications are routed through an
// object of this class. The protocol is defined here:
// https://developer.chrome.com/devtools/docs/protocol/1.1/index
// Debugging commands are sent via the |SendCommand| method, which runs
// a command asynchronously and returns its result via a callback.
// DebugServer will also send notifications when debugging events occur, using
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
// that are passed to the appropriate event handlers. The event notification
// callbacks (onEvent/onDetach) will be run synchronously on the message loop
// of the debug target that sent them - it is the responsibility of the client
// to re-post to its own message loop if necessary.

class DebugServer : public base::SupportsWeakPtr<DebugServer> {
 public:
  // Callback to pass a command response to the client.
  typedef base::Callback<void(const base::optional<std::string>& response)>
      CommandCallback;

  // Callback to notify the client when an event occurs.
  typedef base::Callback<void(const std::string& method,
                              const base::optional<std::string>& params)>
      OnEventCallback;

  // Callback to notify the client when the debugger detaches.
  typedef base::Callback<void(const std::string& reason)> OnDetachCallback;

  // A command execution function stored in the command registry.
  typedef base::Callback<JSONObject(const JSONObject& params)> Command;

  // Objects of this class are used to provide notifications and commands.
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

    // Sends a notification to the |DebugServer| referenced by this object.
    void SendNotification(const std::string& method, const JSONObject& params);

    bool CalledOnValidThread() const {
      return thread_checker_.CalledOnValidThread();
    }

    // Generates an error response that can be returned by any command handler.
    JSONObject ErrorResponse(const std::string& error_message);

   private:
    base::WeakPtr<DebugServer> server_;
    base::ThreadChecker thread_checker_;
    std::vector<std::string> command_methods_;
  };

  // Constructor may be called from any thread, but all internal operations
  // will run on the message loop specified here, which must be the message
  // loop of the WebModule this debug server is attached to, so that
  // operations are executed synchronously with the other methods of that
  // WebModule.
  DebugServer(const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy,
              const OnEventCallback& on_event_callback,
              const OnDetachCallback& on_detach_callback);

  ~DebugServer();

  // Adds a debug component to this object. This object takes ownership.
  void AddComponent(scoped_ptr<Component> component);

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
        : callback(command_callback),
          message_loop_proxy(base::MessageLoopProxy::current()) {}

    CommandCallback callback;
    scoped_refptr<base::MessageLoopProxy> message_loop_proxy;
  };

  // Callback to receive notifications from the debug components.
  // Serializes the method and params object to a JSON string and passes to the
  // external |on_event_callback_| specified in the constructor.
  void OnNotification(const std::string& method, const JSONObject& params);

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

  // The message loop to use for all communication with debug targets.
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;

  // Notification callbacks to send messages to the client.
  OnEventCallback on_event_callback_;
  OnDetachCallback on_detach_callback_;

  // Map of commands, indexed by method name.
  CommandRegistry command_registry_;

  base::ThreadChecker thread_checker_;

  // Debug components owned by this object.
  std::vector<Component*> components_;
  STLElementDeleter<std::vector<Component*> > components_deleter_;

  friend class DebugCommandHandler;
};

}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_DEBUG_SERVER_H_
