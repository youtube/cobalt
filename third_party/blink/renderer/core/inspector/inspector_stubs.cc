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

WebDevToolsAgentImpl* WebDevToolsAgentImpl::CreateForFrame(WebLocalFrameImpl*) {
  return nullptr;
}
void WebDevToolsAgentImpl::SetPageIsScrolling(bool) {}
void WebDevToolsAgentImpl::DispatchBufferedTouchEvents() {}
WebInputEventResult WebDevToolsAgentImpl::HandleInputEvent(const WebInputEvent&) {
  return WebInputEventResult::kNotHandled;
}
void WebDevToolsAgentImpl::ActivatePausedDebuggerWindow(WebLocalFrameImpl*) {}
void WebDevToolsAgentImpl::DidCommitLoadForLocalFrame(LocalFrame*) {}
String WebDevToolsAgentImpl::NavigationInitiatorInfo(LocalFrame*) { return String(); }
String WebDevToolsAgentImpl::EvaluateInOverlayForTesting(const String&) { return String(); }
void WebDevToolsAgentImpl::BindReceiver(
    mojo::PendingAssociatedRemote<mojom::blink::DevToolsAgentHost>,
    mojo::PendingAssociatedReceiver<mojom::blink::DevToolsAgent>) {}
void WebDevToolsAgentImpl::WillBeDestroyed() {}
void WebDevToolsAgentImpl::WaitForDebuggerWhenShown() {}
void WebDevToolsAgentImpl::DidShowNewWindow() {}
void WebDevToolsAgentImpl::UpdateOverlaysPrePaint() {}
void WebDevToolsAgentImpl::PaintOverlays(GraphicsContext&) {}
void WebDevToolsAgentImpl::Trace(Visitor*) const {}

std::unique_ptr<WorkerDevToolsParams> DevToolsAgent::WorkerThreadCreated(
    ExecutionContext*, WorkerThread*, const KURL&, const String&,
    const std::optional<const DedicatedWorkerToken>&) {
  return nullptr;
}
void DevToolsAgent::WorkerThreadTerminated(
    ExecutionContext*, WorkerThread*) {}

// WorkerInspectorController stubs
WorkerInspectorController* WorkerInspectorController::Create(
    WorkerThread*, const KURL&, scoped_refptr<InspectorTaskRunner>,
    std::unique_ptr<WorkerDevToolsParams>) {
  return nullptr;
}
void WorkerInspectorController::WaitForDebuggerIfNeeded() {}
void WorkerInspectorController::Dispose() {}
void WorkerInspectorController::Trace(Visitor*) const {}

// Agent stubs used by inspector_trace_events.cc and probes
String InspectorAnimationAgent::AnimationDisplayName(const Animation&) { return String(); }
void InspectorAnimationAgent::DidClearDocumentOfWindowObject(LocalFrame*) {}

std::unique_ptr<protocol::Network::Initiator> InspectorNetworkAgent::BuildInitiatorObject(
    Document*, const FetchInitiatorInfo&, int) {
  return nullptr;
}
String InspectorNetworkAgent::GetProtocolAsString(const ResourceResponse&) { return String(); }
void InspectorNetworkAgent::DidBlockRequest(
    const ResourceRequest&, DocumentLoader*, const KURL&, const ResourceLoaderOptions&, ResourceRequestBlockedReason, ResourceType) {}
void InspectorNetworkAgent::DidChangeResourcePriority(
    DocumentLoader*, uint64_t, WebURLRequest::Priority) {}
void InspectorNetworkAgent::PrepareRequest(
    DocumentLoader*, ResourceRequest&, ResourceLoaderOptions&, ResourceType) {}
void InspectorNetworkAgent::WillSendRequest(
    ExecutionContext*, DocumentLoader*, const KURL&, const ResourceRequest&, const ResourceResponse&, const ResourceLoaderOptions&, ResourceType, RenderBlockingBehavior, base::TimeTicks) {}
