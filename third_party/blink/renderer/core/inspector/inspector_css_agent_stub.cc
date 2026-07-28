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

void InspectorCSSAgent::Will(const probe::RecalculateStyle&) {}
void InspectorCSSAgent::Did(const probe::RecalculateStyle&) {}
void InspectorCSSAgent::ForcePseudoState(Element*, CSSSelector::PseudoType, bool*) {}
void InspectorCSSAgent::ForceStartingStyle(Element*, bool*) {}
void InspectorCSSAgent::DidMutateStyleSheet(CSSStyleSheet*) {}
void InspectorCSSAgent::DidReplaceStyleSheetText(CSSStyleSheet*, const String&) {}
void InspectorCSSAgent::GetTextPosition(wtf_size_t, const String*, TextPosition*) {}
void InspectorCSSAgent::LocalFontsEnabled(bool*) {}
void InspectorCSSAgent::DidUpdateComputedStyle(Element*, const ComputedStyle*, const ComputedStyle*) {}
void InspectorCSSAgent::Trace(Visitor*) const {}
void InspectorCSSAgent::WillChangeStyleElement(Element*) {}
void InspectorCSSAgent::DocumentDetached(Document*) {}
void InspectorCSSAgent::ActiveStyleSheetsUpdated(Document*) {}
void InspectorCSSAgent::FontsUpdated(const FontFace*, const String&, const FontCustomPlatformData*) {}
void InspectorCSSAgent::MediaQueryResultChanged() {}

} // namespace blink
