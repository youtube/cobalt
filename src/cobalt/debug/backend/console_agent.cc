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

namespace {
// Definitions from the set specified here:
// https://chromedevtools.github.io/devtools-protocol/tot/Console
constexpr char kInspectorDomain[] = "Console";

// The "Console" protocol domain is deprecated, but we still use it to forward
// console messages from our console web API implementation (used only with
// mozjs) to avoid blurring the line to the "Runtime" domain implementation.
}  // namespace

ConsoleAgent::Listener::Listener(dom::Console* console,
                                 ConsoleAgent* console_agent)
    : dom::Console::Listener(console), console_agent_(console_agent) {}

void ConsoleAgent::Listener::OnMessage(const std::string& message,
                                       dom::Console::Level level) {
  console_agent_->OnMessageAdded(message, level);
}

ConsoleAgent::ConsoleAgent(DebugDispatcher* dispatcher, dom::Console* console)
    : dispatcher_(dispatcher),
      ALLOW_THIS_IN_INITIALIZER_LIST(console_listener_(console, this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(commands_(this, kInspectorDomain)) {
  DCHECK(dispatcher_);

  commands_["disable"] = &ConsoleAgent::Disable;
  commands_["enable"] = &ConsoleAgent::Enable;
}

void ConsoleAgent::Thaw(JSONObject agent_state) {
  dispatcher_->AddDomain(kInspectorDomain, commands_.Bind());
}

JSONObject ConsoleAgent::Freeze() {
  dispatcher_->RemoveDomain(kInspectorDomain);
  return JSONObject();
}

void ConsoleAgent::Disable(Command command) { command.SendResponse(); }

void ConsoleAgent::Enable(Command command) { command.SendResponse(); }

void ConsoleAgent::OnMessageAdded(const std::string& text,
                                  dom::Console::Level level) {
  JSONObject params(new base::DictionaryValue());
  params->SetString("message.text", text);
  params->SetString("message.level", dom::Console::GetLevelAsString(level));
  params->SetString("message.source", "console-api");
  dispatcher_->SendEvent(std::string(kInspectorDomain) + ".messageAdded",
                         params);
}

}  // namespace backend
}  // namespace debug
}  // namespace cobalt