void InspectorNetworkAgent::WillSendNavigationRequest(
    uint64_t, DocumentLoader*, const KURL&, const AtomicString&, EncodedFormData*) {}
void InspectorNetworkAgent::WillSendWorkerMainRequest(
    uint64_t, const KURL&) {}
void InspectorNetworkAgent::MarkResourceAsCached(
    DocumentLoader*, uint64_t) {}
void InspectorNetworkAgent::DidReceiveResourceResponse(
    uint64_t, DocumentLoader*, const ResourceResponse&, const Resource*) {}
void InspectorNetworkAgent::DidReceiveData(
    uint64_t, DocumentLoader*, base::SpanOrSize<char const>) {}
void InspectorNetworkAgent::DidReceiveBlob(
    uint64_t, DocumentLoader*, scoped_refptr<BlobDataHandle>) {}
void InspectorNetworkAgent::DidReceiveEncodedDataLength(
    DocumentLoader*, uint64_t, size_t) {}
void InspectorNetworkAgent::DidFailLoading(
    CoreProbeSink*, uint64_t, DocumentLoader*, const ResourceError&, const base::UnguessableToken&) {}
void InspectorNetworkAgent::WillSendEventSourceRequest() {}
void InspectorNetworkAgent::WillDispatchEventSourceEvent(
    uint64_t, const AtomicString&, const AtomicString&, const String&) {}
void InspectorNetworkAgent::WillLoadXHR(
    ExecutionContext*, const AtomicString&, const KURL&, bool, const HTTPHeaderMap&, bool) {}
void InspectorNetworkAgent::DidFinishXHR(XMLHttpRequest*) {}
void InspectorNetworkAgent::ScriptImported(uint64_t, const String&) {}
void InspectorNetworkAgent::DidReceiveScriptResponse(uint64_t) {}
void InspectorNetworkAgent::DidCommitLoad(LocalFrame*, DocumentLoader*) {}
void InspectorNetworkAgent::FrameScheduledNavigation(LocalFrame*, const KURL&, base::TimeDelta, ClientNavigationReason) {}
void InspectorNetworkAgent::WillCreateWebSocket(
    ExecutionContext*, uint64_t, const KURL&, const String&, std::optional<base::UnguessableToken>*) {}
void InspectorNetworkAgent::WillSendWebSocketHandshakeRequest(
    ExecutionContext*, uint64_t, network::mojom::blink::WebSocketHandshakeRequest*) {}
void InspectorNetworkAgent::DidReceiveWebSocketHandshakeResponse(
    ExecutionContext*, uint64_t, network::mojom::blink::WebSocketHandshakeRequest*, network::mojom::blink::WebSocketHandshakeResponse*) {}
void InspectorNetworkAgent::DidCloseWebSocket(ExecutionContext*, uint64_t) {}
void InspectorNetworkAgent::DidReceiveWebSocketMessage(
    uint64_t, int, bool, const Vector<base::span<const char>>&) {}
void InspectorNetworkAgent::DidSendWebSocketMessage(
    uint64_t, int, bool, base::span<const char>) {}
void InspectorNetworkAgent::DidReceiveWebSocketMessageError(uint64_t, const String&) {}
void InspectorNetworkAgent::WebTransportCreated(ExecutionContext*, uint64_t, const KURL&) {}
void InspectorNetworkAgent::WebTransportConnectionEstablished(uint64_t) {}
void InspectorNetworkAgent::WebTransportClosed(uint64_t) {}
void InspectorNetworkAgent::DirectTCPSocketCreated(
    ExecutionContext*, uint64_t, const String&, uint16_t, protocol::Network::DirectTCPSocketOptions&) {}
void InspectorNetworkAgent::DirectTCPSocketOpened(
    uint64_t, const String&, uint16_t, std::optional<String>, std::optional<uint16_t>) {}
