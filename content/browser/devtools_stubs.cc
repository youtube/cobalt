#include "base/memory/ref_counted.h"
#include "base/memory/safe_ref.h"
#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/global_routing_id.h"
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

#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/preloading/prerender/prerender_attributes.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/preloading.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"

namespace download {
struct DownloadCreateInfo;
class DownloadItem;
struct DownloadUrlParameters;
}  // namespace download

namespace net {
struct WebTransportError;
}  // namespace net

namespace network {
class URLLoaderFactoryBuilder;
}

namespace content {

class FencedFrame;
class FrameTree;
class NavigationThrottleRegistry;
class SignedExchangeEnvelope;
struct SignedExchangeError;
class BackForwardCacheCanStoreDocumentResult;
class BackForwardCacheCanStoreTreeResult;
struct PrerenderMismatchedHeaders;
using CertErrorCallback = base::RepeatingCallback<void(int)>;

enum class InterestGroupAuctionEventType;
enum class InterestGroupAuctionFetchType;

class RenderFrameHostImpl;
class RenderProcessHost;
class StoragePartition;
class NavigationRequest;
class FrameTreeNode;
enum class JavaScriptDialogType;
using JavaScriptDialogCallback = base::OnceCallback<void(bool, const std::u16string&)>;

class DevToolsThrottleHandle : public base::RefCounted<DevToolsThrottleHandle> {
 public:
  explicit DevToolsThrottleHandle(base::OnceCallback<void()> throttle_callback);
 private:
  friend class base::RefCounted<DevToolsThrottleHandle>;
  ~DevToolsThrottleHandle();
  base::OnceCallback<void()> throttle_callback_;
};

class DevToolsAgentHostImpl : public DevToolsAgentHost {
 public:
  bool Inspect();
};

class RenderFrameDevToolsAgentHost {
 public:
  static void AttachToWebContents(WebContents* web_contents);
  static DevToolsAgentHostImpl* GetFor(RenderFrameHostImpl* host);
  static bool WasEverAttachedToAnyFrame();
};

class SharedWorkerHost;
class SharedStorageWorkletHost;
class DedicatedWorkerHost;
class WebContentsImpl;
class ServiceWorkerContextWrapper;

class SharedWorkerDevToolsManager {
 public:
  static SharedWorkerDevToolsManager* GetInstance();
  SharedWorkerDevToolsManager();
  ~SharedWorkerDevToolsManager();
  void WorkerCreated(SharedWorkerHost*, bool*, base::UnguessableToken*);
  void WorkerReadyForInspection(SharedWorkerHost*, mojo::PendingRemote<blink::mojom::DevToolsAgent>, mojo::PendingReceiver<blink::mojom::DevToolsAgentHost>);
  void WorkerDestroyed(SharedWorkerHost*);
};

class SharedWorkerDevToolsAgentHost {
 public:
  static SharedWorkerDevToolsAgentHost* GetFor(SharedWorkerHost* host);
};

class SharedStorageWorkletDevToolsManager {
 public:
  static SharedStorageWorkletDevToolsManager* GetInstance();
  SharedStorageWorkletDevToolsManager();
  ~SharedStorageWorkletDevToolsManager();
  void WorkletCreated(SharedStorageWorkletHost&, const base::UnguessableToken&, bool&);
  void WorkletReadyForInspection(SharedStorageWorkletHost&, mojo::PendingRemote<blink::mojom::DevToolsAgent>, mojo::PendingReceiver<blink::mojom::DevToolsAgentHost>);
  void WorkletDestroyed(SharedStorageWorkletHost&);
};

class DedicatedWorkerDevToolsAgentHost {
 public:
  static DedicatedWorkerDevToolsAgentHost* GetFor(const DedicatedWorkerHost* host);
};

class WorkerDevToolsManager {
 public:
  static WorkerDevToolsManager& GetInstance();
  WorkerDevToolsManager();
  ~WorkerDevToolsManager();
  void WorkerCreated(const DedicatedWorkerHost*, int, const GlobalRenderFrameHostId&, scoped_refptr<DevToolsThrottleHandle>);
  void WorkerDestroyed(const DedicatedWorkerHost*);
};

class ServiceWorkerDevToolsAgentHost {};

class DevToolsURLLoaderInterceptor {
 public:
  static void HandleAuthRequest(
      GlobalRequestID request_id,
      const net::AuthChallengeInfo& auth_info,
      base::OnceCallback<void(bool, const std::optional<net::AuthCredentials>&)> callback);
};

class ServiceWorkerDevToolsManager {
 public:
  static ServiceWorkerDevToolsManager* GetInstance();
  ServiceWorkerDevToolsManager();
  ~ServiceWorkerDevToolsManager();
  void WorkerVersionInstalled(int, int);
  void set_debug_service_worker_on_start(bool);
  ServiceWorkerDevToolsAgentHost* GetDevToolsAgentHostForWorker(int, int);
  void WorkerVersionDoomed(int, int, scoped_refptr<ServiceWorkerContextWrapper>, int64_t);
  void WorkerStopped(int, int);
  void WorkerStarting(int, int, scoped_refptr<ServiceWorkerContextWrapper>, int64_t, const GURL&, const GURL&, bool, network::mojom::ClientSecurityStatePtr, mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>, mojo::PendingRemote<network::mojom::DocumentIsolationPolicyReporter>, base::UnguessableToken*, bool*);
  void WorkerReadyForInspection(int, int, mojo::PendingRemote<blink::mojom::DevToolsAgent>, mojo::PendingReceiver<blink::mojom::DevToolsAgentHost>);
  void WorkerDestroyed(int, int, const blink::DedicatedWorkerToken&);
  void WorkerMainScriptFetchingStarting(scoped_refptr<ServiceWorkerContextWrapper>, int64_t, const GURL&, const GURL&, const GlobalRenderFrameHostId&, scoped_refptr<DevToolsThrottleHandle>);
  void WorkerMainScriptFetchingFailed(scoped_refptr<ServiceWorkerContextWrapper>, int64_t);
  void NavigationPreloadRequestSent(int, int, const std::string&, const network::ResourceRequest&);
  void NavigationPreloadResponseReceived(int, int, const std::string&, const GURL&, const network::mojom::URLResponseHead&);
  void NavigationPreloadCompleted(int, int, const std::string&, const network::URLLoaderCompletionStatus&);
};

class NetworkServiceDevToolsObserver {
 public:
  static mojo::PendingRemote<network::mojom::DevToolsObserver> MakeSelfOwned(const std::string&);
  static mojo::PendingRemote<network::mojom::DevToolsObserver> MakeSelfOwned(FrameTreeNode*);
};

namespace protocol {
class PageHandler {
 public:
  using JavaScriptDialogCallback = base::OnceCallback<void(bool, const std::u16string&)>;
  static std::vector<PageHandler*> EnabledForWebContents(WebContentsImpl* contents);
  void DidRunJavaScriptDialog(const GURL&, const base::UnguessableToken&, const std::u16string&, const std::u16string&, JavaScriptDialogType, bool, JavaScriptDialogCallback);
  void DidCloseJavaScriptDialog(const base::UnguessableToken&, bool, const std::u16string&);
  void DidRunBeforeUnloadConfirm(const GURL&, const base::UnguessableToken&, bool, JavaScriptDialogCallback);
};
}  // namespace protocol

// static
SharedWorkerDevToolsManager* SharedWorkerDevToolsManager::GetInstance() {
  return base::Singleton<SharedWorkerDevToolsManager>::get();
}

SharedWorkerDevToolsManager::SharedWorkerDevToolsManager() = default;
SharedWorkerDevToolsManager::~SharedWorkerDevToolsManager() = default;

void SharedWorkerDevToolsManager::WorkerCreated(
    SharedWorkerHost* worker_host,
    bool* pause_on_start,
    base::UnguessableToken* devtools_worker_token) {}

void SharedWorkerDevToolsManager::WorkerReadyForInspection(
    SharedWorkerHost* worker_host,
    mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
    mojo::PendingReceiver<blink::mojom::DevToolsAgentHost> agent_host_receiver) {}

void SharedWorkerDevToolsManager::WorkerDestroyed(SharedWorkerHost* worker_host) {}

// static
SharedWorkerDevToolsAgentHost* SharedWorkerDevToolsAgentHost::GetFor(
    SharedWorkerHost* host) {
  return nullptr;
}

// static
SharedStorageWorkletDevToolsManager* SharedStorageWorkletDevToolsManager::GetInstance() {
  return base::Singleton<SharedStorageWorkletDevToolsManager>::get();
}

SharedStorageWorkletDevToolsManager::SharedStorageWorkletDevToolsManager() = default;
SharedStorageWorkletDevToolsManager::~SharedStorageWorkletDevToolsManager() = default;

void SharedStorageWorkletDevToolsManager::WorkletCreated(
    SharedStorageWorkletHost& worklet_host,
    const base::UnguessableToken& devtools_worklet_token,
    bool& wait_for_debugger) {}

void SharedStorageWorkletDevToolsManager::WorkletReadyForInspection(
    SharedStorageWorkletHost& worklet_host,
    mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
    mojo::PendingReceiver<blink::mojom::DevToolsAgentHost> agent_host_receiver) {}

void SharedStorageWorkletDevToolsManager::WorkletDestroyed(
    SharedStorageWorkletHost& worklet_host) {}

// static
mojo::PendingRemote<network::mojom::DevToolsObserver> NetworkServiceDevToolsObserver::MakeSelfOwned(
    const std::string& id) {
  return {};
}

// static
mojo::PendingRemote<network::mojom::DevToolsObserver> NetworkServiceDevToolsObserver::MakeSelfOwned(
    FrameTreeNode* frame_tree_node) {
  return {};
}

// static
DedicatedWorkerDevToolsAgentHost* DedicatedWorkerDevToolsAgentHost::GetFor(
    const DedicatedWorkerHost* host) {
  return nullptr;
}

DevToolsThrottleHandle::DevToolsThrottleHandle(base::OnceCallback<void()> throttle_callback)
    : throttle_callback_(std::move(throttle_callback)) {}
DevToolsThrottleHandle::~DevToolsThrottleHandle() {
  if (throttle_callback_) {
    std::move(throttle_callback_).Run();
  }
}

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
scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::GetForId(const std::string& id) {
  return nullptr;
}

bool DevToolsAgentHostImpl::Inspect() {
  return false;
}

// static
void RenderFrameDevToolsAgentHost::AttachToWebContents(WebContents* web_contents) {
  // no-op
}

// static
DevToolsAgentHostImpl* RenderFrameDevToolsAgentHost::GetFor(RenderFrameHostImpl* host) {
  return nullptr;
}

bool RenderFrameDevToolsAgentHost::WasEverAttachedToAnyFrame() {
  return false;
}

// static
WorkerDevToolsManager& WorkerDevToolsManager::GetInstance() {
  return *base::Singleton<WorkerDevToolsManager>::get();
}

WorkerDevToolsManager::WorkerDevToolsManager() = default;
WorkerDevToolsManager::~WorkerDevToolsManager() = default;

void WorkerDevToolsManager::WorkerCreated(
    const DedicatedWorkerHost* host,
    int process_id,
    const GlobalRenderFrameHostId& ancestor_render_frame_host_id,
    scoped_refptr<DevToolsThrottleHandle> throttle_handle) {}

void WorkerDevToolsManager::WorkerDestroyed(const DedicatedWorkerHost* host) {}

// static
void DevToolsURLLoaderInterceptor::HandleAuthRequest(
    GlobalRequestID request_id,
    const net::AuthChallengeInfo& auth_info,
    base::OnceCallback<void(bool, const std::optional<net::AuthCredentials>&)> callback) {
  std::move(callback).Run(false, std::nullopt);
}

// static
ServiceWorkerDevToolsManager* ServiceWorkerDevToolsManager::GetInstance() {
  static base::NoDestructor<ServiceWorkerDevToolsManager> instance;
  return instance.get();
}

ServiceWorkerDevToolsManager::ServiceWorkerDevToolsManager() = default;
ServiceWorkerDevToolsManager::~ServiceWorkerDevToolsManager() = default;

void ServiceWorkerDevToolsManager::WorkerMainScriptFetchingStarting(
    scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
    int64_t version_id,
    const GURL& url,
    const GURL& scope,
    const GlobalRenderFrameHostId& requesting_frame_id,
    scoped_refptr<DevToolsThrottleHandle> throttle_handle) {}

void ServiceWorkerDevToolsManager::WorkerMainScriptFetchingFailed(
    scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
    int64_t version_id) {}

void ServiceWorkerDevToolsManager::NavigationPreloadRequestSent(
    int worker_process_id,
    int worker_route_id,
    const std::string& request_id,
    const network::ResourceRequest& request) {}

void ServiceWorkerDevToolsManager::NavigationPreloadResponseReceived(
    int worker_process_id,
    int worker_route_id,
    const std::string& request_id,
    const GURL& url,
    const network::mojom::URLResponseHead& head) {}

void ServiceWorkerDevToolsManager::NavigationPreloadCompleted(
    int worker_process_id,
    int worker_route_id,
    const std::string& request_id,
    const network::URLLoaderCompletionStatus& status) {}

void ServiceWorkerDevToolsManager::WorkerReadyForInspection(
    int worker_process_id,
    int worker_route_id,
    mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
    mojo::PendingReceiver<blink::mojom::DevToolsAgentHost> host_receiver) {}

void ServiceWorkerDevToolsManager::WorkerVersionInstalled(
    int worker_process_id, int worker_route_id) {}

void ServiceWorkerDevToolsManager::set_debug_service_worker_on_start(bool) {}

ServiceWorkerDevToolsAgentHost*
ServiceWorkerDevToolsManager::GetDevToolsAgentHostForWorker(
    int worker_process_id,
    int worker_route_id) {
  return nullptr;
}

void ServiceWorkerDevToolsManager::WorkerVersionDoomed(
    int worker_process_id,
    int worker_route_id,
    scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
    int64_t version_id) {}

void ServiceWorkerDevToolsManager::WorkerStopped(
    int worker_process_id, int worker_route_id) {}

void ServiceWorkerDevToolsManager::WorkerStarting(
    int worker_process_id,
    int worker_route_id,
    scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
    int64_t version_id,
    const GURL& url,
    const GURL& scope,
    bool is_installed_version,
    network::mojom::ClientSecurityStatePtr client_security_state,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter> coep_reporter,
    mojo::PendingRemote<network::mojom::DocumentIsolationPolicyReporter> dip_reporter,
    base::UnguessableToken* devtools_worker_token,
    bool* pause_on_start) {}

namespace protocol {
// static
std::vector<PageHandler*> PageHandler::EnabledForWebContents(WebContentsImpl* contents) {
  return {};
}

void PageHandler::DidRunJavaScriptDialog(
    const GURL& url,
    const base::UnguessableToken& frame_id,
    const std::u16string& message,
    const std::u16string& default_prompt,
    JavaScriptDialogType dialog_type,
    bool has_non_devtools_handlers,
    JavaScriptDialogCallback callback) {}

void PageHandler::DidCloseJavaScriptDialog(
    const base::UnguessableToken& frame_id,
    bool success,
    const std::u16string& user_input) {}

void PageHandler::DidRunBeforeUnloadConfirm(
    const GURL& url,
    const base::UnguessableToken& frame_id,
    bool has_non_devtools_handlers,
    JavaScriptDialogCallback callback) {}
} // namespace protocol

namespace devtools_instrumentation {

struct WillCreateURLLoaderFactoryParams {
  static WillCreateURLLoaderFactoryParams ForFrame(RenderFrameHostImpl* rfh);
  static WillCreateURLLoaderFactoryParams ForServiceWorker(RenderProcessHost& rph, int routing_id);
  static std::optional<WillCreateURLLoaderFactoryParams> ForServiceWorkerMainScript(
      const ServiceWorkerContextWrapper* context_wrapper, std::optional<int64_t> version_id);
  static std::optional<WillCreateURLLoaderFactoryParams> ForSharedWorker(SharedWorkerHost* host);
  static WillCreateURLLoaderFactoryParams ForWorkerMainScript(
      DevToolsAgentHostImpl* agent_host, const base::UnguessableToken& worker_token, RenderFrameHostImpl& ancestor_render_frame_host);

