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

#include "cobalt/dom/document.h"
#include "cobalt/dom/html_element.h"
#include "base/bind.h"

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
      commands_(kInspectorDomain) {
  DCHECK(dispatcher_);

  commands_["disable"] = base::Bind(&CSSAgent::Disable, base::Unretained(this));
  commands_["enable"] = base::Bind(&CSSAgent::Enable, base::Unretained(this));
}

void CSSAgent::Thaw(JSONObject agent_state) {
  dispatcher_->AddDomain(kInspectorDomain, commands_.Bind());
  script_loaded_ = dispatcher_->RunScriptFile(kScriptFile);
  DLOG_IF(ERROR, !script_loaded_) << "Failed to load " << kScriptFile;
}

JSONObject CSSAgent::Freeze() {
  dispatcher_->RemoveDomain(kInspectorDomain);
  return JSONObject();
}

void CSSAgent::Enable(Command command) {
  if (script_loaded_) {
    command.SendResponse();
  } else {
    command.SendErrorResponse(Command::kInternalError,
                              "Cannot create CSS inspector.");
  }
}

void CSSAgent::Disable(Command command) { command.SendResponse(); }

CSSAgent::CSSStyleRuleSequence CSSAgent::GetMatchingCSSRules(
    const scoped_refptr<dom::Element>& element) {
  CSSStyleRuleSequence css_rules;
  auto html_element = element->AsHTMLElement();
  if (html_element) {
    html_element->node_document()->UpdateComputedStyleOnElementAndAncestor(
        html_element.get());
    for (const auto& matching_rule : *html_element->matching_rules()) {
      css_rules.push_back(matching_rule.first);
    }
  }
  return css_rules;
}

}  // namespace backend
}  // namespace debug
}  // namespace cobalt