void InspectorNetworkAgent::DirectTCPSocketAborted(uint64_t, int) {}
void InspectorNetworkAgent::DirectTCPSocketClosed(uint64_t) {}
void InspectorNetworkAgent::FrameClearedScheduledNavigation(LocalFrame*) {}
void InspectorNetworkAgent::DirectTCPSocketChunkSent(uint64_t, base::span<const unsigned char>) {}
void InspectorNetworkAgent::DirectTCPSocketChunkReceived(uint64_t, base::span<const unsigned char>) {}
void InspectorNetworkAgent::DirectUDPSocketCreated(
    ExecutionContext*, uint64_t, protocol::Network::DirectUDPSocketOptions&) {}
void InspectorNetworkAgent::DirectUDPSocketOpened(
    uint64_t, const String&, uint16_t, std::optional<String>, std::optional<uint16_t>) {}
void InspectorNetworkAgent::DirectUDPSocketAborted(uint64_t, int) {}
void InspectorNetworkAgent::DirectUDPSocketClosed(uint64_t) {}
void InspectorNetworkAgent::DirectUDPSocketChunkSent(
    uint64_t, base::span<const unsigned char>, std::optional<String>, std::optional<uint16_t>) {}
void InspectorNetworkAgent::DirectUDPSocketChunkReceived(
    uint64_t, base::span<const unsigned char>, std::optional<String>, std::optional<uint16_t>) {}
void InspectorNetworkAgent::DidFinishLoading(
    uint64_t identifier, DocumentLoader*, base::TimeTicks, int64_t, int64_t) {}
void InspectorNetworkAgent::SetDevToolsIds(ResourceRequest&, const FetchInitiatorInfo&) {}
void InspectorNetworkAgent::IsCacheDisabled(bool* is_cache_disabled) const {}
void InspectorNetworkAgent::ShouldApplyDevtoolsCookieSettingOverrides(bool* should_apply_devtools_overrides) const {}
void InspectorNetworkAgent::ShouldBlockRequest(const KURL&, bool*) {}
void InspectorNetworkAgent::ShouldBypassServiceWorker(bool*) {}
void InspectorNetworkAgent::Trace(Visitor*) const {}

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

void DevToolsSession::DomContentLoadedEventFired(LocalFrame*) {}
void DevToolsSession::DidStartProvisionalLoad(LocalFrame*) {}
void DevToolsSession::DidFailProvisionalLoad(LocalFrame*) {}
void DevToolsSession::DidCommitLoad(LocalFrame*, DocumentLoader*) {}
void DevToolsSession::PaintTiming(Document*, const char*, double) {}
void DevToolsSession::Trace(Visitor*) const {}

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

void InspectorAnimationAgent::DidCreateAnimation(unsigned int) {}
void InspectorAnimationAgent::AnimationUpdated(Animation*) {}
void InspectorAnimationAgent::Trace(Visitor*) const {}

void InspectorLogAgent::ConsoleMessageAdded(ConsoleMessage*) {}
void InspectorLogAgent::Trace(Visitor*) const {}

void InspectorAuditsAgent::InspectorIssueAdded(protocol::Audits::InspectorIssue*) {}
void InspectorAuditsAgent::Trace(Visitor*) const {}

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

void InspectorPerformanceAgent::Will(const probe::RecalculateStyle&) {}
void InspectorPerformanceAgent::Did(const probe::RecalculateStyle&) {}
void InspectorPerformanceAgent::Will(const probe::UpdateLayout&) {}
void InspectorPerformanceAgent::Did(const probe::UpdateLayout&) {}
void InspectorPerformanceAgent::Will(const probe::ExecuteScript&) {}
void InspectorPerformanceAgent::Did(const probe::ExecuteScript&) {}
void InspectorPerformanceAgent::Will(const probe::CallFunction&) {}
void InspectorPerformanceAgent::Did(const probe::CallFunction&) {}
void InspectorPerformanceAgent::Will(const probe::V8Compile&) {}
void InspectorPerformanceAgent::Did(const probe::V8Compile&) {}
void InspectorPerformanceAgent::ConsoleTimeStamp(v8::Isolate*, v8::Local<v8::String>) {}
void InspectorPerformanceAgent::WillStartDebuggerTask() {}
void InspectorPerformanceAgent::DidFinishDebuggerTask() {}
void InspectorPerformanceAgent::Trace(Visitor*) const {}

