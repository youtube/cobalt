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

#include "third_party/blink/renderer/core/inspector/inspector_dom_debugger_agent.h"

namespace blink {

void InspectorDOMDebuggerAgent::Will(const probe::UserCallback&) {}
void InspectorDOMDebuggerAgent::Did(const probe::UserCallback&) {}
void InspectorDOMDebuggerAgent::OnContentSecurityPolicyViolation(
    ContentSecurityPolicyViolationType) {}
void InspectorDOMDebuggerAgent::Trace(Visitor*) const {}
void InspectorDOMDebuggerAgent::WillInsertDOMNode(Node*) {}
void InspectorDOMDebuggerAgent::DidInsertDOMNode(Node*) {}
void InspectorDOMDebuggerAgent::WillModifyDOMAttr(Element*,
                                                  const AtomicString&,
                                                  const AtomicString&) {}
void InspectorDOMDebuggerAgent::CharacterDataModified(CharacterData*) {}
void InspectorDOMDebuggerAgent::WillSendXMLHttpOrFetchNetworkRequest(
    const String&) {}
void InspectorDOMDebuggerAgent::DidInvalidateStyleAttr(Node*) {}
void InspectorDOMDebuggerAgent::EventListenersInfoForTarget(
    v8::Isolate*,
    v8::Local<v8::Value>,
    Vector<V8EventListenerInfo, 0u, PartitionAllocator>*) {}

}  // namespace blink
