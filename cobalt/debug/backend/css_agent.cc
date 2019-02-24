// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/debug/backend/css_agent.h"

namespace cobalt {
namespace debug {
namespace backend {

namespace {
// Definitions from the set specified here:
// https://chromedevtools.github.io/devtools-protocol/tot/DOM
constexpr char kInspectorDomain[] = "CSS";

// File to load JavaScript CSS debugging domain implementation from.
constexpr char kScriptFile[] = "css_agent.js";
}  // namespace

CSSAgent::CSSAgent(DebugDispatcher* dispatcher)
    : dispatcher_(dispatcher),
      ALLOW_THIS_IN_INITIALIZER_LIST(commands_(this, kInspectorDomain)) {
  DCHECK(dispatcher_);

  commands_["disable"] = &CSSAgent::Disable;
  commands_["enable"] = &CSSAgent::Enable;

  dispatcher_->AddDomain(kInspectorDomain, commands_.Bind());
}

CSSAgent::~CSSAgent() { dispatcher_->RemoveDomain(kInspectorDomain); }

void CSSAgent::Enable(const Command& command) {
  bool initialized = dispatcher_->RunScriptFile(kScriptFile);
  if (initialized) {
    command.SendResponse();
  } else {
    command.SendErrorResponse(Command::kInternalError,
                              "Cannot create CSS inspector.");
  }
}

void CSSAgent::Disable(const Command& command) { command.SendResponse(); }

}  // namespace backend
}  // namespace debug
}  // namespace cobalt
