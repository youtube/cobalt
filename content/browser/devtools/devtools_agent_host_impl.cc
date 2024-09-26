// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_agent_host_impl.h"

#include <map>
#include <vector>

#include "base/containers/cxx20_erase.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "content/browser/devtools/auction_worklet_devtools_agent_host.h"
#include "content/browser/devtools/devtools_http_handler.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/devtools/devtools_pipe_handler.h"
#include "content/browser/devtools/devtools_stream_file.h"
#include "content/browser/devtools/forwarding_agent_host.h"
#include "content/browser/devtools/protocol/page.h"
#include "content/browser/devtools/protocol/security_handler.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/devtools/shared_worker_devtools_agent_host.h"
#include "content/browser/devtools/shared_worker_devtools_manager.h"
#include "content/browser/devtools/web_contents_devtools_agent_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_external_agent_proxy_delegate.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace content {

namespace {
typedef std::map<std::string, DevToolsAgentHostImpl*> DevToolsMap;
DevToolsMap& GetDevtoolsInstances() {
  static base::NoDestructor<DevToolsMap> instance;
  return *instance;
}

base::ObserverList<DevToolsAgentHostObserver>::Unchecked&
GetDevtoolsObservers() {
  static base::NoDestructor<
      base::ObserverList<DevToolsAgentHostObserver>::Unchecked>
      instance;
  return *instance;
}

void SetDevToolsHttpHandler(std::unique_ptr<DevToolsHttpHandler> handler) {
  static base::NoDestructor<std::unique_ptr<DevToolsHttpHandler>> instance;
  *instance = std::move(handler);
}

void SetDevToolsPipeHandler(std::unique_ptr<DevToolsPipeHandler> handler) {
  static base::NoDestructor<std::unique_ptr<DevToolsPipeHandler>> instance;
  *instance = std::move(handler);
}

}  // namespace

const char DevToolsAgentHost::kTypeTab[] = "tab";
const char DevToolsAgentHost::kTypePage[] = "page";
const char DevToolsAgentHost::kTypeFrame[] = "iframe";
const char DevToolsAgentHost::kTypeDedicatedWorker[] = "worker";
const char DevToolsAgentHost::kTypeSharedWorker[] = "shared_worker";
const char DevToolsAgentHost::kTypeServiceWorker[] = "service_worker";
const char DevToolsAgentHost::kTypeBrowser[] = "browser";
const char DevToolsAgentHost::kTypeGuest[] = "webview";
const char DevToolsAgentHost::kTypeOther[] = "other";
const char DevToolsAgentHost::kTypeAuctionWorklet[] = "auction_worklet";
int DevToolsAgentHostImpl::s_force_creation_count_ = 0;

// static
std::string DevToolsAgentHost::GetProtocolVersion() {
  // TODO(dgozman): generate this.
  return "1.3";
}

// static
bool DevToolsAgentHost::IsSupportedProtocolVersion(const std::string& version) {
  // TODO(dgozman): generate this.
  return version == "1.0" || version == "1.1" || version == "1.2" ||
         version == "1.3";
}

// static
DevToolsAgentHost::List DevToolsAgentHost::GetAll() {
  DevToolsAgentHost::List result;
  for (auto& instance : GetDevtoolsInstances()) {
    result.push_back(instance.second);
  }
  return result;
}

// static
DevToolsAgentHost::List DevToolsAgentHost::GetOrCreateAll() {
  List result;
  SharedWorkerDevToolsAgentHost::List shared_list;
  SharedWorkerDevToolsManager::GetInstance()->AddAllAgentHosts(&shared_list);
  for (const auto& host : shared_list)
    result.push_back(host);

  ServiceWorkerDevToolsAgentHost::List service_list;
  ServiceWorkerDevToolsManager::GetInstance()->AddAllAgentHosts(&service_list);
  for (const auto& host : service_list)
    result.push_back(host);

  // TODO(dgozman): we should add dedicated workers here, but clients are not
  // ready.
  RenderFrameDevToolsAgentHost::AddAllAgentHosts(&result);
  WebContentsDevToolsAgentHost::AddAllAgentHosts(&result);

  AuctionWorkletDevToolsAgentHostManager::GetInstance().GetAll(&result);

#if DCHECK_IS_ON()
  for (auto it : result) {
    DevToolsAgentHostImpl* host = static_cast<DevToolsAgentHostImpl*>(it.get());
    DCHECK(GetDevtoolsInstances().find(host->id_) !=
           GetDevtoolsInstances().end());
  }
#endif

  return result;
}

