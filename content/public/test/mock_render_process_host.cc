// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_render_process_host.h"

#include <algorithm>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/process/process_handle.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/child_process_host_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/file_system_access/file_system_access_error.h"
#include "content/browser/process_lock.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/renderer.mojom.h"
#include "content/public/browser/android/child_process_importance.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host_priority_client.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/test/fake_network_url_loader_factory.h"
#include "media/media_buildflags.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

namespace {

MockRenderProcessHost::CreateNetworkFactoryCallback&
GetNetworkFactoryCallback() {
  static base::NoDestructor<MockRenderProcessHost::CreateNetworkFactoryCallback>
      callback;
  return *callback;
}

StoragePartitionConfig GetOrCreateStoragePartitionConfig(
    BrowserContext* browser_context,
    SiteInstance* site_instance) {
  if (site_instance) {
    SiteInstanceImpl* site_instance_impl =
        static_cast<SiteInstanceImpl*>(site_instance);
    return site_instance_impl->GetSiteInfo().storage_partition_config();
  }
  return StoragePartitionConfig::CreateDefault(browser_context);
}

}  // namespace

MockRenderProcessHost::MockRenderProcessHost(BrowserContext* browser_context,
                                             bool is_for_guests_only)
    : MockRenderProcessHost(
          browser_context,
          StoragePartitionConfig::CreateDefault(browser_context),
          is_for_guests_only) {}

MockRenderProcessHost::MockRenderProcessHost(
    BrowserContext* browser_context,
    const StoragePartitionConfig& storage_partition_config,
    bool is_for_guests_only)
    : bad_msg_count_(0),
      id_(ChildProcessHostImpl::GenerateChildProcessUniqueId()),
      has_connection_(false),
      browser_context_(browser_context),
      storage_partition_config_(storage_partition_config),
      prev_routing_id_(0),
      shutdown_requested_(false),
      fast_shutdown_started_(false),
      deletion_callback_called_(false),
      is_for_guests_only_(is_for_guests_only),
      is_process_backgrounded_(false),
      is_unused_(true),
      keep_alive_ref_count_(0),
      worker_ref_count_(0),
      pending_reuse_ref_count_(0),
      foreground_service_worker_count_(0),
      url_loader_factory_(std::make_unique<FakeNetworkURLLoaderFactory>()) {
  // Child process security operations can't be unit tested unless we add
  // ourselves as an existing child process.
  ChildProcessSecurityPolicyImpl::GetInstance()->Add(GetID(), browser_context);

  RenderProcessHostImpl::RegisterHost(GetID(), this);
}

MockRenderProcessHost::~MockRenderProcessHost() {
  ChildProcessSecurityPolicyImpl::GetInstance()->Remove(GetID());
  // In unit tests, Cleanup() might not have been called.
  if (!deletion_callback_called_) {
    for (auto& observer : observers_)
      observer.RenderProcessHostDestroyed(this);
    RenderProcessHostImpl::UnregisterHost(GetID());
  }
}

void MockRenderProcessHost::SimulateCrash() {
  SimulateRenderProcessExit(base::TERMINATION_STATUS_PROCESS_CRASHED, 0);
}

void MockRenderProcessHost::SimulateRenderProcessExit(
    base::TerminationStatus status,
    int exit_code) {
  has_connection_ = false;
  ChildProcessTerminationInfo termination_info;
  termination_info.status = status;
  termination_info.exit_code = exit_code;
#if BUILDFLAG(IS_ANDROID)
  termination_info.renderer_has_visible_clients = VisibleClientCount() > 0;
#endif
  NotificationService::current()->Notify(
      NOTIFICATION_RENDERER_PROCESS_CLOSED, Source<RenderProcessHost>(this),
      Details<ChildProcessTerminationInfo>(&termination_info));

  for (auto& observer : observers_)
    observer.RenderProcessExited(this, termination_info);
}

void MockRenderProcessHost::SimulateReady() {
  is_ready_ = true;
  for (auto& observer : observers_)
    observer.RenderProcessReady(this);
}

