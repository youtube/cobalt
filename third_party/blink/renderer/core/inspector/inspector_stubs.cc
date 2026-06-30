#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/worker_inspector_controller.h"
#include "third_party/blink/renderer/core/inspector/inspector_animation_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_network_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_page_agent.h"
#include "third_party/blink/renderer/core/inspector/protocol/network.h"

namespace blink {

// IdentifiersFactory stubs
const String& IdentifiersFactory::FrameId(Frame*) {
  DEFINE_STATIC_LOCAL(String, empty_string, ());
  return empty_string;
}
String IdentifiersFactory::SubresourceRequestId(uint64_t) { return String(); }
String IdentifiersFactory::IdFromToken(const base::UnguessableToken&) { return String(); }
String IdentifiersFactory::IdForCSSStyleSheet(const CSSStyleSheet*) { return String(); }

// WorkerInspectorController stubs
WorkerInspectorController* WorkerInspectorController::Create(
    WorkerThread*, const KURL&, scoped_refptr<InspectorTaskRunner>,
    std::unique_ptr<WorkerDevToolsParams>) {
  return nullptr;
}

// Agent stubs used by inspector_trace_events.cc
String InspectorAnimationAgent::AnimationDisplayName(Animation*) { return String(); }

std::unique_ptr<protocol::Network::Initiator> InspectorNetworkAgent::BuildInitiatorObject(
    Document*, const FetchInitiatorInfo&, int) {
  return nullptr;
}
String InspectorNetworkAgent::GetProtocolAsString(const ResourceResponse&) { return String(); }

String InspectorPageAgent::ResourceTypeJson(InspectorPageAgent::ResourceType) { return String(); }
InspectorPageAgent::ResourceType InspectorPageAgent::ToResourceType(ResourceType) {
  return InspectorPageAgent::kOtherResource;
}

} // namespace blink
