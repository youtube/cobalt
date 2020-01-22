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

#include "cobalt/debug/backend/runtime_agent.h"

#include <string>

#include "base/bind.h"

namespace cobalt {
namespace debug {
namespace backend {

RuntimeAgent::RuntimeAgent(DebugDispatcher* dispatcher, dom::Window* window)
    : AgentBase("Runtime", "runtime_agent.js", dispatcher), window_(window) {
  commands_["compileScript"] =
      base::Bind(&RuntimeAgent::CompileScript, base::Unretained(this));
}

bool RuntimeAgent::DoEnable(Command* command) {
  if (!AgentBase::DoEnable(command)) return false;

  // Send an executionContextCreated event.
  // https://chromedevtools.github.io/devtools-protocol/1-3/Runtime#event-executionContextCreated
  JSONObject params(new base::DictionaryValue());
  params->SetInteger("context.id", 1);
  params->SetString("context.origin", window_->location()->origin());
  params->SetString("context.name", "Cobalt");
  params->SetBoolean("context.auxData.isDefault", true);
  dispatcher_->SendEvent(domain_ + ".executionContextCreated", params);
  return true;
}

void RuntimeAgent::CompileScript(Command command) {
  if (!EnsureEnabled(&command)) return;

  // TODO: Parse the JS without eval-ing it... This is to support:
  // a) Multi-line input from the devtools console
  // b) https://developers.google.com/web/tools/chrome-devtools/snippets
  command.SendResponse();
}

}  // namespace backend
}  // namespace debug
}  // namespace cobalt