// static
void MockRenderProcessHost::SetNetworkFactory(
    const CreateNetworkFactoryCallback& create_network_factory_callback) {
  GetNetworkFactoryCallback() = create_network_factory_callback;
}

bool MockRenderProcessHost::Init() {
  has_connection_ = true;
  return true;
}

void MockRenderProcessHost::EnableSendQueue() {}

int MockRenderProcessHost::GetNextRoutingID() {
  return ++prev_routing_id_;
}

void MockRenderProcessHost::AddRoute(int32_t routing_id,
                                     IPC::Listener* listener) {
  listeners_.AddWithID(listener, routing_id);
}

void MockRenderProcessHost::RemoveRoute(int32_t routing_id) {
  DCHECK(listeners_.Lookup(routing_id) != nullptr);
  listeners_.Remove(routing_id);
  Cleanup();
}

void MockRenderProcessHost::AddObserver(RenderProcessHostObserver* observer) {
  observers_.AddObserver(observer);
}

void MockRenderProcessHost::RemoveObserver(
    RenderProcessHostObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MockRenderProcessHost::ShutdownForBadMessage(
    CrashReportMode crash_report_mode) {
  ++bad_msg_count_;
  shutdown_requested_ = true;
}

void MockRenderProcessHost::UpdateClientPriority(
    RenderProcessHostPriorityClient* client) {}

int MockRenderProcessHost::VisibleClientCount() {
  int count = 0;
  for (auto* client : priority_clients_) {
    const RenderProcessHostPriorityClient::Priority priority =
        client->GetPriority();
    if (!priority.is_hidden) {
      count++;
    }
  }
  return count;
}

unsigned int MockRenderProcessHost::GetFrameDepth() {
  NOTIMPLEMENTED();
  return 0u;
}

bool MockRenderProcessHost::GetIntersectsViewport() {
  NOTIMPLEMENTED();
  return true;
}

bool MockRenderProcessHost::IsForGuestsOnly() {
  return is_for_guests_only_;
}

bool MockRenderProcessHost::IsJitDisabled() {
  return false;
}

bool MockRenderProcessHost::IsPdf() {
  return false;
}

void MockRenderProcessHost::OnMediaStreamAdded() {}

void MockRenderProcessHost::OnMediaStreamRemoved() {}

void MockRenderProcessHost::OnForegroundServiceWorkerAdded() {
  foreground_service_worker_count_ += 1;
}

void MockRenderProcessHost::OnForegroundServiceWorkerRemoved() {
  DCHECK_GT(foreground_service_worker_count_, 0);
  foreground_service_worker_count_ -= 1;
}

StoragePartition* MockRenderProcessHost::GetStoragePartition() {
  return browser_context_->GetStoragePartition(storage_partition_config_);
}

void MockRenderProcessHost::AddWord(const std::u16string& word) {}

bool MockRenderProcessHost::Shutdown(int exit_code) {
  shutdown_requested_ = true;
  return true;
}

bool MockRenderProcessHost::ShutdownRequested() {
  return shutdown_requested_;
}

bool MockRenderProcessHost::FastShutdownIfPossible(size_t page_count,
                                                   bool skip_unload_handlers) {
  if (GetActiveViewCount() != page_count)
    return false;
  // We aren't actually going to do anything, but set |fast_shutdown_started_|
  // to true so that tests know we've been called.
  fast_shutdown_started_ = true;
  return true;
}

bool MockRenderProcessHost::FastShutdownStarted() {
  return fast_shutdown_started_;
}

const base::Process& MockRenderProcessHost::GetProcess() {
  // Return the current-process handle for the IPC::GetPlatformFileForTransit
  // function.
  if (process.IsValid())
    return process;

  static const base::Process current_process(base::Process::Current());
  return current_process;
}

bool MockRenderProcessHost::IsReady() {
  return is_ready_;
}

bool MockRenderProcessHost::Send(IPC::Message* msg) {
  // Save the message in the sink.
  sink_.OnMessageReceived(*msg);
  delete msg;
  return true;
}

int MockRenderProcessHost::GetID() const {
  return id_;
}

base::SafeRef<RenderProcessHost> MockRenderProcessHost::GetSafeRef() const {
  return weak_ptr_factory_.GetSafeRef();
}

bool MockRenderProcessHost::IsInitializedAndNotDead() {
  return has_connection_;
}

void MockRenderProcessHost::SetBlocked(bool blocked) {}

bool MockRenderProcessHost::IsBlocked() {
  return false;
}

base::CallbackListSubscription
MockRenderProcessHost::RegisterBlockStateChangedCallback(
    const BlockStateChangedCallback& cb) {
  return {};
}

void MockRenderProcessHost::Cleanup() {
  if (listeners_.IsEmpty() && !deletion_callback_called_) {
    if (IsInitializedAndNotDead()) {
      ChildProcessTerminationInfo termination_info;
      termination_info.status = base::TERMINATION_STATUS_NORMAL_TERMINATION;
      termination_info.exit_code = 0;
#if BUILDFLAG(IS_ANDROID)
      termination_info.renderer_has_visible_clients = VisibleClientCount() > 0;
#endif
      for (auto& observer : observers_)
        observer.RenderProcessExited(this, termination_info);
    }

    for (auto& observer : observers_)
      observer.RenderProcessHostDestroyed(this);
    RenderProcessHostImpl::UnregisterHost(GetID());
    has_connection_ = false;
    deletion_callback_called_ = true;
  }
}

void MockRenderProcessHost::AddPendingView() {}

void MockRenderProcessHost::RemovePendingView() {}

void MockRenderProcessHost::AddPriorityClient(
    RenderProcessHostPriorityClient* priority_client) {
  priority_clients_.insert(priority_client);
}

void MockRenderProcessHost::RemovePriorityClient(
    RenderProcessHostPriorityClient* priority_client) {
  priority_clients_.erase(priority_client);
}

void MockRenderProcessHost::SetPriorityOverride(bool foreground) {}

bool MockRenderProcessHost::HasPriorityOverride() {
  return false;
}

void MockRenderProcessHost::ClearPriorityOverride() {}

#if BUILDFLAG(IS_ANDROID)
ChildProcessImportance MockRenderProcessHost::GetEffectiveImportance() {
  NOTIMPLEMENTED();
  return ChildProcessImportance::NORMAL;
}

base::android::ChildBindingState
MockRenderProcessHost::GetEffectiveChildBindingState() {
  NOTIMPLEMENTED();
  return base::android::ChildBindingState::UNBOUND;
}

void MockRenderProcessHost::DumpProcessStack() {}
#endif

void MockRenderProcessHost::SetSuddenTerminationAllowed(bool allowed) {}

bool MockRenderProcessHost::SuddenTerminationAllowed() {
  return true;
}

BrowserContext* MockRenderProcessHost::GetBrowserContext() {
  return browser_context_;
}

bool MockRenderProcessHost::InSameStoragePartition(
    StoragePartition* partition) {
  return GetStoragePartition() == partition;
}

IPC::ChannelProxy* MockRenderProcessHost::GetChannel() {
  return nullptr;
}

void MockRenderProcessHost::AddFilter(BrowserMessageFilter* filter) {}

base::TimeDelta MockRenderProcessHost::GetChildProcessIdleTime() {
  return base::Milliseconds(0);
}

void MockRenderProcessHost::BindReceiver(
    mojo::GenericPendingReceiver receiver) {
  auto it = binder_overrides_.find(*receiver.interface_name());
  if (it != binder_overrides_.end())
    it->second.Run(receiver.PassPipe());
}

std::unique_ptr<base::PersistentMemoryAllocator>
MockRenderProcessHost::TakeMetricsAllocator() {
  return nullptr;
}

const base::TimeTicks& MockRenderProcessHost::GetLastInitTime() {
  static base::TimeTicks dummy_time = base::TimeTicks::Now();
  return dummy_time;
}

bool MockRenderProcessHost::IsProcessBackgrounded() {
  return is_process_backgrounded_;
}

size_t MockRenderProcessHost::GetKeepAliveRefCount() const {
  return keep_alive_ref_count_;
}

void MockRenderProcessHost::IncrementKeepAliveRefCount(uint64_t handle_id) {
  ++keep_alive_ref_count_;
}

void MockRenderProcessHost::DecrementKeepAliveRefCount(uint64_t handle_id) {
  --keep_alive_ref_count_;
}

std::string MockRenderProcessHost::GetKeepAliveDurations() const {
  return std::string("MockRenderProcessHost: durations not tracked.");
}

size_t MockRenderProcessHost::GetShutdownDelayRefCount() const {
  return 0;
}

int MockRenderProcessHost::GetRenderFrameHostCount() const {
  return 0;
}

void MockRenderProcessHost::RegisterRenderFrameHost(
    const GlobalRenderFrameHostId& render_frame_host_id) {}

void MockRenderProcessHost::UnregisterRenderFrameHost(
    const GlobalRenderFrameHostId& render_frame_host_id) {}

void MockRenderProcessHost::ForEachRenderFrameHost(
    base::RepeatingCallback<void(RenderFrameHost*)> on_render_frame_host) {}

void MockRenderProcessHost::IncrementWorkerRefCount() {
  ++worker_ref_count_;
}

void MockRenderProcessHost::DecrementWorkerRefCount() {
  DCHECK_GT(worker_ref_count_, 0);
  --worker_ref_count_;
}

void MockRenderProcessHost::IncrementPendingReuseRefCount() {
  ++pending_reuse_ref_count_;
}

void MockRenderProcessHost::DecrementPendingReuseRefCount() {
  DCHECK_GT(pending_reuse_ref_count_, 0);
  --pending_reuse_ref_count_;
}

size_t MockRenderProcessHost::GetWorkerRefCount() const {
  return worker_ref_count_;
}

void MockRenderProcessHost::DisableRefCounts() {
  keep_alive_ref_count_ = 0;
  worker_ref_count_ = 0;
  pending_reuse_ref_count_ = 0;

  // RenderProcessHost::DisableRefCounts() virtual method gets called as part of
  // BrowserContext::NotifyWillBeDestroyed(...).  Normally
  // MockRenderProcessHost::DisableRefCounts() doesn't call Cleanup, because the
  // MockRenderProcessHost might be owned by a test.  However, when the
  // MockRenderProcessHost is the spare RenderProcessHost, we know that it is
  // owned by the SpareRenderProcessHostManager and we need to delete the spare
  // to avoid reports/DCHECKs about memory leaks.
  if (this == RenderProcessHostImpl::GetSpareRenderProcessHostForTesting())
    Cleanup();
}

bool MockRenderProcessHost::AreRefCountsDisabled() {
  return false;
}

mojom::Renderer* MockRenderProcessHost::GetRendererInterface() {
  if (!renderer_interface_) {
    renderer_interface_ =
        std::make_unique<mojo::AssociatedRemote<mojom::Renderer>>();
    std::ignore =
        renderer_interface_->BindNewEndpointAndPassDedicatedReceiver();
  }
  return renderer_interface_->get();
}

void MockRenderProcessHost::CreateURLLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    network::mojom::URLLoaderFactoryParamsPtr params) {
  if (GetNetworkFactoryCallback().is_null()) {
    url_loader_factory_->Clone(std::move(receiver));
    return;
  }

  mojo::Remote<network::mojom::URLLoaderFactory> original_factory;
  url_loader_factory_->Clone(original_factory.BindNewPipeAndPassReceiver());
  GetNetworkFactoryCallback().Run(std::move(receiver), GetID(),
                                  original_factory.Unbind());
}

