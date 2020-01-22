// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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
#ifndef COBALT_DEBUG_BACKEND_AGENT_BASE_H_
#define COBALT_DEBUG_BACKEND_AGENT_BASE_H_

#include <string>

#include "cobalt/debug/backend/command_map.h"
#include "cobalt/debug/backend/debug_dispatcher.h"
#include "cobalt/debug/command.h"
#include "cobalt/debug/json_object.h"

namespace cobalt {
namespace debug {
namespace backend {

// Utility base class with common logic for agents. It is not required that
// agents use this base class if they have other behaviour.
class AgentBase {
 public:
  // Constructor for agents that do not have hybrid JS implementation.
  AgentBase(const std::string& domain, DebugDispatcher* dispatcher)
      : AgentBase(domain, "", dispatcher) {}

  // Constructor for agents that have a script file that is their hybrid JS
  // implementation.
  AgentBase(const std::string& domain, const std::string& script_file,
            DebugDispatcher* dispatcher);

  virtual ~AgentBase() {}

  virtual void Thaw(JSONObject agent_state);
  virtual JSONObject Freeze();

 protected:
  // Commands added to the CommandMap. These just call their respective
  // DoCommand() methods and send a success response if appropriate.
  void Enable(Command command) {
    if (DoEnable(&command)) command.SendResponse();
  }
  void Disable(Command command) {
    if (DoDisable(&command)) command.SendResponse();
  }

  // Default command implementations that may be overridden. Returns false if an
  // error response has already been sent, or true if successful so far and no
  // response has been sent yet.
  virtual bool DoEnable(Command* command);
  virtual bool DoDisable(Command* command);

  // Returns enabled, and if not enabled sends an error response for |command|.
  bool EnsureEnabled(Command* command);

  std::string domain_;
  std::string script_file_;
  DebugDispatcher* dispatcher_;

  // Map of member functions implementing commands.
  CommandMap commands_;

  // Whether the domain is enabled.
  bool enabled_ = false;
};

}  // namespace backend
}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_BACKEND_AGENT_BASE_H_
