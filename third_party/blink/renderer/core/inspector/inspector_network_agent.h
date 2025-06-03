/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_NETWORK_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_NETWORK_AGENT_H_

#include "base/containers/span.h"
#include "base/unguessable_token.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_page_agent.h"
#include "third_party/blink/renderer/core/inspector/protocol/network.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace network {
namespace mojom {
namespace blink {
class WebSocketHandshakeResponse;
class WebSocketHandshakeRequest;
}  // namespace blink
}  // namespace mojom
}  // namespace network

namespace blink {

class Document;
class DocumentLoader;
class ExecutionContext;
struct FetchInitiatorInfo;
class LocalFrame;
class HTTPHeaderMap;
class KURL;
class NetworkResourcesData;
enum class RenderBlockingBehavior : uint8_t;
class Resource;
class ResourceError;
struct ResourceLoaderOptions;
class ResourceResponse;
class XHRReplayData;
class XMLHttpRequest;
class WorkerGlobalScope;
enum class ResourceRequestBlockedReason;
enum class ResourceType : uint8_t;

class CORE_EXPORT InspectorNetworkAgent final
    : public InspectorBaseAgent<protocol::Network::Metainfo> {
 public:
  // TODO(horo): Extract the logic for frames and for workers into different
  // classes.
  InspectorNetworkAgent(InspectedFrames*,
                        WorkerGlobalScope*,
                        v8_inspector::V8InspectorSession*);
  ~InspectorNetworkAgent() override;
  void Trace(Visitor*) const override;

  void Restore() override;

  // Probes.
  void DidBlockRequest(const ResourceRequest&,
                       DocumentLoader*,
                       const KURL& fetch_context_url,
                       const ResourceLoaderOptions&,
                       ResourceRequestBlockedReason,
                       ResourceType);
  void DidChangeResourcePriority(DocumentLoader*,
                                 uint64_t identifier,
                                 ResourceLoadPriority);
  void PrepareRequest(DocumentLoader*,
                      ResourceRequest&,
                      ResourceLoaderOptions&,
                      ResourceType);
  void WillSendRequest(DocumentLoader*,
                       const KURL& fetch_context_url,
                       const ResourceRequest&,
                       const ResourceResponse& redirect_response,
                       const ResourceLoaderOptions&,
                       ResourceType,
                       RenderBlockingBehavior,
                       base::TimeTicks timestamp);
  void WillSendNavigationRequest(uint64_t identifier,
                                 DocumentLoader*,
                                 const KURL&,
                                 const AtomicString& http_method,
                                 EncodedFormData* http_body);
  void MarkResourceAsCached(DocumentLoader*, uint64_t identifier);
  void DidReceiveResourceResponse(uint64_t identifier,
                                  DocumentLoader*,
                                  const ResourceResponse&,
                                  const Resource*);
  void DidReceiveData(uint64_t identifier,
                      DocumentLoader*,
                      const char* data,
                      uint64_t data_length);
  void DidReceiveBlob(uint64_t identifier,
                      DocumentLoader*,
                      scoped_refptr<BlobDataHandle>);
  void DidReceiveEncodedDataLength(DocumentLoader*,
                                   uint64_t identifier,
                                   size_t encoded_data_length);
  void DidFinishLoading(uint64_t identifier,
                        DocumentLoader*,
                        base::TimeTicks monotonic_finish_time,
                        int64_t encoded_data_length,
                        int64_t decoded_body_length);
  void DidReceiveCorsRedirectResponse(uint64_t identifier,
                                      DocumentLoader*,
                                      const ResourceResponse&,
                                      Resource*);
  void DidFailLoading(
      CoreProbeSink* sink,
      uint64_t identifier,
      DocumentLoader*,
      const ResourceError&,
      const base::UnguessableToken& devtools_frame_or_worker_token);
  void DidCommitLoad(LocalFrame*, DocumentLoader*);
  void ScriptImported(uint64_t identifier, const String& source_string);
  void DidReceiveScriptResponse(uint64_t identifier);
  void ShouldForceCorsPreflight(bool* result);
  void ShouldBlockRequest(const KURL&, bool* result);
  void ShouldBypassServiceWorker(bool* result);

  void WillLoadXHR(ExecutionContext*,
                   const AtomicString& method,
                   const KURL&,
                   bool async,
                   const HTTPHeaderMap& headers,
                   bool include_crendentials);
  void DidFinishXHR(XMLHttpRequest*);

  void WillSendEventSourceRequest();
  void WillDispatchEventSourceEvent(uint64_t identifier,
                                    const AtomicString& event_name,
                                    const AtomicString& event_id,
                                    const String& data);

  void WillDestroyResource(Resource*);

  void FrameScheduledNavigation(LocalFrame*,
                                const KURL&,
                                base::TimeDelta delay,
                                ClientNavigationReason);
  void FrameClearedScheduledNavigation(LocalFrame*);

  void WillCreateWebSocket(ExecutionContext*,
                           uint64_t identifier,
                           const KURL& request_url,
                           const String&,
                           absl::optional<base::UnguessableToken>*);
  void WillSendWebSocketHandshakeRequest(
      ExecutionContext*,
      uint64_t identifier,
      network::mojom::blink::WebSocketHandshakeRequest*);
  void DidReceiveWebSocketHandshakeResponse(
      ExecutionContext*,
      uint64_t identifier,
      network::mojom::blink::WebSocketHandshakeRequest*,
      network::mojom::blink::WebSocketHandshakeResponse*);
  void DidCloseWebSocket(ExecutionContext*, uint64_t identifier);
  void DidReceiveWebSocketMessage(uint64_t identifier,
                                  int op_code,
                                  bool masked,
                                  const Vector<base::span<const char>>& data);
  void DidSendWebSocketMessage(uint64_t identifier,
                               int op_code,
                               bool masked,
                               const char* payload,
                               size_t payload_length);
  void DidReceiveWebSocketMessageError(uint64_t identifier, const String&);

  void WebTransportCreated(ExecutionContext*,
                           uint64_t transport_id,
                           const KURL& request_url);
  void WebTransportConnectionEstablished(uint64_t transport_id);
  void WebTransportClosed(uint64_t transport_id);

  void SetDevToolsIds(ResourceRequest& request, const FetchInitiatorInfo&);
  void IsCacheDisabled(bool* is_cache_disabled) const;

  // Called from frontend
  protocol::Response enable(Maybe<int> total_buffer_size,
                            Maybe<int> resource_buffer_size,
                            Maybe<int> max_post_data_size) override;
  protocol::Response disable() override;
  protocol::Response setExtraHTTPHeaders(
      std::unique_ptr<protocol::Network::Headers>) override;
  protocol::Response setAttachDebugStack(bool enabled) override;
  void getResponseBody(const String& request_id,
                       std::unique_ptr<GetResponseBodyCallback>) override;
  protocol::Response searchInResponseBody(
      const String& request_id,
      const String& query,
      Maybe<bool> case_sensitive,
      Maybe<bool> is_regex,
      std::unique_ptr<
          protocol::Array<v8_inspector::protocol::Debugger::API::SearchMatch>>*
          matches) override;

  protocol::Response setBlockedURLs(
      std::unique_ptr<protocol::Array<String>> urls) override;
  protocol::Response replayXHR(const String& request_id) override;
  protocol::Response canClearBrowserCache(bool* result) override;
  protocol::Response canClearBrowserCookies(bool* result) override;
  protocol::Response emulateNetworkConditions(
      bool offline,
      double latency,
      double download_throughput,
      double upload_throughput,
      Maybe<String> connection_type) override;
  protocol::Response setCacheDisabled(bool) override;
  protocol::Response setBypassServiceWorker(bool) override;
  protocol::Response getCertificate(
      const String& origin,
      std::unique_ptr<protocol::Array<String>>* certificate) override;

  void getRequestPostData(const String& request_id,
                          std::unique_ptr<GetRequestPostDataCallback>) override;

  protocol::Response setAcceptedEncodings(
      std::unique_ptr<protocol::Array<protocol::Network::ContentEncoding>>
          encodings) override;

  protocol::Response clearAcceptedEncodingsOverride() override;

  // Called from other agents.
  protocol::Response GetResponseBody(const String& request_id,
                                     String* content,
                                     bool* base64_encoded);
  bool FetchResourceContent(Document*,
                            const KURL&,
                            String* content,
                            bool* base64_encoded,
                            bool* loadingFailed);
  String NavigationInitiatorInfo(LocalFrame*);

 private:
  String RequestId(DocumentLoader*, uint64_t identifier);
  void Enable();
  void WillSendRequestInternal(DocumentLoader*,
                               const KURL& fetch_context_url,
                               const ResourceRequest&,
                               const ResourceResponse& redirect_response,
                               const ResourceLoaderOptions&,
                               InspectorPageAgent::ResourceType,
                               base::TimeTicks timestamp);

  bool CanGetResponseBodyBlob(const String& request_id);
  void GetResponseBodyBlob(const String& request_id,
                           std::unique_ptr<GetResponseBodyCallback>);
  ExecutionContext* GetTargetExecutionContext() const;

  static std::unique_ptr<protocol::Network::Initiator> BuildInitiatorObject(
      Document*,
      const FetchInitiatorInfo&,
      int max_async_depth);
  static bool IsNavigation(DocumentLoader*, uint64_t identifier);

  // This is null while inspecting workers.
  Member<InspectedFrames> inspected_frames_;
  // This is null while inspecting frames.
  Member<WorkerGlobalScope> worker_global_scope_;
  v8_inspector::V8InspectorSession* v8_session_;
  Member<NetworkResourcesData> resources_data_;
  const base::UnguessableToken devtools_token_;

  // Stores the pending request type till an identifier for the load is
  // generated by the loader and passed to the inspector via the
  // WillSendRequest() method.
  absl::optional<InspectorPageAgent::ResourceType> pending_request_type_;

  Member<XHRReplayData> pending_xhr_replay_data_;

  HashMap<String, std::unique_ptr<protocol::Network::Initiator>>
      frame_navigation_initiator_map_;

  HeapHashSet<Member<XMLHttpRequest>> replay_xhrs_;
  InspectorAgentState::Boolean enabled_;
  InspectorAgentState::Boolean cache_disabled_;
  InspectorAgentState::Boolean bypass_service_worker_;
  InspectorAgentState::BooleanMap blocked_urls_;
  InspectorAgentState::StringMap extra_request_headers_;
  InspectorAgentState::Boolean attach_debug_stack_enabled_;
  InspectorAgentState::Integer total_buffer_size_;
  InspectorAgentState::Integer resource_buffer_size_;
  InspectorAgentState::Integer max_post_data_size_;
  InspectorAgentState::BooleanMap accepted_encodings_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_NETWORK_AGENT_H_