void InspectorDOMDebuggerAgent::Will(const probe::UserCallback&) {}
void InspectorDOMDebuggerAgent::Did(const probe::UserCallback&) {}
void InspectorDOMDebuggerAgent::OnContentSecurityPolicyViolation(ContentSecurityPolicyViolationType) {}
void InspectorDOMDebuggerAgent::Trace(Visitor*) const {}

void InspectorLayerTreeAgent::LayerTreeDidChange() {}
void InspectorLayerTreeAgent::LayerTreePainted() {}
void InspectorLayerTreeAgent::Trace(Visitor*) const {}

void InspectorEmulationAgent::ApplyAcceptLanguageOverride(String*) {}
void InspectorEmulationAgent::ApplyHardwareConcurrencyOverride(unsigned int&) {}
void InspectorEmulationAgent::ApplyUserAgentOverride(String*) {}
void InspectorEmulationAgent::ApplyUserAgentMetadataOverride(std::optional<UserAgentMetadata>*) {}
void InspectorEmulationAgent::PrepareRequest(
    DocumentLoader*, ResourceRequest&, ResourceLoaderOptions&, ResourceType) {}
void InspectorEmulationAgent::WillCommitLoad(LocalFrame*, DocumentLoader*) {}
void InspectorEmulationAgent::WillCreateDocumentParser(bool&) {}
void InspectorEmulationAgent::GetDisabledImageTypes(HashSet<String>*) {}
void InspectorEmulationAgent::ApplyAutomationOverride(bool&) const {}
void InspectorEmulationAgent::Trace(Visitor*) const {}

void InspectorDOMDebuggerAgent::WillInsertDOMNode(Node*) {}
void InspectorDOMDebuggerAgent::DidInsertDOMNode(Node*) {}
void InspectorDOMDebuggerAgent::WillModifyDOMAttr(Element*, const AtomicString&, const AtomicString&) {}
void InspectorDOMDebuggerAgent::CharacterDataModified(CharacterData*) {}
void InspectorDOMDebuggerAgent::WillSendXMLHttpOrFetchNetworkRequest(const String&) {}
void InspectorDOMDebuggerAgent::DidInvalidateStyleAttr(Node*) {}
void InspectorDOMDebuggerAgent::EventListenersInfoForTarget(
    v8::Isolate*, v8::Local<v8::Value>, Vector<V8EventListenerInfo, 0u, WTF::PartitionAllocator>*) {}

void InspectorDOMAgent::DidInsertDOMNode(Node*) {}
void InspectorDOMAgent::WillRemoveDOMNode(Node*) {}
void InspectorDOMAgent::WillModifyDOMAttr(Element*, const AtomicString&, const AtomicString&) {}
void InspectorDOMAgent::DidModifyDOMAttr(Element*, const QualifiedName&, const AtomicString&) {}
void InspectorDOMAgent::DidRemoveDOMAttr(Element*, const QualifiedName&) {}
void InspectorDOMAgent::CharacterDataModified(CharacterData*) {}
void InspectorDOMAgent::DidInvalidateStyleAttr(Node*) {}
void InspectorDOMAgent::DidPerformSlotDistribution(HTMLSlotElement*) {}
void InspectorDOMAgent::DidPushShadowRoot(Element*, ShadowRoot*) {}

void InspectorDOMSnapshotAgent::DidInsertDOMNode(Node*) {}
void InspectorDOMSnapshotAgent::CharacterDataModified(CharacterData*) {}
void InspectorDOMSnapshotAgent::Trace(Visitor*) const {}

