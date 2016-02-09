/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/debug/dom_component.h"

#include <string>

#include "base/bind.h"

namespace cobalt {
namespace debug {

namespace {
// Command "methods" (names) from the set specified here:
// https://developer.chrome.com/devtools/docs/protocol/1.1/dom
const char kGetDocument[] = "DOM.getDocument";
const char kRequestChildNodes[] = "DOM.requestChildNodes";
const char kHideHighlight[] = "DOM.hideHighlight";
const char kHighlightNode[] = "DOM.highlightNode";
}  // namespace

DOMComponent::DOMComponent(const base::WeakPtr<DebugServer>& server,
                           dom::Document* document,
                           RenderOverlay* debug_overlay)
    : DebugServer::Component(server),
      ALLOW_THIS_IN_INITIALIZER_LIST(dom_inspector_(new DOMInspector(
          document,
          base::Bind(&DOMComponent::OnNotification, base::Unretained(this)),
          debug_overlay))) {
  AddCommand(kGetDocument,
             base::Bind(&DOMComponent::GetDocument, base::Unretained(this)));
  AddCommand(kRequestChildNodes, base::Bind(&DOMComponent::RequestChildNodes,
                                            base::Unretained(this)));
  AddCommand(kHighlightNode,
             base::Bind(&DOMComponent::HighlightNode, base::Unretained(this)));
  AddCommand(kHideHighlight,
             base::Bind(&DOMComponent::HideHighlight, base::Unretained(this)));
}

void DOMComponent::OnNotification(const std::string& method,
                                  const JSONObject& params) {
  SendNotification(method, params);
}

JSONObject DOMComponent::GetDocument(const JSONObject& params) {
  DCHECK(CalledOnValidThread());
  return dom_inspector_->GetDocument(params);
}

JSONObject DOMComponent::RequestChildNodes(const JSONObject& params) {
  DCHECK(CalledOnValidThread());
  return dom_inspector_->RequestChildNodes(params);
}

JSONObject DOMComponent::HighlightNode(const JSONObject& params) {
  DCHECK(CalledOnValidThread());
  return dom_inspector_->HighlightNode(params);
}

JSONObject DOMComponent::HideHighlight(const JSONObject& params) {
  DCHECK(CalledOnValidThread());
  return dom_inspector_->HideHighlight(params);
}

}  // namespace debug
}  // namespace cobalt