// static
void DevToolsAgentHost::StartRemoteDebuggingServer(
    std::unique_ptr<DevToolsSocketFactory> server_socket_factory,
    const base::FilePath& active_port_output_directory,
    const base::FilePath& debug_frontend_dir) {
  DevToolsManagerDelegate* delegate =
      DevToolsManager::GetInstance()->delegate();
  CHECK(delegate);
  SetDevToolsHttpHandler(std::make_unique<DevToolsHttpHandler>(
      delegate, std::move(server_socket_factory), active_port_output_directory,
      debug_frontend_dir));
}

// static
void DevToolsAgentHost::StartRemoteDebuggingPipeHandler(
    base::OnceClosure on_disconnect) {
  SetDevToolsPipeHandler(
      std::make_unique<DevToolsPipeHandler>(std::move(on_disconnect)));
}

// static
void DevToolsAgentHost::StopRemoteDebuggingServer() {
  SetDevToolsHttpHandler(nullptr);
}

// static
void DevToolsAgentHost::StopRemoteDebuggingPipeHandler() {
  SetDevToolsPipeHandler(nullptr);
}

DevToolsAgentHostImpl::DevToolsAgentHostImpl(const std::string& id)
    : id_(id), renderer_channel_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

DevToolsAgentHostImpl::~DevToolsAgentHostImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  NotifyDestroyed();
}

// static
scoped_refptr<DevToolsAgentHostImpl> DevToolsAgentHostImpl::GetForId(
    const std::string& id) {
  auto it = GetDevtoolsInstances().find(id);
  if (it == GetDevtoolsInstances().end())
    return nullptr;
  return it->second;
}

// static
scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::GetForId(
    const std::string& id) {
  return DevToolsAgentHostImpl::GetForId(id);
}

// static
scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::Forward(
    const std::string& id,
    std::unique_ptr<DevToolsExternalAgentProxyDelegate> delegate) {
  scoped_refptr<DevToolsAgentHost> result = DevToolsAgentHost::GetForId(id);
  if (result)
    return result;
  return new ForwardingAgentHost(id, std::move(delegate));
}

DevToolsSession* DevToolsAgentHostImpl::SessionByClient(
    DevToolsAgentHostClient* client) {
  auto it = session_by_client_.find(client);
  return it == session_by_client_.end() ? nullptr : it->second.get();
}

bool DevToolsAgentHostImpl::AttachInternal(
    std::unique_ptr<DevToolsSession> session_owned) {
  return AttachInternal(std::move(session_owned), true);
}

bool DevToolsAgentHostImpl::AttachInternal(
    std::unique_ptr<DevToolsSession> session_owned,
    bool acquire_wake_lock) {
  scoped_refptr<DevToolsAgentHostImpl> protect(this);
  DevToolsSession* session = session_owned.get();
  session->SetAgentHost(this);
  if (!AttachSession(session, acquire_wake_lock))
    return false;
  renderer_channel_.AttachSession(session);
  sessions_.push_back(session);
  DCHECK(session_by_client_.find(session->GetClient()) ==
         session_by_client_.end());
  session_by_client_.emplace(session->GetClient(), std::move(session_owned));
  if (sessions_.size() == 1)
    NotifyAttached();
  DevToolsManager* manager = DevToolsManager::GetInstance();
  if (manager->delegate())
    manager->delegate()->ClientAttached(session);
  return true;
}

bool DevToolsAgentHostImpl::AttachClient(DevToolsAgentHostClient* client) {
  if (SessionByClient(client))
    return false;
  return AttachInternal(
      std::make_unique<DevToolsSession>(client, GetSessionMode()),
      /*acquire_wake_lock=*/true);
}

bool DevToolsAgentHostImpl::AttachClientWithoutWakeLock(
    content::DevToolsAgentHostClient* client) {
  if (SessionByClient(client))
    return false;
  return AttachInternal(
      std::make_unique<DevToolsSession>(client, GetSessionMode()),
      /*acquire_wake_lock=*/false);
}

bool DevToolsAgentHostImpl::DetachClient(DevToolsAgentHostClient* client) {
  DevToolsSession* session = SessionByClient(client);
  if (!session)
    return false;
  scoped_refptr<DevToolsAgentHostImpl> protect(this);
  DetachInternal(session);
  return true;
}

void DevToolsAgentHostImpl::DispatchProtocolMessage(
    DevToolsAgentHostClient* client,
    base::span<const uint8_t> message) {
  DevToolsSession* session = SessionByClient(client);
  if (session)
    session->DispatchProtocolMessage(message);
}

