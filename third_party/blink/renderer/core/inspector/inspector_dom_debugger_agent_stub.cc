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

void InspectorDOMDebuggerAgent::Will(const probe::UserCallback&) {}
void InspectorDOMDebuggerAgent::Did(const probe::UserCallback&) {}
void InspectorDOMDebuggerAgent::OnContentSecurityPolicyViolation(ContentSecurityPolicyViolationType) {}
void InspectorDOMDebuggerAgent::Trace(Visitor*) const {}
void InspectorDOMDebuggerAgent::WillInsertDOMNode(Node*) {}
void InspectorDOMDebuggerAgent::DidInsertDOMNode(Node*) {}
void InspectorDOMDebuggerAgent::WillModifyDOMAttr(Element*, const AtomicString&, const AtomicString&) {}
void InspectorDOMDebuggerAgent::CharacterDataModified(CharacterData*) {}
void InspectorDOMDebuggerAgent::WillSendXMLHttpOrFetchNetworkRequest(const String&) {}
void InspectorDOMDebuggerAgent::DidInvalidateStyleAttr(Node*) {}
void InspectorDOMDebuggerAgent::EventListenersInfoForTarget(
    v8::Isolate*, v8::Local<v8::Value>, Vector<V8EventListenerInfo, 0u, WTF::PartitionAllocator>*) {}

} // namespace blink
