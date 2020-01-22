// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DEBUG_BACKEND_RUNTIME_AGENT_H_
#define COBALT_DEBUG_BACKEND_RUNTIME_AGENT_H_

#include "cobalt/debug/backend/command_map.h"
#include "cobalt/debug/backend/debug_dispatcher.h"
#include "cobalt/debug/command.h"
#include "cobalt/debug/json_object.h"
#include "cobalt/dom/window.h"

namespace cobalt {
namespace debug {
namespace backend {

// Implements a small part of the the "Runtime" inspector protocol domain with
// just enough to support console input. When using the V8 JavaScript engine,
// this class is not needed since the V8 inspector implements the Runtime domain
// for us.
class RuntimeAgent {
 public:
  RuntimeAgent(DebugDispatcher* dispatcher, dom::Window* window);

  void Thaw(JSONObject agent_state);
  JSONObject Freeze();

 private:
  void CompileScript(Command command);
  void Disable(Command command);
  void Enable(Command command);

  DebugDispatcher* dispatcher_;
  dom::Window* window_;

  // Map of member functions implementing commands.
  CommandMap commands_;

  // Whether we successfully loaded the agent's JavaScript implementation.
  bool script_loaded_ = false;
};

}  // namespace backend
}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_BACKEND_RUNTIME_AGENT_H_