void InspectorCSSAgent::WillChangeStyleElement(Element*) {}
void InspectorCSSAgent::DocumentDetached(Document*) {}
void InspectorCSSAgent::ActiveStyleSheetsUpdated(Document*) {}
void InspectorCSSAgent::FontsUpdated(const FontFace*, const String&, const FontCustomPlatformData*) {}
void InspectorCSSAgent::MediaQueryResultChanged() {}

void InspectorEventBreakpointsAgent::DidCreateCanvasContext() {}
void InspectorEventBreakpointsAgent::DidCreateOffscreenCanvasContext() {}
void InspectorEventBreakpointsAgent::DidFireWebGLError(const String&) {}
void InspectorEventBreakpointsAgent::DidFireWebGLWarning() {}
void InspectorEventBreakpointsAgent::DidFireWebGLErrorOrWarning(const String&) {}
void InspectorEventBreakpointsAgent::ScriptExecutionBlockedByCSP(const String&) {}
void InspectorEventBreakpointsAgent::BreakableLocation(const char*) {}
void InspectorEventBreakpointsAgent::Will(const probe::ExecuteScript&) {}
void InspectorEventBreakpointsAgent::Did(const probe::ExecuteScript&) {}
void InspectorEventBreakpointsAgent::Will(const probe::UserCallback&) {}
void InspectorEventBreakpointsAgent::Did(const probe::UserCallback&) {}
void InspectorEventBreakpointsAgent::DidCreateAudioContext() {}
void InspectorEventBreakpointsAgent::DidCloseAudioContext() {}
void InspectorEventBreakpointsAgent::DidResumeAudioContext() {}
void InspectorEventBreakpointsAgent::DidSuspendAudioContext() {}

void InspectorMediaAgent::PlayerErrorsRaised(const WebString&, const Vector<InspectorPlayerError>&) {}
void InspectorMediaAgent::PlayerEventsAdded(const WebString&, const Vector<InspectorPlayerEvent>&) {}
void InspectorMediaAgent::PlayerMessagesLogged(const WebString&, const Vector<InspectorPlayerMessage>&) {}
void InspectorMediaAgent::PlayerPropertiesChanged(const WebString&, const Vector<InspectorPlayerProperty>&) {}
void InspectorMediaAgent::PlayersCreated(const Vector<WebString>&) {}
void InspectorMediaAgent::Trace(Visitor*) const {}

void InspectorPerformanceTimelineAgent::PerformanceEntryAdded(ExecutionContext*, PerformanceEntry*) {}
void InspectorPerformanceTimelineAgent::Trace(Visitor*) const {}

void InspectorPreloadAgent::DidAddSpeculationRuleSet(Document&, const SpeculationRuleSet&) {}
void InspectorPreloadAgent::DidRemoveSpeculationRuleSet(const SpeculationRuleSet&) {}
void InspectorPreloadAgent::SpeculationCandidatesUpdated(Document&, const HeapVector<Member<SpeculationCandidate>>&) {}
void InspectorPreloadAgent::Trace(Visitor*) const {}

void InspectorOverlayAgent::DidInitializeFrameWidget() {}
void InspectorOverlayAgent::Trace(Visitor*) const {}

MainThreadDebugger::MainThreadDebugger(v8::Isolate* isolate)
    : ThreadDebuggerCommonImpl(isolate) {}
MainThreadDebugger::~MainThreadDebugger() = default;

