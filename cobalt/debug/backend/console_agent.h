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

#ifndef COBALT_DEBUG_BACKEND_CONSOLE_AGENT_H_
#define COBALT_DEBUG_BACKEND_CONSOLE_AGENT_H_

#include <string>

#include "cobalt/debug/backend/agent_base.h"
#include "cobalt/debug/backend/debug_dispatcher.h"
#include "cobalt/dom/console.h"

namespace cobalt {
namespace debug {
namespace backend {

// Even though the "Console" protocol domain is deprecated, we still use it to
// forward console messages from our own console web API implementation to avoid
// blurring the line to our "Runtime" domain implementation (the modern event
// for console logging is "Runtime.consoleAPICalled"). This is only
// relevant when running mozjs since V8 obscures our console with its own
// builtin implementation.
//
// https://chromedevtools.github.io/devtools-protocol/tot/Console
class ConsoleAgent : public AgentBase {
 public:
  ConsoleAgent(DebugDispatcher* dispatcher, dom::Console* console);

 private:
  class Listener : public dom::Console::Listener {
   public:
    Listener(dom::Console* console, ConsoleAgent* console_agent);
    void OnMessage(const std::string& message,
                   dom::Console::Level level) override;
    ConsoleAgent* console_agent_;
  };

  // Called by |console_listener_| when a new message is output.
  void OnMessageAdded(const std::string& text, dom::Console::Level level);

  Listener console_listener_;
};

}  // namespace backend
}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_BACKEND_CONSOLE_AGENT_H_
