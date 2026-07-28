#include <memory>
#include <optional>

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/renderer/core/inspector/worker_inspector_controller.h"
#include "third_party/blink/renderer/core/inspector/worker_devtools_params.h"
#include "third_party/blink/renderer/core/inspector/devtools_agent.h"
#include "third_party/blink/renderer/core/inspector/devtools_session.h"
#include "third_party/blink/renderer/core/inspector/inspector_animation_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_css_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_debugger_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_snapshot_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_emulation_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_event_breakpoints_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_layer_tree_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_log_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_media_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_media_context_impl.h"
#include "third_party/blink/renderer/core/inspector/inspector_network_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_overlay_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_page_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_performance_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_performance_timeline_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_preload_agent.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/inspector/worker_thread_debugger.h"
#include "third_party/blink/renderer/core/inspector/protocol/network.h"
#include "third_party/blink/renderer/core/exported/web_dev_tools_agent_impl.h"

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