MainThreadDebugger* MainThreadDebugger::Instance(v8::Isolate*) { return nullptr; }
void MainThreadDebugger::ContextWillBeDestroyed(ScriptState*) {}
void MainThreadDebugger::ContextCreated(ScriptState*, LocalFrame*, const SecurityOrigin*) {}
void MainThreadDebugger::runMessageLoopOnPause(int) {}
void MainThreadDebugger::runMessageLoopOnInstrumentationPause(int) {}
void MainThreadDebugger::quitMessageLoopOnPause() {}
void MainThreadDebugger::runIfWaitingForDebugger(int) {}
void MainThreadDebugger::muteMetrics(int) {}
void MainThreadDebugger::unmuteMetrics(int) {}
v8::Local<v8::Context> MainThreadDebugger::ensureDefaultContextInGroup(int) { return {}; }
void MainThreadDebugger::beginEnsureAllContextsInGroup(int) {}
void MainThreadDebugger::endEnsureAllContextsInGroup(int) {}
void MainThreadDebugger::installAdditionalCommandLineAPI(v8::Local<v8::Context>, v8::Local<v8::Object>) {}
void MainThreadDebugger::consoleAPIMessage(int, v8::Isolate::MessageErrorLevel, const v8_inspector::StringView&, const v8_inspector::StringView&, unsigned, unsigned, v8_inspector::V8StackTrace*) {}
v8::MaybeLocal<v8::Value> MainThreadDebugger::memoryInfo(v8::Isolate*, v8::Local<v8::Context>) { return {}; }
void MainThreadDebugger::consoleClear(int) {}
bool MainThreadDebugger::canExecuteScripts(int) { return true; }
int MainThreadDebugger::ContextGroupId(ExecutionContext*) { return 0; }
int MainThreadDebugger::ContextGroupId(LocalFrame*) { return 0; }
void MainThreadDebugger::ReportConsoleMessage(ExecutionContext*, mojom::ConsoleMessageSource, mojom::ConsoleMessageLevel, const WTF::String&, SourceLocation*) {}
void MainThreadDebugger::DidClearContextsForFrame(LocalFrame*) {}
void MainThreadDebugger::ExceptionThrown(ExecutionContext*, ErrorEvent*) {}

MediaInspectorContextImpl* MediaInspectorContextImpl::From(ExecutionContext&) { return nullptr; }

WorkerThreadDebugger::WorkerThreadDebugger(v8::Isolate* isolate)
    : ThreadDebuggerCommonImpl(isolate) {}
WorkerThreadDebugger::~WorkerThreadDebugger() = default;

WorkerThreadDebugger* WorkerThreadDebugger::From(v8::Isolate*) { return nullptr; }
void WorkerThreadDebugger::ExceptionThrown(WorkerThread*, ErrorEvent*) {}
void WorkerThreadDebugger::WorkerThreadCreated(WorkerThread*) {}
void WorkerThreadDebugger::WorkerThreadDestroyed(WorkerThread*) {}
void WorkerThreadDebugger::ContextCreated(WorkerThread*, const KURL&, v8::Local<v8::Context>) {}
void WorkerThreadDebugger::ContextWillBeDestroyed(WorkerThread*, v8::Local<v8::Context>) {}
int WorkerThreadDebugger::ContextGroupId(ExecutionContext*) { return 0; }
void WorkerThreadDebugger::ReportConsoleMessage(ExecutionContext*, mojom::ConsoleMessageSource, mojom::ConsoleMessageLevel, const WTF::String&, SourceLocation*) {}
void WorkerThreadDebugger::runMessageLoopOnPause(int) {}
void WorkerThreadDebugger::quitMessageLoopOnPause() {}
void WorkerThreadDebugger::muteMetrics(int) {}
void WorkerThreadDebugger::unmuteMetrics(int) {}
v8::Local<v8::Context> WorkerThreadDebugger::ensureDefaultContextInGroup(int) { return {}; }
void WorkerThreadDebugger::beginEnsureAllContextsInGroup(int) {}
void WorkerThreadDebugger::endEnsureAllContextsInGroup(int) {}
bool WorkerThreadDebugger::canExecuteScripts(int) { return true; }
void WorkerThreadDebugger::runIfWaitingForDebugger(int) {}
v8::MaybeLocal<v8::Value> WorkerThreadDebugger::memoryInfo(v8::Isolate*, v8::Local<v8::Context>) { return {}; }
void WorkerThreadDebugger::consoleAPIMessage(int, v8::Isolate::MessageErrorLevel, const v8_inspector::StringView&, const v8_inspector::StringView&, unsigned, unsigned, v8_inspector::V8StackTrace*) {}
void WorkerThreadDebugger::consoleClear(int) {}

} // namespace blink
