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

namespace {
// Definitions from the set specified here:
// https://chromedevtools.github.io/devtools-protocol/tot/Runtime
constexpr char kInspectorDomain[] = "Runtime";

// File to load JavaScript runtime implementation from.
constexpr char kScriptFile[] = "runtime_agent.js";
}  // namespace

RuntimeAgent::RuntimeAgent(DebugDispatcher* dispatcher, dom::Window* window)
    : dispatcher_(dispatcher),
      window_(window),
      commands_(kInspectorDomain) {
  DCHECK(dispatcher_);
  if (!dispatcher_->RunScriptFile(kScriptFile)) {
    DLOG(WARNING) << "Cannot execute Runtime initialization script.";
  }

  commands_["enable"] = base::Bind(&RuntimeAgent::Enable, base::Unretained(this));
  commands_["disable"] = base::Bind(&RuntimeAgent::Disable, base::Unretained(this));
  commands_["compileScript"] = base::Bind(&RuntimeAgent::CompileScript, base::Unretained(this));
}

void RuntimeAgent::Thaw(JSONObject agent_state) {
  dispatcher_->AddDomain(kInspectorDomain, commands_.Bind());
  script_loaded_ = dispatcher_->RunScriptFile(kScriptFile);
  DLOG_IF(ERROR, !script_loaded_) << "Failed to load " << kScriptFile;
}

JSONObject RuntimeAgent::Freeze() {
  dispatcher_->RemoveDomain(kInspectorDomain);
  return JSONObject();
}

void RuntimeAgent::Enable(Command command) {
  if (!script_loaded_) {
    command.SendErrorResponse(Command::kInternalError,
                              "Cannot create Runtime inspector.");
    return;
  }

  // Send an executionContextCreated event.
  // https://chromedevtools.github.io/devtools-protocol/1-3/Runtime#event-executionContextCreated
  JSONObject params(new base::DictionaryValue());
  params->SetInteger("context.id", 1);
  params->SetString("context.origin", window_->location()->origin());
  params->SetString("context.name", "Cobalt");
  params->SetBoolean("context.auxData.isDefault", true);
  dispatcher_->SendEvent(
      std::string(kInspectorDomain) + ".executionContextCreated", params);

  command.SendResponse();
}

void RuntimeAgent::Disable(Command command) { command.SendResponse(); }

void RuntimeAgent::CompileScript(Command command) {
  // TODO: Parse the JS without eval-ing it... This is to support:
  // a) Multi-line input from the devtools console
  // b) https://developers.google.com/web/tools/chrome-devtools/snippets
  command.SendResponse();
}

}  // namespace backend
}  // namespace debug
}  // namespace cobalt
