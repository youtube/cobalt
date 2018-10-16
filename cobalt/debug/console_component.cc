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

#include "cobalt/debug/console_component.h"

#include "base/bind.h"
#include "base/values.h"

namespace cobalt {
namespace debug {

namespace {
// Definitions from the set specified here:
// https://developer.chrome.com/devtools/docs/protocol/1.1/console

// Parameter fields:
const char kMessageText[] = "message.text";
const char kMessageLevel[] = "message.level";
const char kMessageSource[] = "message.source";

// Constant parameter values:
const char kMessageSourceValue[] = "console-api";

// Events:
const char kMessageAdded[] = "Console.messageAdded";
}  // namespace

ConsoleComponent::Listener::Listener(dom::Console* console,
                                     ConsoleComponent* console_component)
    : dom::Console::Listener(console), console_component_(console_component) {}

void ConsoleComponent::Listener::OnMessage(const std::string& message,
                                           dom::Console::Level level) {
  console_component_->OnMessageAdded(message, level);
}

ConsoleComponent::ConsoleComponent(DebugDispatcher* dispatcher,
                                   dom::Console* console)
    : dispatcher_(dispatcher),
      ALLOW_THIS_IN_INITIALIZER_LIST(console_listener_(console, this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(commands_(this)) {
  DCHECK(dispatcher_);
  commands_["Console.disable"] = &ConsoleComponent::Disable;
  commands_["Console.enable"] = &ConsoleComponent::Enable;

  dispatcher_->AddDomain("Console", commands_.Bind());
}

void ConsoleComponent::Disable(const Command& command) {
  command.SendResponse();
}

void ConsoleComponent::Enable(const Command& command) {
  command.SendResponse();
}

void ConsoleComponent::OnMessageAdded(const std::string& text,
                                      dom::Console::Level level) {
  JSONObject params(new base::DictionaryValue());
  params->SetString(kMessageText, text);
  params->SetString(kMessageLevel, dom::Console::GetLevelAsString(level));
  params->SetString(kMessageSource, kMessageSourceValue);
  dispatcher_->SendEvent(kMessageAdded, params);
}

}  // namespace debug
}  // namespace cobalt
