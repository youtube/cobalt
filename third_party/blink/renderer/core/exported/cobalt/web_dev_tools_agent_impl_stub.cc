// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "third_party/blink/renderer/core/exported/web_dev_tools_agent_impl.h"

namespace blink {

WebDevToolsAgentImpl* WebDevToolsAgentImpl::CreateForFrame(WebLocalFrameImpl*) {
  return nullptr;
}
void WebDevToolsAgentImpl::SetPageIsScrolling(bool) {}
void WebDevToolsAgentImpl::DispatchBufferedTouchEvents() {}
WebInputEventResult WebDevToolsAgentImpl::HandleInputEvent(
    const WebInputEvent&) {
  return WebInputEventResult::kNotHandled;
}
void WebDevToolsAgentImpl::ActivatePausedDebuggerWindow(WebLocalFrameImpl*) {}
void WebDevToolsAgentImpl::DidCommitLoadForLocalFrame(LocalFrame*) {}
String WebDevToolsAgentImpl::NavigationInitiatorInfo(LocalFrame*) {
  return String();
}
String WebDevToolsAgentImpl::EvaluateInOverlayForTesting(const String&) {
  return String();
}
void WebDevToolsAgentImpl::BindReceiver(
    mojo::PendingAssociatedRemote<mojom::blink::DevToolsAgentHost>,
    mojo::PendingAssociatedReceiver<mojom::blink::DevToolsAgent>) {}
void WebDevToolsAgentImpl::WillBeDestroyed() {}
void WebDevToolsAgentImpl::WaitForDebuggerWhenShown() {}
void WebDevToolsAgentImpl::DidShowNewWindow() {}
void WebDevToolsAgentImpl::UpdateOverlaysPrePaint() {}
void WebDevToolsAgentImpl::PaintOverlays(GraphicsContext&) {}
void WebDevToolsAgentImpl::Trace(Visitor*) const {}

}  // namespace blink
