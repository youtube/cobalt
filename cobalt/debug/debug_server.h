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
#ifndef DEBUG_DEBUG_SERVER_H_
#define DEBUG_DEBUG_SERVER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/optional.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "cobalt/script/global_object_proxy.h"
#include "cobalt/script/javascript_debugger_interface.h"

namespace cobalt {
namespace debug {

// The core of the debugging system. The overall architecture of the debugging
// system is documented here:
// https://docs.google.com/document/d/1lZhrBTusQZJsacpt21J3kPgnkj7pyQObhFqYktvm40Y/
//
// When a client (either local or remote) wants to connect to the debugger, it
// should create an instance of this class. Construction of this object will
// create the subcomponents needed (e.g. JavaScript debugger) and attach to the
// targets (Window, GlobalObject, etc.) specified in the constructor. When the
// instance is destructed, it will detach from the debug targets.
//
// All client debugging commands and event notifications are routed through an
// object of this class. The protocol is defined here:
// https://developer.chrome.com/devtools/docs/protocol/1.1/index
// Debugging commands are sent via the |SendCommand| method, which runs
// a command asynchronously and returns its result via a callback.
// DebugServer will also send notifications when debugging events occur, using
// the callbacks specified in the constructor.
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

class DebugServer {
 public:
  // Type for command callback function.
  typedef base::Callback<void(const base::optional<std::string>&)>
      CommandCallback;

  // Type for onEvent callback function.
  typedef base::Callback<void(
      const std::string&, const base::optional<std::string>&)> OnEventCallback;

  // Type for onDetach callback function.
  typedef base::Callback<void(const std::string&)> OnDetachCallback;

  // Constructor may be called from any thread, but all internal operations
  // will run on the message loop specified here, which must be the message
  // loop of the WebModule this debug server is attached to, so that
  // operations are executed synchronously with the other methods of that
  // WebModule.
  DebugServer(script::GlobalObjectProxy* global_object_proxy,
              const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy,
              const OnEventCallback& on_event_callback,
              const OnDetachCallback& on_detach_callback);

  ~DebugServer();

  // Called to send a command to this debug server, with a callback for the
  // response. The command is one from the Chrome Remote Debugging Protocol:
  // https://developer.chrome.com/devtools/docs/protocol/1.1/index
  // May be called from any thread - the command will be run on the
  // message loop specified in the constructor and the callback will be run on
  // the message loop of the caller.
  void SendCommand(const std::string& method, const std::string& json_params,
                   CommandCallback callback);

 private:
  // Type to store a command callback and the current message loop.
  struct CommandCallbackInfo {
    explicit CommandCallbackInfo(const CommandCallback& command_callback)
        : callback(command_callback),
          message_loop_proxy(base::MessageLoopProxy::current()) {}

    CommandCallback callback;
    scoped_refptr<base::MessageLoopProxy> message_loop_proxy;
  };

  // Type for a scoped pointer to a JSON object stored as a dictionary.
  typedef scoped_ptr<base::DictionaryValue> ScopedDictionary;

  // Type for a command execution function stored in the command registry.
  typedef base::Callback<ScopedDictionary(const ScopedDictionary&)>
      CommandExecutor;

  // Type for a registry of commands, mapping method names from the protocol
  // to command execution callbacks.
  typedef std::map<std::string, CommandExecutor> CommandRegistry;

  // Creates the JavaScript debugger. Must be run on the message loop of the
  // JavaScript global object the debugger will connect to, which should be the
  // message loop passed in the contructor. Will signal completion using the
  // specified waitable event.
  void CreateJavaScriptDebugger(script::GlobalObjectProxy* global_object_proxy);

  // Registers the supported commands.
  void RegisterCommands();

  // Dispatches a command received via |SendCommand| by looking up the method
  // name in the command registry and running the corresponding function.
  // The command callback will be run on the message loop specified in the info
  // structure with the result as an argument.
  void DispatchCommand(std::string method, std::string json_params,
                       CommandCallbackInfo callback_info);

  // Callback to receive onEvent notifications from the JavaScript debugger.
  // Serializes the dictionary object to a JSON string and passes to the
  // external |on_event_callback_| specified in the constructor.
  void OnEvent(const std::string& method,
               const scoped_ptr<base::DictionaryValue>& params);

  // Methods to convert JSON objects between dictionary value and string.
  ScopedDictionary CreateDictionaryFromJSONString(const std::string& json);
  std::string CreateJSONStringFromDictionary(
      const ScopedDictionary& dictionary);

  // Gets the source of a script from the JavaScript debugger.
  ScopedDictionary GetScriptSource(const ScopedDictionary& params);

  // The message loop to use for all communication with debug targets.
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;

  // Notification callbacks.
  OnEventCallback on_event_callback_;
  OnDetachCallback on_detach_callback_;

  // Handles all debugging interaction with the JavaScript engine.
  scoped_ptr<script::JavaScriptDebuggerInterface> javascript_debugger_;

  // Map of commands, indexed by method name.
  CommandRegistry command_registry_;

  base::ThreadChecker thread_checker_;
};

}  // namespace debug
}  // namespace cobalt

#endif  // DEBUG_DEBUG_SERVER_H_