  WillCreateURLLoaderFactoryParams(DevToolsAgentHostImpl*, const base::UnguessableToken&, int, StoragePartition*);
  bool Run(bool, bool, network::URLLoaderFactoryBuilder&, network::mojom::URLLoaderFactoryOverridePtr*);

  DevToolsAgentHostImpl* agent_host_;
  base::UnguessableToken devtools_token_;
  int process_id_;
  StoragePartition* storage_partition_;
};

WillCreateURLLoaderFactoryParams WillCreateURLLoaderFactoryParams::ForFrame(RenderFrameHostImpl* rfh) {
  return WillCreateURLLoaderFactoryParams(nullptr, {}, -1, nullptr);
}

WillCreateURLLoaderFactoryParams WillCreateURLLoaderFactoryParams::ForServiceWorker(
    RenderProcessHost& rph, int routing_id) {
  return WillCreateURLLoaderFactoryParams(nullptr, {}, -1, nullptr);
}

std::optional<WillCreateURLLoaderFactoryParams>
WillCreateURLLoaderFactoryParams::ForServiceWorkerMainScript(
    const ServiceWorkerContextWrapper* context_wrapper,
    std::optional<int64_t> version_id) {
  return std::nullopt;
}

std::optional<WillCreateURLLoaderFactoryParams>
WillCreateURLLoaderFactoryParams::ForSharedWorker(SharedWorkerHost* host) {
  return std::nullopt;
}

WillCreateURLLoaderFactoryParams WillCreateURLLoaderFactoryParams::ForWorkerMainScript(
    DevToolsAgentHostImpl* agent_host,
    const base::UnguessableToken& worker_token,
    RenderFrameHostImpl& ancestor_render_frame_host) {
  return WillCreateURLLoaderFactoryParams(nullptr, {}, -1, nullptr);
}

WillCreateURLLoaderFactoryParams::WillCreateURLLoaderFactoryParams(
    DevToolsAgentHostImpl* agent_host,
    const base::UnguessableToken& devtools_token,
    int process_id,
    StoragePartition* storage_partition)
    : agent_host_(agent_host),
      devtools_token_(devtools_token),
      process_id_(process_id),
      storage_partition_(storage_partition) {
  (void)agent_host_;
  (void)devtools_token_;
  (void)process_id_;
  (void)storage_partition_;
}

bool WillCreateURLLoaderFactoryParams::Run(
    bool is_navigation,
    bool is_download,
    network::URLLoaderFactoryBuilder& factory_builder,
    network::mojom::URLLoaderFactoryOverridePtr* factory_override) {
  return false;
}

bool ApplyNetworkCookieControlsOverrides(
    RenderFrameHostImpl& rfh,
    base::EnumSet<net::CookieSettingOverride, (net::CookieSettingOverride)0, (net::CookieSettingOverride)10>& overrides) {
  return false;
}

void WillSwapFrameTreeNode(FrameTreeNode& old_node, FrameTreeNode& new_node) {}

void OnFetchKeepAliveRequestWillBeSent(
    FrameTreeNode* frame_tree_node,
    const std::string& request_id,
    const network::ResourceRequest& request,
    std::optional<std::pair<const GURL&, const network::mojom::URLResponseHeadDevToolsInfo&>> redirect_info) {}

void OnFetchKeepAliveResponseReceived(
    FrameTreeNode* frame_tree_node,
    const std::string& request_id,
    const GURL& url,
    const network::mojom::URLResponseHead& response_head) {}

void OnFetchKeepAliveRequestComplete(
    FrameTreeNode* frame_tree_node,
    const std::string& request_id,
    const network::URLLoaderCompletionStatus& status) {}

bool ShouldBypassCSP(const NavigationRequest& nav_request) {
  return false;
}

void OnNavigationResponseReceived(
    const NavigationRequest& navigation_request,
    const network::mojom::URLResponseHead& response_head) {}

void ApplyNetworkRequestOverrides(
    FrameTreeNode* frame_tree_node,
    blink::mojom::BeginNavigationParams* begin_params,
    bool* report_raw_headers,
    std::optional<std::vector<net::SourceStreamType>>* devtools_accepted_stream_types,
    bool* devtools_user_agent_overridden,
    bool* devtools_accept_language_overridden) {}

void OnNavigationRequestWillBeSent(const NavigationRequest& navigation_request) {}

void DragEnded(FrameTreeNode& frame_tree_node) {}

void MaybeAssignResourceRequestId(
    FrameTreeNodeId frame_tree_node_id,
    const std::string& id,
    network::ResourceRequest& request) {}

void MaybeAssignResourceRequestId(
    FrameTreeNode* ftn,
    const std::string& id,
    network::ResourceRequest& request) {}

bool NeedInterestGroupAuctionEvents(FrameTreeNodeId frame_tree_node_id) {
  return false;
}

void OnInterestGroupAuctionEventOccurred(
    FrameTreeNodeId frame_tree_node_id,
    base::Time event_time,
    InterestGroupAuctionEventType type,
    const std::string& unique_auction_id,
    base::optional_ref<const std::string> parent_auction_id,
    const base::Value::Dict& auction_config) {}

void OnInterestGroupAuctionNetworkRequestCreated(
    FrameTreeNodeId frame_tree_node_id,
    InterestGroupAuctionFetchType type,
    const std::string& request_id,
    const std::vector<std::string>& devtools_auction_ids) {}

void DidUpdatePrefetchStatus(
    FrameTreeNode* ftn,
    const base::UnguessableToken& initiator_devtools_navigation_token,
    const GURL& prefetch_url,
    const base::UnguessableToken& preload_pipeline_id,
    PreloadingTriggeringOutcome status,
    PrefetchStatus prefetch_status,
    const std::string& request_id) {}

void DidUpdatePrerenderStatus(
    FrameTreeNodeId initiator_frame_tree_node_id,
    const base::UnguessableToken& initiator_devtools_navigation_token,
    const GURL& prerender_url,
    std::optional<blink::mojom::SpeculationTargetHint> target_hint,
    const base::UnguessableToken& preload_pipeline_id,
    PreloadingTriggeringOutcome status,
    std::optional<PrerenderFinalStatus> prerender_status,
    std::optional<std::string> disallowed_mojo_interface,
    const std::vector<PrerenderMismatchedHeaders>* mismatched_headers) {}

void FencedFrameCreated(
    base::SafeRef<RenderFrameHostImpl> owner_render_frame_host,
    FencedFrame* fenced_frame) {}

bool ApplyUserAgentMetadataOverrides(
    FrameTreeNode* frame_tree_node,
    std::optional<blink::UserAgentMetadata>* override_out) {
  return false;
}

void WillBeginDownload(download::DownloadCreateInfo* info, download::DownloadItem* item) {}

void OnFencedFrameReportRequestSent(
    FrameTreeNodeId initiator_frame_tree_node_id,
    const std::string& devtools_request_id,
    network::ResourceRequest& request,
    const std::string& event_data) {}

void OnFencedFrameReportResponseReceived(
    FrameTreeNodeId initiator_frame_tree_node_id,
    const std::string& devtools_request_id,
    const GURL& final_url,
    scoped_refptr<net::HttpResponseHeaders> headers) {}

bool ApplyNetworkCookieControlsOverrides(
    DevToolsAgentHostImpl* params,
    net::CookieSettingOverrides& overrides) {
  return false;
}

void WillSendFedCmNetworkRequest(
    FrameTreeNodeId frame_tree_node_id,
    const network::ResourceRequest& request) {}

void DidReceiveFedCmNetworkResponse(
    FrameTreeNodeId frame_tree_node_id,
    const std::string& request_id,
    const GURL& url,
    const network::mojom::URLResponseHead* response_head,
    const std::string& response_body,
    const network::URLLoaderCompletionStatus& status) {}

void BackForwardCacheNotUsed(
    const NavigationRequest* navigation_request,
    const BackForwardCacheCanStoreDocumentResult* can_store_file_result,
    const BackForwardCacheCanStoreTreeResult* can_store_tree_result) {}

void DidShowFedCmDialog(RenderFrameHost& render_frame_host) {}
void DidCloseFedCmDialog(RenderFrameHost& render_frame_host) {}

bool HandleCertificateError(
    WebContents* web_contents,
    int cert_error,
    const GURL& request_url,
    CertErrorCallback callback) {
  return false;
}

void OnFrameTreeNodeDestroyed(FrameTreeNode& frame_tree_node) {}
void OnResetNavigationRequest(NavigationRequest* navigation_request) {}

void DidStartNavigating(
    FrameTreeNode& ftn,
    const GURL& url,
    const base::UnguessableToken& loader_id,
    const blink::mojom::NavigationType& navigation_type) {}

void WillStartDragging(
    FrameTreeNode* frame_tree_node,
    const DropData& drop_data,
    mojo::StructPtr<blink::mojom::DragData> drag_data,
    blink::DragOperationsMask ops,
    bool* filtered) {}

void OnWorkerMainScriptLoadingFailed(
    const GURL& url,
    const base::UnguessableToken& worker_token,
    FrameTreeNode* frame_tree_node,
    RenderFrameHostImpl* render_frame_host,
    const network::URLLoaderCompletionStatus& status) {}

void OnServiceWorkerMainScriptRequestWillBeSent(
    const GlobalRenderFrameHostId& requesting_frame_id,
    const ServiceWorkerContextWrapper* context_wrapper,
    int64_t version_id,
    network::ResourceRequest& request) {}

void OnWorkerMainScriptRequestWillBeSent(
    RenderFrameHostImpl& render_frame_host,
    const base::UnguessableToken& devtools_worker_token,
    network::ResourceRequest& request) {}

void DidChangeFrameLoadingState(FrameTreeNode& ftn) {}

void OnAuctionWorkletNetworkRequestWillBeSent(
    FrameTreeNodeId frame_tree_node_id,
    const network::ResourceRequest& request,
    base::TimeTicks timestamp) {}

void OnAuctionWorkletNetworkResponseReceived(
    FrameTreeNodeId frame_tree_node_id,
    const std::string& request_id,
    const std::string& devtools_auction_id,
    const GURL& url,
    const network::mojom::URLResponseHead& head) {}

void OnServiceWorkerMainScriptFetchingFailed(
    const GlobalRenderFrameHostId& requesting_frame_id,
    const ServiceWorkerContextWrapper* context_wrapper,
    int64_t version_id,
    const std::string& request_id,
    const network::URLLoaderCompletionStatus& status,
    const network::mojom::URLResponseHead* response_head,
    const GURL& url) {}

void LogWorkletMessage(
    RenderFrameHostImpl& frame_host,
    blink::mojom::ConsoleMessageLevel log_level,
    const std::string& message) {}

void ApplyNetworkContextParamsOverrides(
    BrowserContext* browser_context,
    network::mojom::NetworkContextParams* network_context_params) {}

void OnWebTransportHandshakeFailed(
    RenderFrameHostImpl* frame_host,
    const GURL& url,
    const std::optional<net::WebTransportError>& error) {}

void WillSendFedCmRequest(
    RenderFrameHost& render_frame_host,
    bool* intercept_request,
    bool* is_disabled) {}

void OnNavigationRequestFailed(
    const NavigationRequest& navigation_request,
    const network::URLLoaderCompletionStatus& status) {}

void WillShowFedCmDialog(
    RenderFrameHost& render_frame_host,
    bool* intercept) {}

bool IsPrerenderAllowed(FrameTree& frame_tree) {
  return true;
}

void DidActivatePrerender(
    const NavigationRequest& navigation_request,
    const std::optional<base::UnguessableToken>& initiated_by_devtools_token) {}

void WillInitiatePrerender(FrameTree& frame_tree) {}

void ReportCookieIssue(
    RenderFrameHostImpl* render_frame_host,
    const mojo::StructPtr<network::mojom::CookieOrLineWithAccessResult>& cookie_with_access_result,
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    blink::mojom::CookieOperation operation,
    const std::optional<std::string>& devtools_request_id,
    const std::optional<std::string>& devtools_client_security_state_issue_id) {}

void CreateAndAddNavigationThrottles(NavigationThrottleRegistry& registry) {}

bool ShouldBypassCertificateErrors() {
  return false;
}

void ApplyAuctionNetworkRequestOverrides(
    FrameTreeNode* frame_tree_node,
    network::ResourceRequest* request,
    bool* network_instrumentation_enabled) {}

void OnAuctionWorkletNetworkRequestWillBeSent(
    FrameTreeNode* frame_tree_node,
    const std::string& request_id,
    const network::ResourceRequest& request) {}

void OnAuctionWorkletNetworkResponseReceived(
    FrameTreeNode* frame_tree_node,
    const std::string& request_id,
    const GURL& url,
    const network::mojom::URLResponseHead& response_head) {}

void OnAuctionWorkletNetworkRequestComplete(
    FrameTreeNode* frame_tree_node,
    const std::string& request_id,
    const network::URLLoaderCompletionStatus& status) {}

void OnAuctionWorkletNetworkRequestComplete(
    FrameTreeNodeId frame_tree_node_id,
    const std::string& request_id,
    const network::URLLoaderCompletionStatus& status) {}

void DidUpdateSpeculationCandidates(
    RenderFrameHost& frame,
    const std::vector<blink::mojom::SpeculationCandidatePtr>& candidates) {}

void OnSignedExchangeCertificateRequestSent(
    FrameTreeNode* frame_tree_node,
    const base::UnguessableToken& request_id,
    const base::UnguessableToken& loader_id,
    const network::ResourceRequest& request,
    const GURL& url) {}

void OnSignedExchangeCertificateResponseReceived(
    FrameTreeNode* frame_tree_node,
    const base::UnguessableToken& request_id,
    const base::UnguessableToken& loader_id,
    const GURL& url,
    const network::mojom::URLResponseHead& response_head) {}

void OnSignedExchangeCertificateRequestCompleted(
    FrameTreeNode* frame_tree_node,
    const base::UnguessableToken& request_id,
    const network::URLLoaderCompletionStatus& status) {}

void OnSignedExchangeReceived(
    FrameTreeNode* frame_tree_node,
    std::optional<const base::UnguessableToken> devtools_navigation_token,
    const GURL& outer_response_url,
    const network::mojom::URLResponseHead& outer_response_header,
    const std::optional<SignedExchangeEnvelope>& envelope,
    const scoped_refptr<net::X509Certificate>& certificate,
    const std::optional<net::SSLInfo>& ssl_info,
    const std::vector<SignedExchangeError>& errors) {}

void OnPrefetchRequestComplete(
    FrameTreeNode* frame_tree_node,
    const std::string& request_id,
    const network::URLLoaderCompletionStatus& status) {}

void OnPrefetchRequestWillBeSent(
    FrameTreeNode& frame_tree_node,
    const std::string& request_id,
    const GURL& initiator_url,
    const network::ResourceRequest& request,
    std::optional<std::pair<const GURL&, const network::mojom::URLResponseHeadDevToolsInfo&>> redirect_info) {}

void OnPrefetchResponseReceived(
    FrameTreeNode* frame_tree_node,
    const std::string& request_id,
    const GURL& url,
    const network::mojom::URLResponseHead& response_head) {}

void DidUpdatePolicyContainerHost(FrameTreeNode* frame_tree_node) {}

void ApplyNetworkOverridesForDownload(
    RenderFrameHostImpl* rfh,
    download::DownloadUrlParameters* params) {}

void BuildAndReportBrowserInitiatedIssue(
    RenderFrameHostImpl* frame,
    mojo::StructPtr<blink::mojom::InspectorIssueInfo> issue_info) {}

bool ShouldWaitForDebuggerInWindowOpen() {
  return false;
}

}  // namespace devtools_instrumentation

} // namespace content