void DevToolsAgentHostImpl::DetachInternal(DevToolsSession* session) {
  std::unique_ptr<DevToolsSession> session_owned =
      std::move(session_by_client_[session->GetClient()]);
  DCHECK_EQ(session, session_owned.get());
  // Make sure we dispose session prior to reporting it to the host.
  session->Dispose();
  base::Erase(sessions_, session);
  session_by_client_.erase(session->GetClient());
  DetachSession(session);
  DevToolsManager* manager = DevToolsManager::GetInstance();
  if (manager->delegate())
    manager->delegate()->ClientDetached(session);
  if (sessions_.empty()) {
    io_context_.DiscardAllStreams();
    NotifyDetached();
  }
}

bool DevToolsAgentHostImpl::IsAttached() {
  return !sessions_.empty();
}

void DevToolsAgentHostImpl::InspectElement(RenderFrameHost* frame_host,
                                           int x,
                                           int y) {}

std::string DevToolsAgentHostImpl::GetId() {
  return id_;
}

std::string DevToolsAgentHostImpl::CreateIOStreamFromData(
    scoped_refptr<base::RefCountedMemory> data) {
  scoped_refptr<DevToolsStreamFile> stream =
      DevToolsStreamFile::Create(GetIOContext(), true /* binary */);
  std::string text(reinterpret_cast<const char*>(data->front()), data->size());
  stream->Append(std::make_unique<std::string>(text));
  return stream->handle();
}

std::string DevToolsAgentHostImpl::GetParentId() {
  return std::string();
}

std::string DevToolsAgentHostImpl::GetOpenerId() {
  return std::string();
}

std::string DevToolsAgentHostImpl::GetOpenerFrameId() {
  return std::string();
}

bool DevToolsAgentHostImpl::CanAccessOpener() {
  return false;
}

std::string DevToolsAgentHostImpl::GetDescription() {
  return std::string();
}

GURL DevToolsAgentHostImpl::GetFaviconURL() {
  return GURL();
}

std::string DevToolsAgentHostImpl::GetFrontendURL() {
  return std::string();
}

base::TimeTicks DevToolsAgentHostImpl::GetLastActivityTime() {
  return base::TimeTicks();
}

BrowserContext* DevToolsAgentHostImpl::GetBrowserContext() {
  return nullptr;
}

WebContents* DevToolsAgentHostImpl::GetWebContents() {
  return nullptr;
}

void DevToolsAgentHostImpl::DisconnectWebContents() {
}

void DevToolsAgentHostImpl::ConnectWebContents(WebContents* wc) {
}

DevToolsSession::Mode DevToolsAgentHostImpl::GetSessionMode() {
  return DevToolsSession::Mode::kDoesNotSupportTabTarget;
}

bool DevToolsAgentHostImpl::Inspect() {
  DevToolsManager* manager = DevToolsManager::GetInstance();
  if (manager->delegate()) {
    manager->delegate()->Inspect(this);
    return true;
  }
  return false;
}

void DevToolsAgentHostImpl::ForceDetachAllSessions() {
  std::ignore = ForceDetachAllSessionsImpl();
}

scoped_refptr<DevToolsAgentHost>
DevToolsAgentHostImpl::ForceDetachAllSessionsImpl() {
  scoped_refptr<DevToolsAgentHost> retain_this(this);
  while (!sessions_.empty()) {
    DevToolsAgentHostClient* client = (*sessions_.begin())->GetClient();
    DetachClient(client);
    client->AgentHostClosed(this);
  }
  return retain_this;
}

void DevToolsAgentHostImpl::ForceDetachRestrictedSessions(
    const std::vector<DevToolsSession*>& restricted_sessions) {
  scoped_refptr<DevToolsAgentHostImpl> protect(this);

  for (DevToolsSession* session : restricted_sessions) {
    DevToolsAgentHostClient* client = session->GetClient();
    DetachClient(client);
    client->AgentHostClosed(this);
  }
}

bool DevToolsAgentHostImpl::AttachSession(DevToolsSession* session,
                                          bool acquire_wake_lock) {
  return false;
}

void DevToolsAgentHostImpl::DetachSession(DevToolsSession* session) {}

void DevToolsAgentHostImpl::UpdateRendererChannel(bool force) {}

// static
void DevToolsAgentHost::DetachAllClients() {
  // Make a copy, since detaching may lead to agent destruction, which
  // removes it from the instances.
  std::vector<scoped_refptr<DevToolsAgentHostImpl>> copy;
  for (auto& instance : GetDevtoolsInstances()) {
    copy.push_back(instance.second);
  }
  for (auto& instance : copy) {
    instance->ForceDetachAllSessions();
  }
}

