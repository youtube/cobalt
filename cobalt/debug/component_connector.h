// Copyright 2016 Google Inc. All Rights Reserved.
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
#ifndef COBALT_DEBUG_COMPONENT_CONNECTOR_H_
#define COBALT_DEBUG_COMPONENT_CONNECTOR_H_

#include <string>
#include <vector>

#include "base/threading/thread_checker.h"
#include "cobalt/debug/debug_server.h"
#include "cobalt/debug/json_object.h"
#include "cobalt/script/script_debugger.h"
#include "cobalt/script/value_handle.h"

namespace cobalt {
namespace debug {

// Helper class for debug component objects that provide events and commands for
// |DebugServer|.
//
// An object of this class allows a debug component to access the debug server,
// script debugger, etc. and provides some common functionality.
class ComponentConnector {
 public:
  explicit ComponentConnector(DebugServer* server,
                              script::ScriptDebugger* script_debugger);

  virtual ~ComponentConnector();

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

  // Creates a Runtime.Remote object from an ValueHandleHolder.
  JSONObject CreateRemoteObject(const script::ValueHandleHolder* object);

  // Sends an event to the |DebugServer| referenced by this object.
  void SendEvent(const std::string& method, const JSONObject& params);

  bool CalledOnValidThread() const {
    return thread_checker_.CalledOnValidThread();
  }

  // Generates an error response that can be returned by any command handler.
  static JSONObject ErrorResponse(const std::string& error_message);

  DebugServer* server() { return server_; }
  script::ScriptDebugger* script_debugger() const { return script_debugger_; }

 private:
  DebugServer* server_;
  script::ScriptDebugger* script_debugger_;
  base::ThreadChecker thread_checker_;
  std::vector<std::string> command_methods_;
};

}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_COMPONENT_CONNECTOR_H_
