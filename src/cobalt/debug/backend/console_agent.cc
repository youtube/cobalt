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

#include "cobalt/debug/backend/console_agent.h"

#include "base/bind.h"
#include "base/values.h"

namespace cobalt {
namespace debug {
namespace backend {

ConsoleAgent::ConsoleAgent(DebugDispatcher* dispatcher, dom::Console* console)
    : AgentBase("Console", dispatcher),
      ALLOW_THIS_IN_INITIALIZER_LIST(console_listener_(console, this)) {}

ConsoleAgent::Listener::Listener(dom::Console* console,
                                 ConsoleAgent* console_agent)
    : dom::Console::Listener(console), console_agent_(console_agent) {}

void ConsoleAgent::Listener::OnMessage(const std::string& message,
                                       dom::Console::Level level) {
  console_agent_->OnMessageAdded(message, level);
}

void ConsoleAgent::OnMessageAdded(const std::string& text,
                                  dom::Console::Level level) {
  JSONObject params(new base::DictionaryValue());
  params->SetString("message.text", text);
  params->SetString("message.level", dom::Console::GetLevelAsString(level));
  params->SetString("message.source", "console-api");
  dispatcher_->SendEvent(domain_ + ".messageAdded", params);
}

}  // namespace backend
}  // namespace debug
}  // namespace cobalt