// static
void DevToolsAgentHost::AddObserver(DevToolsAgentHostObserver* observer) {
  if (observer->ShouldForceDevToolsAgentHostCreation()) {
    if (!DevToolsAgentHostImpl::s_force_creation_count_) {
      // Force all agent hosts when first observer is added.
      DevToolsAgentHost::GetOrCreateAll();
    }
    DevToolsAgentHostImpl::s_force_creation_count_++;
  }

  GetDevtoolsObservers().AddObserver(observer);
  for (const auto& id_host : GetDevtoolsInstances())
    observer->DevToolsAgentHostCreated(id_host.second);
}

// static
void DevToolsAgentHost::RemoveObserver(DevToolsAgentHostObserver* observer) {
  if (observer->ShouldForceDevToolsAgentHostCreation())
    DevToolsAgentHostImpl::s_force_creation_count_--;
  GetDevtoolsObservers().RemoveObserver(observer);
}

// static
bool DevToolsAgentHostImpl::ShouldForceCreation() {
  return !!s_force_creation_count_;
}

std::string DevToolsAgentHostImpl::GetSubtype() {
  return "";
}

void DevToolsAgentHostImpl::NotifyCreated() {
  DCHECK(GetDevtoolsInstances().find(id_) == GetDevtoolsInstances().end());
  GetDevtoolsInstances()[id_] = this;
  for (auto& observer : GetDevtoolsObservers())
    observer.DevToolsAgentHostCreated(this);
}

void DevToolsAgentHostImpl::NotifyNavigated() {
  for (auto& observer : GetDevtoolsObservers())
    observer.DevToolsAgentHostNavigated(this);
}

void DevToolsAgentHostImpl::NotifyAttached() {
  for (auto& observer : GetDevtoolsObservers())
    observer.DevToolsAgentHostAttached(this);
}

void DevToolsAgentHostImpl::NotifyDetached() {
  for (auto& observer : GetDevtoolsObservers())
    observer.DevToolsAgentHostDetached(this);
}

void DevToolsAgentHostImpl::NotifyCrashed(base::TerminationStatus status) {
  for (auto& observer : GetDevtoolsObservers())
    observer.DevToolsAgentHostCrashed(this, status);
}

void DevToolsAgentHostImpl::NotifyDestroyed() {
  DCHECK(GetDevtoolsInstances().find(id_) != GetDevtoolsInstances().end());
  for (auto& observer : GetDevtoolsObservers())
    observer.DevToolsAgentHostDestroyed(this);
  GetDevtoolsInstances().erase(id_);
}

DevToolsAgentHostImpl::NetworkLoaderFactoryParamsAndInfo::
    NetworkLoaderFactoryParamsAndInfo() = default;
DevToolsAgentHostImpl::NetworkLoaderFactoryParamsAndInfo::
    NetworkLoaderFactoryParamsAndInfo(
        url::Origin origin,
        net::SiteForCookies site_for_cookies,
        network::mojom::URLLoaderFactoryParamsPtr factory_params)
    : origin(std::move(origin)),
      site_for_cookies(std::move(site_for_cookies)),
      factory_params(std::move(factory_params)) {}
DevToolsAgentHostImpl::NetworkLoaderFactoryParamsAndInfo::
    NetworkLoaderFactoryParamsAndInfo(
        DevToolsAgentHostImpl::NetworkLoaderFactoryParamsAndInfo&&) = default;
DevToolsAgentHostImpl::NetworkLoaderFactoryParamsAndInfo::
    ~NetworkLoaderFactoryParamsAndInfo() = default;

DevToolsAgentHostImpl::NetworkLoaderFactoryParamsAndInfo
DevToolsAgentHostImpl::CreateNetworkFactoryParamsForDevTools() {
  return {};
}

RenderProcessHost* DevToolsAgentHostImpl::GetProcessHost() {
  return nullptr;
}

absl::optional<network::CrossOriginEmbedderPolicy>
DevToolsAgentHostImpl::cross_origin_embedder_policy(const std::string& id) {
  return absl::nullopt;
}

absl::optional<network::CrossOriginOpenerPolicy>
DevToolsAgentHostImpl::cross_origin_opener_policy(const std::string& id) {
  return absl::nullopt;
}

protocol::TargetAutoAttacher* DevToolsAgentHostImpl::auto_attacher() {
  return nullptr;
}

}  // namespace content
