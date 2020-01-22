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

#include "cobalt/debug/backend/dom_agent.h"

namespace cobalt {
namespace debug {
namespace backend {

namespace {
// Definitions from the set specified here:
// https://chromedevtools.github.io/devtools-protocol/tot/DOM
constexpr char kInspectorDomain[] = "DOM";

// File to load JavaScript DOM debugging domain implementation from.
constexpr char kScriptFile[] = "dom_agent.js";
}  // namespace

DOMAgent::DOMAgent(DebugDispatcher* dispatcher)
    : dispatcher_(dispatcher),
      commands_(kInspectorDomain) {
  DCHECK(dispatcher_);

  commands_["disable"] = base::Bind(&DOMAgent::Disable, base::Unretained(this));
  commands_["enable"] = base::Bind(&DOMAgent::Enable, base::Unretained(this));
}

void DOMAgent::Thaw(JSONObject agent_state) {
  dispatcher_->AddDomain(kInspectorDomain, commands_.Bind());
  script_loaded_ = dispatcher_->RunScriptFile(kScriptFile);
  DLOG_IF(ERROR, !script_loaded_) << "Failed to load " << kScriptFile;
}

JSONObject DOMAgent::Freeze() {
  dispatcher_->RemoveDomain(kInspectorDomain);
  return JSONObject();
}

void DOMAgent::Enable(Command command) {
  if (script_loaded_) {
    command.SendResponse();
  } else {
    command.SendErrorResponse(Command::kInternalError,
                              "Cannot create DOM inspector.");
  }
}

void DOMAgent::Disable(Command command) { command.SendResponse(); }

}  // namespace backend
}  // namespace debug
}  // namespace cobalt
