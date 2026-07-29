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

#include "content/browser/devtools/devtools_agent_host_impl.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/enum_set.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/safe_ref.h"
#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/devtools/dedicated_worker_devtools_agent_host.h"
#include "content/browser/devtools/devtools_preload_storage.h"
#include "content/browser/devtools/devtools_throttle_handle.h"
#include "content/browser/devtools/devtools_url_loader_interceptor.h"
#include "content/browser/devtools/network_service_devtools_observer.h"
#include "content/browser/devtools/protocol/page_handler.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/devtools/shared_worker_devtools_agent_host.h"
#include "content/browser/devtools/shared_worker_devtools_manager.h"
#include "content/browser/devtools/worker_devtools_manager.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/preloading/prerender/prerender_attributes.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/preloading.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/auth.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"
#include "third_party/blink/public/mojom/drag/drag.mojom.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"
#include "url/gurl.h"

namespace download {
struct DownloadCreateInfo;
class DownloadItem;
class DownloadUrlParameters;
}  // namespace download

namespace net {
struct WebTransportError;
}  // namespace net

namespace network {
class URLLoaderFactoryBuilder;
}

#include "content/public/browser/certificate_request_result_type.h"

namespace content {

class FencedFrame;
class FrameTree;
class NavigationThrottleRegistry;
class SignedExchangeEnvelope;
struct SignedExchangeError;
class BackForwardCacheCanStoreDocumentResult;
class BackForwardCacheCanStoreTreeResult;
struct PrerenderMismatchedHeaders;
using CertErrorCallback =
    base::RepeatingCallback<void(CertificateRequestResultType)>;

enum class InterestGroupAuctionEventType;
enum class InterestGroupAuctionFetchType;

class RenderFrameHostImpl;
class RenderProcessHost;
class StoragePartition;
class NavigationRequest;
class FrameTreeNode;
enum JavaScriptDialogType;
using JavaScriptDialogCallback =
    base::OnceCallback<void(bool, const std::u16string&)>;

// static
bool DevToolsAgentHost::IsDebuggerAttached(WebContents* web_contents) {
  return false;
}

// static
void DevToolsAgentHost::DetachAllClients() {}

// static
DevToolsAgentHost::List DevToolsAgentHost::GetOrCreateAll() {
  return {};
}

// static
scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::GetForId(
    const std::string& id) {
  return nullptr;
}

// static
scoped_refptr<DevToolsAgentHostImpl> DevToolsAgentHostImpl::GetForId(
    const std::string& id) {
  return nullptr;
}

// static
void DevToolsAgentHostImpl::GetOrCreateAll() {}

bool DevToolsAgentHostImpl::Inspect() {
  return false;
}

// static
}  // namespace content
