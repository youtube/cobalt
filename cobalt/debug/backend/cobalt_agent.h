// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DEBUG_BACKEND_COBALT_AGENT_H_
#define COBALT_DEBUG_BACKEND_COBALT_AGENT_H_

#include "cobalt/debug/backend/agent_base.h"
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
//
// https://chromedevtools.github.io/devtools-protocol/tot/Runtime
class CobaltAgent : public AgentBase {
 public:
  explicit CobaltAgent(DebugDispatcher* dispatcher);

 private:
  void GetConsoleCommands(Command command);
  void SendConsoleCommand(Command command);
};

}  // namespace backend
}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_BACKEND_COBALT_AGENT_H_
