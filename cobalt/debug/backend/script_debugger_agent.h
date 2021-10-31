// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DEBUG_BACKEND_SCRIPT_DEBUGGER_AGENT_H_
#define COBALT_DEBUG_BACKEND_SCRIPT_DEBUGGER_AGENT_H_

#include <map>
#include <set>
#include <string>

#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "cobalt/debug/backend/debug_dispatcher.h"
#include "cobalt/debug/backend/script_debugger_agent.h"
#include "cobalt/debug/command.h"
#include "cobalt/script/script_debugger.h"

namespace cobalt {
namespace debug {
namespace backend {

// Routes protocol messages to the V8 inspector through the V8cScriptDebugger.
//
// Since V8cScriptDebugger keeps track of its own enabled state for the domains
// it implements, the ScriptDebuggerAgent doesn't use AgentBase.
class ScriptDebuggerAgent {
 public:
  ScriptDebuggerAgent(DebugDispatcher* dispatcher,
                      script::ScriptDebugger* script_debugger);

  void Thaw(JSONObject agent_state);
  JSONObject Freeze();

  bool IsSupportedDomain(const std::string& domain) {
    return supported_domains_.count(domain) != 0;
  }
  base::Optional<Command> RunCommand(Command command);
  void SendCommandResponse(const std::string& json_response);
  void SendEvent(const std::string& json_event);

 private:
  THREAD_CHECKER(thread_checker_);
  DebugDispatcher* dispatcher_;
  script::ScriptDebugger* script_debugger_;

  const std::set<std::string> supported_domains_;

  int last_command_id_ = 0;
  std::map<int, Command> pending_commands_;
};

}  // namespace backend
}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_BACKEND_SCRIPT_DEBUGGER_AGENT_H_
