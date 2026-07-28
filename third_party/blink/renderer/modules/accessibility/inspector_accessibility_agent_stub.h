// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
