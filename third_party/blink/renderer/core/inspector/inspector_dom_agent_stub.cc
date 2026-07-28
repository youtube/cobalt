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

void InspectorDOMAgent::DomContentLoadedEventFired(LocalFrame*) {}
void InspectorDOMAgent::DidCommitLoad(LocalFrame*, DocumentLoader*) {}
void InspectorDOMAgent::DidRestoreFromBackForwardCache(LocalFrame*) {}
void InspectorDOMAgent::FrameDocumentUpdated(LocalFrame*) {}
void InspectorDOMAgent::FrameOwnerContentUpdated(LocalFrame*, HTMLFrameOwnerElement*) {}
void InspectorDOMAgent::PseudoElementCreated(PseudoElement*) {}
void InspectorDOMAgent::TopLayerElementsChanged() {}
void InspectorDOMAgent::PseudoElementDestroyed(PseudoElement*) {}
void InspectorDOMAgent::NodeCreated(Node*) {}
void InspectorDOMAgent::UpdateScrollableFlag(Node*, std::optional<bool>) {}
void InspectorDOMAgent::Trace(Visitor*) const {}
void InspectorDOMAgent::DidInsertDOMNode(Node*) {}
void InspectorDOMAgent::WillRemoveDOMNode(Node*) {}
void InspectorDOMAgent::WillModifyDOMAttr(Element*, const AtomicString&, const AtomicString&) {}
void InspectorDOMAgent::DidModifyDOMAttr(Element*, const QualifiedName&, const AtomicString&) {}
void InspectorDOMAgent::DidRemoveDOMAttr(Element*, const QualifiedName&) {}
void InspectorDOMAgent::CharacterDataModified(CharacterData*) {}
void InspectorDOMAgent::DidInvalidateStyleAttr(Node*) {}
void InspectorDOMAgent::DidPerformSlotDistribution(HTMLSlotElement*) {}
void InspectorDOMAgent::DidPushShadowRoot(Element*, ShadowRoot*) {}

} // namespace blink