bool MockRenderProcessHost::MayReuseHost() {
  return true;
}

bool MockRenderProcessHost::IsUnused() {
  return is_unused_;
}

void MockRenderProcessHost::SetIsUsed() {
  is_unused_ = false;
}

bool MockRenderProcessHost::HostHasNotBeenUsed() {
  return IsUnused() && listeners_.IsEmpty() && GetKeepAliveRefCount() == 0;
}

void MockRenderProcessHost::SetProcessLock(
    const IsolationContext& isolation_context,
    const ProcessLock& process_lock) {
  ChildProcessSecurityPolicyImpl::GetInstance()->LockProcess(
      isolation_context, GetID(), !IsUnused(), process_lock);
  if (process_lock.IsASiteOrOrigin())
    is_renderer_locked_to_site_ = true;
}

ProcessLock MockRenderProcessHost::GetProcessLock() const {
  return ChildProcessSecurityPolicyImpl::GetInstance()->GetProcessLock(GetID());
}

bool MockRenderProcessHost::IsProcessLockedToSiteForTesting() {
  return GetProcessLock().is_locked_to_site();
}

void MockRenderProcessHost::BindCacheStorage(
    const network::CrossOriginEmbedderPolicy&,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>,
    const storage::BucketLocator& bucket_locator,
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
  cache_storage_receiver_ = std::move(receiver);
}

