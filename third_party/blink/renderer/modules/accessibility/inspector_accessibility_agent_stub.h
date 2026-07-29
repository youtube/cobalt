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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_INSPECTOR_ACCESSIBILITY_AGENT_STUB_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_INSPECTOR_ACCESSIBILITY_AGENT_STUB_H_

#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "ui/accessibility/ax_enums.mojom-blink.h"

namespace blink {

class AXObject;
class Document;
class InspectedFrames;
class InspectorDOMAgent;
class LocalFrame;

class MODULES_EXPORT InspectorAccessibilityAgent
    : public InspectorAgent {
 public:
  InspectorAccessibilityAgent(InspectedFrames*, InspectorDOMAgent*) {}
  static void ProvideTo(LocalFrame* frame) {}
  void Init(CoreProbeSink*, protocol::UberDispatcher*, InspectorSessionState*) override {}
  void Dispose() override {}

  void AXReadyCallback(Document&) {}
  void AXEventFired(AXObject*, ax::mojom::blink::Event) {}
  void AXObjectModified(AXObject*, bool) {}
};

}  // namespace blink


#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_INSPECTOR_ACCESSIBILITY_AGENT_STUB_H_
