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

#include "third_party/blink/renderer/core/inspector/inspector_page_agent.h"


namespace blink {

String InspectorPageAgent::ResourceTypeJson(InspectorPageAgent::ResourceType) { return String(); }
InspectorPageAgent::ResourceType InspectorPageAgent::ToResourceType(const blink::ResourceType) {
  return InspectorPageAgent::kOtherResource;
}
void InspectorPageAgent::DidCreateMainWorldContext(LocalFrame*) {}
void InspectorPageAgent::DidResizeMainFrame() {}
void InspectorPageAgent::DomContentLoadedEventFired(LocalFrame*) {}
void InspectorPageAgent::LoadEventFired(LocalFrame*) {}
void InspectorPageAgent::FrameAttachedToParent(
    LocalFrame*, const Vector<AdScriptIdentifier, 0u, WTF::PartitionAllocator>&) {}
void InspectorPageAgent::FrameDetachedFromParent(LocalFrame*, FrameDetachType) {}
void InspectorPageAgent::FrameSubtreeWillBeDetached(Frame*) {}
void InspectorPageAgent::WillCommitLoad(LocalFrame*, DocumentLoader*) {}
void InspectorPageAgent::DidNavigateWithinDocument(LocalFrame*, mojom::SameDocumentNavigationType) {}
void InspectorPageAgent::DidRestoreFromBackForwardCache(LocalFrame*) {}
void InspectorPageAgent::DidOpenDocument(LocalFrame*, DocumentLoader*) {}
void InspectorPageAgent::FrameStoppedLoading(LocalFrame*) {}
void InspectorPageAgent::FrameRequestedNavigation(Frame*, const KURL&, ClientNavigationReason, NavigationPolicy) {}
void InspectorPageAgent::FrameScheduledNavigation(LocalFrame*, const KURL&, base::TimeDelta, ClientNavigationReason) {}
void InspectorPageAgent::FrameClearedScheduledNavigation(LocalFrame*) {}
void InspectorPageAgent::WindowOpen(const KURL&, const AtomicString&, const WebWindowFeatures&, bool) {}
void InspectorPageAgent::WillRunJavaScriptDialog() {}
void InspectorPageAgent::DidRunJavaScriptDialog() {}
void InspectorPageAgent::DidChangeViewport() {}
void InspectorPageAgent::Will(const probe::RecalculateStyle&) {}
void InspectorPageAgent::Did(const probe::RecalculateStyle&) {}
void InspectorPageAgent::Will(const probe::UpdateLayout&) {}
void InspectorPageAgent::Did(const probe::UpdateLayout&) {}
void InspectorPageAgent::LifecycleEvent(LocalFrame*, DocumentLoader*, const char*, double) {}
void InspectorPageAgent::PaintTiming(Document*, const char*, double) {}
void InspectorPageAgent::DidProduceCompilationCache(const ClassicScript&, v8::Local<v8::Script>) {}
void InspectorPageAgent::ApplyCompilationModeOverride(const ClassicScript&, v8::ScriptCompiler::CachedData**, v8::ScriptCompiler::CompileOptions*) {}
void InspectorPageAgent::FileChooserOpened(LocalFrame*, HTMLInputElement*, bool, bool*, bool*) {}
void InspectorPageAgent::Trace(Visitor*) const {}

} // namespace blink