void MockRenderProcessHost::BindIndexedDB(
    const blink::StorageKey& storage_key,
    const GlobalRenderFrameHostId& rfh_id,
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) {
  idb_factory_receiver_ = std::move(receiver);
}

void MockRenderProcessHost::GetSandboxedFileSystemForBucket(
    const storage::BucketLocator& bucket,
    blink::mojom::FileSystemAccessManager::GetSandboxedFileSystemCallback
        callback) {
  std::move(callback).Run(file_system_access_error::Ok(), {});
}

std::string
MockRenderProcessHost::GetInfoForBrowserContextDestructionCrashReporting() {
  return std::string();
}

void MockRenderProcessHost::WriteIntoTrace(
    perfetto::TracedProto<TraceProto> proto) const {
  proto->set_id(GetID());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void MockRenderProcessHost::ReinitializeLogging(
    uint32_t logging_dest,
    base::ScopedFD log_file_descriptor) {
  NOTIMPLEMENTED();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void MockRenderProcessHost::FilterURL(bool empty_allowed, GURL* url) {
  RenderProcessHostImpl::FilterURL(this, empty_allowed, url);
}

void MockRenderProcessHost::EnableAudioDebugRecordings(
    const base::FilePath& file) {}

void MockRenderProcessHost::DisableAudioDebugRecordings() {}

RenderProcessHost::WebRtcStopRtpDumpCallback
MockRenderProcessHost::StartRtpDump(bool incoming,
                                    bool outgoing,
                                    WebRtcRtpPacketCallback packet_callback) {
  return base::NullCallback();
}

bool MockRenderProcessHost::OnMessageReceived(const IPC::Message& msg) {
  IPC::Listener* listener = listeners_.Lookup(msg.routing_id());
  if (listener)
    return listener->OnMessageReceived(msg);
  return false;
}

void MockRenderProcessHost::OnChannelConnected(int32_t peer_pid) {}

void MockRenderProcessHost::OverrideBinderForTesting(
    const std::string& interface_name,
    const InterfaceBinder& binder) {
  binder_overrides_[interface_name] = binder;
}

void MockRenderProcessHost::OverrideRendererInterfaceForTesting(
    std::unique_ptr<mojo::AssociatedRemote<mojom::Renderer>>
        renderer_interface) {
  renderer_interface_ = std::move(renderer_interface);
}

MockRenderProcessHostFactory::MockRenderProcessHostFactory() = default;

MockRenderProcessHostFactory::~MockRenderProcessHostFactory() = default;

RenderProcessHost* MockRenderProcessHostFactory::CreateRenderProcessHost(
    BrowserContext* browser_context,
    SiteInstance* site_instance) {
  const bool is_for_guests_only = site_instance && site_instance->IsGuest();
  StoragePartitionConfig storage_partition_config =
      GetOrCreateStoragePartitionConfig(browser_context, site_instance);
  std::unique_ptr<MockRenderProcessHost> host =
      std::make_unique<MockRenderProcessHost>(
          browser_context, storage_partition_config, is_for_guests_only);
  processes_.push_back(std::move(host));
  return processes_.back().get();
}

void MockRenderProcessHostFactory::Remove(MockRenderProcessHost* host) const {
  for (auto it = processes_.begin(); it != processes_.end(); ++it) {
    if (it->get() == host) {
      processes_.erase(it);
      break;
    }
  }
}

}  // namespace content
