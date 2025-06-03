// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_frame_host_impl.h"

#include <cstdint>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/kill.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/state_transitions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/syslog_logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/optional_trace_event.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/viz/common/features.h"
#include "content/browser/about_url_loader_factory.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/render_accessibility_host.h"
#include "content/browser/bad_message.h"
#include "content/browser/blob_storage/file_backed_blob_factory_frame_impl.h"
#include "content/browser/bluetooth/web_bluetooth_service_impl.h"
#include "content/browser/broadcast_channel/broadcast_channel_provider.h"
#include "content/browser/broadcast_channel/broadcast_channel_service.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/can_commit_status.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/closewatcher/close_listener_host.h"
#include "content/browser/code_cache/generated_code_cache_context.h"
#include "content/browser/cookie_deprecation_label/cookie_deprecation_label_manager_impl.h"
#include "content/browser/data_url_loader_factory.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/download/data_url_blob_reader.h"
#include "content/browser/feature_observer.h"
#include "content/browser/fenced_frame/fenced_frame.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "content/browser/file_system/file_system_manager_impl.h"
#include "content/browser/file_system/file_system_url_loader_factory.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/font_access/font_access_manager.h"
#include "content/browser/generic_sensor/frame_sensor_provider_proxy.h"
#include "content/browser/geolocation/geolocation_service_impl.h"
#include "content/browser/idle/idle_manager_impl.h"
#include "content/browser/installedapp/installed_app_provider_impl.h"
#include "content/browser/interest_group/ad_auction_document_data.h"
#include "content/browser/loader/file_url_loader_factory.h"
#include "content/browser/loader/keep_alive_url_loader_service.h"
#include "content/browser/loader/navigation_early_hints_manager.h"
#include "content/browser/loader/resource_cache_manager.h"
#include "content/browser/loader/subresource_proxying_url_loader_service.h"
#include "content/browser/log_console_message.h"
#include "content/browser/media/media_devices_util.h"
#include "content/browser/media/media_interface_proxy.h"
#include "content/browser/media/webaudio/audio_context_manager_impl.h"
#include "content/browser/navigation_or_document_handle.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/browser/network/cross_origin_embedder_policy_reporter.h"
#include "content/browser/network/cross_origin_opener_policy_reporter.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/permissions/permission_service_context.h"
#include "content/browser/portal/portal.h"
#include "content/browser/preloading/preloading_decider.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/preloading/prerender/prerender_metrics.h"
#include "content/browser/presentation/presentation_service_impl.h"
#include "content/browser/process_lock.h"
#include "content/browser/push_messaging/push_messaging_manager.h"
#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/browser/renderer_host/back_forward_cache_disable.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/renderer_host/code_cache_host_impl.h"
#include "content/browser/renderer_host/cookie_utils.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/input/input_injector_impl.h"
#include "content/browser/renderer_host/input/input_router.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"
#include "content/browser/renderer_host/ipc_utils.h"
#include "content/browser/renderer_host/media/peer_connection_tracker_host.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/page_delegate.h"
#include "content/browser/renderer_host/page_factory.h"
#include "content/browser/renderer_host/pending_beacon_host.h"
#include "content/browser/renderer_host/pending_beacon_service.h"
#include "content/browser/renderer_host/private_network_access_util.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_owner.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_factory.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/renderer_host/view_transition_opt_in_state.h"
#include "content/browser/runtime_feature_state/runtime_feature_state_document_data.h"
#include "content/browser/scoped_active_url.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_object_host.h"
#include "content/browser/site_info.h"
#include "content/browser/sms/webotp_service.h"
#include "content/browser/speech/speech_synthesis_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_params_helper.h"
#include "content/browser/usb/web_usb_service_impl.h"
#include "content/browser/web_exposed_isolation_info.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/browser/webauth/authenticator_impl.h"
#include "content/browser/webauth/webauth_request_security_checker.h"
#include "content/browser/webid/federated_auth_request_impl.h"
#include "content/browser/webid/flags.h"
#include "content/browser/websockets/websocket_connector_impl.h"
#include "content/browser/webtransport/web_transport_connector_impl.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/browser/worker_host/dedicated_worker_host.h"
#include "content/browser/worker_host/dedicated_worker_host_factory_impl.h"
#include "content/browser/worker_host/dedicated_worker_hosts_for_document.h"
#include "content/common/associated_interfaces.mojom.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/debug_utils.h"
#include "content/common/features.h"
#include "content/common/frame.mojom.h"
#include "content/common/frame_messages.mojom.h"
#include "content/common/navigation_client.mojom.h"
#include "content/common/navigation_params_utils.h"
#include "content/public/browser/active_url_message_filter.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "content/public/browser/document_ref.h"
#include "content/public/browser/document_service_internal.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/peak_gpu_memory_tracker.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/sms_fetcher.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_ui_url_loader_factory.h"
#include "content/public/common/alternative_error_page_override_info.mojom.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/extra_mojo_js_features.mojom.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/page_visibility_state.h"
#include "content/public/common/referrer.h"
#include "content/public/common/referrer_type_converters.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "media/base/media_switches.h"
#include "media/learning/common/value.h"
#include "media/media_buildflags.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/schemeful_site.h"
#include "net/net_buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "render_frame_host_impl.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/attribution_reporting_runtime_features.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/network_service_buildflags.h"
#include "services/network/public/cpp/not_implemented_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "storage/browser/blob/blob_url_store_impl.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/frame/fenced_frame_sandbox_flags.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/common/loader/inter_process_time_ticks_converter.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/common/navigation/navigation_params_mojom_traits.h"
#include "third_party/blink/public/common/page/browsing_context_group_info.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/common/permissions_policy/document_policy.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_document_created.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/runtime_feature_state/runtime_feature_state_context.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/back_forward_cache_not_restored_reasons.mojom.h"
#include "third_party/blink/public/mojom/broadcastchannel/broadcast_channel.mojom.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "third_party/blink/public/mojom/frame/media_player_action.mojom.h"
#include "third_party/blink/public/mojom/frame/text_autosizer_page_info.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "third_party/blink/public/mojom/loader/transferrable_url_loader.mojom.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"
#include "third_party/blink/public/mojom/opengraph/metadata.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/storage_key/ancestor_chain_bit.mojom.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "ui/accessibility/ax_action_handler_registry.h"
#include "ui/accessibility/ax_common.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/android/content_url_loader_factory.h"
#include "content/browser/android/java_interfaces_impl.h"
#include "content/browser/renderer_host/render_frame_host_android.h"
#include "content/public/browser/android/java_interfaces.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#else
#include "content/browser/hid/hid_service.h"
#include "content/browser/host_zoom_map_impl.h"
#include "content/browser/serial/serial_service.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "content/browser/smart_card/smart_card_service.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "content/browser/renderer_host/popup_menu_helper_mac.h"
#endif

#if BUILDFLAG(ENABLE_PPAPI)
#include "content/browser/renderer_host/render_frame_host_impl_ppapi_support.h"
#endif

#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#endif

namespace features {
BASE_FEATURE(kDisableFrameNameUpdateOnNonCurrentRenderFrameHost,
             "DisableFrameNameUpdateOnNonCurrentRenderFrameHost",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Evict when accessibility events occur while in back/forward cache.
// Disabling on all platforms since https://crbug.com/1341507 has been addressed
// and no significant crashes are happening with experiments.
BASE_FEATURE(kEvictOnAXEvents,
             "EvictOnAXEvents",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

namespace content {

#if defined(AX_FAIL_FAST_BUILD)
// Enable fast fails on clusterfuzz and other builds used to debug Chrome,
// which should help narrow down illegal ax trees more quickly.
// static
int RenderFrameHostImpl::max_accessibility_resets_ = 0;
#else
int RenderFrameHostImpl::max_accessibility_resets_ = 4;
#endif  // AX_FAIL_FAST_BUILD

class RenderFrameHostOrProxy {
 public:
  RenderFrameHostOrProxy(RenderFrameHostImpl* frame,
                         RenderFrameProxyHost* proxy) {
    DCHECK(!frame || !proxy)
        << "Both frame and proxy can't be non-null at the same time";
    if (proxy) {
      frame_or_proxy_ = proxy;
      return;
    }
    if (frame) {
      frame_or_proxy_ = frame;
      return;
    }
  }

  explicit operator bool() { return frame_or_proxy_.index() > 0; }

  FrameTreeNode* GetFrameTreeNode() {
    if (auto* proxy = GetProxy()) {
      return proxy->frame_tree_node();
    } else if (auto* frame = GetFrame()) {
      return frame->frame_tree_node();
    }
    return nullptr;
  }

  RenderFrameHostImpl* GetCurrentFrameHost() {
    if (auto* proxy = GetProxy()) {
      return proxy->frame_tree_node()->current_frame_host();
    } else if (auto* frame = GetFrame()) {
      return frame;
    }
    return nullptr;
  }

 private:
  RenderFrameProxyHost* GetProxy() {
    if (auto** proxy = absl::get_if<RenderFrameProxyHost*>(&frame_or_proxy_)) {
      return *proxy;
    }
    return nullptr;
  }

  RenderFrameHostImpl* GetFrame() {
    if (auto** frame = absl::get_if<RenderFrameHostImpl*>(&frame_or_proxy_)) {
      return *frame;
    }
    return nullptr;
  }

  absl::variant<absl::monostate, RenderFrameHostImpl*, RenderFrameProxyHost*>
      frame_or_proxy_;
};

namespace {

// Maximum amount of time to wait for beforeunload/unload handlers to be
// processed by the renderer process.
constexpr int kUnloadTimeoutInMSec = 500;
constexpr base::TimeDelta kUnloadTimeout =
    base::Milliseconds(kUnloadTimeoutInMSec);

constexpr int kSubframeProcessShutdownDelayInMSec = 2 * 1000;
static_assert(kSubframeProcessShutdownDelayInMSec + kUnloadTimeoutInMSec <
                  RenderProcessHostImpl::kKeepAliveHandleFactoryTimeoutInMSec,
              "The maximum process shutdown delay should not exceed the "
              "keepalive timeout. This has security implications, see "
              "https://crbug.com/1177674.");

#if BUILDFLAG(IS_ANDROID)
const void* const kRenderFrameHostAndroidKey = &kRenderFrameHostAndroidKey;
#endif  // BUILDFLAG(IS_ANDROID)

// The next value to use for the accessibility reset token.
uint32_t g_accessibility_reset_token = 0;

// Whether to allow injecting javascript into any kind of frame, for Android
// WebView, WebLayer, Fuchsia web.ContextProvider and CastOS content shell.
bool g_allow_injecting_javascript = false;

const char kDotGoogleDotCom[] = ".google.com";

typedef std::unordered_map<GlobalRenderFrameHostId,
                           RenderFrameHostImpl*,
                           GlobalRenderFrameHostIdHasher>
    RoutingIDFrameMap;
base::LazyInstance<RoutingIDFrameMap>::DestructorAtExit g_routing_id_frame_map =
    LAZY_INSTANCE_INITIALIZER;

// A global set of all sandboxed RenderFrameHosts that could be isolated from
// the rest of their SiteInstance.
typedef std::unordered_set<GlobalRenderFrameHostId,
                           GlobalRenderFrameHostIdHasher>
    RoutingIDIsolatableSandboxedIframesSet;
base::LazyInstance<RoutingIDIsolatableSandboxedIframesSet>::DestructorAtExit
    g_routing_id_isolatable_sandboxed_iframes_set = LAZY_INSTANCE_INITIALIZER;

using TokenFrameMap = std::unordered_map<blink::LocalFrameToken,
                                         RenderFrameHostImpl*,
                                         blink::LocalFrameToken::Hasher>;
base::LazyInstance<TokenFrameMap>::Leaky g_token_frame_map =
    LAZY_INSTANCE_INITIALIZER;

auto& GetDocumentTokenMap() {
  static base::NoDestructor<std::unordered_map<
      blink::DocumentToken, RenderFrameHostImpl*, blink::DocumentToken::Hasher>>
      map;
  return *map;
}

BackForwardCacheMetrics::NotRestoredReason
RendererEvictionReasonToNotRestoredReason(
    blink::mojom::RendererEvictionReason reason) {
  switch (reason) {
    case blink::mojom::RendererEvictionReason::kJavaScriptExecution:
      return BackForwardCacheMetrics::NotRestoredReason::kJavaScriptExecution;
    case blink::mojom::RendererEvictionReason::
        kNetworkRequestDatapipeDrainedAsBytesConsumer:
      return BackForwardCacheMetrics::NotRestoredReason::
          kNetworkRequestDatapipeDrainedAsBytesConsumer;
    case blink::mojom::RendererEvictionReason::kNetworkRequestRedirected:
      return BackForwardCacheMetrics::NotRestoredReason::
          kNetworkRequestRedirected;
    case blink::mojom::RendererEvictionReason::kNetworkRequestTimeout:
      return BackForwardCacheMetrics::NotRestoredReason::kNetworkRequestTimeout;
    case blink::mojom::RendererEvictionReason::kNetworkExceedsBufferLimit:
      return BackForwardCacheMetrics::NotRestoredReason::
          kNetworkExceedsBufferLimit;
  }
  NOTREACHED();
  return BackForwardCacheMetrics::NotRestoredReason::kUnknown;
}

// Ensure that we reset nav_entry_id_ in DidCommitProvisionalLoad if any of
// the validations fail and lead to an early return.  Call disable() once we
// know the commit will be successful.  Resetting nav_entry_id_ avoids acting on
// any UpdateState or UpdateTitle messages after an ignored commit.
class ScopedCommitStateResetter {
 public:
  explicit ScopedCommitStateResetter(RenderFrameHostImpl* render_frame_host)
      : render_frame_host_(render_frame_host) {}

  ~ScopedCommitStateResetter() {
    if (!disabled_) {
      render_frame_host_->set_nav_entry_id(0);
    }
  }

  void disable() { disabled_ = true; }

 private:
  raw_ptr<RenderFrameHostImpl> render_frame_host_;
  bool disabled_ = false;
};

void GrantFileAccess(int child_id,
                     const std::vector<base::FilePath>& file_paths) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  for (const auto& file : file_paths) {
    if (!policy->CanReadFile(child_id, file))
      policy->GrantReadFile(child_id, file);
  }
}

#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
// RemoterFactory that delegates Create() calls to the ContentBrowserClient.
//
// Since Create() could be called at any time, perhaps by a stray task being run
// after a RenderFrameHost has been destroyed, the RemoterFactoryImpl uses the
// process/routing IDs as a weak reference to the RenderFrameHostImpl.
class RemoterFactoryImpl final : public media::mojom::RemoterFactory {
 public:
  RemoterFactoryImpl(int process_id, int routing_id)
      : process_id_(process_id), routing_id_(routing_id) {}

  RemoterFactoryImpl(const RemoterFactoryImpl&) = delete;
  RemoterFactoryImpl& operator=(const RemoterFactoryImpl&) = delete;

 private:
  void Create(mojo::PendingRemote<media::mojom::RemotingSource> source,
              mojo::PendingReceiver<media::mojom::Remoter> receiver) final {
    if (auto* host = RenderFrameHostImpl::FromID(process_id_, routing_id_)) {
      GetContentClient()->browser()->CreateMediaRemoter(host, std::move(source),
                                                        std::move(receiver));
    }
  }

  const int process_id_;
  const int routing_id_;
};
#endif  // BUILDFLAG(ENABLE_MEDIA_REMOTING)

RenderFrameHostOrProxy LookupRenderFrameHostOrProxy(int process_id,
                                                    int routing_id) {
  RenderFrameHostImpl* rfh =
      RenderFrameHostImpl::FromID(process_id, routing_id);
  RenderFrameProxyHost* proxy = nullptr;
  if (!rfh)
    proxy = RenderFrameProxyHost::FromID(process_id, routing_id);
  return RenderFrameHostOrProxy(rfh, proxy);
}

RenderFrameHostOrProxy LookupRenderFrameHostOrProxy(
    int process_id,
    const blink::FrameToken& frame_token) {
  if (frame_token.Is<blink::LocalFrameToken>()) {
    auto it = g_token_frame_map.Get().find(
        frame_token.GetAs<blink::LocalFrameToken>());
    // The check against |process_id| isn't strictly necessary, but represents
    // an extra level of protection against a renderer trying to force a frame
    // token.
    if (it == g_token_frame_map.Get().end() ||
        process_id != it->second->GetProcess()->GetID()) {
      return RenderFrameHostOrProxy(nullptr, nullptr);
    }
    return RenderFrameHostOrProxy(it->second, nullptr);
  }
  DCHECK(frame_token.Is<blink::RemoteFrameToken>());
  return RenderFrameHostOrProxy(
      nullptr, RenderFrameProxyHost::FromFrameToken(
                   process_id, frame_token.GetAs<blink::RemoteFrameToken>()));
}

// Set crash keys that will help understand the circumstances of a renderer
// kill.  Note that the commit URL is already reported in a crash key, and
// additional keys are logged in RenderProcessHostImpl::ShutdownForBadMessage.
void LogRendererKillCrashKeys(const SiteInfo& site_info) {
  static auto* const site_info_key = base::debug::AllocateCrashKeyString(
      "current_site_info", base::debug::CrashKeySize::Size256);
  base::debug::SetCrashKeyString(site_info_key, site_info.GetDebugString());
}

void LogCanCommitOriginAndUrlFailureReason(const std::string& failure_reason) {
  static auto* const failure_reason_key = base::debug::AllocateCrashKeyString(
      "rfhi_can_commit_failure_reason", base::debug::CrashKeySize::Size64);
  base::debug::SetCrashKeyString(failure_reason_key, failure_reason);
}

std::unique_ptr<blink::PendingURLLoaderFactoryBundle> CloneFactoryBundle(
    scoped_refptr<blink::URLLoaderFactoryBundle> bundle) {
  return base::WrapUnique(static_cast<blink::PendingURLLoaderFactoryBundle*>(
      bundle->Clone().release()));
}

// Helper method to download a URL on UI thread.
void StartDownload(
    std::unique_ptr<download::DownloadUrlParameters> parameters,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(parameters->render_process_host_id());
  if (!render_process_host)
    return;

  BrowserContext* browser_context = render_process_host->GetBrowserContext();

  DownloadManager* download_manager = browser_context->GetDownloadManager();
  parameters->set_download_source(download::DownloadSource::FROM_RENDERER);
  download_manager->DownloadUrl(std::move(parameters),
                                std::move(blob_url_loader_factory));
}

// Called on the UI thread when the data URL in the BlobDataHandle
// is read.
void OnDataURLRetrieved(
    std::unique_ptr<download::DownloadUrlParameters> parameters,
    GURL data_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!data_url.is_valid())
    return;
  parameters->set_url(std::move(data_url));
  StartDownload(std::move(parameters), nullptr);
}

// Analyzes trusted sources of a frame's trust-token-redemption Permissions
// Policy feature to see if the feature is definitely disabled or potentially
// enabled.
//
// This information will be bound to a URLLoaderFactory; if the answer is
// "definitely disabled," the network service will report a bad message if it
// receives a request from the renderer to execute a Trust Tokens redemption or
// signing operation in the frame.
//
// A return value of kForbid denotes that the feature is disabled for the
// frame. A return value of kPotentiallyPermit means that all trusted
// information sources say that the policy is enabled.
network::mojom::TrustTokenOperationPolicyVerdict
DetermineWhetherToForbidTrustTokenOperation(
    const RenderFrameHostImpl* frame,
    const blink::mojom::CommitNavigationParams& commit_params,
    const url::Origin& subframe_origin,
    const network::mojom::TrustTokenOperationType& operation) {
  std::unique_ptr<blink::PermissionsPolicy> subframe_policy;
  // TODO(https://crbug.com/1430514): Add WPT to test how TrustTokens behave in
  // a FencedFrame's subframe.
  if (frame->IsFencedFrameRoot()) {
    // Fenced frames have a list of required permission policies to load and
    // can't be granted extra policies, so use the required policies instead of
    // inheriting from its parent. Note that the parent policies must allow the
    // required policies, which is checked separately in
    // NavigationRequest::CheckPermissionsPoliciesForFencedFrames.
    const absl::optional<FencedFrameProperties>& fenced_frame_properties =
        frame->frame_tree_node()->GetFencedFrameProperties();
    base::span<const blink::mojom::PermissionsPolicyFeature> permissions;
    if (fenced_frame_properties) {
      permissions = fenced_frame_properties->effective_enabled_permissions;
    }
    subframe_policy = blink::PermissionsPolicy::CreateForFencedFrame(
        subframe_origin, permissions);
  } else {
    // For main frame loads, the frame's permissions policy is determined
    // entirely by response headers, which are provided by the renderer.
    if (!frame->GetParent())
      return network::mojom::TrustTokenOperationPolicyVerdict::
          kPotentiallyPermit;

    const blink::PermissionsPolicy* parent_policy =
        frame->GetParent()->permissions_policy();
    blink::ParsedPermissionsPolicy container_policy =
        commit_params.frame_policy.container_policy;

    subframe_policy = blink::PermissionsPolicy::CreateFromParentPolicy(
        parent_policy, container_policy, subframe_origin);
  }

  switch (operation) {
    case network::mojom::TrustTokenOperationType::kRedemption:
    case network::mojom::TrustTokenOperationType::kSigning:
      if (subframe_policy->IsFeatureEnabled(
              blink::mojom::PermissionsPolicyFeature::kTrustTokenRedemption)) {
        return network::mojom::TrustTokenOperationPolicyVerdict::
            kPotentiallyPermit;
      }
      return network::mojom::TrustTokenOperationPolicyVerdict::kForbid;
    case network::mojom::TrustTokenOperationType::kIssuance:
      if (subframe_policy->IsFeatureEnabled(
              blink::mojom::PermissionsPolicyFeature::
                  kPrivateStateTokenIssuance)) {
        return network::mojom::TrustTokenOperationPolicyVerdict::
            kPotentiallyPermit;
      }
      return network::mojom::TrustTokenOperationPolicyVerdict::kForbid;
  }
  return network::mojom::TrustTokenOperationPolicyVerdict::kForbid;
}

// When a frame creates its initial subresource loaders, it needs to know
// whether the trust-token-redemption Permissions Policy feature will be enabled
// after the commit finishes, which is a little involved (see
// DetermineWhetherToForbidTrustTokenOperation). In contrast, if it needs to
// make this decision once the frame has committted---for instance, to create
// more loaders after the network service crashes---it can directly consult the
// current Permissions Policy state to determine whether the feature is enabled.
network::mojom::TrustTokenOperationPolicyVerdict
DetermineAfterCommitWhetherToForbidTrustTokenOperation(
    RenderFrameHostImpl& impl,
    const network::mojom::TrustTokenOperationType& operation) {
  switch (operation) {
    case network::mojom::TrustTokenOperationType::kRedemption:
    case network::mojom::TrustTokenOperationType::kSigning:
      if (impl.IsFeatureEnabled(
              blink::mojom::PermissionsPolicyFeature::kTrustTokenRedemption)) {
        return network::mojom::TrustTokenOperationPolicyVerdict::
            kPotentiallyPermit;
      }
      return network::mojom::TrustTokenOperationPolicyVerdict::kForbid;
    case network::mojom::TrustTokenOperationType::kIssuance:
      if (impl.IsFeatureEnabled(blink::mojom::PermissionsPolicyFeature::
                                    kPrivateStateTokenIssuance)) {
        return network::mojom::TrustTokenOperationPolicyVerdict::
            kPotentiallyPermit;
      }
      return network::mojom::TrustTokenOperationPolicyVerdict::kForbid;
  }
  return network::mojom::TrustTokenOperationPolicyVerdict::kForbid;
}

// Verify that |browser_side_origin| and |renderer_side_origin| match.  See also
// https://crbug.com/888079. Returns true if the origins match, and false
// otherwise.
bool VerifyThatBrowserAndRendererCalculatedOriginsToCommitMatch(
    NavigationRequest* navigation_request,
    const mojom::DidCommitProvisionalLoadParams& params) {
  DCHECK(navigation_request);

  // This should be called only when a new document is created. Navigations in
  // the same document and page activations do not create a new document.
  DCHECK(!navigation_request->IsSameDocument());
  DCHECK(!navigation_request->IsPageActivation());

  // Ignore for now cases where the NavigationRequest is in an unexpectedly
  // early state. Triggered by the following tests:
  // NavigationBrowserTest.OpenerNavigation_DownloadPolicy,
  // WebContentsImplBrowserTest.NewNamedWindow.
  if (navigation_request->state() < NavigationRequest::WILL_PROCESS_RESPONSE)
    return true;

  // Blob urls with content scheme are opaque on browser side because the
  // browser doesn't have access to the BlobURLNullOriginMap.
  // (https://crbug.com/1295268).
  const url::Origin& renderer_side_origin = params.origin;
  std::pair<absl::optional<url::Origin>, std::string>
      browser_side_origin_and_debug_info =
          navigation_request->GetOriginToCommitWithDebugInfo();
  if (renderer_side_origin.scheme() == url::kContentScheme &&
      browser_side_origin_and_debug_info.first->opaque()) {
    return true;
  }

  // For non-opaque origins, we say the browser and renderer calculated origins
  // match if they are exactly the same.
  bool origins_match = (browser_side_origin_and_debug_info.first.value() ==
                        renderer_side_origin);

  // For opaque origins, we say the browser and renderer calculated origins
  // match if their precursor origins match (their nonces might not match).
  if (renderer_side_origin.opaque() &&
      browser_side_origin_and_debug_info.first->opaque()) {
    origins_match = (renderer_side_origin.GetTupleOrPrecursorTupleIfOpaque() ==
                     browser_side_origin_and_debug_info.first
                         ->GetTupleOrPrecursorTupleIfOpaque());
  }

  // TODO(https://crbug.com/888079): Remove the DumpWithoutCrashing below, once
  // we are sure that the `browser_side_origin` is always the same as the
  // `renderer_side_origin`.
  if (!origins_match) {
    NavigationRequest::ScopedCrashKeys navigation_request_crash_keys(
        *navigation_request);
    SCOPED_CRASH_KEY_STRING256(
        "", "browser_origin",
        browser_side_origin_and_debug_info.first->GetDebugString());
    SCOPED_CRASH_KEY_STRING256("", "browser_debug_info",
                               browser_side_origin_and_debug_info.second);

    SCOPED_CRASH_KEY_STRING256(
        "", "browser_ready_to_commit_origin",
        navigation_request->browser_side_origin_to_commit_with_debug_info()
            .first->GetDebugString());
    SCOPED_CRASH_KEY_STRING256(
        "", "browser_ready_to_commit_debug_info",
        navigation_request->browser_side_origin_to_commit_with_debug_info()
            .second);

    SCOPED_CRASH_KEY_STRING256("", "renderer_origin",
                               renderer_side_origin.GetDebugString());
    SCOPED_CRASH_KEY_STRING256("", "renderer_debug_info",
                               params.origin_calculation_debug_info);
    CaptureTraceForNavigationDebugScenario(
        DebugScenario::kDebugBrowserVsRendererOriginToCommit);
    base::debug::DumpWithoutCrashing();
    DCHECK_EQ(browser_side_origin_and_debug_info.first.value(),
              renderer_side_origin)
        << "; navigation_request->GetURL() = " << navigation_request->GetURL();
    return false;
  }

  return true;
}

// Enum used for Navigation.VerifyDidCommitParams histogram, to indicate which
// DidCommitProvisionalLoadParams differ when comparing browser- vs
// renderer-calculated values.
// Do NOT delete or reorder existing entries.
enum class VerifyDidCommitParamsDifference {
  kIntendedAsNewEntry = 0,
  kMethod = 1,
  kURLIsUnreachable = 2,
  kBaseURL = 3,
  kPostID = 4,
  kIsOverridingUserAgent = 5,
  kHTTPStatusCode = 6,
  kShouldUpdateHistory = 7,
  kGesture = 8,
  kShouldReplaceCurrentEntry = 9,
  kURL = 10,
  kDidCreateNewEntry = 11,
  kTransition = 12,
  kHistoryListWasCleared = 13,
  kOrigin = 14,
  kMaxValue = kOrigin,
};

// A simplified version of Blink's WebFrameLoadType, used to simulate renderer
// calculations. See CalculateRendererLoadType() further below.
// TODO(https://crbug.com/1131832): This should only be here temporarily.
// Remove this once the renderer behavior at commit time is more consistent with
// what the browser instructed it to do (e.g. reloads will always be classified
// as kReload).
enum class RendererLoadType {
  kStandard,
  kBackForward,
  kReload,
  kReplaceCurrentItem,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Used to log whether the call to SSLManager::DidStartResourceResponse()
// resulted in a no-op or if the exceptions were cleared out when a good
// certificate was seen. Matches histogram enum (SSLSubresourceResponseType).
enum class SSLSubresourceResponseType {
  // Includes cases when call resulted in a no-op.
  kIgnored = 0,
  // Includes cases when exceptions were cleared after seeing a good cert.
  kProcessed = 1,
  kMaxValue = kProcessed,
};

bool ValidateCSPAttribute(const std::string& value) {
  static const size_t kMaxLengthCSPAttribute = 4096;
  if (!base::IsStringASCII(value))
    return false;
  if (value.length() > kMaxLengthCSPAttribute ||
      value.find('\n') != std::string::npos ||
      value.find('\r') != std::string::npos) {
    return false;
  }
  return true;
}

perfetto::protos::pbzero::FrameDeleteIntention FrameDeleteIntentionToProto(
    mojom::FrameDeleteIntention intent) {
  using ProtoLevel = perfetto::protos::pbzero::FrameDeleteIntention;
  switch (intent) {
    case mojom::FrameDeleteIntention::kNotMainFrame:
      return ProtoLevel::FRAME_DELETE_INTENTION_NOT_MAIN_FRAME;
    case mojom::FrameDeleteIntention::kSpeculativeMainFrameForShutdown:
      return ProtoLevel::
          FRAME_DELETE_INTENTION_SPECULATIVE_MAIN_FRAME_FOR_SHUTDOWN;
    case mojom::FrameDeleteIntention::
        kSpeculativeMainFrameForNavigationCancelled:
      return ProtoLevel::
          FRAME_DELETE_INTENTION_SPECULATIVE_MAIN_FRAME_FOR_NAVIGATION_CANCELLED;
  }
  // All cases should've been handled by the switch case above.
  NOTREACHED();
  return ProtoLevel::FRAME_DELETE_INTENTION_NOT_MAIN_FRAME;
}

void WriteRenderFrameImplDeletion(perfetto::EventContext& ctx,
                                  RenderFrameHostImpl* rfh,
                                  mojom::FrameDeleteIntention intent) {
  auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
  auto* data = event->set_render_frame_impl_deletion();
  data->set_has_pending_commit(rfh->HasPendingCommitNavigation());
  data->set_has_pending_cross_document_commit(
      rfh->HasPendingCommitForCrossDocumentNavigation());
  data->set_frame_tree_node_id(rfh->GetFrameTreeNodeId());
  data->set_intent(FrameDeleteIntentionToProto(intent));
}

// Returns the amount of time to keep subframe processes alive in case they can
// be reused. Returns zero if under memory pressure, as memory should be freed
// up as soon as possible if it's limited.
base::TimeDelta GetSubframeProcessShutdownDelay(
    BrowserContext* browser_context) {
  static constexpr base::TimeDelta kZeroDelay;
  if (!RenderProcessHostImpl::ShouldDelayProcessShutdown()) {
    return kZeroDelay;
  }

  // Don't delay process shutdown under memory pressure. Does not cancel
  // existing shutdown delays for processes already in delayed-shutdown state.
  const auto* const memory_monitor = base::MemoryPressureMonitor::Get();
  if (memory_monitor &&
      memory_monitor->GetCurrentPressureLevel() >=
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE) {
    return kZeroDelay;
  }

  return base::Milliseconds(kSubframeProcessShutdownDelayInMSec);
}

// Returns the "document" URL used for a navigation, which might be different
// than the commit URL (CommonNavigationParam's URL) for certain cases such as
// error document and loadDataWithBaseURL() commits.
GURL GetLastDocumentURL(
    NavigationRequest* request,
    const mojom::DidCommitProvisionalLoadParams& params,
    bool last_document_is_error_document,
    const RenderFrameHostImpl::RendererURLInfo& renderer_url_info) {
  if (request->DidEncounterError() ||
      (request->IsSameDocument() && last_document_is_error_document)) {
    // If the navigation happens on an error document, the document URL is set
    // to kUnreachableWebDataURL. Note that if a same-document navigation
    // happens in an error document it's possible for the document URL to have
    // changed, but the browser has no way of knowing that URL since it isn't
    // exposed in any way. Additionally, all current known ways to do a
    // same-document navigation on an error document
    // (history.pushState/replaceState without changing the URL) won't change
    // the URL, so it's probably OK to keep using kUnreachableWebDataURL here.
    return GURL(kUnreachableWebDataURL);
  }
  if (request->IsLoadDataWithBaseURL()) {
    // loadDataWithBaseURL() navigation can set its own "base URL", which is
    // also used by the renderer as the document URL unless the navigation
    // failed (which is already accounted for in the error document case above).
    return request->common_params().base_url_for_data_url;
  }
  if (renderer_url_info.was_loaded_from_load_data_with_base_url &&
      request->IsSameDocument()) {
    // If this is a same-document navigation on a document loaded from
    // loadDataWithBaseURL(), it is not currently possible to figure out the
    // document URL. This is because the renderer can navigate to any
    // same-document URL, but that URL will not be used for
    // DidCommitProvisionalLoadParams' `url` if the loading URL for the document
    // is set to the data: URL. In this case, just return the last document URL,
    // since at least it will have the correct origin.
    return renderer_url_info.last_document_url;
  }
  // For all other navigations, the document URL should be the same as the URL
  // that is used to commit.
  return params.url;
}

bool IsAvoidUnnecessaryBeforeUnloadCheckSyncEnabled() {
  const bool is_feature_enabled = base::FeatureList::IsEnabled(
      features::kAvoidUnnecessaryBeforeUnloadCheckSync);
#if BUILDFLAG(IS_ANDROID)
  return is_feature_enabled &&
         GetContentClient()
             ->browser()
             ->SupportsAvoidUnnecessaryBeforeUnloadCheckSync();
#else
  return is_feature_enabled;
#endif
}

bool IsAvoidUnnecessaryBeforeUnloadCheckPostTaskEnabled() {
  // Only one of sync or posttask should be used. If both are set, use sync.
  return !IsAvoidUnnecessaryBeforeUnloadCheckSyncEnabled();
}

// Returns true if `host` has the Window Management permission granted.
bool IsWindowManagementGranted(RenderFrameHost* host) {
  content::PermissionController* permission_controller =
      host->GetBrowserContext()->GetPermissionController();
  DCHECK(permission_controller);

  return permission_controller->GetPermissionStatusForCurrentDocument(
             blink::PermissionType::WINDOW_MANAGEMENT, host) ==
         blink::mojom::PermissionStatus::GRANTED;
}

bool IsOpenGraphMetadataValid(const blink::mojom::OpenGraphMetadata* metadata) {
  return !metadata->image || metadata->image->SchemeIsHTTPOrHTTPS();
}

void ForwardOpenGraphMetadataIfValid(
    base::OnceCallback<void(blink::mojom::OpenGraphMetadataPtr)> callback,
    blink::mojom::OpenGraphMetadataPtr metadata) {
  if (IsOpenGraphMetadataValid(metadata.get()))
    std::move(callback).Run(std::move(metadata));
  else
    std::move(callback).Run({});
}

// Creates a JavaScriptExecuteRequestForTestsCallback callback that delegates
// to the given JavaScriptResultCallback.
blink::mojom::LocalFrame::JavaScriptExecuteRequestForTestsCallback
CreateJavaScriptExecuteRequestForTestsCallback(
    RenderFrameHost::JavaScriptResultCallback callback) {
  if (!callback)
    return base::NullCallback();
  return base::BindOnce(
      [](RenderFrameHost::JavaScriptResultCallback callback,
         blink::mojom::JavaScriptExecutionResultType type, base::Value value) {
        if (type == blink::mojom::JavaScriptExecutionResultType::kSuccess)
          std::move(callback).Run(value.Clone());
        else
          std::move(callback).Run(base::Value());
      },
      std::move(callback));
}

bool ValidateUnfencedTopNavigation(
    RenderFrameHostImpl* render_frame_host,
    GURL& url,
    int initiator_process_id,
    const scoped_refptr<network::ResourceRequestBody>& post_body,
    bool user_gesture) {
  // Validate and modify `url` as needed.
  render_frame_host->GetSiteInstance()->GetProcess()->FilterURL(
      /*empty_allowed=*/false, &url);

  // It should only be possible to send this IPC with this flag from an
  // opaque-ads fenced frame. Opaque-ads fenced frames should always
  // have the sandbox flag `allow-top-navigation-by-user-activation`.
  if ((render_frame_host->frame_tree_node()->GetDeprecatedFencedFrameMode() !=
       blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds) ||
      render_frame_host->IsSandboxed(
          network::mojom::WebSandboxFlags::kTopNavigationByUserActivation)) {
    // If we get the IPC elsewhere, assume the renderer is compromised.
    bad_message::ReceivedBadMessage(
        initiator_process_id,
        bad_message::RFHI_UNFENCED_TOP_IPC_OUTSIDE_FENCED_FRAME);
    return false;
  }

  // Perform checks that normally would be performed in
  // `blink::CanNavigate` but that we skipped because the target
  // frame wasn't available in the renderer.
  // TODO(crbug.com/1123606): Change these checks to send a BadMessage
  // when the renderer-side refactor is complete.

  // Javascript URLs are not allowed, because they can be used to
  // communicate from the fenced frame to the embedder.
  // TODO(crbug.com/1315802): It does not seem possible to reach this code
  // with an uncompromised renderer, because javascript URLs don't reach
  // the same IPC; instead they run inside the fenced frame as _self.
  // It also seems that Javascript URLs would be caught earlier in this
  // particular code path by VerifyOpenURLParams().
  // In this code's final IPC resting place after the factor, make sure
  // to check whether this code is redundant.
  if (url.SchemeIs(url::kJavaScriptScheme)) {
    render_frame_host->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "The frame attempting navigation must be in the same fenced "
        "frame tree as the target if navigating to a javascript: url");
    return false;
  }

  // Blob URLs are not allowed, because you should not be able to exfiltrate
  // arbitrary amounts of information from a fenced frame.
  if (url.SchemeIs(url::kBlobScheme)) {
    render_frame_host->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "_unfencedTop may not be used with a blob: url");
    return false;
  }

  // POST requests are not allowed, because they are asynchronous and more
  // difficult to account for in privacy budgets.
  if (post_body) {
    render_frame_host->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "_unfencedTop may not be used to send POST requests");
    return false;
  }

  // User activation is required, because fenced frames use the sandbox
  // flag `allow-top-navigation-by-user-activation`.
  // It would be better to instead check
  // `render_frame_host->HasTransientUserActivation()`,
  // but it has already been consumed at this point.
  // TODO(crbug.com/848778): use the browser's source of truth for user
  // activation here (and elsewhere in this file) rather than trust the
  // renderer.
  if (!user_gesture) {
    render_frame_host->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "The frame attempting navigation of the top-level window is "
        "sandboxed with the 'allow-top-navigation-by-user-activation' "
        "flag, but has no user activation (aka gesture). See "
        "https://www.chromestatus.com/feature/5629582019395584.");
    return false;
  }

  return true;
}

// Records the identifiable surface metric associated with a document created
// event when the identifiability study is active.
void RecordIdentifiabilityDocumentCreatedMetrics(
    const ukm::SourceId document_ukm_source_id,
    ukm::UkmRecorder* ukm_recorder,
    ukm::SourceId navigation_source_id,
    bool is_cross_origin_frame,
    bool is_cross_site_frame,
    bool is_main_frame) {
  if (blink::IdentifiabilityStudySettings::Get()->IsActive()) {
    blink::IdentifiabilityStudyDocumentCreated(document_ukm_source_id)
        .SetNavigationSourceId(navigation_source_id)
        .SetIsMainFrame(is_main_frame)
        .SetIsCrossOriginFrame(is_cross_origin_frame)
        .SetIsCrossSiteFrame(is_cross_site_frame)
        .Record(ukm_recorder);
  }
}

bool IsOpenerSameOriginFrame(const RenderFrameHostImpl* opener) {
  return opener->GetLastCommittedOrigin() ==
         opener->GetMainFrame()->GetLastCommittedOrigin();
}

// See https://html.spec.whatwg.org/C/#browsing-context-names (step 8)
// ```
// If current's top-level browsing context's active document's
// cross-origin opener policy's value is "same-origin" or
// "same-origin-plus-COEP", then [...] set noopener to true, name to
// "_blank", and windowType to "new with no opener".
// ```
bool CoopSuppressOpener(const RenderFrameHostImpl* opener) {
  // Those values are explicitly listed here, to force creator of new
  // values to make an explicit decision in the future.
  // See regression: https://crbug.com/1181673
  switch (opener->GetMainFrame()->cross_origin_opener_policy().value) {
    case network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone:
    case network::mojom::CrossOriginOpenerPolicyValue::kSameOriginAllowPopups:
    case network::mojom::CrossOriginOpenerPolicyValue::kRestrictProperties:
    case network::mojom::CrossOriginOpenerPolicyValue::
        kRestrictPropertiesPlusCoep:
      return false;

    case network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin:
    case network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep:
      return !IsOpenerSameOriginFrame(opener);
  }
}

void RecordAutomaticBeaconOutcome(const blink::AutomaticBeaconOutcome outcome) {
  base::UmaHistogramEnumeration(blink::kAutomaticBeaconOutcomeHistogram,
                                outcome);
}

}  // namespace

class RenderFrameHostImpl::SubresourceLoaderFactoriesConfig {
 public:
  static SubresourceLoaderFactoriesConfig ForLastCommittedNavigation(
      RenderFrameHostImpl& frame) {
    SubresourceLoaderFactoriesConfig result;
    result.origin_ = frame.GetLastCommittedOrigin();
    result.isolation_info_ = frame.GetIsolationInfoForSubresources();
    result.client_security_state_ = frame.BuildClientSecurityState();
    if (frame.coep_reporter_) {
      frame.coep_reporter_->Clone(
          result.coep_reporter_.BindNewPipeAndPassReceiver());
    }
    result.trust_token_redemption_policy_ =
        DetermineAfterCommitWhetherToForbidTrustTokenOperation(
            frame, network::mojom::TrustTokenOperationType::kRedemption);
    result.trust_token_issuance_policy_ =
        DetermineAfterCommitWhetherToForbidTrustTokenOperation(
            frame, network::mojom::TrustTokenOperationType::kIssuance);

    // Our data collection policy disallows collecting UKMs while prerendering.
    // So, assign a valid ID only when the page is not in the prerendering
    // state. See //content/browser/preloading/prerender/README.md and ask the
    // team to explore options to record data for prerendering pages.
    result.ukm_source_id_ = ukm::SourceIdObj::FromInt64(
        frame.IsInLifecycleState(LifecycleState::kPrerendering)
            ? ukm::kInvalidSourceId
            : frame.GetPageUkmSourceId());

    return result;
  }

  static SubresourceLoaderFactoriesConfig ForPendingNavigation(
      NavigationRequest& navigation_request) {
    SubresourceLoaderFactoriesConfig result;
    result.origin_ = navigation_request.GetOriginToCommit().value();
    result.client_security_state_ =
        navigation_request.BuildClientSecurityStateForCommittedDocument();
    result.ukm_source_id_ = ukm::SourceIdObj::FromInt64(
        navigation_request.GetNextPageUkmSourceId());

    // TODO(lukasza): Consider pushing the ok-vs-error differentiation into
    // NavigationRequest methods (e.g. into |isolation_info_for_subresources|
    // and/or |coep_reporter| methods).
    if (navigation_request.DidEncounterError()) {
      // Error frames gets locked down `isolation_info_`,
      // `trust_token_redemption_policy_` and `trust_token_issuance_policy_`
      // plus an empty/uninitialized `coep_reporter_`.
      result.isolation_info_ = net::IsolationInfo::CreateTransient();
      result.trust_token_redemption_policy_ =
          network::mojom::TrustTokenOperationPolicyVerdict::kForbid;
      result.trust_token_issuance_policy_ =
          network::mojom::TrustTokenOperationPolicyVerdict::kForbid;
    } else {
      result.isolation_info_ =
          navigation_request.isolation_info_for_subresources();
      if (navigation_request.coep_reporter()) {
        navigation_request.coep_reporter()->Clone(
            result.coep_reporter_.BindNewPipeAndPassReceiver());
      }
      result.trust_token_redemption_policy_ =
          DetermineWhetherToForbidTrustTokenOperation(
              navigation_request.GetRenderFrameHost(),
              navigation_request.commit_params(), result.origin(),
              network::mojom::TrustTokenOperationType::kRedemption);
      result.trust_token_issuance_policy_ =
          DetermineWhetherToForbidTrustTokenOperation(
              navigation_request.GetRenderFrameHost(),
              navigation_request.commit_params(), result.origin(),
              network::mojom::TrustTokenOperationType::kIssuance);
    }

    return result;
  }

  // ForPendingOrLastCommittedNavigation is useful in scenarios where there is
  // no coordination between the timing of 1) a navigation commit and 2)
  // subresource loader factories bundle creation.  For example, using
  // ForPendingOrLastCommittedNavigation from UpdateSubresourceLoaderFactories
  // leads to using the correct SubresourceLoaderFactoriesConfig regardless of
  // the timing of when a NetworkService crash triggers a call to
  // UpdateSubresourceLoaderFactories:
  // 1. If the crash happens when there is an in-flight Commit IPC to the
  //    renderer process, then the newly created subresource loader factories
  //    will arrive at the renderer *after* the Commit IPC and therefore the
  //    factories need to use the configuration (e.g. the origin) based on the
  //    pending navigation.
  // 2. OTOH, if the crash happens when there is no in-flight Commit IPC then
  //    the newly created factories should use the configuration based on the
  //    last committed navigation.
  //
  // TODO(https://crbug.com/729021): ForPendingOrLastCommittedNavigation might
  // not be needed once we have RenderDocumentHost (e.g. we swap on every
  // cross-document navigation), because with RenderDocumentHost there is no
  // risk of sending last-commited-navigation-based subresource loaders to a
  // document different from the last-committed one.
  static SubresourceLoaderFactoriesConfig ForPendingOrLastCommittedNavigation(
      RenderFrameHostImpl& frame) {
    NavigationRequest* navigation_request =
        frame.FindLatestNavigationRequestThatIsStillCommitting();
    return navigation_request ? ForPendingNavigation(*navigation_request)
                              : ForLastCommittedNavigation(frame);
  }

  ~SubresourceLoaderFactoriesConfig() = default;

  SubresourceLoaderFactoriesConfig(SubresourceLoaderFactoriesConfig&&) =
      default;
  SubresourceLoaderFactoriesConfig& operator=(
      SubresourceLoaderFactoriesConfig&&) = default;

  SubresourceLoaderFactoriesConfig(const SubresourceLoaderFactoriesConfig&) =
      delete;
  SubresourceLoaderFactoriesConfig& operator=(
      const SubresourceLoaderFactoriesConfig&) = delete;

  const url::Origin& origin() const { return origin_; }
  const net::IsolationInfo& isolation_info() const { return isolation_info_; }

  network::mojom::ClientSecurityStatePtr GetClientSecurityState() const {
    return mojo::Clone(client_security_state_);
  }

  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
  GetCoepReporter() const {
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter> p;
    if (coep_reporter_) {
      coep_reporter_->Clone(p.InitWithNewPipeAndPassReceiver());
    }
    return p;
  }

  const network::mojom::TrustTokenOperationPolicyVerdict&
  trust_token_redemption_policy() const {
    return trust_token_redemption_policy_;
  }

  const network::mojom::TrustTokenOperationPolicyVerdict&
  trust_token_issuance_policy() const {
    return trust_token_issuance_policy_;
  }

  const ukm::SourceIdObj& ukm_source_id() const { return ukm_source_id_; }

  const net::CookieSettingOverrides& cookie_setting_overrides() const {
    return cookie_setting_overrides_;
  }

 private:
  // Private constructor - please go through the static For... methods.
  SubresourceLoaderFactoriesConfig() = default;

  url::Origin origin_;
  net::IsolationInfo isolation_info_;
  network::mojom::ClientSecurityStatePtr client_security_state_;
  mojo::Remote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter_;
  network::mojom::TrustTokenOperationPolicyVerdict trust_token_issuance_policy_;
  network::mojom::TrustTokenOperationPolicyVerdict
      trust_token_redemption_policy_;
  ukm::SourceIdObj ukm_source_id_;
  net::CookieSettingOverrides cookie_setting_overrides_;
};

struct PendingNavigation {
  blink::mojom::CommonNavigationParamsPtr common_params;
  blink::mojom::BeginNavigationParamsPtr begin_navigation_params;
  scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;
  mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client;
  mojo::PendingReceiver<mojom::NavigationRendererCancellationListener>
      renderer_cancellation_listener;

  PendingNavigation(
      blink::mojom::CommonNavigationParamsPtr common_params,
      blink::mojom::BeginNavigationParamsPtr begin_navigation_params,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client,
      mojo::PendingReceiver<mojom::NavigationRendererCancellationListener>
          renderer_cancellation_listener);
};

PendingNavigation::PendingNavigation(
    blink::mojom::CommonNavigationParamsPtr common_params,
    blink::mojom::BeginNavigationParamsPtr begin_navigation_params,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client,
    mojo::PendingReceiver<mojom::NavigationRendererCancellationListener>
        renderer_cancellation_listener)
    : common_params(std::move(common_params)),
      begin_navigation_params(std::move(begin_navigation_params)),
      blob_url_loader_factory(std::move(blob_url_loader_factory)),
      navigation_client(std::move(navigation_client)),
      renderer_cancellation_listener(
          std::move(renderer_cancellation_listener)) {}

// static
RenderFrameHost* RenderFrameHost::FromID(const GlobalRenderFrameHostId& id) {
  return RenderFrameHostImpl::FromID(id);
}

// static
RenderFrameHost* RenderFrameHost::FromID(int render_process_id,
                                         int render_frame_id) {
  return RenderFrameHostImpl::FromID(
      GlobalRenderFrameHostId(render_process_id, render_frame_id));
}

// static
RenderFrameHost* RenderFrameHost::FromFrameToken(
    int process_id,
    const blink::LocalFrameToken& token) {
  return RenderFrameHostImpl::FromFrameToken(process_id, token);
}

// static
void RenderFrameHost::AllowInjectingJavaScript() {
  g_allow_injecting_javascript = true;
}

// static
RenderFrameHostImpl* RenderFrameHostImpl::FromID(GlobalRenderFrameHostId id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RoutingIDFrameMap* frames = g_routing_id_frame_map.Pointer();
  auto it = frames->find(id);
  return it == frames->end() ? nullptr : it->second;
}

// static
RenderFrameHostImpl* RenderFrameHostImpl::FromID(int render_process_id,
                                                 int render_frame_id) {
  return RenderFrameHostImpl::FromID(
      GlobalRenderFrameHostId(render_process_id, render_frame_id));
}

// static
RenderFrameHostImpl* RenderFrameHostImpl::FromFrameToken(
    int process_id,
    const blink::LocalFrameToken& frame_token,
    mojo::ReportBadMessageCallback* process_mismatch_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = g_token_frame_map.Get().find(frame_token);
  if (it == g_token_frame_map.Get().end())
    return nullptr;

  if (it->second->GetProcess()->GetID() != process_id) {
    if (process_mismatch_callback) {
      SYSLOG(WARNING)
          << "Denying illegal RenderFrameHost::FromFrameToken request.";
      std::move(*process_mismatch_callback)
          .Run(
              "Unknown LocalFrame made RenderFrameHost::FromFrameToken "
              "request.");
    }
    return nullptr;
  }

  return it->second;
}

// static
RenderFrameHostImpl* RenderFrameHostImpl::FromDocumentToken(
    int process_id,
    const blink::DocumentToken& document_token,
    mojo::ReportBadMessageCallback* process_mismatch_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const auto it = GetDocumentTokenMap().find(document_token);
  if (it == GetDocumentTokenMap().end())
    return nullptr;

  if (it->second->GetProcess()->GetID() != process_id) {
    if (process_mismatch_callback) {
      SYSLOG(WARNING)
          << "Denying illegal RenderFrameHost::FromDocumentToken request.";
      std::move(*process_mismatch_callback)
          .Run("process ID does not match requested DocumentToken");
    }
    return nullptr;
  }

  return it->second;
}

// static
RenderFrameHost* RenderFrameHost::FromAXTreeID(const ui::AXTreeID& ax_tree_id) {
  return RenderFrameHostImpl::FromAXTreeID(ax_tree_id);
}

// static
RenderFrameHostImpl* RenderFrameHostImpl::FromAXTreeID(
    ui::AXTreeID ax_tree_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ui::AXActionHandlerRegistry::FrameID frame_id =
      ui::AXActionHandlerRegistry::GetInstance()->GetFrameID(ax_tree_id);
  return RenderFrameHostImpl::FromID(frame_id.first, frame_id.second);
}

// static
RenderFrameHostImpl* RenderFrameHostImpl::FromOverlayRoutingToken(
    const base::UnguessableToken& token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = g_token_frame_map.Get().find(blink::LocalFrameToken(token));
  return it == g_token_frame_map.Get().end() ? nullptr : it->second;
}

// static
void RenderFrameHostImpl::ClearAllPrefetchedSignedExchangeCache() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RoutingIDFrameMap* frames = g_routing_id_frame_map.Pointer();
  for (auto it : *frames)
    it.second->ClearPrefetchedSignedExchangeCache();
}

// TODO(crbug.com/1213818): Get/SetCodeCacheHostReceiverHandler are used only
// for a test in content/browser/service_worker/service_worker_browsertest
// that tests a bad message is returned on an incorrect origin. Try to find a
// way to test this without adding these additional methods.
RenderFrameHostImpl::CodeCacheHostReceiverHandler&
GetCodeCacheHostReceiverHandler() {
  static base::NoDestructor<RenderFrameHostImpl::CodeCacheHostReceiverHandler>
      instance;
  return *instance;
}

// static
void RenderFrameHostImpl::SetCodeCacheHostReceiverHandlerForTesting(
    CodeCacheHostReceiverHandler handler) {
  GetCodeCacheHostReceiverHandler() = handler;
}

// static
const char* RenderFrameHostImpl::LifecycleStateImplToString(
    RenderFrameHostImpl::LifecycleStateImpl state) {
  using LifecycleStateImpl = RenderFrameHostImpl::LifecycleStateImpl;
  switch (state) {
    case LifecycleStateImpl::kSpeculative:
      return "Speculative";
    case LifecycleStateImpl::kPrerendering:
      return "Prerendering";
    case LifecycleStateImpl::kPendingCommit:
      return "PendingCommit";
    case LifecycleStateImpl::kActive:
      return "Active";
    case LifecycleStateImpl::kInBackForwardCache:
      return "InBackForwardCache";
    case LifecycleStateImpl::kRunningUnloadHandlers:
      return "RunningUnloadHandlers";
    case LifecycleStateImpl::kReadyToBeDeleted:
      return "ReadyToBeDeleted";
  }
}

RenderFrameHostImpl::RenderFrameHostImpl(
    SiteInstance* site_instance,
    scoped_refptr<RenderViewHostImpl> render_view_host,
    RenderFrameHostDelegate* delegate,
    FrameTree* frame_tree,
    FrameTreeNode* frame_tree_node,
    int32_t routing_id,
    mojo::PendingAssociatedRemote<mojom::Frame> frame_remote,
    const blink::LocalFrameToken& frame_token,
    const blink::DocumentToken& document_token,
    base::UnguessableToken devtools_frame_token,
    bool renderer_initiated_creation_of_main_frame,
    LifecycleStateImpl lifecycle_state,
    scoped_refptr<BrowsingContextState> browsing_context_state,
    blink::FrameOwnerElementType frame_owner_element_type,
    RenderFrameHostImpl* parent,
    FencedFrameStatus fenced_frame_status)
    : render_view_host_(std::move(render_view_host)),
      delegate_(delegate),
      site_instance_(static_cast<SiteInstanceImpl*>(site_instance)),
      agent_scheduling_group_(
          site_instance_->GetOrCreateAgentSchedulingGroup().GetSafeRef()),
      frame_tree_(frame_tree),
      frame_tree_node_(frame_tree_node),
      owner_(frame_tree_node),
      browsing_context_state_(std::move(browsing_context_state)),
      frame_owner_element_type_(frame_owner_element_type),
      parent_(parent),
      depth_(parent_ ? parent_->GetFrameDepth() + 1 : 0),
      last_committed_url_derived_site_info_(
          site_instance_->GetBrowserContext()),
      routing_id_(routing_id),
      beforeunload_timeout_delay_(kUnloadTimeout),
      frame_(std::move(frame_remote)),
      waiting_for_init_(renderer_initiated_creation_of_main_frame),
      frame_token_(frame_token),
      keep_alive_handle_factory_(
          agent_scheduling_group_->GetProcess(),
          RenderProcessHostImpl::kKeepAliveHandleFactoryTimeout),
      subframe_unload_timeout_(kUnloadTimeout),
      media_device_id_salt_base_(CreateRandomMediaDeviceIDSalt()),
      document_associated_data_(absl::in_place, *this, document_token),
      lifecycle_state_(lifecycle_state),
      inner_tree_main_frame_tree_node_id_(
          FrameTreeNode::kFrameTreeNodeInvalidId),
      code_cache_host_receivers_(
          GetProcess()->GetStoragePartition()->GetGeneratedCodeCacheContext()),
      fenced_frame_status_(fenced_frame_status),
      devtools_frame_token_(devtools_frame_token) {
  TRACE_EVENT_BEGIN("navigation", "RenderFrameHostImpl",
                    perfetto::Track::FromPointer(this),
                    "render_frame_host_when_created", this);
  // Update lifecycle state on track of RenderFrameHostImpl.
  TRACE_EVENT_BEGIN(
      "navigation",
      perfetto::StaticString{LifecycleStateImplToString(lifecycle_state_)},
      perfetto::Track::FromPointer(this));

  DCHECK_NE(routing_id_, MSG_ROUTING_NONE);
  DCHECK(delegate_);
  DCHECK(lifecycle_state_ == LifecycleStateImpl::kSpeculative ||
         lifecycle_state_ == LifecycleStateImpl::kPrerendering ||
         lifecycle_state_ == LifecycleStateImpl::kActive);
  // Only main frames have `waiting_for_init_` set.
  DCHECK(!waiting_for_init_ || !parent_);

  GetAgentSchedulingGroup().AddRoute(routing_id_, this);
  g_routing_id_frame_map.Get().emplace(
      GlobalRenderFrameHostId(GetProcess()->GetID(), routing_id_), this);
  g_token_frame_map.Get().insert(std::make_pair(frame_token_, this));
  site_instance_->group()->AddObserver(this);
  auto* process = GetProcess();
  process->RegisterRenderFrameHost(GetGlobalId());
  GetSiteInstance()->group()->IncrementActiveFrameCount();

  if (parent_) {
    // All frames in a frame tree should use the same storage partition.
    CHECK_EQ(parent_->GetStoragePartition(), GetStoragePartition());

    // New child frames should inherit the nav_entry_id of their parent.
    set_nav_entry_id(parent_->nav_entry_id());
  }

  if (frame_tree_->is_prerendering()) {
    // TODO(https://crbug.com/1132752): Check the prerendering page is
    // same-origin to the prerender trigger page.
    mojo_binder_policy_applier_ =
        MojoBinderPolicyApplier::CreateForSameOriginPrerendering(base::BindOnce(
            &RenderFrameHostImpl::CancelPrerenderingByMojoBinderPolicy,
            base::Unretained(this)));
    broker_.ApplyMojoBinderPolicies(mojo_binder_policy_applier_.get());
  } else if (frame_tree_->page_delegate()->IsInPreviewMode()) {
    // TODO(b:299240273): Relax the policy on the tab promotion.
    mojo_binder_policy_applier_ = MojoBinderPolicyApplier::CreateForPreview(
        base::BindOnce(&RenderFrameHostImpl::CancelPreviewByMojoBinderPolicy,
                       base::Unretained(this)));
    broker_.ApplyMojoBinderPolicies(mojo_binder_policy_applier_.get());
  }

  if (lifecycle_state_ != LifecycleStateImpl::kSpeculative) {
    // Creating a RFH in kActive state implies that it is the RFH for a
    // newly-created FTN, which should still be on its initial empty document.
    DCHECK(frame_tree_node_->is_on_initial_empty_document());
  }

  InitializePolicyContainerHost(renderer_initiated_creation_of_main_frame);

  InitializePrivateNetworkRequestPolicy();

  unload_event_monitor_timeout_ = std::make_unique<TimeoutMonitor>(
      base::BindRepeating(&RenderFrameHostImpl::OnNavigationUnloadTimeout,
                          weak_ptr_factory_.GetWeakPtr()));
  beforeunload_timeout_ = std::make_unique<TimeoutMonitor>(
      base::BindRepeating(&RenderFrameHostImpl::BeforeUnloadTimeout,
                          weak_ptr_factory_.GetWeakPtr()));

  // Local roots are:
  // - main frames; or
  // - subframes that use a proxy to talk to their parent.
  //
  // Local roots require a RenderWidget for input/layout/painting.
  // Note: We cannot use is_local_root() here because this block sets up the
  // fields that are used by that method.
  const bool setup_local_render_widget_host =
      is_main_frame() || RequiresProxyToParent();
  if (setup_local_render_widget_host) {
    if (is_main_frame()) {
      // For main frames, the RenderWidgetHost is owned by the RenderViewHost.
      // TODO(https://crbug.com/545684): Once RenderViewHostImpl has-a
      // RenderWidgetHostImpl, the main render frame should probably start
      // owning the RenderWidgetHostImpl itself.
      DCHECK(GetLocalRenderWidgetHost());
    } else {
      // For local child roots, the RenderFrameHost directly creates and owns
      // its RenderWidgetHost.
      int32_t widget_routing_id =
          site_instance->GetProcess()->GetNextRoutingID();
      DCHECK_EQ(nullptr, GetLocalRenderWidgetHost());

      auto* previous_rfh =
          lifecycle_state_ == LifecycleStateImpl::kSpeculative &&
                  frame_tree_node_->current_frame_host()
                      ->ShouldReuseCompositing(*GetSiteInstance())
              ? frame_tree_node_->current_frame_host()
              : nullptr;
      auto frame_sink_id =
          previous_rfh
              ? previous_rfh->GetLocalRenderWidgetHost()->GetFrameSinkId()
              : RenderWidgetHostImpl::DefaultFrameSinkId(
                    *site_instance_->group(), widget_routing_id);
      owned_render_widget_host_ = RenderWidgetHostFactory::Create(
          frame_tree_, frame_tree_->render_widget_delegate(), frame_sink_id,
          site_instance_->group()->GetSafeRef(), widget_routing_id,
          /*hidden=*/true,
          /*renderer_initiated_creation=*/false);
      owned_render_widget_host_->SetViewIsFrameSinkIdOwner(!previous_rfh);
    }

    if (is_main_frame())
      GetLocalRenderWidgetHost()->SetIntersectsViewport(true);
    GetLocalRenderWidgetHost()->SetFrameDepth(depth_);
  }
  // Verify is_local_root() now indicates whether this frame is a local root or
  // not. It is safe to use this method anywhere beyond this point.
  DCHECK_EQ(setup_local_render_widget_host, is_local_root());
  ResetPermissionsPolicy();

  // New RenderFrameHostImpl are put in their own virtual browsing context
  // group. Then, they can inherit from:
  // 1) Their opener in RenderFrameHostImpl::CreateNewWindow().
  // 2) Their navigation in RenderFrameHostImpl::DidCommitNavigationInternal().
  virtual_browsing_context_group_ = CrossOriginOpenerPolicyAccessReportManager::
      GetNewVirtualBrowsingContextGroup();
  soap_by_default_virtual_browsing_context_group_ =
      CrossOriginOpenerPolicyAccessReportManager::
          GetNewVirtualBrowsingContextGroup();

  // IdleManager should be unique per RenderFrame to provide proper isolation
  // of overrides.
  idle_manager_ = std::make_unique<IdleManagerImpl>(this);
}

RenderFrameHostImpl::~RenderFrameHostImpl() {
  SCOPED_CRASH_KEY_STRING256("Bug1407526", "lifecycle",
                             LifecycleStateImplToString(lifecycle_state()));
  TRACE_EVENT("navigation", "~RenderFrameHostImpl()",
              ChromeTrackEvent::kRenderFrameHost, this);
  // See https://crbug.com/1276535
  if (check_deletion_for_bug_1276535_) {
    base::debug::DumpWithoutCrashing();
  }

  // The lifetime of this object has ended, so remove it from the id map before
  // calling any delegates/observers, so that any calls to |FromID| no longer
  // return |this|.
  g_routing_id_frame_map.Get().erase(
      GlobalRenderFrameHostId(GetProcess()->GetID(), routing_id_));

  // Remove this object from the isolatable sandboxed iframe set as well, if
  // necessary.
  g_routing_id_isolatable_sandboxed_iframes_set.Get().erase(GetGlobalId());

  // When a RenderFrameHostImpl is deleted, it may still contain children. This
  // can happen with the unload timer. It causes a RenderFrameHost to delete
  // itself even if it is still waiting for its children to complete their
  // unload handlers.
  //
  // Observers expect children to be deleted first. Do it now before notifying
  // them.
  ResetChildren();

  // Destroying NavigationRequests may call into delegates/observers,
  // so we do it early while |this| object is still in a sane state.
  ResetOwnedNavigationRequests(
      NavigationDiscardReason::kRenderFrameHostDestruction);

  // Cancel the navigations (including the ones that are not owned by this
  // RenderFrameHost) that intends to commit in this RenderFrameHost, as they
  // can no longer do so.
  {
    CHECK(frame_tree_node_);
    NavigationRequest* navigation_request =
        frame_tree_node_->navigation_request();
    if (navigation_request) {
      if (navigation_request
              ->GetRenderFrameHostRestoredFromBackForwardCache() == this) {
        CHECK(navigation_request->IsServedFromBackForwardCache());
        frame_tree_node_->RestartBackForwardCachedNavigationAsync(
            navigation_request->nav_entry_id());
      } else if (navigation_request->state() >=
                     NavigationRequest::WILL_PROCESS_RESPONSE &&
                 navigation_request->GetRenderFrameHost() == this) {
        // As we are unable to come up with a case that will lead to this path,
        // we instead record the dumps for debugging the scenario.
        // TODO(crbug.com/1430653): if we verify that this path is impossible,
        // replace the `DumpWithoutCrashing` with a `CHECK`. Otherwise, add a
        // new browser test for it.
        base::debug::DumpWithoutCrashing();
        NOTREACHED();
      }
    }
  }

  // Release the WebUI instances before all else as the WebUI may accesses the
  // RenderFrameHost during cleanup.
  base::WeakPtr<RenderFrameHostImpl> self = GetWeakPtr();
  ClearWebUI();
  // `ClearWebUI()` may indirectly call content's embedders and delete this.
  // There are no known occurrences of it, so we assume this never happen and
  // crash immediately if it does, because there are no easy ways to recover.
  CHECK(self);

  SetLastCommittedSiteInfo(UrlInfo());

  g_token_frame_map.Get().erase(frame_token_);

  auto* process = GetProcess();
  SCOPED_CRASH_KEY_BOOL("Bug1407526", "si_exists", !!site_instance_);
  SCOPED_CRASH_KEY_BOOL("Bug1407526", "sig_exists", !!site_instance_->group());
  SCOPED_CRASH_KEY_BOOL("Bug1407526", "process_exists", !!process);
  site_instance_->group()->RemoveObserver(this);
  process->UnregisterRenderFrameHost(GetGlobalId());

  const bool was_created = is_render_frame_created();
  SCOPED_CRASH_KEY_BOOL("Bug1407526", "was_created", !!was_created);
  SCOPED_CRASH_KEY_BOOL("Bug1407526", "delegate_exists", !!delegate_);
  render_frame_state_ = RenderFrameState::kDeleted;
  if (was_created)
    delegate_->RenderFrameDeleted(this);

  // Ensure that the render process host has been notified that all audio
  // streams from this frame have terminated. This is required to ensure the
  // process host has the correct media stream count, which affects its
  // background priority.
  if (is_audible_) {
    OnAudibleStateChanged(false);
  }

  // Resetting `document_associated_data_` destroys live `DocumentService` and
  // `DocumentUserData` instances. It is important for them to be
  // destroyed before the body of the `RenderFrameHostImpl` destructor
  // completes. Among other things, this ensures that any `SafeRef`s from
  // `DocumentService` and `RenderFrameHostUserData` subclasses are still valid
  // when their destructors run.
  document_associated_data_.reset();

  // If this was the last active frame in the SiteInstanceGroup, the
  // DecrementActiveFrameCount call will trigger the deletion of the
  // SiteInstanceGroup's proxies.
  GetSiteInstance()->group()->DecrementActiveFrameCount();

  // Once a RenderFrame is created in the renderer, there are three possible
  // clean-up paths:
  // 1. The RenderFrame can be the main frame. In this case, closing the
  //    associated `blink::WebView` will clean up the resources associated with
  //    the main RenderFrame.
  // 2. The RenderFrame can be unloaded. In this case, the browser sends a
  //    mojom::FrameNavigationControl::UnloadFrame message for the RenderFrame
  //    to replace itself with a `blink::RemoteFrame`and release its associated
  //    resources. |lifecycle_state_| is advanced to
  //    LifecycleStateImpl::kRunningUnloadHandlers to track that this IPC is in
  //    flight.
  // 3. The RenderFrame can be detached, as part of removing a subtree (due to
  //    navigation, unload, or DOM mutation). In this case, the browser sends
  //    a mojom::FrameNavigationControl::Delete message for the RenderFrame
  //    to detach itself and release its associated resources. If the subframe
  //    contains an unload handler, |lifecycle_state_| is advanced to
  //    LifecycleStateImpl::kRunningUnloadHandlers to track that the detach is
  //    in progress; otherwise, it is advanced directly to
  //    LifecycleStateImpl::kReadyToBeDeleted.
  //
  // For BackForwardCache or Prerender case:
  //
  // Deleting the BackForwardCache::Entry deletes immediately all the
  // Render{View,Frame,FrameProxy}Host. This will destroy the main RenderFrame
  // eventually as part of path #1 above:
  //
  // - The RenderFrameHost/RenderFrameProxyHost of the main frame are owned by
  //   the BackForwardCache::Entry.
  // - RenderFrameHost/RenderFrameProxyHost for sub-frames are owned by their
  //   parent RenderFrameHost.
  // - The RenderViewHost(s) are refcounted by the
  //   RenderFrameHost/RenderFrameProxyHost of the page. They are guaranteed not
  //   to be referenced by any other pages.
  //
  // The browser side gives the renderer a small timeout to finish processing
  // unload / detach messages. When the timeout expires, the RFH will be
  // removed regardless of whether or not the renderer acknowledged that it
  // completed the work, to avoid indefinitely leaking browser-side state. To
  // avoid leaks, ~RenderFrameHostImpl still validates that the appropriate
  // cleanup IPC was sent to the renderer, by checking IsPendingDeletion().
  //
  // TODO(dcheng): Due to how frame detach is signalled today, there are some
  // bugs in this area. In particular, subtree detach is reported from the
  // bottom up, so the replicated mojom::FrameNavigationControl::Delete
  // messages actually operate on a node-by-node basis rather than detaching an
  // entire subtree at once...
  //
  // Note that this logic is fairly subtle. It needs to include all subframes
  // and all speculative frames, but it should exclude case #1 (a main
  // RenderFrame owned by the `blink::WebView`). It can't simply check
  // |frame_tree_node_->render_manager()->speculative_frame_host()| for
  // equality against |this|. The speculative frame host is unset before the
  // speculative frame host is destroyed, so this condition would never be
  // matched for a speculative RFH that needs to be destroyed.
  //
  // Note that `RenderViewHostImpl::GetMainRenderFrameHost()` can never return
  // a speculative RFH.  So, a speculative main frame being deleted will always
  // pass this condition as well.
  if (was_created && render_view_host_->GetMainRenderFrameHost() != this) {
    CHECK_NE(lifecycle_state(), LifecycleStateImpl::kActive);
  }

  GetAgentSchedulingGroup().RemoveRoute(routing_id_);

  // Null out the unload timer; in crash dumps this member will be null only if
  // the dtor has run.  (It may also be null in tests.)
  unload_event_monitor_timeout_.reset();

  // Delete this before destroying the widget, to guard against reentrancy
  // by in-process screen readers such as JAWS.
  browser_accessibility_manager_.reset();

  // Note: The RenderWidgetHost of the main frame is owned by the RenderViewHost
  // instead. In this case the RenderViewHost is responsible for shutting down
  // its RenderViewHost.
  if (owned_render_widget_host_)
    owned_render_widget_host_->ShutdownAndDestroyWidget(false);

  ResourceCacheManager* resource_cache_manager =
      GetStoragePartition()->GetResourceCacheManager();
  if (resource_cache_manager) {
    resource_cache_manager->RenderFrameHostBecameIneligible(*this);
  }

  render_view_host_.reset();

  // If another frame is waiting for a beforeunload completion callback from
  // this frame, simulate it now.
  RenderFrameHostImpl* beforeunload_initiator = GetBeforeUnloadInitiator();
  if (beforeunload_initiator && beforeunload_initiator != this) {
    base::TimeTicks approx_renderer_start_time = send_before_unload_start_time_;
    beforeunload_initiator->ProcessBeforeUnloadCompletedFromFrame(
        /*proceed=*/true, /*treat_as_final_completion_callback=*/false, this,
        /*is_frame_being_destroyed=*/true, approx_renderer_start_time,
        base::TimeTicks::Now(), /*for_legacy=*/false);
  }

  if (prefetched_signed_exchange_cache_)
    prefetched_signed_exchange_cache_->RecordHistograms();

  // Matches the pair of TRACE_EVENT_BEGINS in the constructor: one for
  // "RenderFrameHostImpl" slice itself, one for the slice with the lifecycle
  // state name.
  TRACE_EVENT_END("navigation", perfetto::Track::FromPointer(this));
  TRACE_EVENT_END("navigation", perfetto::Track::FromPointer(this));
}

const blink::StorageKey& RenderFrameHostImpl::GetStorageKey() const {
  return storage_key_;
}

int RenderFrameHostImpl::GetRoutingID() const {
  return routing_id_;
}

const blink::LocalFrameToken& RenderFrameHostImpl::GetFrameToken() const {
  return frame_token_;
}

const base::UnguessableToken& RenderFrameHostImpl::GetReportingSource() {
  DCHECK(!document_associated_data_->reporting_source().is_empty());
  return document_associated_data_->reporting_source();
}

ui::AXTreeID RenderFrameHostImpl::GetAXTreeID() {
  return ax_tree_id();
}

const blink::LocalFrameToken& RenderFrameHostImpl::GetTopFrameToken() {
  RenderFrameHostImpl* frame = this;
  while (frame->parent_) {
    frame = frame->parent_;
  }
  return frame->GetFrameToken();
}

void RenderFrameHostImpl::AudioContextPlaybackStarted(int audio_context_id) {
  delegate_->AudioContextPlaybackStarted(this, audio_context_id);
}

void RenderFrameHostImpl::AudioContextPlaybackStopped(int audio_context_id) {
  delegate_->AudioContextPlaybackStopped(this, audio_context_id);
}

// The current frame went into the BackForwardCache.
void RenderFrameHostImpl::DidEnterBackForwardCache() {
  TRACE_EVENT0("navigation", "RenderFrameHostImpl::EnterBackForwardCache");
  DCHECK(IsBackForwardCacheEnabled());
  DCHECK(IsInPrimaryMainFrame());

  if (base::FeatureList::IsEnabled(
          features::kInvalidateLocalSurfaceIdPreCommit)) {
    // Notifies the View that the page is stored in the `BackForwardCache`.
    //
    // We shouldn't BFCache a renderer without a View.
    CHECK(GetView());
    static_cast<RenderWidgetHostViewBase*>(GetView())
        ->DidEnterBackForwardCache();
  }

  DidEnterBackForwardCacheInternal();
  // Pages in the back-forward cache are automatically evicted after a certain
  // time.
  StartBackForwardCacheEvictionTimer();

  for (FrameTreeNode* node : FrameTree::SubtreeAndInnerTreeNodes(
           this,
           /*include_delegate_nodes_for_inner_frame_trees=*/true)) {
    if (RenderFrameHostImpl* rfh = node->current_frame_host())
      rfh->DidEnterBackForwardCacheInternal();
  }
}

void RenderFrameHostImpl::DidEnterBackForwardCacheInternal() {
  DCHECK_EQ(lifecycle_state(), LifecycleStateImpl::kActive);
  SetLifecycleState(LifecycleStateImpl::kInBackForwardCache);

  // If we are a delegate node we don't need to do anything else.
  if (inner_tree_main_frame_tree_node_id_ !=
      FrameTreeNode::kFrameTreeNodeInvalidId) {
    return;
  }

  for (auto& entry : service_worker_container_hosts_) {
    if (base::WeakPtr<ServiceWorkerContainerHost> host = entry.second)
      host->OnEnterBackForwardCache();
  }

  DedicatedWorkerHostsForDocument::GetOrCreateForCurrentDocument(this)
      ->OnEnterBackForwardCache();
#if BUILDFLAG(IS_P2P_ENABLED)
  GetProcess()->PauseSocketManagerForRenderFrameHost(GetGlobalId());
#endif  // BUILDFLAG(IS_P2P_ENABLED)

  if (auto* permission_service_context =
          PermissionServiceContext::GetForCurrentDocument(this)) {
    permission_service_context->StoreStatusAtBFCacheEntry();
  }
}

// The frame as been restored from the BackForwardCache.
void RenderFrameHostImpl::WillLeaveBackForwardCache() {
  TRACE_EVENT0("navigation", "RenderFrameHostImpl::LeaveBackForwardCache");
  DCHECK(IsBackForwardCacheEnabled());
  DCHECK(IsOutermostMainFrame());
  if (back_forward_cache_eviction_timer_.IsRunning())
    back_forward_cache_eviction_timer_.Stop();

  WillLeaveBackForwardCacheInternal();
  for (FrameTreeNode* node : FrameTree::SubtreeAndInnerTreeNodes(
           this,
           /*include_delegate_nodes_for_inner_frame_trees=*/true)) {
    if (RenderFrameHostImpl* rfh = node->current_frame_host())
      rfh->WillLeaveBackForwardCacheInternal();
  }
}

void RenderFrameHostImpl::WillLeaveBackForwardCacheInternal() {
  DCHECK_EQ(lifecycle_state(), LifecycleStateImpl::kInBackForwardCache);
  DCHECK(!back_forward_cache_eviction_timer_.IsRunning());

  // If we are a delegate node we don't need to do anything else.
  if (inner_tree_main_frame_tree_node_id_ !=
      FrameTreeNode::kFrameTreeNodeInvalidId) {
    return;
  }

  for (auto& entry : service_worker_container_hosts_) {
    if (base::WeakPtr<ServiceWorkerContainerHost> host = entry.second)
      host->OnRestoreFromBackForwardCache();
  }

  DedicatedWorkerHostsForDocument::GetOrCreateForCurrentDocument(this)
      ->OnRestoreFromBackForwardCache();
#if BUILDFLAG(IS_P2P_ENABLED)
  GetProcess()->ResumeSocketManagerForRenderFrameHost(GetGlobalId());
#endif  // BUILDFLAG(IS_P2P_ENABLED)
}

mojom::DidCommitProvisionalLoadParamsPtr
RenderFrameHostImpl::TakeLastCommitParams() {
  return std::move(last_commit_params_);
}

void RenderFrameHostImpl::StartBackForwardCacheEvictionTimer() {
  DCHECK(IsInBackForwardCache());
  base::TimeDelta evict_after =
      BackForwardCacheImpl::GetTimeToLiveInBackForwardCache(
          LoadedWithCacheControlNoStoreHeader()
              ? BackForwardCacheImpl::kInCCNSContext
              : BackForwardCacheImpl::kNotInCCNSContext);

  back_forward_cache_eviction_timer_.SetTaskRunner(
      GetBackForwardCache().GetTaskRunner());

  back_forward_cache_eviction_timer_.Start(
      FROM_HERE, evict_after,
      base::BindOnce(&RenderFrameHostImpl::EvictFromBackForwardCacheWithReason,
                     weak_ptr_factory_.GetWeakPtr(),
                     BackForwardCacheMetrics::NotRestoredReason::kTimeout));
}

void RenderFrameHostImpl::DisableBackForwardCache(
    BackForwardCache::DisabledReason reason,
    absl::optional<ukm::SourceId> source_id) {
  back_forward_cache_disabled_reasons_[reason].insert(source_id);
  MaybeEvictFromBackForwardCache();
}

void RenderFrameHostImpl::DisableProactiveBrowsingInstanceSwapForTesting() {
  // This should only be called on primary main frames.
  DCHECK(IsInPrimaryMainFrame());
  has_test_disabled_proactive_browsing_instance_swap_ = true;
}

void RenderFrameHostImpl::OnGrantedMediaStreamAccess() {
  was_granted_media_access_ = true;
  MaybeEvictFromBackForwardCache();
}

void RenderFrameHostImpl::OnPortalActivated(
    std::unique_ptr<Portal> predecessor,
    mojo::PendingAssociatedRemote<blink::mojom::Portal> pending_portal,
    mojo::PendingAssociatedReceiver<blink::mojom::PortalClient> client_receiver,
    blink::TransferableMessage data,
    uint64_t trace_id,
    base::OnceCallback<void(blink::mojom::PortalActivateResult)> callback) {
  auto it = portals_.insert(std::move(predecessor)).first;

  TRACE_EVENT_WITH_FLOW0("navigation", "RenderFrameHostImpl::OnPortalActivated",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  GetAssociatedLocalMainFrame()->OnPortalActivated(
      (*it)->portal_token(), std::move(pending_portal),
      std::move(client_receiver), std::move(data), trace_id,
      base::BindOnce(
          [](base::OnceCallback<void(blink::mojom::PortalActivateResult)>
                 callback,
             blink::mojom::PortalActivateResult result) {
            switch (result) {
              case blink::mojom::PortalActivateResult::kPredecessorWillUnload:
              case blink::mojom::PortalActivateResult::kPredecessorWasAdopted:
                // These values are acceptable from the renderer.
                break;
              case blink::mojom::PortalActivateResult::
                  kRejectedDueToPredecessorNavigation:
              case blink::mojom::PortalActivateResult::
                  kRejectedDueToPortalNotReady:
              case blink::mojom::PortalActivateResult::
                  kRejectedDueToErrorInPortal:
              case blink::mojom::PortalActivateResult::kDisconnected:
              case blink::mojom::PortalActivateResult::kNotImplemented:
              case blink::mojom::PortalActivateResult::kAbortedDueToBug:
                // The renderer is misbehaving.
                mojo::ReportBadMessage(
                    "Unexpected PortalActivateResult from renderer");
                result = blink::mojom::PortalActivateResult::kAbortedDueToBug;
                break;
            }
            std::move(callback).Run(result);
          },
          std::move(callback)));

  if (browser_accessibility_manager_) {
    browser_accessibility_manager_->DidActivatePortal();
  }
}

void RenderFrameHostImpl::OnPortalCreatedForTesting(
    std::unique_ptr<Portal> portal) {
  portals_.insert(std::move(portal));
}

Portal* RenderFrameHostImpl::FindPortalByToken(
    const blink::PortalToken& portal_token) {
  auto it = base::ranges::find(portals_, portal_token, &Portal::portal_token);
  return it == portals_.end() ? nullptr : it->get();
}

std::vector<Portal*> RenderFrameHostImpl::GetPortals() const {
  std::vector<Portal*> result;
  for (const auto& portal : portals_)
    result.push_back(portal.get());
  return result;
}

void RenderFrameHostImpl::DestroyPortal(Portal* portal) {
  auto it = portals_.find(portal);
  CHECK(it != portals_.end());
  std::unique_ptr<Portal> owned_portal(std::move(*it));
  portals_.erase(it);
  // |owned_portal| is deleted as it goes out of scope.
}

void RenderFrameHostImpl::ForwardMessageFromHost(
    blink::TransferableMessage message,
    const url::Origin& source_origin) {
  DCHECK_EQ(source_origin, GetLastCommittedOrigin());
  GetAssociatedLocalMainFrame()->ForwardMessageFromHost(std::move(message),
                                                        source_origin);
}

SiteInstanceImpl* RenderFrameHostImpl::GetSiteInstance() const {
  return site_instance_.get();
}

RenderProcessHost* RenderFrameHostImpl::GetProcess() const {
  return agent_scheduling_group_->GetProcess();
}

AgentSchedulingGroupHost& RenderFrameHostImpl::GetAgentSchedulingGroup() {
  return *agent_scheduling_group_;
}

RenderFrameHostImpl* RenderFrameHostImpl::GetParent() const {
  return parent_;
}

PageImpl& RenderFrameHostImpl::GetPage() {
  return *GetMainFrame()->document_associated_data_->owned_page();
}

const PageImpl& RenderFrameHostImpl::GetPage() const {
  return *GetMainFrame()->document_associated_data_->owned_page();
}

bool RenderFrameHostImpl::IsDescendantOfWithinFrameTree(
    RenderFrameHostImpl* ancestor) {
  if (!ancestor || !ancestor->child_count())
    return false;

  for (RenderFrameHostImpl* current = GetParent(); current;
       current = current->GetParent()) {
    if (current == ancestor)
      return true;
  }
  return false;
}

bool RenderFrameHostImpl::IsFencedFrameRoot() const {
  return fenced_frame_status_ == FencedFrameStatus::kFencedFrameRoot;
}

bool RenderFrameHostImpl::IsNestedWithinFencedFrame() const {
  switch (fenced_frame_status_) {
    case FencedFrameStatus::kNotNestedInFencedFrame:
      return false;
    case FencedFrameStatus::kFencedFrameRoot:
      return true;
    case FencedFrameStatus::kIframeNestedWithinFencedFrame:
      return true;
  }
}

void RenderFrameHostImpl::ForEachRenderFrameHostWithAction(
    base::FunctionRef<FrameIterationAction(RenderFrameHost*)> on_frame) {
  ForEachRenderFrameHostWithAction(
      [on_frame](RenderFrameHostImpl* rfh) { return on_frame(rfh); });
}

void RenderFrameHostImpl::ForEachRenderFrameHost(
    base::FunctionRef<void(RenderFrameHost*)> on_frame) {
  ForEachRenderFrameHost(
      [on_frame](RenderFrameHostImpl* rfh) { on_frame(rfh); });
}

void RenderFrameHostImpl::ForEachRenderFrameHostWithAction(
    base::FunctionRef<FrameIterationAction(RenderFrameHostImpl*)> on_frame) {
  ForEachRenderFrameHostImpl(on_frame, /*include_speculative=*/false);
}

void RenderFrameHostImpl::ForEachRenderFrameHost(
    base::FunctionRef<void(RenderFrameHostImpl*)> on_frame) {
  ForEachRenderFrameHostWithAction([on_frame](RenderFrameHostImpl* rfh) {
    on_frame(rfh);
    return FrameIterationAction::kContinue;
  });
}

void RenderFrameHostImpl::ForEachRenderFrameHostIncludingSpeculativeWithAction(
    base::FunctionRef<FrameIterationAction(RenderFrameHostImpl*)> on_frame) {
  ForEachRenderFrameHostImpl(on_frame, /*include_speculative=*/true);
}

void RenderFrameHostImpl::ForEachRenderFrameHostIncludingSpeculative(
    base::FunctionRef<void(RenderFrameHostImpl*)> on_frame) {
  ForEachRenderFrameHostIncludingSpeculativeWithAction(
      [on_frame](RenderFrameHostImpl* rfh) {
        on_frame(rfh);
        return FrameIterationAction::kContinue;
      });
}

void RenderFrameHostImpl::ForEachRenderFrameHostImpl(
    base::FunctionRef<FrameIterationAction(RenderFrameHostImpl*)> on_frame,
    bool include_speculative) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!include_speculative &&
      (lifecycle_state() == LifecycleStateImpl::kSpeculative ||
       lifecycle_state() == LifecycleStateImpl::kPendingCommit)) {
    return;
  }

  // Since |this| may not be current in its FrameTree, we can't begin iterating
  // from |frame_tree_node()|, so we special case the first invocation for
  // |this| and then actually start iterating over the subtree starting with
  // our children's FrameTreeNodes.
  bool skip_children_of_starting_frame = false;
  switch (on_frame(this)) {
    case FrameIterationAction::kContinue:
      break;
    case FrameIterationAction::kSkipChildren:
      skip_children_of_starting_frame = true;
      break;
    case FrameIterationAction::kStop:
      return;
  }

  // Potentially include our FrameTreeNode's speculative RenderFrameHost, but
  // only if |this| is current in its FrameTree.
  // TODO(1264145): Avoid having a RenderFrameHost access its FrameTreeNode's
  // speculative RenderFrameHost by moving
  // ForEachRenderFrameHostIncludingSpeculative from RenderFrameHostImpl or
  // possibly removing it entirely.
  if (include_speculative && frame_tree_node()->current_frame_host() == this) {
    RenderFrameHostImpl* speculative_frame_host =
        frame_tree_node()->render_manager()->speculative_frame_host();
    if (speculative_frame_host) {
      DCHECK_EQ(speculative_frame_host->child_count(), 0U);
      switch (on_frame(speculative_frame_host)) {
        case FrameIterationAction::kContinue:
        case FrameIterationAction::kSkipChildren:
          break;
        case FrameIterationAction::kStop:
          return;
      }
    }
  }

  if (skip_children_of_starting_frame)
    return;

  FrameTree::NodeRange ftn_range = FrameTree::SubtreeAndInnerTreeNodes(this);
  FrameTree::NodeIterator it = ftn_range.begin();
  const FrameTree::NodeIterator end = ftn_range.end();

  while (it != end) {
    FrameTreeNode* node = *it;
    RenderFrameHostImpl* frame_host = node->current_frame_host();
    if (frame_host) {
      switch (on_frame(frame_host)) {
        case FrameIterationAction::kContinue:
          ++it;
          break;
        case FrameIterationAction::kSkipChildren:
          it.AdvanceSkippingChildren();
          break;
        case FrameIterationAction::kStop:
          return;
      }
    }

    if (include_speculative) {
      RenderFrameHostImpl* speculative_frame_host =
          node->render_manager()->speculative_frame_host();
      if (speculative_frame_host) {
        DCHECK_EQ(speculative_frame_host->child_count(), 0U);
        switch (on_frame(speculative_frame_host)) {
          case FrameIterationAction::kContinue:
          case FrameIterationAction::kSkipChildren:
            break;
          case FrameIterationAction::kStop:
            return;
        }
      }
    }
  }
}

int RenderFrameHostImpl::GetFrameTreeNodeId() const {
  return frame_tree_node_->frame_tree_node_id();
}

const base::UnguessableToken& RenderFrameHostImpl::GetDevToolsFrameToken() {
  return devtools_frame_token();
}

absl::optional<base::UnguessableToken>
RenderFrameHostImpl::GetEmbeddingToken() {
  return embedding_token_;
}

const std::string& RenderFrameHostImpl::GetFrameName() {
  return browsing_context_state_->frame_name();
}

bool RenderFrameHostImpl::IsFrameDisplayNone() {
  return frame_tree_node()->frame_owner_properties().is_display_none;
}

const absl::optional<gfx::Size>& RenderFrameHostImpl::GetFrameSize() {
  return frame_size_;
}

size_t RenderFrameHostImpl::GetFrameDepth() {
  return depth_;
}

bool RenderFrameHostImpl::IsCrossProcessSubframe() {
  if (is_main_frame() || GetSiteInstance() == parent_->GetSiteInstance())
    return false;
  return GetSiteInstance()->GetProcess() !=
         parent_->GetSiteInstance()->GetProcess();
}

bool RenderFrameHostImpl::RequiresProxyToParent() {
  if (is_main_frame())
    return false;
  return GetSiteInstance() != parent_->GetSiteInstance();
}

WebExposedIsolationLevel RenderFrameHostImpl::GetWebExposedIsolationLevel() {
  DCHECK_EQ(GetSiteInstance()->GetSiteInfo().web_exposed_isolation_info(),
            GetProcess()->GetProcessLock().GetWebExposedIsolationInfo());
  if (!IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kCrossOriginIsolated)) {
    return WebExposedIsolationLevel::kNotIsolated;
  }

  WebExposedIsolationLevel weil = GetProcess()->GetWebExposedIsolationLevel();
  if (weil >= WebExposedIsolationLevel::kMaybeIsolatedApplication) {
    return WebExposedIsolationLevel::kIsolatedApplication;
  }
  if (weil >= WebExposedIsolationLevel::kMaybeIsolated) {
    return WebExposedIsolationLevel::kIsolated;
  }
  return WebExposedIsolationLevel::kNotIsolated;
}

const GURL& RenderFrameHostImpl::GetLastCommittedURL() const {
  return last_committed_url_;
}

const url::Origin& RenderFrameHostImpl::GetLastCommittedOrigin() const {
  return last_committed_origin_;
}

const GURL& RenderFrameHostImpl::GetInheritedBaseUrl() const {
  return inherited_base_url_;
}

const net::NetworkIsolationKey& RenderFrameHostImpl::GetNetworkIsolationKey() {
  DCHECK(!isolation_info_.IsEmpty());
  return isolation_info_.network_isolation_key();
}

const net::IsolationInfo&
RenderFrameHostImpl::GetIsolationInfoForSubresources() {
  DCHECK(!isolation_info_.IsEmpty());
  return isolation_info_;
}

net::IsolationInfo
RenderFrameHostImpl::GetPendingIsolationInfoForSubresources() {
  // TODO(https://crbug.com/1211126): Figure out if
  // ForPendingOrLastCommittedNavigation is correct below (it might not be).
  auto config =
      SubresourceLoaderFactoriesConfig::ForPendingOrLastCommittedNavigation(
          *this);
  DCHECK(!config.isolation_info().IsEmpty());
  return config.isolation_info();
}

void RenderFrameHostImpl::GetCanonicalUrl(
    base::OnceCallback<void(const absl::optional<GURL>&)> callback) {
  if (IsRenderFrameLive()) {
    // Validate that the URL returned by the renderer is HTTP(S) only. It is
    // allowed to be cross-origin.
    auto validate_and_forward =
        [](base::OnceCallback<void(const absl::optional<GURL>&)> callback,
           const absl::optional<GURL>& url) {
          if (url && url->is_valid() && url->SchemeIsHTTPOrHTTPS()) {
            std::move(callback).Run(url);
          } else {
            std::move(callback).Run(absl::nullopt);
          }
        };
    GetAssociatedLocalFrame()->GetCanonicalUrlForSharing(
        base::BindOnce(validate_and_forward, std::move(callback)));
  } else {
    std::move(callback).Run(absl::nullopt);
  }
}

void RenderFrameHostImpl::GetOpenGraphMetadata(
    base::OnceCallback<void(blink::mojom::OpenGraphMetadataPtr)> callback) {
  if (IsRenderFrameLive()) {
    GetAssociatedLocalFrame()->GetOpenGraphMetadata(
        base::BindOnce(&ForwardOpenGraphMetadataIfValid, std::move(callback)));
  } else {
    std::move(callback).Run({});
  }
}

bool RenderFrameHostImpl::IsErrorDocument() {
  // This shouldn't be called before committing the document as this value is
  // set during call to RenderFrameHostImpl::DidNavigate which happens after
  // commit.
  DCHECK_NE(lifecycle_state(), LifecycleStateImpl::kSpeculative);
  DCHECK_NE(lifecycle_state(), LifecycleStateImpl::kPendingCommit);
  return is_error_document_;
}

DocumentRef RenderFrameHostImpl::GetDocumentRef() {
  return DocumentRef(document_associated_data_->weak_factory().GetSafeRef());
}

WeakDocumentPtr RenderFrameHostImpl::GetWeakDocumentPtr() {
  return WeakDocumentPtr(
      document_associated_data_->weak_factory().GetWeakPtr());
}

void RenderFrameHostImpl::GetSerializedHtmlWithLocalLinks(
    const base::flat_map<GURL, base::FilePath>& url_map,
    const base::flat_map<blink::FrameToken, base::FilePath>& frame_token_map,
    bool save_with_empty_url,
    mojo::PendingRemote<mojom::FrameHTMLSerializerHandler> serializer_handler) {
  if (!IsRenderFrameLive())
    return;
  GetMojomFrameInRenderer()->GetSerializedHtmlWithLocalLinks(
      url_map, frame_token_map, save_with_empty_url,
      std::move(serializer_handler));
}

void RenderFrameHostImpl::SetWantErrorMessageStackTrace() {
  GetMojomFrameInRenderer()->SetWantErrorMessageStackTrace();
}

void RenderFrameHostImpl::ExecuteMediaPlayerActionAtLocation(
    const gfx::Point& location,
    const blink::mojom::MediaPlayerAction& action) {
  auto media_player_action = blink::mojom::MediaPlayerAction::New();
  media_player_action->type = action.type;
  media_player_action->enable = action.enable;
  gfx::PointF point_in_view = GetView()->TransformRootPointToViewCoordSpace(
      gfx::PointF(location.x(), location.y()));
  GetAssociatedLocalFrame()->MediaPlayerActionAt(
      gfx::Point(point_in_view.x(), point_in_view.y()),
      std::move(media_player_action));
}

bool RenderFrameHostImpl::CreateNetworkServiceDefaultFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>
        default_factory_receiver) {
  // Factory config below is based on the last committed navigation, under the
  // assumptions that the caller wants a factory that acts on behalf of the
  // *currently* committed document.  This assumption is typically valid for
  // callers that are responding to an IPC coming from the renderer process.  If
  // the caller wanted a factory associated with a pending navigation, then the
  // config won't be correct.  For more details, please see the doc comment of
  // ForPendingOrLastCommittedNavigation.
  auto subresource_loader_factories_config =
      SubresourceLoaderFactoriesConfig::ForLastCommittedNavigation(*this);

  return CreateNetworkServiceDefaultFactoryAndObserve(
      CreateURLLoaderFactoryParamsForMainWorld(
          subresource_loader_factories_config,
          "RFHI::CreateNetworkServiceDefaultFactory"),
      subresource_loader_factories_config.ukm_source_id(),
      std::move(default_factory_receiver));
}

std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
RenderFrameHostImpl::CreateSubresourceLoaderFactoriesForInitialEmptyDocument() {
  // This method should only be called (by RenderViewHostImpl::CreateRenderView)
  // when creating a new local main frame in a Renderer process.
  DCHECK(!GetParent());

  // Expecting the frame to be at the initial empty document.
  // Not DCHECK-ing `last_committed_origin_`, because it is not reset by
  // RenderFrameHostImpl::RenderProcessExited for crashed frames.
  DCHECK_EQ(GURL(), last_committed_url_);

  auto subresource_loader_factories =
      std::make_unique<blink::PendingURLLoaderFactoryBundle>();
  switch (lifecycle_state()) {
    case RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache:
    case RenderFrameHostImpl::LifecycleStateImpl::kPendingCommit:
    case RenderFrameHostImpl::LifecycleStateImpl::kReadyToBeDeleted:
    case RenderFrameHostImpl::LifecycleStateImpl::kRunningUnloadHandlers:
      // A newly-created frame shouldn't be in any of the states above.
      NOTREACHED();
      break;
    case RenderFrameHostImpl::LifecycleStateImpl::kSpeculative:
      // No subresource requests should be initiated in the speculative frame.
      // Serving an empty bundle of `subresource_loader_factories` will
      // desirably lead to a crash in URLLoaderFactoryBundle::GetFactory (see
      // also the DCHECK there) if the speculative frame attempts to start a
      // subresource load.
      break;
    case RenderFrameHostImpl::LifecycleStateImpl::kActive:
    case RenderFrameHostImpl::LifecycleStateImpl::kPrerendering:
      CreateNetworkServiceDefaultFactory(
          subresource_loader_factories->pending_default_factory()
              .InitWithNewPipeAndPassReceiver());

      // The caller will send the returned factory to the renderer process (as
      // the default factory).  Therefore, after a NetworkService crash we need
      // to push to the renderer an updated factory.
      recreate_default_url_loader_factory_after_network_service_crash_ = true;
      break;
  }

  return subresource_loader_factories;
}

void RenderFrameHostImpl::MarkIsolatedWorldsAsRequiringSeparateURLLoaderFactory(
    const base::flat_set<url::Origin>& isolated_world_origins,
    bool push_to_renderer_now) {
  size_t old_size =
      isolated_worlds_requiring_separate_url_loader_factory_.size();
  isolated_worlds_requiring_separate_url_loader_factory_.insert(
      isolated_world_origins.begin(), isolated_world_origins.end());
  size_t new_size =
      isolated_worlds_requiring_separate_url_loader_factory_.size();
  bool insertion_took_place = (old_size != new_size);

  // Push the updated set of factories to the renderer, but only if
  // 1) the caller requested an immediate push (e.g. for content scripts
  //    injected programmatically chrome.tabs.executeCode, but not for content
  //    scripts declared in the manifest - the difference is that the latter
  //    happen at a commit and the factories can just be send in the commit
  //    IPC).
  // 2) an insertion actually took place / the factories have been modified.
  //
  // See also the doc comment of `PendingURLLoaderFactoryBundle::OriginMap`
  // (the type of `pending_isolated_world_factories` that are set below).
  if (push_to_renderer_now && insertion_took_place) {
    // The `config` of the new factories might need to depend on the pending
    // (rather than the last committed) navigation, because we can't predict if
    // an in-flight Commit IPC might be present when an extension injects a
    // content script and MarkIsolatedWorlds... is called.  See also the doc
    // comment for the ForPendingOrLastCommittedNavigation method.
    auto config =
        SubresourceLoaderFactoriesConfig::ForPendingOrLastCommittedNavigation(
            *this);

    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories =
            std::make_unique<blink::PendingURLLoaderFactoryBundle>();
    subresource_loader_factories->pending_isolated_world_factories() =
        CreateURLLoaderFactoriesForIsolatedWorlds(config,
                                                  isolated_world_origins);
    GetMojomFrameInRenderer()->UpdateSubresourceLoaderFactories(
        std::move(subresource_loader_factories));
  }
}

bool RenderFrameHostImpl::IsSandboxed(network::mojom::WebSandboxFlags flags) {
  return static_cast<int>(active_sandbox_flags()) & static_cast<int>(flags);
}

blink::web_pref::WebPreferences
RenderFrameHostImpl::GetOrCreateWebPreferences() {
  return delegate()->GetOrCreateWebPreferences();
}

blink::PendingURLLoaderFactoryBundle::OriginMap
RenderFrameHostImpl::CreateURLLoaderFactoriesForIsolatedWorlds(
    const SubresourceLoaderFactoriesConfig& config,
    const base::flat_set<url::Origin>& isolated_world_origins) {
  blink::PendingURLLoaderFactoryBundle::OriginMap result;
  for (const url::Origin& isolated_world_origin : isolated_world_origins) {
    network::mojom::URLLoaderFactoryParamsPtr factory_params =
        URLLoaderFactoryParamsHelper::CreateForIsolatedWorld(
            this, isolated_world_origin, config.origin(),
            config.isolation_info(), config.GetClientSecurityState(),
            config.trust_token_issuance_policy(),
            config.trust_token_redemption_policy(),
            config.cookie_setting_overrides());

    mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
    CreateNetworkServiceDefaultFactoryAndObserve(
        std::move(factory_params),
        ukm::kInvalidSourceIdObj, /* isolated from page */
        factory_remote.InitWithNewPipeAndPassReceiver());
    result[isolated_world_origin] = std::move(factory_remote);
  }
  return result;
}

gfx::NativeView RenderFrameHostImpl::GetNativeView() {
  RenderWidgetHostView* view = render_view_host_->GetWidget()->GetView();
  if (!view)
    return gfx::NativeView();
  return view->GetNativeView();
}

void RenderFrameHostImpl::AddMessageToConsole(
    blink::mojom::ConsoleMessageLevel level,
    const std::string& message) {
  AddMessageToConsoleImpl(level, message, /*discard_duplicates=*/false);
}

void RenderFrameHostImpl::ExecuteJavaScriptMethod(
    const std::u16string& object_name,
    const std::u16string& method_name,
    base::Value::List arguments,
    JavaScriptResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(CanExecuteJavaScript());
  AssertFrameWasCommitted();

  const bool wants_result = !callback.is_null();
  GetAssociatedLocalFrame()->JavaScriptMethodExecuteRequest(
      object_name, method_name, std::move(arguments), wants_result,
      std::move(callback));
}

void RenderFrameHostImpl::ExecuteJavaScript(const std::u16string& javascript,
                                            JavaScriptResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(CanExecuteJavaScript());
  AssertFrameWasCommitted();

  const bool wants_result = !callback.is_null();
  GetAssociatedLocalFrame()->JavaScriptExecuteRequest(javascript, wants_result,
                                                      std::move(callback));
}

void RenderFrameHostImpl::ExecuteJavaScriptInIsolatedWorld(
    const std::u16string& javascript,
    JavaScriptResultCallback callback,
    int32_t world_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_GT(world_id, ISOLATED_WORLD_ID_GLOBAL);
  DCHECK_LE(world_id, ISOLATED_WORLD_ID_MAX);
  AssertFrameWasCommitted();

  const bool wants_result = !callback.is_null();
  GetAssociatedLocalFrame()->JavaScriptExecuteRequestInIsolatedWorld(
      javascript, wants_result, world_id, std::move(callback));
}

void RenderFrameHostImpl::ExecuteJavaScriptForTests(
    const std::u16string& javascript,
    JavaScriptResultCallback callback,
    int32_t world_id) {
  ExecuteJavaScriptForTests(  // IN-TEST
      javascript, /*has_user_gesture=*/false,
      /*resolve_promises=*/false, world_id,
      CreateJavaScriptExecuteRequestForTestsCallback(std::move(callback)));
}

void RenderFrameHostImpl::ExecuteJavaScriptWithUserGestureForTests(
    const std::u16string& javascript,
    JavaScriptResultCallback callback,
    int32_t world_id) {
  ExecuteJavaScriptForTests(  // IN-TEST
      javascript, /*has_user_gesture=*/true,
      /*resolve_promises=*/false, world_id,
      CreateJavaScriptExecuteRequestForTestsCallback(std::move(callback)));
}

void RenderFrameHostImpl::ExecuteJavaScriptForTests(
    const std::u16string& javascript,
    bool has_user_gesture,
    bool resolve_promises,
    int32_t world_id,
    JavaScriptResultAndTypeCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  AssertFrameWasCommitted();

  if (has_user_gesture && owner_) {
    // TODO(mustaq): The render-to-browser state update caused by the below
    // JavaScriptExecuteRequestsForTests call is redundant with this update. We
    // should determine if the redundancy can be removed.
    owner_->UpdateUserActivationState(
        blink::mojom::UserActivationUpdateType::kNotifyActivation,
        blink::mojom::UserActivationNotificationType::kTest);
  }

  GetAssociatedLocalFrame()->JavaScriptExecuteRequestForTests(  // IN-TEST
      javascript, has_user_gesture, resolve_promises, world_id,
      std::move(callback));
}

void RenderFrameHostImpl::ExecutePluginActionAtLocalLocation(
    const gfx::Point& local_location,
    blink::mojom::PluginActionType plugin_action) {
  GetAssociatedLocalFrame()->PluginActionAt(local_location, plugin_action);
}

void RenderFrameHostImpl::CopyImageAt(int x, int y) {
  gfx::PointF point_in_view =
      GetView()->TransformRootPointToViewCoordSpace(gfx::PointF(x, y));
  GetAssociatedLocalFrame()->CopyImageAt(
      gfx::Point(point_in_view.x(), point_in_view.y()));
}

void RenderFrameHostImpl::SaveImageAt(int x, int y) {
  gfx::PointF point_in_view =
      GetView()->TransformRootPointToViewCoordSpace(gfx::PointF(x, y));
  GetAssociatedLocalFrame()->SaveImageAt(
      gfx::Point(point_in_view.x(), point_in_view.y()));
}

RenderViewHost* RenderFrameHostImpl::GetRenderViewHost() const {
  return render_view_host_.get();
}

service_manager::InterfaceProvider* RenderFrameHostImpl::GetRemoteInterfaces() {
  DCHECK(IsRenderFrameLive());
  return remote_interfaces_.get();
}

blink::AssociatedInterfaceProvider*
RenderFrameHostImpl::GetRemoteAssociatedInterfaces() {
  if (!remote_associated_interfaces_) {
    mojo::AssociatedRemote<blink::mojom::AssociatedInterfaceProvider>
        remote_interfaces;
    if (GetAgentSchedulingGroup().GetChannel()) {
      GetAgentSchedulingGroup().GetRemoteRouteProvider()->GetRoute(
          GetRoutingID(), remote_interfaces.BindNewEndpointAndPassReceiver());
    } else {
      // The channel may not be initialized in some tests environments. In this
      // case we set up a dummy interface provider.
      std::ignore = remote_interfaces.BindNewEndpointAndPassDedicatedReceiver();
    }
    remote_associated_interfaces_ =
        std::make_unique<blink::AssociatedInterfaceProvider>(
            remote_interfaces.Unbind());
  }
  return remote_associated_interfaces_.get();
}

PageVisibilityState RenderFrameHostImpl::GetVisibilityState() {
  // Works around the crashes seen in https://crbug.com/501863, where the
  // active WebContents from a browser iterator may contain a render frame
  // detached from the frame tree. This tries to find a RenderWidgetHost
  // attached to an ancestor frame, and defaults to visibility hidden if
  // it fails.
  // TODO(yfriedman, peter): Ideally this would never be called on an
  // unattached frame and we could omit this check. See
  // https://crbug.com/615867.
  RenderFrameHostImpl* frame = this;
  while (frame) {
    if (frame->GetLocalRenderWidgetHost())
      break;
    frame = frame->GetParent();
  }
  if (!frame)
    return PageVisibilityState::kHidden;

  PageVisibilityState visibility_state = GetRenderWidgetHost()->is_hidden()
                                             ? PageVisibilityState::kHidden
                                             : PageVisibilityState::kVisible;
  GetContentClient()->browser()->OverridePageVisibilityState(this,
                                                             &visibility_state);
  return visibility_state;
}

bool RenderFrameHostImpl::Send(IPC::Message* message) {
  return GetAgentSchedulingGroup().Send(message);
}

bool RenderFrameHostImpl::OnMessageReceived(const IPC::Message& msg) {
  // Only process messages if the RenderFrame is alive.
  if (!is_render_frame_created())
    return false;

  // Crash reports triggered by IPC messages for this frame should be associated
  // with its URL.
  ScopedActiveURL scoped_active_url(this);

  if (delegate_->OnMessageReceived(this, msg))
    return true;

  return false;
}

void RenderFrameHostImpl::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  // `this` is an `IPC::Listener`, but there is no path by which `this` would
  // receive associated interface requests through this method. Associated
  // interface requests come in through `GetAssociatedInterface()`.
  NOTREACHED();
}

std::string RenderFrameHostImpl::ToDebugString() {
  return "RFHI:" +
         render_view_host_->GetDelegate()->GetCreatorLocation().ToString();
}

void RenderFrameHostImpl::AccessibilityPerformAction(
    const ui::AXActionData& action_data) {
  // Don't perform any Accessibility action on an inactive frame.
  if (IsInactiveAndDisallowActivation(
          DisallowActivationReasonId::kAXPerformAction) ||
      !render_accessibility_)
    return;

  if (action_data.action == ax::mojom::Action::kHitTest) {
    AccessibilityHitTest(action_data.target_point,
                         action_data.hit_test_event_to_fire,
                         action_data.request_id, {});
    return;
  }
  // Set the input modality in RenderWidgetHostViewAura to touch so the
  // VK shows up.
  if (action_data.action == ax::mojom::Action::kFocus) {
    RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
        render_view_host_->GetWidget()->GetView());
    if (view)
      view->SetLastPointerType(ui::EventPointerType::kTouch);
  }

  render_accessibility_->PerformAction(action_data);
}

bool RenderFrameHostImpl::AccessibilityViewHasFocus() {
  RenderWidgetHostView* view = render_view_host_->GetWidget()->GetView();
  if (view)
    return view->AccessibilityHasFocus();
  return false;
}

void RenderFrameHostImpl::AccessibilityViewSetFocus() {
  // Don't update Accessibility for inactive frames.
  if (IsInactiveAndDisallowActivation(DisallowActivationReasonId::kAXSetFocus))
    return;

  RenderWidgetHostView* view = render_view_host_->GetWidget()->GetView();
  if (view)
    view->Focus();
}

gfx::Rect RenderFrameHostImpl::AccessibilityGetViewBounds() {
  RenderWidgetHostView* view = render_view_host_->GetWidget()->GetView();
  if (view)
    return view->GetViewBounds();
  return gfx::Rect();
}

float RenderFrameHostImpl::AccessibilityGetDeviceScaleFactor() {
  return GetScaleFactorForView(GetView());
}

void RenderFrameHostImpl::AccessibilityReset() {
  if (!render_accessibility_)
    return;

  accessibility_reset_token_ = ++g_accessibility_reset_token;
  render_accessibility_->Reset(*accessibility_reset_token_);
}

void RenderFrameHostImpl::AccessibilityFatalError() {
  CHECK(!BrowserAccessibilityManager::IsFailFastMode());
  browser_accessibility_manager_.reset();
  if (!render_accessibility_) {
    return;
  }

  static auto* ax_rfhi_url_crash_key = base::debug::AllocateCrashKeyString(
      "ax_rfhi_url", base::debug::CrashKeySize::Size256);
  base::debug::ScopedCrashKeyString ax_rfhi_url(ax_rfhi_url_crash_key,
                                                GetLastCommittedURL().spec());
  static auto* ax_rfhi_top_url_crash_key = base::debug::AllocateCrashKeyString(
      "ax_rfhi_top_url", base::debug::CrashKeySize::Size256);
  base::debug::ScopedCrashKeyString ax_rfhi_top_url(
      ax_rfhi_top_url_crash_key,
      GetOutermostMainFrameOrEmbedder()->GetLastCommittedURL().spec());

  accessibility_fatal_error_count_++;
  if (accessibility_fatal_error_count_ > max_accessibility_resets_) {
    // This will both create an "Aw Snap..." and generate a second crash report
    // in addition to the DumpWithoutCrashing() for the first reset.
    render_accessibility_->FatalError();
  } else {
    if (base::RandGenerator(20) == 0) {
      // Only report crash ~5% of the time -- don't skew crash stats too much.
      // Crash keys were set in BrowserAccessibilityManager::Unserialize().
      base::debug::DumpWithoutCrashing();
    }
    AccessibilityReset();
  }
}

gfx::AcceleratedWidget
RenderFrameHostImpl::AccessibilityGetAcceleratedWidget() {
  DCHECK(AccessibilityIsRootFrame());
  // Only the active RenderFrameHost is connected to the native widget tree for
  // accessibility, so return null if this is queried on any other frame.
  if (!IsActive())
    return gfx::kNullAcceleratedWidget;

  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      render_view_host_->GetWidget()->GetView());
  if (view)
    return view->AccessibilityGetAcceleratedWidget();
  return gfx::kNullAcceleratedWidget;
}

gfx::NativeViewAccessible
RenderFrameHostImpl::AccessibilityGetNativeViewAccessible() {
  if (base::FeatureList::IsEnabled(features::kEvictOnAXEvents) &&
      base::FeatureList::IsEnabled(
          features::kEnableBackForwardCacheForScreenReader) &&
      IsInactiveAndDisallowActivation(
          DisallowActivationReasonId::kAXGetNativeView)) {
    // |AccessibilityGetNativeViewAccessible()| should be only accessible when
    // we process AX events. Otherwise this should not be used while in
    // back/forward cache and the document should be evicted.
    return nullptr;
  }
  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      render_view_host_->GetWidget()->GetView());
  if (view)
    return view->AccessibilityGetNativeViewAccessible();
  return nullptr;
}

gfx::NativeViewAccessible
RenderFrameHostImpl::AccessibilityGetNativeViewAccessibleForWindow() {
  // If this method is called when the frame is in BackForwardCache, evict
  // the frame to avoid ignoring any accessibility related events which are not
  // expected.
  if (IsInactiveAndDisallowActivation(
          DisallowActivationReasonId::kAXGetNativeViewForWindow))
    return nullptr;

  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      render_view_host_->GetWidget()->GetView());
  if (view)
    return view->AccessibilityGetNativeViewAccessibleForWindow();
  return nullptr;
}

void RenderFrameHostImpl::AccessibilityHitTest(
    const gfx::Point& point_in_frame_pixels,
    const ax::mojom::Event& opt_event_to_fire,
    int opt_request_id,
    base::OnceCallback<void(ui::AXPlatformTreeManager* hit_manager,
                            ui::AXNodeID hit_node_id)> opt_callback) {
  // This is called by BrowserAccessibilityManager. During teardown it's
  // possible that render_accessibility_ is null but the corresponding
  // BrowserAccessibilityManager still exists and could call this.
  if (IsInactiveAndDisallowActivation(DisallowActivationReasonId::kAXHitTest) ||
      !render_accessibility_) {
    if (opt_callback)
      std::move(opt_callback).Run(nullptr, 0);
    return;
  }

  render_accessibility_->HitTest(
      point_in_frame_pixels, opt_event_to_fire, opt_request_id,
      base::BindOnce(&RenderFrameHostImpl::AccessibilityHitTestCallback,
                     weak_ptr_factory_.GetWeakPtr(), opt_request_id,
                     opt_event_to_fire, std::move(opt_callback)));
}

gfx::NativeWindow RenderFrameHostImpl::GetTopLevelNativeWindow() {
  WebContents* web_contents = WebContents::FromRenderFrameHost(this);
  return web_contents ? web_contents->GetTopLevelNativeWindow()
                      : gfx::NativeWindow();
}

bool RenderFrameHostImpl::AccessibilityIsRootFrame() const {
  // Do not use is_main_frame() or IsOutermostMainFrame().
  // Frame trees may be nested so it can be the case that is_main_frame() is
  // true, but is not the outermost RenderFrameHost (it only checks for nullity
  // of |parent_|. In particular, !is_main_frame() cannot be used to check if
  // this RenderFrameHost is embedded. In addition, IsOutermostMainFrame()
  // does not escape guest views. Therefore, we must check for any kind of
  // parent document or embedder.
  return !GetParentOrOuterDocumentOrEmbedderExcludingProspectiveOwners();
}

RenderFrameHostImpl* RenderFrameHostImpl::AccessibilityRenderFrameHost() {
  return this;
}

WebContentsAccessibility*
RenderFrameHostImpl::AccessibilityGetWebContentsAccessibility() {
  DCHECK(AccessibilityIsRootFrame());
  auto* view = static_cast<RenderWidgetHostViewBase*>(GetView());
  if (!view)
    return nullptr;
  return view->GetWebContentsAccessibility();
}

void RenderFrameHostImpl::InitializePolicyContainerHost(
    bool renderer_initiated_creation_of_main_frame) {
  // No policy container for speculative frames.
  if (lifecycle_state_ == LifecycleStateImpl::kSpeculative) {
    return;
  }

  // During initialization, a RenderFrameHost always has a non-null `owner_`.
  // This is called from the constructor.
  CHECK(owner_);

  // The initial empty document inherits its policy container from its creator.
  // The creator is either its parent for subframes and embedded frames, or its
  // opener for new windows.
  //
  // Note 1: For normal document created from a navigation, the policy container
  // is computed from the NavigationRequest and assigned in
  // DidCommitNewDocument().
  if (parent_) {
    SetPolicyContainerHost(parent_->policy_container_host()->Clone());
  } else if (GetParentOrOuterDocument()) {
    // In the MPArch implementation of FencedFrame, this RenderFrameHost's
    // SiteInstance has been adjusted to match its parent. During navigations,
    // COOP and COEP are used to determine the SiteInstance. It means that if
    // SiteInstance has been inherited, COOP/COEP must also be inherited to
    // avoid creating inconsistencies. See:
    // https://chromium-review.googlesource.com/c/chromium/src/+/3645368
    //
    // TODO(https://crbug.com/1338603): What makes sense for GuestView?
    // TODO(https://crbug.com/1338601): What makes sense for Portals?
    const PolicyContainerPolicies& parent_policies =
        GetParentOrOuterDocument()->policy_container_host()->policies();

    // Note: the full constructor is used, to force developers to make an
    // explicit decision when adding new fields to the PolicyContainer.
    // The IP address space of fenced frame is set to `kPublic`.
    // 1. This makes it subject to local network access checks, restricting
    // its ability to access the private network.
    // 2. The IP address space of the parent does not leak to the fenced frame.
    SetPolicyContainerHost(
        base::MakeRefCounted<PolicyContainerHost>(PolicyContainerPolicies(
            network::mojom::ReferrerPolicy::kDefault,
            IsFencedFrameRoot() ? network::mojom::IPAddressSpace::kPublic
                                : network::mojom::IPAddressSpace::kUnknown,
            /*is_web_secure_context=*/false,
            std::vector<network::mojom::ContentSecurityPolicyPtr>(),
            parent_policies.cross_origin_opener_policy,
            parent_policies.cross_origin_embedder_policy,
            network::mojom::WebSandboxFlags::kNone,
            /*is_credentialless=*/false,
            /*can_navigate_top_without_user_gesture=*/true,
            parent_policies.allow_cross_origin_isolation)));
  } else if (owner_->GetOpener()) {
    // During a `window.open(...)` without `noopener`, a new popup is created
    // and always starts from the initial empty document. The opener has
    // synchronous access toward its openee. So they must both share the same
    // policies.
    SetPolicyContainerHost(owner_->GetOpener()
                               ->current_frame_host()
                               ->policy_container_host()
                               ->Clone());
    const absl::optional<url::Origin>& coop_origin =
        policy_container_host_->cross_origin_opener_policy().origin;
    policy_container_host_->SetAllowCrossOriginIsolation(
        !coop_origin.has_value() ||
        coop_origin->IsSameOriginWith(owner_->GetOpener()
                                          ->current_frame_host()
                                          ->GetLastCommittedOrigin()));
  } else {
    // In all the other cases, there is no environment to inherit policies
    // from. This is "probably" a new top-level about:blank document created by
    // the browser directly (omnibox, bookmarks, ...).
    PolicyContainerPolicies policies;

    // Main frames created by the browser are treated as belonging the `local`
    // address space, so that they can make requests to any address space
    // unimpeded. The only way to execute code in such a context is to inject it
    // via DevTools, WebView APIs, or extensions; it is impossible to do so with
    // Web Platform means only.
    //
    // See also https://crbug.com/1191161.
    //
    // We also exclude prerendering from this case manually, since prendering
    // RenderFrameHosts are unconditionally created with the
    // `renderer_initiated_creation_of_main_frame` set to false, even though the
    // frames arguably are renderer-created.
    //
    // TODO(https://crbug.com/1194421): Address the prerendering case.
    DCHECK(IsOutermostMainFrame());
    if (!renderer_initiated_creation_of_main_frame &&
        lifecycle_state_ != LifecycleStateImpl::kPrerendering) {
      policies.ip_address_space = network::mojom::IPAddressSpace::kLocal;
    }

    SetPolicyContainerHost(
        base::MakeRefCounted<PolicyContainerHost>(std::move(policies)));
  }

  // The initial empty documents sandbox flags is the union from:
  // - The parent or opener document's CSP sandbox inherited by policy
  //   container.
  // - The frame's sandbox flags, contained in browsing_context_state. This
  //   are either:
  //   1. For iframe: the parent + iframe.sandbox attribute.
  //   2. For popups: the opener if "allow-popups-to-escape-sandbox" isn't
  //   set.
  network::mojom::WebSandboxFlags sandbox_flags_to_commit =
      browsing_context_state_->effective_frame_policy().sandbox_flags;
  for (const auto& csp :
       policy_container_host_->policies().content_security_policies) {
    sandbox_flags_to_commit |= csp->sandbox;
  }
  policy_container_host_->set_sandbox_flags(sandbox_flags_to_commit);

  // The initial empty document's credentialless bit was inherited from the
  // parent document. The frame's credentialless bit can also turn it on.
  if (owner_->Credentialless()) {
    policy_container_host_->SetIsCredentialless();
  }
}

void RenderFrameHostImpl::SetPolicyContainerHost(
    scoped_refptr<PolicyContainerHost> policy_container_host) {
  policy_container_host_ = std::move(policy_container_host);
  policy_container_host_->AssociateWithFrameToken(GetFrameToken(),
                                                  GetProcess()->GetID());
  // Top-level document are never credentialless.
  // Note: It is never inherited from the opener, because they are forced to
  // open windows using noopener.
  CHECK(parent_ || !IsCredentialless());
}

void RenderFrameHostImpl::InitializePrivateNetworkRequestPolicy() {
  if (!policy_container_host_) {
    // Only speculative RFHs may lack a policy container.
    DCHECK_EQ(lifecycle_state_, LifecycleStateImpl::kSpeculative);
    return;
  }

  private_network_request_policy_ = DerivePrivateNetworkRequestPolicy(
      policy_container_host_->policies(),
      PrivateNetworkRequestContext::kSubresource);
}

void RenderFrameHostImpl::RenderProcessGone(
    SiteInstanceGroup* site_instance_group,
    const ChildProcessTerminationInfo& info) {
  DCHECK_EQ(site_instance_->group(), site_instance_group);

  if (IsInBackForwardCache()) {
    EvictFromBackForwardCacheWithReason(
        info.status == base::TERMINATION_STATUS_PROCESS_CRASHED
            ? BackForwardCacheMetrics::NotRestoredReason::
                  kRendererProcessCrashed
            : BackForwardCacheMetrics::NotRestoredReason::
                  kRendererProcessKilled);
  }

  CancelPrerendering(PrerenderCancellationReason(
      info.status == base::TERMINATION_STATUS_PROCESS_CRASHED
          ? PrerenderFinalStatus::kRendererProcessCrashed
          : PrerenderFinalStatus::kRendererProcessKilled));

  if (owned_render_widget_host_)
    owned_render_widget_host_->RendererExited();

  // The renderer process is gone, so this frame can no longer be loading.
  ResetOwnedNavigationRequests(NavigationDiscardReason::kRenderProcessGone);
  ResetLoadingState();

  // Also, clear any pending navigations that have been blocked while the
  // embedder is processing window.open() requests.  This is consistent
  // with clearing NavigationRequests and loading state above, and it also
  // makes sense because certain parts of `pending_navigate_`, like the
  // NavigationClient remote interface, can no longer be used.
  pending_navigate_.reset();

  // Any future UpdateState or UpdateTitle messages from this or a recreated
  // process should be ignored until the next commit.
  set_nav_entry_id(0);

  if (is_audible_)
    OnAudibleStateChanged(false);

  ++renderer_exit_count_;

  if (base::FeatureList::IsEnabled(features::kCrashReporting))
    MaybeGenerateCrashReport(info.status, info.exit_code);

  // Reporting API: Send any queued reports and mark the reporting source as
  // expired so that the reporting configuration in the network service can be
  // removed. This is done here, rather than in the destructor, as it needs the
  // mojo pipe to the network service.
  GetProcess()
      ->GetStoragePartition()
      ->GetNetworkContext()
      ->SendReportsAndRemoveSource(GetReportingSource());

  // When a frame's process dies, its RenderFrame no longer exists, which means
  // that its child frames must be cleaned up as well.
  ResetChildren();

  // Reset state for the current RenderFrameHost once the FrameTreeNode has been
  // reset.
  RenderFrameDeleted();
  SetLastCommittedUrl(GURL());
  SetInheritedBaseUrl(GURL());
  renderer_url_info_ = RendererURLInfo();

  must_be_replaced_ = true;
  has_committed_any_navigation_ = false;

#if BUILDFLAG(IS_ANDROID)
  // Execute any pending Samsung smart clip callbacks.
  for (base::IDMap<std::unique_ptr<ExtractSmartClipDataCallback>>::iterator
           iter(&smart_clip_callbacks_);
       !iter.IsAtEnd(); iter.Advance()) {
    std::move(*iter.GetCurrentValue())
        .Run(std::u16string(), std::u16string(), gfx::Rect());
  }
  smart_clip_callbacks_.Clear();
#endif  // BUILDFLAG(IS_ANDROID)

  // Ensure that future remote interface requests are associated with the new
  // process's channel.
  remote_associated_interfaces_.reset();

  // Any termination disablers in content loaded by the new process will
  // be sent again.
  has_before_unload_handler_ = false;
  has_unload_handler_ = false;
  has_pagehide_handler_ = false;
  has_visibilitychange_handler_ = false;

  has_navigate_event_handler_ = false;

  if (IsPendingDeletion()) {
    // If the process has died, we don't need to wait for the ACK. Complete the
    // deletion immediately.
    // Note that it is possible for a frame to already be in kReadyToBeDeleted.
    // This happens when this RenderFrameHost is pending deletion and is
    // waiting on one of its children to run its unload handler. While running
    // it, it can request its parent to detach itself.
    if (lifecycle_state() != LifecycleStateImpl::kReadyToBeDeleted)
      SetLifecycleState(LifecycleStateImpl::kReadyToBeDeleted);

    DCHECK(children_.empty());
    PendingDeletionCheckCompleted();  // Can delete |this|.
    // |this| is deleted. Don't add any more code at this point in the function.
    return;
  }

  // If this was the current pending or speculative RFH dying, cancel and
  // destroy it.
  if (lifecycle_state_ == LifecycleStateImpl::kSpeculative ||
      lifecycle_state_ == LifecycleStateImpl::kPendingCommit) {
    CHECK(owner_);  // See `owner_` invariants about `lifecycle_state_`.
    owner_->GetRenderFrameHostManager()
        .CleanupSpeculativeRfhForRenderProcessGone();
  }

  // Note: don't add any more code at this point in the function because
  // |this| may be deleted. Any additional cleanup should happen before
  // the last block of code here.
}

void RenderFrameHostImpl::PerformAction(const ui::AXActionData& data) {
  AccessibilityPerformAction(data);
}

bool RenderFrameHostImpl::RequiresPerformActionPointInPixels() const {
  return true;
}

bool RenderFrameHostImpl::CreateRenderFrame(
    const absl::optional<blink::FrameToken>& previous_frame_token,
    const absl::optional<blink::FrameToken>& opener_frame_token,
    const absl::optional<blink::FrameToken>& parent_frame_token,
    const absl::optional<blink::FrameToken>& previous_sibling_frame_token) {
  TRACE_EVENT0("navigation", "RenderFrameHostImpl::CreateRenderFrame");
  DCHECK(!IsRenderFrameLive()) << "Creating frame twice";

  // The process may (if we're sharing a process with another host that already
  // initialized it) or may not (we have our own process or the old process
  // crashed) have been initialized. Calling Init() multiple times will be
  // ignored, so this is safe.
  if (!GetAgentSchedulingGroup().Init())
    return false;

  DCHECK(GetProcess()->IsInitializedAndNotDead());

  mojom::CreateFrameParamsPtr params = mojom::CreateFrameParams::New();
  BindBrowserInterfaceBrokerReceiver(
      params->interface_broker.InitWithNewPipeAndPassReceiver());

  params->routing_id = routing_id_;
  params->previous_frame_token = previous_frame_token;
  params->opener_frame_token = opener_frame_token;
  params->parent_frame_token = parent_frame_token;
  params->previous_sibling_frame_token = previous_sibling_frame_token;
  params->tree_scope_type = frame_tree_node()->tree_scope_type();
  params->replication_state =
      browsing_context_state_->current_replication_state().Clone();
  params->frame_token = frame_token_;
  params->devtools_frame_token = devtools_frame_token();
  BindAssociatedInterfaceProviderReceiver(
      params->associated_interface_provider_remote
          .InitWithNewEndpointAndPassReceiver());
  params->document_token = document_associated_data_->token();

  // If this is a new RenderFrameHost for a frame that has already committed a
  // document, we don't have a policy container yet. Indeed, in that case, this
  // RenderFrameHost will not display any document until it commits a
  // navigation. The policy container for the navigated document will be sent to
  // Blink at CommitNavigation time and then stored in this RenderFrameHost in
  // DidCommitNewDocument.
  if (policy_container_host())
    params->policy_container =
        policy_container_host()->CreatePolicyContainerForBlink();

  // Normally, the replication state contains effective frame policy, excluding
  // sandbox flags and permissions policy attributes that were updated but have
  // not taken effect. However, a new RenderFrame should use the pending frame
  // policy, since it is being created as part of the navigation that will
  // commit it. (I.e., the RenderFrame needs to know the policy to use when
  // initializing the new document once it commits).
  params->replication_state->frame_policy =
      frame_tree_node()->pending_frame_policy();

  // If we switched BrowsingInstances because of the COOP header, we should
  // clear the frame name. This below informs the renderer at frame creation.
  NavigationRequest* navigation_request =
      frame_tree_node()->navigation_request();

  bool should_clear_browsing_instance_name =
      navigation_request &&
      (navigation_request->browsing_context_group_swap()
           .ShouldClearWindowName() ||
       (navigation_request->commit_params()
            .is_cross_site_cross_browsing_context_group &&
        base::FeatureList::IsEnabled(
            features::kClearCrossSiteCrossBrowsingContextGroupWindowName)));

  if (should_clear_browsing_instance_name) {
    params->replication_state->name = "";
    // The "swaps" only affect main frames, that have an empty unique name.
    DCHECK(params->replication_state->unique_name.empty());
  }

  params->frame_owner_properties =
      frame_tree_node()->frame_owner_properties().Clone();

  params->is_on_initial_empty_document =
      frame_tree_node()->is_on_initial_empty_document();

  // The RenderWidgetHost takes ownership of its view. It is tied to the
  // lifetime of the current RenderProcessHost for this RenderFrameHost.
  // TODO(avi): This will need to change to initialize a
  // RenderWidgetHostViewAura for the main frame once RenderViewHostImpl has-a
  // RenderWidgetHostImpl. https://crbug.com/545684
  if (owned_render_widget_host_) {
    DCHECK(parent_);
    DCHECK(parent_frame_token);
    RenderWidgetHostView* rwhv = RenderWidgetHostViewChildFrame::Create(
        owned_render_widget_host_.get(),
        parent_->GetRenderWidgetHost()->GetScreenInfos());
    // The child frame should be created hidden.
    DCHECK(!rwhv->IsShowing());
  }

  if (auto* rwh = GetLocalRenderWidgetHost()) {
    params->widget_params = rwh->BindAndGenerateCreateFrameWidgetParams();

    auto* previous_rfh =
        lifecycle_state_ == LifecycleStateImpl::kSpeculative &&
                frame_tree_node_->current_frame_host()->ShouldReuseCompositing(
                    *GetSiteInstance())
            ? frame_tree_node_->current_frame_host()
            : nullptr;
    if (previous_rfh) {
      waiting_for_renderer_widget_creation_after_commit_ = true;
      params->widget_params->previous_frame_token_for_compositor_reuse =
          previous_rfh->GetFrameToken();
    }
  }
  mojo::PendingAssociatedRemote<mojom::Frame> pending_frame_remote;
  params->frame = pending_frame_remote.InitWithNewEndpointAndPassReceiver();
  SetMojomFrameRemote(std::move(pending_frame_remote));

  // https://crbug.com/1006814. The renderer needs at least one of these tokens
  // to be able to insert the new frame in the frame tree.
  DCHECK(params->previous_frame_token || params->parent_frame_token);
  GetAgentSchedulingGroup().CreateFrame(std::move(params));

  if (previous_frame_token &&
      previous_frame_token->Is<blink::RemoteFrameToken>()) {
    RenderFrameProxyHost* proxy = RenderFrameProxyHost::FromFrameToken(
        GetProcess()->GetID(),
        previous_frame_token->GetAs<blink::RemoteFrameToken>());
    // We have also created a `blink::RemoteFrame` in CreateFrame above, so
    // remember that.
    CHECK(proxy);
    proxy->SetRenderFrameProxyCreated(true);
  }

  // The renderer now has a RenderFrame for this RenderFrameHost.  Note that
  // this path is only used for out-of-process iframes.  Main frame RenderFrames
  // are created with their RenderView, and same-site iframes are created at the
  // time of OnCreateChildFrame.
  RenderFrameCreated();

  return true;
}

void RenderFrameHostImpl::SetMojomFrameRemote(
    mojo::PendingAssociatedRemote<mojom::Frame> frame_remote) {
  DCHECK(!frame_);
  frame_.Bind(std::move(frame_remote));
}

void RenderFrameHostImpl::DeleteRenderFrame(
    mojom::FrameDeleteIntention intent) {
  TRACE_EVENT("navigation", "RenderFrameHostImpl::DeleteRenderFrame",
              [&](perfetto::EventContext ctx) {
                WriteRenderFrameImplDeletion(ctx, this, intent);
              });
  if (IsPendingDeletion())
    return;

  if (IsRenderFrameLive()) {
    GetMojomFrameInRenderer()->Delete(intent);

    // We change the lifecycle state to kRunningUnloadHandlers at the end of
    // this method to wait until OnUnloadACK() is invoked.
    // For subframes, process shutdown may be delayed for two reasons:
    // (1) to allow the process to be potentially reused by future navigations
    // withjin a short time window, and
    // (2) to give the subframe unload handlers a chance to execute.
    // TODO(crbug.com/1356280): consider delaying process shutdown for fenced
    // frames.
    if (!is_main_frame() && IsActive()) {
      base::TimeDelta subframe_shutdown_timeout =
          frame_tree_->IsBeingDestroyed()
              ? base::TimeDelta()
              : GetSubframeProcessShutdownDelay(
                    GetSiteInstance()->GetBrowserContext());
      // If this document has unload handlers (and is active), ensure that they
      // have a chance to execute by delaying process cleanup. This will prevent
      // the process from shutting down immediately in the case where this is
      // the last active frame in the process. See https://crbug.com/852204.
      // Note that in the majority of cases, this is not necessary now that we
      // keep track of pending delete RenderFrameHost
      // (https://crbug.com/609963), but there are still a few exceptions where
      // this is needed (https://crbug.com/1014550).
      const base::TimeDelta unload_handler_timeout =
          has_unload_handlers() ? subframe_unload_timeout_ : base::TimeDelta();

      if (!subframe_shutdown_timeout.is_zero() ||
          !unload_handler_timeout.is_zero()) {
        GetProcess()->DelayProcessShutdown(subframe_shutdown_timeout,
                                           unload_handler_timeout,
                                           site_instance_->GetSiteInfo());
      }
      // If the subframe takes too long to unload, force its removal from the
      // tree. See https://crbug.com/950625.
      subframe_unload_timer_.Start(
          FROM_HERE, subframe_unload_timeout_, this,
          &RenderFrameHostImpl::OnSubframeDeletionUnloadTimeout);
    }
  }

  // In case of speculative RenderFrameHosts deletion, we don't run any unload
  // handlers and RenderFrameHost is deleted directly without any lifecycle
  // state transitions.
  if (lifecycle_state() == LifecycleStateImpl::kSpeculative) {
    DCHECK(!has_unload_handlers());
    return;
  }

  // In case of BackForwardCache, page is evicted directly from the cache and
  // deleted immediately, without waiting for unload handlers.
  SetLifecycleState(ShouldWaitForUnloadHandlers()
                        ? LifecycleStateImpl::kRunningUnloadHandlers
                        : LifecycleStateImpl::kReadyToBeDeleted);
}

void RenderFrameHostImpl::RenderFrameCreated() {
  // In https://crbug.com/1146573 a WebContentsObserver was causing the frame to
  // be reinitialized during deletion. It is not valid to re-enter navigation
  // code like that and it led to an invalid state. This is not a DCHECK because
  // the corruption will not be visible until later, making the bug very
  // difficult to understand.
  CHECK_NE(render_frame_state_, RenderFrameState::kDeleting);
  // We should not create new RenderFrames while `frame_tree_` is being
  // destroyed (e.g., via a WebContentsObserver during WebContents shutdown).
  // This seems to have caused crashes in https://crbug.com/717650.
  CHECK(!frame_tree_->IsBeingDestroyed());
  DCHECK_NE(render_frame_state_, RenderFrameState::kCreated);

  const RenderFrameState old_render_frame_state = render_frame_state_;
  render_frame_state_ = RenderFrameState::kCreated;

  if (old_render_frame_state == RenderFrameState::kDeleted) {
    // Dispatch update notification when a Page is recreated after a crash. Only
    // main RenderFrameHosts should ever be reused.
    DCHECK(is_main_frame());
    // Only a current RenderFrameHost should be recreating its RenderFrame
    // here, since speculative and pending deletion RenderFrameHosts get
    // deleted immediately after crash, whereas prerender gets cancelled and
    // bfcache entry gets evicted.
    DCHECK_EQ(lifecycle_state(), LifecycleStateImpl::kActive);
    GetPage().NotifyPageBecameCurrent();
  }

  // Initialize the RenderWidgetHost which marks it and the RenderViewHost as
  // live before calling to the `delegate_`.
  if (!waiting_for_renderer_widget_creation_after_commit_) {
    RendererWidgetCreated();
  }

  // Set up mojo connections to the renderer from the `frame_` connection before
  // notifying the delegate.
  SetUpMojoConnection();

  delegate_->RenderFrameCreated(this);

  if (enabled_bindings_)
    GetFrameBindingsControl()->AllowBindings(enabled_bindings_);

  if (web_ui_ && enabled_bindings_ & BINDINGS_POLICY_WEB_UI)
    web_ui_->SetUpMojoConnection();
}

void RenderFrameHostImpl::RendererWidgetCreated() {
  if (GetLocalRenderWidgetHost()) {
#if BUILDFLAG(IS_ANDROID)
    GetLocalRenderWidgetHost()->SetForceEnableZoom(
        delegate_->GetOrCreateWebPreferences().force_enable_zoom);
#endif  // BUILDFLAG(IS_ANDROID)
    GetLocalRenderWidgetHost()->RendererWidgetCreated(
        /*for_frame_widget=*/true);
  }

  waiting_for_renderer_widget_creation_after_commit_ = false;
}

void RenderFrameHostImpl::RenderFrameDeleted() {
  // In https://crbug.com/1146573 a WebContentsObserver was causing the frame to
  // be reinitialized during deletion. It is not valid to re-enter navigation
  // code like that and it led to an invalid state. This is not a DCHECK because
  // the corruption will cause a crash but later, making the bug very
  // difficult to understand.
  CHECK_NE(render_frame_state_, RenderFrameState::kDeleting);
  bool was_created = is_render_frame_created();
  render_frame_state_ = RenderFrameState::kDeleting;
  render_frame_scoped_weak_ptr_factory_.InvalidateWeakPtrs();

  // If the current status is different than the new status, the delegate
  // needs to be notified.
  if (was_created) {
    delegate_->RenderFrameDeleted(this);
  }
  TearDownMojoConnection();

#if !BUILDFLAG(IS_ANDROID)
  HostZoomMap* host_zoom_map = HostZoomMap::Get(GetSiteInstance());
  host_zoom_map->ClearTemporaryZoomLevel(GetGlobalId());
#endif  // !BUILDFLAG(IS_ANDROID)

  if (web_ui_) {
    web_ui_->RenderFrameDeleted();
    web_ui_->TearDownMojoConnection();
  }
  render_frame_state_ = RenderFrameState::kDeleted;
}

void RenderFrameHostImpl::SwapIn() {
  GetAssociatedLocalFrame()->SwapInImmediately();
}

void RenderFrameHostImpl::Init() {
  // This is only called on the main frame, for renderer-created windows. These
  // windows wait for the renderer to signal that we can show them and begin
  // navigations.
  DCHECK(is_main_frame());
  DCHECK(waiting_for_init_);

  waiting_for_init_ = false;

  GetLocalRenderWidgetHost()->Init();

  if (pending_navigate_) {
    // `pending_navigate_` is set only by BeginNavigation(), and
    // BeginNavigation() should only be triggered when the navigation is
    // initiated by a document in the same process.
    const int initiator_process_id = GetProcess()->GetID();

    // Transfer `pending_navigate_` to a local variable, to avoid resetting it
    // after OnBeginNavigation since `this` might already be destroyed (see
    // below).
    //
    // This shouldn't matter for early RFH swaps out of crashed frames, since
    // `pending_navigate_` is cleared when the renderer process dies, but it
    // may matter for other current/future use cases of the early RFH swap.
    std::unique_ptr<PendingNavigation> pending_navigation =
        std::move(pending_navigate_);
    frame_tree_node()->navigator().OnBeginNavigation(
        frame_tree_node(), std::move(pending_navigation->common_params),
        std::move(pending_navigation->begin_navigation_params),
        std::move(pending_navigation->blob_url_loader_factory),
        std::move(pending_navigation->navigation_client),
        EnsurePrefetchedSignedExchangeCache(), initiator_process_id,
        std::move(pending_navigation->renderer_cancellation_listener));
    // DO NOT ADD CODE after this, as `this` might be deleted if an early
    // RenderFrameHost swap was performed when starting the navigation above.
  }
}

RenderFrameProxyHost* RenderFrameHostImpl::GetProxyToParent() {
  if (is_main_frame())
    return nullptr;

  return browsing_context_state_->GetRenderFrameProxyHost(
      GetParent()->GetSiteInstance()->group());
}

RenderFrameProxyHost* RenderFrameHostImpl::GetProxyToOuterDelegate() {
  // Precondition:RFH in subframe, in pending deletion, speculative, or in the
  // BFCache are not expected to access the outer WebContents.
  CHECK(lifecycle_state_ == LifecycleStateImpl::kActive ||
        lifecycle_state_ == LifecycleStateImpl::kPrerendering);
  DCHECK(is_main_frame());

  CHECK(owner_);  // See `owner_` invariants about `lifecycle_state_`.
  return owner_->GetRenderFrameHostManager().GetProxyToOuterDelegate();
}

void RenderFrameHostImpl::DidChangeReferrerPolicy(
    network::mojom::ReferrerPolicy referrer_policy) {
  if (!IsActive())
    return;
  DCHECK(owner_);  // See `owner_` invariants about `IsActive()`.
  owner_->DidChangeReferrerPolicy(referrer_policy);
}

void RenderFrameHostImpl::PropagateEmbeddingTokenToParentFrame() {
  // Protect against calls from WebContentsImpl::AttachInnerWebContents() that
  // happen before RFHI::SetEmbeddingToken() gets called, which is where the
  // token gets set. It's safe to early return in those cases since this method
  // will get called anyway by RFHI::SetEmbeddingToken(), at which time the
  // outer delegate for the inner web contents will have been created already.
  if (!embedding_token_)
    return;

  // We need to propagate the token to the parent frame if it's either remote or
  // part of an outer web contents, therefore we need to figure out the right
  // proxy to send the token to, if any.
  // For local parents the propagation occurs within the renderer process. The
  // token is also present on the main frame for generalization when the main
  // frame in embedded in another context (e.g. browser UI). The main frame is
  // not embedded in the context of the frame tree so it is not propagated here.
  // See RenderFrameHost::GetEmbeddingToken for more details.
  RenderFrameProxyHost* target_render_frame_proxy = nullptr;

  if (RequiresProxyToParent()) {
    // This subframe should have a remote parent frame.
    target_render_frame_proxy = GetProxyToParent();
    DCHECK(target_render_frame_proxy);
  } else if (is_main_frame()) {
    // The main frame in an inner web contents could have a delegate in the
    // outer web contents, so we need to account for that as well.
    target_render_frame_proxy = GetProxyToOuterDelegate();
  }

  // Propagate the token to the right process, if a proxy was found.
  if (target_render_frame_proxy &&
      target_render_frame_proxy->is_render_frame_proxy_live()) {
    target_render_frame_proxy->GetAssociatedRemoteFrame()->SetEmbeddingToken(
        embedding_token_.value());
  }
}

void RenderFrameHostImpl::OnAudibleStateChanged(bool is_audible) {
  DCHECK_NE(is_audible_, is_audible);
  if (is_audible) {
    DCHECK_NE(lifecycle_state(), LifecycleStateImpl::kPrerendering);
    GetProcess()->OnMediaStreamAdded();
  } else {
    GetProcess()->OnMediaStreamRemoved();
  }
  is_audible_ = is_audible;
  delegate_->OnFrameAudioStateChanged(this, is_audible_);
}

void RenderFrameHostImpl::DidAddMessageToConsole(
    blink::mojom::ConsoleMessageLevel log_level,
    const std::u16string& message,
    uint32_t line_no,
    const absl::optional<std::u16string>& source_id,
    const absl::optional<std::u16string>& untrusted_stack_trace) {
  std::u16string updated_source_id;
  if (source_id.has_value())
    updated_source_id = *source_id;
  if (delegate_->DidAddMessageToConsole(this, log_level, message, line_no,
                                        updated_source_id,
                                        untrusted_stack_trace)) {
    return;
  }

  // Pass through log severity only on builtin components pages to limit console
  // spew.
  const bool is_web_ui = HasWebUIScheme(GetMainFrame()->GetLastCommittedURL());
  if (is_web_ui) {
    DCHECK_EQ(GetMainFrame(), GetOutermostMainFrame())
        << "The mainframe and outermost mainframe should be the same in WebUI.";
  }
  const bool is_builtin_component =
      is_web_ui ||
      GetContentClient()->browser()->IsBuiltinComponent(
          GetProcess()->GetBrowserContext(), GetLastCommittedOrigin());
  const bool is_off_the_record =
      GetSiteInstance()->GetBrowserContext()->IsOffTheRecord();

  LogConsoleMessage(log_level, message, line_no, is_builtin_component,
                    is_off_the_record, updated_source_id);
}

void RenderFrameHostImpl::FrameSizeChanged(const gfx::Size& frame_size) {
  frame_size_ = frame_size;
  delegate_->FrameSizeChanged(this, frame_size);
}

void RenderFrameHostImpl::RendererDidActivateForPrerendering() {
  // RendererDidActivateForPrerendering() is called after the renderer has
  // notified that it fired the prerenderingchange event on the documents. The
  // browser now runs any binders that were deferred during prerendering. This
  // corresponds to the following steps of the activate algorithm:
  //
  // https://wicg.github.io/nav-speculation/prerendering.html#prerendering-browsing-context-activate
  // Step 8.3.4. "For each steps in doc's post-prerendering activation steps
  // list:"
  // Step 8.3.4.1. "Run steps."

  // Release Mojo capability control to run the binders. The RenderFrameHostImpl
  // may have been created after activation started, in which case it already
  // does not have Mojo capability control applied.
  if (mojo_binder_policy_applier_) {
    mojo_binder_policy_applier_->GrantAll();

    // As per `ReleaseMojoBinderPolicies` method requirement, the policy applier
    // owner should call the method when destroying the object, so we first
    // need to call this method before resetting the unique pointer.
    broker_.ReleaseMojoBinderPolicies();
    mojo_binder_policy_applier_.reset();
  }
}

void RenderFrameHostImpl::SetCrossOriginOpenerPolicyReporter(
    std::unique_ptr<CrossOriginOpenerPolicyReporter> coop_reporter) {
  coop_access_report_manager_.set_coop_reporter(std::move(coop_reporter));
}

bool RenderFrameHostImpl::IsCredentialless() const {
  return policy_container_host_->policies().is_credentialless;
}

bool RenderFrameHostImpl::IsLastCrossDocumentNavigationStartedByUser() const {
  return last_cross_document_navigation_started_by_user_;
}

void RenderFrameHostImpl::OnCreateChildFrame(
    int new_routing_id,
    mojo::PendingAssociatedRemote<mojom::Frame> frame_remote,
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
        browser_interface_broker_receiver,
    blink::mojom::PolicyContainerBindParamsPtr policy_container_bind_params,
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterfaceProvider>
        associated_interface_provider_receiver,
    blink::mojom::TreeScopeType scope,
    const std::string& frame_name,
    const std::string& frame_unique_name,
    bool is_created_by_script,
    const blink::LocalFrameToken& frame_token,
    const base::UnguessableToken& devtools_frame_token,
    const blink::DocumentToken& document_token,
    const blink::FramePolicy& frame_policy,
    const blink::mojom::FrameOwnerProperties& frame_owner_properties,
    blink::FrameOwnerElementType owner_type,
    ukm::SourceId document_ukm_source_id) {
  // TODO(lukasza): Call ReceivedBadMessage when |frame_unique_name| is empty.
  DCHECK(!frame_unique_name.empty());
  DCHECK(browser_interface_broker_receiver.is_valid());
  DCHECK(policy_container_bind_params->receiver.is_valid());
  DCHECK(associated_interface_provider_receiver.is_valid());
  DCHECK(devtools_frame_token);

  // The RenderFrame corresponding to this host sent an IPC message to create a
  // child, but by the time we get here, it's possible for its process to have
  // disconnected (maybe due to browser shutdown). Ignore such messages.
  if (!is_render_frame_created())
    return;

  // Only active and prerendered documents are allowed to create child
  // frames.
  if (lifecycle_state() != LifecycleStateImpl::kPrerendering) {
    // The RenderFrame corresponding to this host sent an IPC message to create
    // a child, but by the time we get here, it's possible for the
    // RenderFrameHost to become inactive. Ignore such messages.
    if (IsInactiveAndDisallowActivation(
            DisallowActivationReasonId::kCreateChildFrame))
      return;
  }

  // `new_routing_id`, `frame_token`, `devtools_frame_token` and
  // `document_token` were generated on the browser's IO thread and not taken
  // from the renderer process.
  FrameTreeNode* new_frame_tree_node = frame_tree_->AddFrame(
      this, GetProcess()->GetID(), new_routing_id, std::move(frame_remote),
      std::move(browser_interface_broker_receiver),
      std::move(policy_container_bind_params),
      std::move(associated_interface_provider_receiver), scope, frame_name,
      frame_unique_name, is_created_by_script, frame_token,
      devtools_frame_token, document_token, frame_policy,
      frame_owner_properties, was_discarded_, owner_type,
      /*is_dummy_frame_for_inner_tree=*/false);

  // Record the DocumentCreated identifiability metric for initial empty
  // documents in child frames (other cases are taken care of by the
  // NavigationRequest).
  //
  // Note: We do not want to record the corresponding DocumentCreated UKM event
  // here, see https://crbug.com/1326431.
  new_frame_tree_node->current_frame_host()->RecordDocumentCreatedUkmEvent(
      GetLastCommittedOrigin(), document_ukm_source_id, ukm::UkmRecorder::Get(),
      /*only_record_identifiability_metric=*/true);
}

void RenderFrameHostImpl::OnPreloadingHeuristicsModelDone(const GURL& url,
                                                          float score) {
  if (auto* preloading_decider =
          PreloadingDecider::GetOrCreateForCurrentDocument(this)) {
    preloading_decider->OnPreloadingHeuristicsModelDone(url, score);
  }
}

void RenderFrameHostImpl::CreateChildFrame(
    int new_routing_id,
    mojo::PendingAssociatedRemote<mojom::Frame> frame_remote,
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
        browser_interface_broker_receiver,
    blink::mojom::PolicyContainerBindParamsPtr policy_container_bind_params,
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterfaceProvider>
        associated_interface_provider_receiver,
    blink::mojom::TreeScopeType scope,
    const std::string& frame_name,
    const std::string& frame_unique_name,
    bool is_created_by_script,
    const blink::FramePolicy& frame_policy,
    blink::mojom::FrameOwnerPropertiesPtr frame_owner_properties,
    blink::FrameOwnerElementType owner_type,
    ukm::SourceId document_ukm_source_id) {
  blink::LocalFrameToken frame_token;
  base::UnguessableToken devtools_frame_token;
  blink::DocumentToken document_token;
  if (!static_cast<RenderProcessHostImpl*>(GetProcess())
           ->TakeFrameTokensForFrameRoutingID(new_routing_id, frame_token,
                                              devtools_frame_token,
                                              document_token)) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFH_CREATE_CHILD_FRAME_TOKENS_NOT_FOUND);
    return;
  }

  // Documents create iframes, iframes host new documents. Both are associated
  // with sandbox flags. They are required to be stricter or equal to their
  // owner when they are created, as we go down.
  if (frame_policy.sandbox_flags !=
      (frame_policy.sandbox_flags | active_sandbox_flags())) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFH_CREATE_CHILD_FRAME_SANDBOX_FLAGS);
    return;
  }

  // TODO(crbug.com/1145708). The interface exposed to tests should
  // match the mojo interface.
  OnCreateChildFrame(new_routing_id, std::move(frame_remote),
                     std::move(browser_interface_broker_receiver),
                     std::move(policy_container_bind_params),
                     std::move(associated_interface_provider_receiver), scope,
                     frame_name, frame_unique_name, is_created_by_script,
                     frame_token, devtools_frame_token, document_token,
                     frame_policy, *frame_owner_properties, owner_type,
                     document_ukm_source_id);
}

void RenderFrameHostImpl::DidNavigate(
    const mojom::DidCommitProvisionalLoadParams& params,
    NavigationRequest* navigation_request,
    bool was_within_same_document) {
  // The `url` and `origin` of the current document are stored in the
  // RenderFrameHost, because:
  // - The FrameTreeNode represents the frame.
  // - The RenderFrameHost represents a document hosted inside the frame.
  //
  // The URL is set regardless of whether it's for a net error or not.
  SetLastCommittedUrl(params.url);
  SetLastCommittedOrigin(params.origin);

  // If the navigation was a cross-document navigation and it's not the
  // synchronous about:blank commit, then it committed a document that is not
  // the initial empty document.
  if (!navigation_request->IsSameDocument() &&
      (!navigation_request->is_synchronous_renderer_commit() ||
       !navigation_request->GetURL().IsAboutBlank())) {
    SetNotInitialEmptyDocument();
  }

  isolation_info_ = ComputeIsolationInfoInternal(
      GetLastCommittedOrigin(), isolation_info_.request_type(),
      navigation_request->is_credentialless(),
      navigation_request->ComputeFencedFrameNonce());

  // Separately, update the frame's last successful URL except for net error
  // pages, since those do not end up in the correct process after transfers
  // (see https://crbug.com/560511).  Instead, the next cross-process navigation
  // or transfer should decide whether to swap as if the net error had not
  // occurred.
  // TODO(creis): Remove this block and always set the URL.
  // See https://crbug.com/588314.
  if (!navigation_request->DidEncounterError())
    last_successful_url_ = params.url;

  renderer_url_info_.last_document_url = GetLastDocumentURL(
      navigation_request, params, is_error_document_, renderer_url_info_);

  // Set the last committed HTTP method and POST ID. Note that we're setting
  // this here instead of in DidCommitNewDocument because same-document
  // navigations triggered by the History API (history.replaceState/pushState)
  // will reset the method to "GET" (while fragment navigations won't).
  // TODO(arthursonzogni): Stop relying on DidCommitProvisionalLoadParams. Use
  // the NavigationRequest instead. The browser process doesn't need to rely on
  // the renderer process.
  last_http_method_ = params.method;
  last_post_id_ = params.post_id;

  // TODO(arthursonzogni): Stop relying on DidCommitProvisionalLoadParams. Use
  // the NavigationRequest instead. The browser process doesn't need to rely on
  // the renderer process.
  last_http_status_code_ = params.http_status_code;

  // Sets whether the last navigation has user gesture/transient activation or
  // not.
  last_committed_common_params_has_user_gesture_ =
      navigation_request->common_params().has_user_gesture;

  // Sets whether the last cross-document navigation was initiated from the
  // browser (e.g. typing on the location bar) or from the renderer while having
  // transient user activation
  if (!was_within_same_document) {
    last_cross_document_navigation_started_by_user_ =
        !navigation_request->IsRendererInitiated() ||
        (navigation_request->begin_params()
             .initiator_activation_and_ad_status !=
         blink::mojom::NavigationInitiatorActivationAndAdStatus::
             kDidNotStartWithTransientActivation);
  }

  // Navigations that activate an existing bfcached or prerendered document do
  // not create a new document.
  bool did_create_new_document =
      !navigation_request->IsPageActivation() && !was_within_same_document;
  if (did_create_new_document)
    DidCommitNewDocument(params, navigation_request);

  // When the frame hosts a different document, its state must be replicated
  // via its proxies to the other processes where it appears as remote.
  //
  // This includes new documents. It also includes documents restored from the
  // BackForwardCache. This is because the cached state in
  // BrowsingContextState::replication_state_ needs to be refreshed with the
  // actual values.
  if (!navigation_request->IsSameDocument()) {
    // Permissions policy's inheritance from parent frame's permissions policy
    // is through accessing parent frame's security context(either remote or
    // local) when initializing child's security context, so the update to
    // proxies is needed.
    browsing_context_state_->UpdateFramePolicyHeaders(
        active_sandbox_flags(), permissions_policy_header_);
    // Document policy's inheritance from parent frame's required document
    // policy is done at |HTMLFrameOwnerElement::UpdateRequiredPolicy|. Parent
    // frame owns both parent's required document policy and child frame's frame
    // owner element which contains child's required document policy, so there
    // is no need to store required document policy in proxies.
  }
}

void RenderFrameHostImpl::SetLastCommittedOrigin(const url::Origin& origin) {
  last_committed_origin_ = origin;
}

void RenderFrameHostImpl::SetInheritedBaseUrl(const GURL& inherited_base_url) {
  if (blink::features::IsNewBaseUrlInheritanceBehaviorEnabled()) {
    inherited_base_url_ = inherited_base_url;
  }
}

void RenderFrameHostImpl::SetLastCommittedOriginForTesting(
    const url::Origin& origin) {
  SetLastCommittedOrigin(origin);
}

const url::Origin& RenderFrameHostImpl::ComputeTopFrameOrigin(
    const url::Origin& frame_origin) const {
  if (is_main_frame()) {
    return frame_origin;
  }

  DCHECK(parent_);
  // It's important to go through parent_ rather than via
  // frame_free_->root() here in case we're in process of being deleted, as the
  // latter might point to what our ancestor is being replaced with rather than
  // the actual ancestor.
  RenderFrameHostImpl* host = parent_;
  while (host->parent_) {
    host = host->parent_;
  }
  return host->GetLastCommittedOrigin();
}

void RenderFrameHostImpl::SetStorageKey(const blink::StorageKey& storage_key) {
  storage_key_ = storage_key;
}

net::IsolationInfo RenderFrameHostImpl::ComputeIsolationInfoForNavigation(
    const GURL& destination) {
  return ComputeIsolationInfoForNavigation(
      destination, IsCredentialless(),
      /*fenced_frame_nonce_for_navigation=*/absl::nullopt);
}

net::IsolationInfo RenderFrameHostImpl::ComputeIsolationInfoForNavigation(
    const GURL& destination,
    bool is_credentialless,
    absl::optional<base::UnguessableToken> fenced_frame_nonce_for_navigation) {
  net::IsolationInfo::RequestType request_type =
      is_main_frame() ? net::IsolationInfo::RequestType::kMainFrame
                      : net::IsolationInfo::RequestType::kSubFrame;
  return ComputeIsolationInfoInternal(url::Origin::Create(destination),
                                      request_type, is_credentialless,
                                      fenced_frame_nonce_for_navigation);
}

net::IsolationInfo
RenderFrameHostImpl::ComputeIsolationInfoForSubresourcesForPendingCommit(
    const url::Origin& main_world_origin,
    bool is_credentialless,
    absl::optional<base::UnguessableToken> fenced_frame_nonce_for_navigation) {
  return ComputeIsolationInfoInternal(
      main_world_origin, net::IsolationInfo::RequestType::kOther,
      is_credentialless, fenced_frame_nonce_for_navigation);
}

net::SiteForCookies RenderFrameHostImpl::ComputeSiteForCookies() {
  return isolation_info_.site_for_cookies();
}

net::IsolationInfo RenderFrameHostImpl::ComputeIsolationInfoInternal(
    const url::Origin& frame_origin,
    net::IsolationInfo::RequestType request_type,
    bool is_credentialless,
    absl::optional<base::UnguessableToken> fenced_frame_nonce_for_navigation) {
  url::Origin top_frame_origin = ComputeTopFrameOrigin(frame_origin);
  net::SchemefulSite top_frame_site = net::SchemefulSite(top_frame_origin);

  net::SiteForCookies candidate_site_for_cookies(top_frame_site);

  // Walk up the frame tree to check SiteForCookies.
  //
  // If |request_type| is kOther, then IsolationInfo is being computed
  // for subresource requests. Check/compute starting from the frame itself.
  // Otherwise, it's OK to skip checking the frame itself since it's stored in
  // IsolationInfo for each request and will be validated later anyway.
  //
  // If origins are not consistent with the candidate SiteForCookies
  // of the main document, SameSite cookies may not be used.
  const RenderFrameHostImpl* initial_rfh = this;
  if (request_type != net::IsolationInfo::RequestType::kOther)
    initial_rfh = this->parent_;

  for (const RenderFrameHostImpl* rfh = initial_rfh; rfh; rfh = rfh->parent_) {
    const url::Origin& cur_origin =
        rfh == this ? frame_origin : rfh->last_committed_origin_;
    net::SchemefulSite cur_site = net::SchemefulSite(cur_origin);

    candidate_site_for_cookies.CompareWithFrameTreeSiteAndRevise(cur_site);
  }

  // Reset the SiteForCookies if the top frame origin is of a scheme that should
  // always be treated as the SiteForCookies.
  if (GetContentClient()
          ->browser()
          ->ShouldTreatURLSchemeAsFirstPartyWhenTopLevel(
              top_frame_origin.scheme(),
              GURL::SchemeIsCryptographic(frame_origin.scheme()))) {
    candidate_site_for_cookies = net::SiteForCookies(top_frame_site);
  }

  absl::optional<base::UnguessableToken> nonce =
      ComputeNonce(is_credentialless, fenced_frame_nonce_for_navigation);
  return net::IsolationInfo::Create(request_type, top_frame_origin,
                                    frame_origin, candidate_site_for_cookies,
                                    nonce);
}

absl::optional<base::UnguessableToken> RenderFrameHostImpl::ComputeNonce(
    bool is_credentialless,
    absl::optional<base::UnguessableToken> fenced_frame_nonce_for_navigation) {
  // If it's a credentialless frame tree, use its nonce even if it's within a
  // fenced frame tree to maintain the guarantee that a credentialless frame
  // tree has a unique nonce.
  if (is_credentialless) {
    RenderFrameHostImpl* main_rfh = this;
    while (main_rfh->parent_ && !main_rfh->IsFencedFrameRoot()) {
      main_rfh = main_rfh->parent_;
    }
    return main_rfh->credentialless_iframes_nonce();
  }

  // Otherwise, use the fenced frame nonce for this navigation.
  // If this call is for a pending navigation, the fenced frame nonce should
  // have been computed with `NavigationRequest::ComputeFencedFrameNonce()` and
  // passed in `fenced_frame_nonce_for_navigation`. If there is no navigation
  // associated with this call, then we get the nonce from this RFHI's
  // FrameTreeNode with FrameTreeNode::GetFencedFrameNonce().
  //
  // Note that MPArch will ensure that fenced frame tree within an
  // credentialless iframe does not have `is_credentialless` set to true. The
  // nonce was moved from PageImpl to RenderFrameHostImpl to fix
  // crbug.com/1287458. In the case of a credentialless iframe embedded in a
  // fenced frame, we get the `credentialless_iframes_nonce_` of the fenced
  // frame root to prevent credentialless iframes embedded inside a fenced frame
  // from sharing nonce with credentialless iframes outside the fenced frame.
  if (fenced_frame_nonce_for_navigation.has_value()) {
    return fenced_frame_nonce_for_navigation;
  }
  return frame_tree_node_->GetFencedFrameNonce();
}

bool RenderFrameHostImpl::IsThirdPartyStoragePartitioningEnabled(
    const url::Origin& new_rfh_origin) {
  const std::vector<RenderFrameHostImpl*> ancestor_chain =
      GetAncestorChainForStorageKeyCalculation(new_rfh_origin);
  RenderFrameHostImpl* main_frame_for_storage_partitioning =
      ancestor_chain.back();
  // If we're in the main frame the state of third-party storage partitioning
  // doesn't matter as the StorageKey will be first-party no matter what.
  if (main_frame_for_storage_partitioning == this) {
    return false;
  }

  RuntimeFeatureStateDocumentData* rfs_document_data_for_storage_key =
      RuntimeFeatureStateDocumentData::GetForCurrentDocument(
          main_frame_for_storage_partitioning);

  DCHECK(rfs_document_data_for_storage_key);

  // If the deprecation trial is enabled, we have directive to override the
  // current value of net::features::ThirdPartyStoragePartitioning.
  if (rfs_document_data_for_storage_key->runtime_feature_state_read_context()
          .IsDisableThirdPartyStoragePartitioningEnabled()) {
    return false;
  }
  // Compile the list of third-party origins we need to check in addition to the
  // main frame origin. Ensure that the `new_rfh_origin` is used for this frame,
  // rather than its last-committed origin.
  CHECK_EQ(ancestor_chain[0], this);
  std::vector<url::Origin> third_party_origins = {new_rfh_origin};
  for (size_t i = 1; i < ancestor_chain.size() - 1; ++i) {
    third_party_origins.push_back(ancestor_chain[i]->GetLastCommittedOrigin());
  }
  // If the deprecation trial is enabled for this third-party frame or parent
  // frame we have directive to override the current value of
  // net::features::ThirdPartyStoragePartitioning.
  if (rfs_document_data_for_storage_key->runtime_feature_state_read_context()
          .IsDisableThirdPartyStoragePartitioningEnabledForThirdParty(
              main_frame_for_storage_partitioning->GetLastCommittedOrigin(),
              third_party_origins)) {
    return false;
  }
  // If the enterprise policy blocks, we have directive to override the
  // current value of net::features::ThirdPartyStoragePartitioning.
  // We can safely read the last committed-origin (even during navigation)
  // as we know we are not in the main-frame since that case is filtered above.
  if (!GetContentClient()->browser()->IsThirdPartyStoragePartitioningAllowed(
          GetBrowserContext(),
          main_frame_for_storage_partitioning->GetLastCommittedOrigin())) {
    return false;
  }
  return blink::StorageKey::IsThirdPartyStoragePartitioningEnabled();
}

std::vector<RenderFrameHostImpl*>
RenderFrameHostImpl::GetAncestorChainForStorageKeyCalculation(
    const url::Origin& new_rfh_origin) {
  std::vector<RenderFrameHostImpl*> ancestor_chain;
  RenderFrameHostImpl* current = this;
  while (current) {
    ancestor_chain.push_back(current);
    current = current->parent_;
  }

  // Make sure to always use the `new_rfh_origin` when referring to the current
  // RenderFrameHost. The origin might differ when the RenderFrameHost is reused
  // when SiteIsolation is off.
  auto origin = [&](RenderFrameHostImpl* rfh) {
    return rfh == this ? new_rfh_origin : rfh->GetLastCommittedOrigin();
  };

  // When the top level RenderFrameHost is a Chrome extension, with host
  // permissions to its child in the ancestor chain, then behave "as-if" the
  // child was the top-level one computing the StorageKey.
  //
  // https://github.com/wanderview/quota-storage-partitioning/blob/main/explainer.md#interaction-with-extension-pages
  //
  // Sites with host permissions are saved in
  // `browser_context->GetSharedCorsOriginAccessList()` because they are also
  // used to bypass CORS restrictions. We can reuse this permissions list here
  // because sites that are explicitly granted access permissions should also be
  // able to access partitioned storage based not partitioned by the top level
  // extension URL. A origin will only have access to another origin via
  // OriginAccessList if the origin is an extension.
  bool ignore_top_level_extension =
      !is_main_frame() &&
      GetBrowserContext()
              ->GetSharedCorsOriginAccessList()
              ->GetOriginAccessList()
              .CheckAccessState(origin(ancestor_chain.end()[-1]),
                                origin(ancestor_chain.end()[-2]).GetURL()) ==
          network::cors::OriginAccessList::AccessState::kAllowed;
  if (ignore_top_level_extension) {
    ancestor_chain.pop_back();
  }
  return ancestor_chain;
}

blink::StorageKey RenderFrameHostImpl::CalculateStorageKey(
    const url::Origin& new_rfh_origin,
    const base::UnguessableToken* nonce) {
  if (nonce) {
    // If the nonce isn't null, we can use the simpler form of the constructor.
    return blink::StorageKey::CreateWithNonce(new_rfh_origin, *nonce);
  }

  if (base::FeatureList::IsEnabled(
          features::kShouldAllowFirstPartyStorageKeyOverrideFromEmbedder) &&
      GetContentClient()->browser()->ShouldUseFirstPartyStorageKey(
          new_rfh_origin)) {
    // Extension subframes should not take their top-level site into account
    // when determining storage access. Thus, we construct all extension frame
    // StorageKeys as first-party using the extension origin.
    return blink::StorageKey::CreateFirstParty(new_rfh_origin);
  }

  const std::vector<RenderFrameHostImpl*> ancestor_chain =
      GetAncestorChainForStorageKeyCalculation(new_rfh_origin);

  // Make sure to always use the `new_rfh_origin` when referring to the current
  // RenderFrameHost. The origin might differ when the RenderFrameHost is reused
  // when SiteIsolation is off.
  auto origin = [&](RenderFrameHostImpl* rfh) {
    return rfh == this ? new_rfh_origin : rfh->GetLastCommittedOrigin();
  };
  net::SchemefulSite top_level_site(origin(ancestor_chain.back()));

  // Compute the AncestorChainBit. It represents whether every ancestors are
  // all same-site or not. If `origin` or `top_level_site` is opaque the bit
  // must be kCrossSite as this is the default (which won't be serialized).
  blink::mojom::AncestorChainBit ancestor_chain_bit =
      blink::mojom::AncestorChainBit::kSameSite;
  if (!new_rfh_origin.opaque() && !top_level_site.opaque()) {
    for (auto* ancestor : ancestor_chain) {
      if (top_level_site != net::SchemefulSite(origin(ancestor))) {
        ancestor_chain_bit = blink::mojom::AncestorChainBit::kCrossSite;
        break;
      }
    }
  } else {
    ancestor_chain_bit = blink::mojom::AncestorChainBit::kCrossSite;
  }

  // We want the RuntimeFeatureStateReadContext from the effective main frame
  // (keeping in mind `ignore_top_level_extension`).
  bool is_third_party_storage_partitioning_allowed =
      IsThirdPartyStoragePartitioningEnabled(new_rfh_origin);

  return blink::StorageKey::Create(new_rfh_origin, top_level_site,
                                   ancestor_chain_bit,
                                   is_third_party_storage_partitioning_allowed);
}

void RenderFrameHostImpl::SetOriginDependentStateOfNewFrame(
    RenderFrameHostImpl* creator_frame) {
  // This method should only be called for *new* frames, that haven't committed
  // a navigation yet.
  DCHECK(!has_committed_any_navigation_);
  DCHECK(GetLastCommittedOrigin().opaque());

  url::Origin creator_origin =
      creator_frame ? creator_frame->GetLastCommittedOrigin() : url::Origin();

  // Calculate and set |new_frame_origin|.
  bool new_frame_should_be_sandboxed =
      network::mojom::WebSandboxFlags::kOrigin ==
      (browsing_context_state_->active_sandbox_flags() &
       network::mojom::WebSandboxFlags::kOrigin);
  url::Origin new_frame_origin = new_frame_should_be_sandboxed
                                     ? creator_origin.DeriveNewOpaqueOrigin()
                                     : creator_origin;
  isolation_info_ = ComputeIsolationInfoInternal(
      new_frame_origin, net::IsolationInfo::RequestType::kOther,
      IsCredentialless(),
      /*fenced_frame_nonce_for_navigation=*/absl::nullopt);
  SetLastCommittedOrigin(new_frame_origin);

  if (creator_frame) {
    // If we're given a parent/opener frame, copy the
    // RuntimeFeatureStateReadContext.
    RuntimeFeatureStateDocumentData* rfs_document_data_from_creator =
        RuntimeFeatureStateDocumentData::GetForCurrentDocument(creator_frame);
    DCHECK(rfs_document_data_from_creator);
    RuntimeFeatureStateDocumentData::CreateForCurrentDocument(
        this,
        rfs_document_data_from_creator->runtime_feature_state_read_context());
  } else {
    // Otherwise create a RuntimeFeatureStateContext. We need to construct a
    // RuntimeFeatureStateContext because its constructor initializes default
    // values while the RuntimeFeatureStateReadContext's doesn't.
    RuntimeFeatureStateDocumentData::CreateForCurrentDocument(
        this, blink::RuntimeFeatureStateContext());
  }

  // For the StorageKey, we want the main frame's
  // RuntimeFeatureStateReadContext.
  SetStorageKey(CalculateStorageKey(
      new_frame_origin, base::OptionalToPtr(isolation_info_.nonce())));

  // Apply private network request policy according to our new origin.
  switch (
      GetContentClient()->browser()->ShouldOverridePrivateNetworkRequestPolicy(
          GetBrowserContext(), new_frame_origin)) {
    case ContentBrowserClient::PrivateNetworkRequestPolicyOverride::kForceAllow:
      private_network_request_policy_ =
          network::mojom::PrivateNetworkRequestPolicy::kAllow;
      break;
    case ContentBrowserClient::PrivateNetworkRequestPolicyOverride::
        kBlockInsteadOfWarn:
      private_network_request_policy_ =
          OverrideBlockWithWarn(private_network_request_policy_);
      break;
    case ContentBrowserClient::PrivateNetworkRequestPolicyOverride::kDefault:
      break;
  }

  // Construct the frame's permissions policy only once we know its initial
  // committed origin. It's necessary to wait for the origin because the
  // permissions policy's state depends on the origin, so the PermissionsPolicy
  // object could be configured incorrectly if it were initialized before
  // knowing the value of |last_committed_origin_|. More at crbug.com/1112959.
  ResetPermissionsPolicy();
}

FrameTreeNode* RenderFrameHostImpl::AddChild(
    std::unique_ptr<FrameTreeNode> child,
    int frame_routing_id,
    mojo::PendingAssociatedRemote<mojom::Frame> frame_remote,
    const blink::LocalFrameToken& frame_token,
    const blink::DocumentToken& document_token,
    base::UnguessableToken devtools_frame_token,
    const blink::FramePolicy& frame_policy,
    std::string frame_name,
    std::string frame_unique_name) {
  DCHECK(lifecycle_state_ == LifecycleStateImpl::kActive ||
         lifecycle_state_ == LifecycleStateImpl::kPrerendering);

  // Initialize the RenderFrameHost for the new node.  We always create child
  // frames in the same SiteInstance as the current frame, and they can swap to
  // a different one if they navigate away.
  child->render_manager()->InitChild(
      GetSiteInstance(), frame_routing_id, std::move(frame_remote), frame_token,
      document_token, devtools_frame_token, frame_policy, frame_name,
      frame_unique_name);

  // Other renderer processes in this BrowsingInstance may need to find out
  // about the new frame.  Create a proxy for the child frame in all
  // SiteInstances that have a proxy for the frame's parent, since all frames
  // in a frame tree should have the same set of proxies.
  CHECK(owner_);  // See `owner_` invariants about `lifecycle_state_`.
  owner_->GetRenderFrameHostManager().CreateProxiesForChildFrame(child.get());

  // When the child is added, it hasn't committed any navigation yet - its
  // initial empty document should inherit the origin (the origin may change
  // after the first commit) and other state (such as the
  // RuntimeFeatureStateReadContext) from its parent. See also
  // https://crbug.com/932067.
  child->current_frame_host()->SetOriginDependentStateOfNewFrame(this);

  children_.push_back(std::move(child));
  return children_.back().get();
}

void RenderFrameHostImpl::RemoveChild(FrameTreeNode* child) {
  for (auto iter = children_.begin(); iter != children_.end(); ++iter) {
    if (iter->get() == child) {
      // Subtle: we need to make sure the node is gone from the tree before
      // observers are notified of its deletion.
      std::unique_ptr<FrameTreeNode> node_to_delete(std::move(*iter));
      children_.erase(iter);
      // TODO(dcheng): Removing a subtree is still very piecemeal and somewhat
      // buggy. The entire subtree should be removed as a group, but it can
      // actually happen incrementally. For example, given the frame tree:
      //
      //   A1(A2(B3(A4(B5))))
      //
      //
      // Suppose A1 executes:
      //
      //   window.frames[0].frameElement.remove();
      //
      // What ends up happening is this:
      //
      // 1. Renderer A detaches the subtree beginning at A2.
      // 2. Renderer A starts detaching A2.
      // 3. Renderer A starts detaching B3.
      // 4. Renderer A starts detaching A4.
      // 5. Renderer A starts detaching B5
      // 6. Renderer A reports B5 is complete detaching with the Mojo IPC
      //    `RenderFrameProxyHost::Detach()`, which calls
      //    `RenderFrameHostImpl::DetachFromProxy()`. `DetachFromProxy()`
      //    deletes RenderFrame B5 in renderer B, which means that the unload
      //    handler for B5 runs immediately--before the unload handler for B3.
      //    However, per the spec, the right order to run unload handlers is
      //    top-down (e.g. B3's unload handler should run before B5's in this
      //    scenario).
      node_to_delete->current_frame_host()->DeleteRenderFrame(
          mojom::FrameDeleteIntention::kNotMainFrame);
      RenderFrameHostImpl* speculative_frame_host =
          node_to_delete->render_manager()->speculative_frame_host();
      if (speculative_frame_host) {
        if (speculative_frame_host->lifecycle_state() ==
            LifecycleStateImpl::kPendingCommit) {
          // A speculative RenderFrameHost that has reached `kPendingCommit` has
          // already sent a `CommitNavigation()` to the renderer. Any subsequent
          // IPCs will only be processed after the renderer has already swapped
          // in the provisional RenderFrame and swapped out the provisional
          // frame's reference frame (which is either a RenderFrame or a
          // `blink::RemoteFrame`).
          //
          // Since the swapped out `RenderFrame`/`blink::RemoteFrame` is already
          // gone, a `DeleteRenderFrame()` (routed to the RenderFrame) or a
          // `DetachAndDispose()` (routed to the `blink::RemoteFrame`) won't do
          // anything. The browser must also instruct the already-committed but
          // not-yet-acknowledged speculative RFH to detach itself as well.
          speculative_frame_host->DeleteRenderFrame(
              mojom::FrameDeleteIntention::kNotMainFrame);
        } else {
          // Otherwise, the provisional RenderFrame has not yet been instructed
          // to swap in but is already associated with the RenderFrame or
          // `blink::RemoteFrame` it is expected to replace. The associated
          // `RenderFrame`/`blink::RemoteFrame` (which is still in the frame
          // tree) will be responsible for tearing down any associated
          // provisional RenderFrame, so the browser does not need to take any
          // explicit cleanup actions.
        }
      }
      // No explicit cleanup is needed here for `RenderFrameProxyHost`s.
      // Destroying `FrameTreeNode` destroys the map of `RenderFrameProxyHost`s,
      // and `~RenderFrameProxyHost()` sends a Mojo `DetachAndDispose()` IPC for
      // child frame proxies.
      node_to_delete.reset();
      PendingDeletionCheckCompleted();  // Can delete |this|.
      // |this| is potentially deleted. Do not add code after this.
      return;
    }
  }
}

void RenderFrameHostImpl::ResetChildren() {
  // Remove child nodes from the tree, then delete them. This destruction
  // operation will notify observers. See https://crbug.com/612450 for
  // explanation why we don't just call the std::vector::clear method.
  std::vector<std::unique_ptr<FrameTreeNode>> children;
  children.swap(children_);
  // TODO(dcheng): Ideally, this would be done by messaging all the proxies of
  // this RenderFrameHostImpl to detach the current frame's children, rather
  // than messaging each child's current frame host...
  for (auto& child : children)
    child->current_frame_host()->DeleteRenderFrame(
        mojom::FrameDeleteIntention::kNotMainFrame);
}

void RenderFrameHostImpl::SetLastCommittedUrl(const GURL& url) {
  last_committed_url_ = url;
}

void RenderFrameHostImpl::Detach() {
  // Detach() can be called in both speculative and pending-commit states.
  // - a speculative RenderFrameHost as a result of its associated Frame being
  //   detached (i.e., the Frame in the renderer with a provisional_frame_ field
  //   that points to `this`'s LocalFrame). We don't expect it to self-detach
  //   otherwise.
  // - a pending commit RenderFrameHost might detach itself due to unload events
  //   running that remove it from the tree when swapping it in.
  //
  // In both cases speculative and pending-commit RenderFrameHosts, it's OK to
  // early-return. The logical FrameTreeNode is going to be torn down as well,
  // and the speculative / pending commit RenderFrameHost (which is still
  // strongly owned by the RenderFrameHostManager via unique_ptr) will be torn
  // down then. If we do proceed, this ends up with a use-after-free, since
  // StartPendingDeletionOnSubtree() will call
  // ResetAllNavigationsInSubtreeForFrameDetach(), which deletes `this`.
  if (lifecycle_state() == LifecycleStateImpl::kSpeculative ||
      lifecycle_state() == LifecycleStateImpl::kPendingCommit) {
    return;
  }

  if (!parent_) {
    bad_message::ReceivedBadMessage(GetProcess(),
                                    bad_message::RFH_DETACH_MAIN_FRAME);
    return;
  }

  // A frame is removed while replacing this document with the new one. When it
  // happens, delete the frame and both the new and old documents. Unload
  // handlers aren't guaranteed to run here.
  if (is_waiting_for_unload_ack_) {
    parent_->RemoveChild(GetFrameTreeNodeForUnload());
    return;
  }

  // Ignore Detach Mojo API, if the RenderFrameHost should be left in pending
  // deletion state.
  if (do_not_delete_for_testing_)
    return;

  if (IsPendingDeletion()) {
    // The frame is pending deletion. Detach Mojo API is used to confirm its
    // unload handlers ran. Note that it is possible for a frame to already be
    // in kReadyToBeDeleted. This happens when this RenderFrameHost is pending
    // deletion and is waiting on one of its children to run its unload
    // handler. While running it, it can request its parent to detach itself.
    // See test: SitePerProcessBrowserTest.PartialUnloadHandler.
    if (lifecycle_state() != LifecycleStateImpl::kReadyToBeDeleted)
      SetLifecycleState(LifecycleStateImpl::kReadyToBeDeleted);
    PendingDeletionCheckCompleted();  // Can delete |this|.
    // |this| is potentially deleted. Do not add code after this.
    return;
  }

  // This frame is being removed by the renderer, and it has already executed
  // its unload handler.
  SetLifecycleState(LifecycleStateImpl::kReadyToBeDeleted);

  // Before completing the removal, we still need to wait for all of its
  // descendant frames to execute unload handlers. Start executing those
  // handlers now.
  StartPendingDeletionOnSubtree(PendingDeletionReason::kFrameDetach);
  frame_tree()->FrameUnloading(GetFrameTreeNodeForUnload());

  // Some children with no unload handler may be eligible for immediate
  // deletion. Cut the dead branches now. This is a performance optimization.
  PendingDeletionCheckCompletedOnSubtree();  // Can delete |this|.
  // |this| is potentially deleted. Do not add code after this.
}

void RenderFrameHostImpl::DidFailLoadWithError(const GURL& url,
                                               int32_t error_code) {
  TRACE_EVENT("navigation", "RenderFrameHostImpl::DidFailLoadWithError",
              ChromeTrackEvent::kRenderFrameHost, *this, "error", error_code);

  // Cancel prerendering if DidFailLoadWithError is called on the outermost main
  // document during prerendering. Don't dispatch the DidFailLoad event in such
  // a case as the embedders are unaware of prerender page yet and shouldn't
  // show any user-visible changes from an inactive RenderFrameHost.
  if (!GetParentOrOuterDocument() &&
      CancelPrerendering(
          PrerenderCancellationReason::BuildForLoadingError(error_code))) {
    return;
  }

  GURL validated_url(url);
  GetProcess()->FilterURL(false, &validated_url);

  delegate_->DidFailLoadWithError(this, validated_url, error_code);
}

bool RenderFrameHostImpl::TakingFocusWillCrossFencedBoundary(
    RenderFrameHostImpl* focused_rfh) {
  if (!focused_rfh) {
    return false;
  }

  if (this == focused_rfh) {
    return false;
  }

  if (frame_tree() == focused_rfh->frame_tree()) {
    return false;
  }

  // We only care if the focus change is ENTERING a fenced frame. Focus is still
  // allowed to be pulled out of a fenced frame. This is done because an outer
  // frame should be allowed to re-gain focus from a child frame, and since
  // gating focus in one direction is enough to prevent a communication channel
  // from opening.
  if (!IsNestedWithinFencedFrame()) {
    return false;
  }

  return true;
}

bool RenderFrameHostImpl::VerifyFencedFrameFocusChange(
    RenderFrameHostImpl* focused_rfh) {
  if (GetOutermostMainFrameOrEmbedder()
          ->GetRenderWidgetHost()
          ->HasLostFocus()) {
    ActivateFocusSourceUserActivation();
    return true;
  }

  if (HasTransientUserActivation() || FocusSourceHasTransientUserActivation()) {
    return true;
  }

  if (!TakingFocusWillCrossFencedBoundary(focused_rfh)) {
    return true;
  }

  SCOPED_CRASH_KEY_BOOL("FencedFocus", "is_fenced_root", IsFencedFrameRoot());

  // Information about the previously focused frame
  SCOPED_CRASH_KEY_BOOL("FencedFocus", "current_in_fenced_tree",
                        focused_rfh->IsNestedWithinFencedFrame());
  SCOPED_CRASH_KEY_BOOL("FencedFocus", "current_is_fenced_root",
                        focused_rfh->IsFencedFrameRoot());

  // If none of the other cases were hit, disallow the focus change.
  // TODO(crbug.com/1458985): We will later badmessage the renderer, but, for
  // now, we will dump without crashing to monitor if any legitimate cases are
  // reaching this point.
  if (base::FeatureList::IsEnabled(features::kFencedFramesEnforceFocus)) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFH_FOCUS_ACROSS_FENCED_BOUNDARY);
  } else {
    base::debug::DumpWithoutCrashing();
  }
  return false;
}

void RenderFrameHostImpl::DidFocusFrame() {
  TRACE_EVENT("navigation", "RenderFrameHostImpl::DidFocusFrame",
              ChromeTrackEvent::kRenderFrameHost, *this,
              ChromeTrackEvent::kSiteInstanceGroup,
              *GetSiteInstance()->group());
  // We don't handle this IPC signal for non-active RenderFrameHost.
  if (!IsActive())
    return;

  DCHECK(owner_);  // See `owner_` invariants about `IsActive()`.
  owner_->SetFocusedFrame(GetSiteInstance()->group());

#if BUILDFLAG(IS_WIN)
  // If the frame has a url, notify the view to allow it to supply the Url to
  // any interested IME (e.g. Windows 11's TSF uses this information).
  if (!last_committed_url_.is_empty()) {
    RenderWidgetHostView* view = render_view_host_->GetWidget()->GetView();
    if (view) {
      ui::TextInputClient* input_client = view->GetTextInputClient();
      if (input_client) {
        input_client->NotifyOnFrameFocusChanged();
      }
    }
  }
#endif  // BUILDFLAG(IS_WIN)

  // The lost focus tracker is cleared out after a focus call.
  GetOutermostMainFrameOrEmbedder()->GetRenderWidgetHost()->ResetLostFocus();
}

void RenderFrameHostImpl::DidCallFocus() {
  // This should not occur for prerenders but may occur for pages in
  // the BackForwardCache depending on timing.
  if (!IsActive())
    return;
  delegate_->DidCallFocus();
}

void RenderFrameHostImpl::CancelInitialHistoryLoad() {
  // A Javascript navigation interrupted the initial history load.  Check if an
  // initial subframe cross-process navigation needs to be canceled as a result.
  // TODO(creis, clamy): Cancel any cross-process navigation.
  NOTIMPLEMENTED();
}

void RenderFrameHostImpl::DidChangeBackForwardCacheDisablingFeatures(
    BackForwardCacheBlockingDetails details) {
  renderer_reported_bfcache_blocking_details_ = std::move(details);

  MaybeEvictFromBackForwardCache();

  if (back_forward_cache_disabling_features_callback_for_testing_) {
    back_forward_cache_disabling_features_callback_for_testing_.Run(
        GetBackForwardCacheDisablingFeatures());
  }
}

using BackForwardCacheDisablingFeatureHandle =
    RenderFrameHostImpl::BackForwardCacheDisablingFeatureHandle;

BackForwardCacheDisablingFeatureHandle::
    BackForwardCacheDisablingFeatureHandle() {
  // |render_frame_host_| will be null, so this value is never used.
  feature_ = BackForwardCacheDisablingFeature::kDummy;
}

BackForwardCacheDisablingFeatureHandle::BackForwardCacheDisablingFeatureHandle(
    BackForwardCacheDisablingFeatureHandle&& other) = default;

BackForwardCacheDisablingFeatureHandle::BackForwardCacheDisablingFeatureHandle(
    RenderFrameHostImpl* render_frame_host,
    BackForwardCacheDisablingFeature feature)
    : render_frame_host_(render_frame_host->GetWeakPtr()), feature_(feature) {
  CHECK(render_frame_host_);
  render_frame_host_->OnBackForwardCacheDisablingFeatureUsed(feature_);
}

void RenderFrameHostImpl::RecordBackForwardCacheDisablingReason(
    BackForwardCacheDisablingFeature feature) {
  ++browser_reported_bfcache_disabling_features_counts_[feature];
}

void RenderFrameHostImpl::OnBackForwardCacheDisablingFeatureUsed(
    BackForwardCacheDisablingFeature feature) {
  RecordBackForwardCacheDisablingReason(feature);

  MaybeEvictFromBackForwardCache();
}

void RenderFrameHostImpl::OnBackForwardCacheDisablingStickyFeatureUsed(
    BackForwardCacheDisablingFeature feature) {
  OnBackForwardCacheDisablingFeatureUsed(feature);
}

void RenderFrameHostImpl::OnBackForwardCacheDisablingFeatureRemoved(
    BackForwardCacheDisablingFeature feature) {
  auto it = browser_reported_bfcache_disabling_features_counts_.find(feature);
  DCHECK(it->second >= 1);
  if (it->second == 1) {
    browser_reported_bfcache_disabling_features_counts_.erase(it);
  } else {
    --it->second;
  }
}

RenderFrameHostImpl::BackForwardCacheDisablingFeatures
RenderFrameHostImpl::GetBackForwardCacheDisablingFeatures() const {
  BackForwardCacheDisablingFeatures features;
  for (const auto& details : GetBackForwardCacheBlockingDetails()) {
    features.Put(static_cast<blink::scheduler::WebSchedulerTrackedFeature>(
        details->feature));
  }
  return features;
}

RenderFrameHostImpl::BackForwardCacheBlockingDetails
RenderFrameHostImpl::GetBackForwardCacheBlockingDetails() const {
  BackForwardCacheBlockingDetails combined_details_list =
      DedicatedWorkerHostsForDocument::GetOrCreateForCurrentDocument(
          const_cast<RenderFrameHostImpl*>(this))
          ->GetBackForwardCacheBlockingDetails();

  for (const auto& details : renderer_reported_bfcache_blocking_details_) {
    combined_details_list.push_back(details.Clone());
  }

  for (const auto& it : browser_reported_bfcache_disabling_features_counts_) {
    // Browser reported features do not have JS location details. Create a
    // blocking details struct with only the feature filled.
    auto details_ptr = blink::mojom::BlockingDetails::New();
    details_ptr->feature = static_cast<uint32_t>(it.first);
    combined_details_list.push_back(std::move(details_ptr));
  }
  return combined_details_list;
}

RenderFrameHostImpl::BackForwardCacheDisablingFeatureHandle
RenderFrameHostImpl::RegisterBackForwardCacheDisablingNonStickyFeature(
    BackForwardCacheDisablingFeature feature) {
  return BackForwardCacheDisablingFeatureHandle(this, feature);
}

bool RenderFrameHostImpl::IsFrozen() {
  // TODO(crbug.com/1081920): Account for non-bfcache freezing here as well.
  return lifecycle_state() == LifecycleStateImpl::kInBackForwardCache;
}

void RenderFrameHostImpl::DidCommitProvisionalLoad(
    mojom::DidCommitProvisionalLoadParamsPtr params,
    mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params) {
  if (!MaybeInterceptCommitCallback(nullptr, &params, &interface_params))
    return;

  DCHECK(params);
  DidCommitNavigation(nullptr, std::move(params), std::move(interface_params));
}

void RenderFrameHostImpl::DidCommitPageActivation(
    NavigationRequest* committing_navigation_request,
    mojom::DidCommitProvisionalLoadParamsPtr params) {
  DCHECK(committing_navigation_request->IsPageActivation());
  DCHECK(is_main_frame());

  auto request = navigation_requests_.find(committing_navigation_request);
  CHECK(request != navigation_requests_.end());

  base::TimeTicks navigation_start =
      committing_navigation_request->NavigationStart();
  bool is_prerender_page_activation =
      committing_navigation_request->IsPrerenderedPageActivation();

  std::unique_ptr<NavigationRequest> owned_request = std::move(request->second);
  navigation_requests_.erase(committing_navigation_request);

  // Copy the prerendering frame replication state to have it available after
  // the navigation commit to be able to check that it didn't change, as
  // NavigationRequest will be destroyed by that point.
  blink::mojom::FrameReplicationState prerender_main_frame_replication_state;
  // Copy the prerendering trigger type and the embbeder histogram suffix for
  // metrics before NavigationRequest is destroyed.
  PrerenderTriggerType prerender_trigger_type;
  std::string prerender_embedder_histogram_suffix;
  if (is_prerender_page_activation) {
    prerender_main_frame_replication_state =
        owned_request->prerender_main_frame_replication_state();
    prerender_trigger_type = owned_request->GetPrerenderTriggerType();
    prerender_embedder_histogram_suffix =
        owned_request->GetPrerenderEmbedderHistogramSuffix();
  }

#if DCHECK_IS_ON()
  // We do not support activating a page while subframes have any ongoing
  // NavigationRequest with a NavigationEntry (this would happen on a navigation
  // to an existing entry; this is called a "history navigation"). This would be
  // tricky to support because the NavigationRequest would change its
  // NavigationController in the course of the activation, and while that may be
  // safe for a normal navigation, it has more implications for a history
  // navigation as it already has an associated NavigationEntry and we do not
  // transfer pending NavigationEntries during activation. Fortunately, for
  // prerendering, we do not expect there to be any ongoing history navigations
  // in a subframe, because we maintain a trivial session history, so check that
  // nav_entry_id() is 0 here. Reloading subframes are considered
  // renderer-initiated navigations and do not create a new navigation entry
  // when NavigationRequest is created.
  //
  // Note that due to PrerenderCommitDeferringCondition, the main frame should
  // have no ongoing NavigationRequest at all, so it is not checked here.
  ForEachRenderFrameHost([](RenderFrameHostImpl* rfh) {
    // Interested only in subframes.
    if (rfh->is_main_frame())
      return;
    for (const auto& pair : rfh->navigation_requests_)
      DCHECK_EQ(pair.first->nav_entry_id(), 0);
  });
#endif

  DidCommitNavigationInternal(std::move(owned_request), std::move(params),
                              /*same_document_params=*/nullptr);

  // If any load events occurred pre-activation and were deferred until
  // activation, dispatch them now. This must happen before DidStopLoading() is
  // called because observers expect them to occur before that.
  if (is_prerender_page_activation)
    GetPage().MaybeDispatchLoadEventsOnPrerenderActivation();

  // Try to dispatch DidStopLoading event (note that
  // RenderFrameHostImpl::DidStopLoading implementation won't dispatch the event
  // if the page is still loading). We dispatch it here as it hasn't been
  // dispatched pre-activation because the back-forward cache page is already
  // loaded, whereas, for initial prerendering navigation, prerendered page
  // might still be loading.
  DidStopLoading();

  if (is_prerender_page_activation) {
    // Record metric to check navigation time with prerender activation.
    base::TimeDelta delta = base::TimeTicks::Now() - navigation_start;
    RecordPrerenderActivationTime(delta, prerender_trigger_type,
                                  prerender_embedder_histogram_suffix);

    // We haven't sent any updates to the proxies during prerendering
    // activation, so we need to ensure that the new frame replication being
    // stored in the browser is the same as the old and consistent with the
    // state we've sent to the renderers.
    // TODO - can we check main frame replication state?
    DCHECK(prerender_main_frame_replication_state ==
           frame_tree()->root()->current_replication_state());
  }
}

void RenderFrameHostImpl::StartLoadingForAsyncNavigationApiCommit() {
  // A same document navigation commit was requested, but was intercepted by
  // the navigation event. This should cause loading UI to appear, because
  // unlike other same-document navigations, this one is asynchronous. Note that
  // the renderer notifies the browser after a short delay, in order to lessen
  // the risk of the loading UI jittering if there are several short
  // asynchronous navigations in a row.
  LoadingState previous_frame_tree_loading_state =
      frame_tree()->LoadingTree()->GetLoadingState();
  loading_state_ = LoadingState::LOADING_UI_REQUESTED;
  frame_tree_node()->DidStartLoading(previous_frame_tree_loading_state);
}

void RenderFrameHostImpl::DidCommitSameDocumentNavigation(
    mojom::DidCommitProvisionalLoadParamsPtr params,
    mojom::DidCommitSameDocumentNavigationParamsPtr same_document_params) {
  TRACE_EVENT("navigation",
              "RenderFrameHostImpl::DidCommitSameDocumentNavigation",
              ChromeTrackEvent::kRenderFrameHost, this, "url",
              params->url.possibly_invalid_spec());

  // TODO(peilinwang): remove after the kScrollToTLDOptimization experiment is
  // complete.
  SCOPED_UMA_HISTOGRAM_TIMER(
      "Navigation.DidCommitSameDocumentNavigation.Duration");

  ScopedActiveURL scoped_active_url(params->url,
                                    GetMainFrame()->GetLastCommittedOrigin());
  ScopedCommitStateResetter commit_state_resetter(this);

  // When the frame is pending deletion, the browser is waiting for it to unload
  // properly. In the meantime, because of race conditions, it might tries to
  // commit a same-document navigation before unloading. Similarly to what is
  // done with cross-document navigations, such navigation are ignored. The
  // browser already committed to destroying this RenderFrameHost.
  // See https://crbug.com/805705 and https://crbug.com/930132.
  // TODO(https://crbug.com/1467502): Investigate to see if this can be removed
  // when the NavigationClient interface is implemented.
  //
  // If this is called when the frame is in BackForwardCache, evict the frame
  // to avoid ignoring the renderer-initiated navigation, which the frame
  // might not expect.
  //
  // If this is called when the frame is in Prerendering, do not cancel
  // Prerendering as prerendered frames can be navigated, including
  // same-document navigations like push/replaceState.
  if (lifecycle_state() != LifecycleStateImpl::kPrerendering &&
      IsInactiveAndDisallowActivation(
          DisallowActivationReasonId::kCommitSameDocumentNavigation)) {
    return;
  }

  // Check if the navigation matches a stored same-document NavigationRequest.
  // In that case it is browser-initiated.
  auto request_entry =
      same_document_navigation_requests_.find(params->navigation_token);
  bool is_browser_initiated =
      (request_entry != same_document_navigation_requests_.end());
  std::unique_ptr<NavigationRequest> request =
      is_browser_initiated ? std::move(request_entry->second) : nullptr;
  same_document_navigation_requests_.erase(params->navigation_token);
  if (!MaybeInterceptCommitCallback(request.get(), &params, nullptr)) {
    return;
  }
  if (!DidCommitNavigationInternal(std::move(request), std::move(params),
                                   std::move(same_document_params))) {
    return;
  }

  // Since we didn't early return, it's safe to keep the commit state.
  commit_state_resetter.disable();
}

void RenderFrameHostImpl::DidOpenDocumentInputStream(const GURL& url) {
  // Check if the URL can actually be committed to the current origin. When
  // checking, the restrictions should be the same as a same-document navigation
  // since document.open() can only update the URL to a same-origin URL.
  if (!ValidateURLAndOrigin(url, last_committed_origin_,
                            /*is_same_document_navigation=*/true,
                            /*navigation_request=*/nullptr)) {
    return;
  }
  // Filter the URL, then update `renderer_url_info_`'s `last_document_url`.
  // Note that we won't update `last_committed_url_` because this doesn't really
  // count as committing a real navigation and won't update NavigationEntry etc.
  // See https://crbug.com/1046898 and https://github.com/whatwg/html/pull/6649
  // for more details.
  GURL filtered_url(url);
  GetProcess()->FilterURL(/*empty_allowed=*/false, &filtered_url);
  renderer_url_info_.last_document_url = filtered_url;
  DidOpenDocumentInputStream();
}

RenderWidgetHostImpl* RenderFrameHostImpl::GetRenderWidgetHost() {
  RenderFrameHostImpl* frame = this;
  while (frame) {
    if (frame->GetLocalRenderWidgetHost())
      return frame->GetLocalRenderWidgetHost();
    frame = frame->GetParent();
  }

  NOTREACHED();
  return nullptr;
}

RenderWidgetHostView* RenderFrameHostImpl::GetView() {
  return GetRenderWidgetHost()->GetView();
}

GlobalRenderFrameHostId RenderFrameHostImpl::GetGlobalId() const {
  return GlobalRenderFrameHostId(GetProcess()->GetID(), GetRoutingID());
}

bool RenderFrameHostImpl::HasPendingCommitNavigation() const {
  return HasPendingCommitForCrossDocumentNavigation() ||
         !same_document_navigation_requests_.empty();
}

bool RenderFrameHostImpl::HasPendingCommitForCrossDocumentNavigation() const {
  return !navigation_requests_.empty();
}

void RenderFrameHostImpl::RecordMetricsForBlockedGetFrameHostAttempt(
    bool commit_attempt) {
  DCHECK_EQ(lifecycle_state_, LifecycleStateImpl::kPendingCommit);
  DCHECK_EQ(1u, navigation_requests_.size());
  navigation_requests_.begin()
      ->first->RecordMetricsForBlockedGetFrameHostAttempt(commit_attempt);
}

NavigationRequest* RenderFrameHostImpl::GetSameDocumentNavigationRequest(
    const base::UnguessableToken& token) {
  auto request = same_document_navigation_requests_.find(token);
  return (request == same_document_navigation_requests_.end())
             ? nullptr
             : request->second.get();
}

void RenderFrameHostImpl::ResetOwnedNavigationRequests(
    NavigationDiscardReason reason) {
  if (lifecycle_state_ == LifecycleStateImpl::kPendingCommit) {
    // Pending commit RenderFrameHosts should never have same document
    // navigation requests yet, as they do not have a real document committed
    // yet.
    DCHECK(same_document_navigation_requests_.empty());

    if (ShouldQueueNavigationsWhenPendingCommitRFHExists() &&
        HasPendingCommitForCrossDocumentNavigation()) {
      // With navigation queueing, pending commit navigations shouldn't get
      // canceled, unless the FrameTreeNode or renderer process
      // is gone/will be gone soon.
      CHECK(reason == NavigationDiscardReason::kRenderProcessGone ||
            reason == NavigationDiscardReason::kWillRemoveFrame);
    }
  }

  // Move the NavigationRequests to new maps first before deleting them. This
  // avoids issues if a re-entrant call is made when a NavigationRequest is
  // being deleted (e.g., if the process goes away as the tab is closing).
  std::map<NavigationRequest*, std::unique_ptr<NavigationRequest>>
      navigation_requests;
  navigation_requests_.swap(navigation_requests);

  base::flat_map<base::UnguessableToken, std::unique_ptr<NavigationRequest>>
      same_document_navigation_requests;
  same_document_navigation_requests_.swap(same_document_navigation_requests);
}

void RenderFrameHostImpl::SetNavigationRequest(
    std::unique_ptr<NavigationRequest> navigation_request) {
  DCHECK(navigation_request);

  if (NavigationTypeUtils::IsSameDocument(
          navigation_request->common_params().navigation_type)) {
    loading_state_ = LoadingState::LOADING_WITHOUT_UI;
    same_document_navigation_requests_[navigation_request->commit_params()
                                           .navigation_token] =
        std::move(navigation_request);
    return;
  }
  loading_state_ = LoadingState::LOADING_UI_REQUESTED;
  navigation_requests_[navigation_request.get()] =
      std::move(navigation_request);
}

const scoped_refptr<NavigationOrDocumentHandle>&
RenderFrameHostImpl::GetNavigationOrDocumentHandle() {
  if (!document_associated_data_->navigation_or_document_handle()) {
    document_associated_data_->set_navigation_or_document_handle(
        NavigationOrDocumentHandle::CreateForDocument(GetGlobalId()));
  }
  return document_associated_data_->navigation_or_document_handle();
}

void RenderFrameHostImpl::Unload(RenderFrameProxyHost* proxy, bool is_loading) {
  // The end of this event is in OnUnloadACK when the RenderFrame has completed
  // the operation and sends back an IPC message.
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("navigation", "RenderFrameHostImpl::Unload",
                                    TRACE_ID_LOCAL(this), "render_frame_host",
                                    this);

  // If this RenderFrameHost is already pending deletion, it must have already
  // gone through this, therefore just return.
  if (IsPendingDeletion()) {
    NOTREACHED() << "RFH should be in default state when calling Unload.";
    return;
  }

  if (unload_event_monitor_timeout_ && !do_not_delete_for_testing_) {
    unload_event_monitor_timeout_->Start(kUnloadTimeout);
  }

  // TODO(nasko): If the frame is not live, the RFH should just be deleted by
  // simulating the receipt of unload ack.
  is_waiting_for_unload_ack_ = true;

  if (proxy) {
    SetLifecycleState(LifecycleStateImpl::kRunningUnloadHandlers);
    if (IsRenderFrameLive()) {
      GetMojomFrameInRenderer()->Unload(
          is_loading,
          proxy->frame_tree_node()->current_replication_state().Clone(),
          proxy->GetFrameToken(), proxy->CreateAndBindRemoteFrameInterfaces(),
          proxy->CreateAndBindRemoteMainFrameInterfaces());
      // Remember that a `blink::RemoteFrame` was created as part of processing
      // the Unload message above.
      proxy->SetRenderFrameProxyCreated(true);
    }
  } else {
    // RenderDocument: After a local<->local swap, this function is called with
    // a null |proxy|.
    CHECK(ShouldChangeRenderFrameHostOnSameSiteNavigation());

    // The unload handlers already ran for this document during the
    // local<->local swap. Hence, there is no need to send
    // mojom::FrameNavigationControl::Unload here. It can be marked at
    // completed.
    SetLifecycleState(LifecycleStateImpl::kReadyToBeDeleted);
  }

  if (web_ui())
    web_ui()->RenderFrameHostUnloading();

  StartPendingDeletionOnSubtree(PendingDeletionReason::kSwappedOut);
  // Some children with no unload handler may be eligible for deletion. Cut the
  // dead branches now. This is a performance optimization.
  PendingDeletionCheckCompletedOnSubtree();
  // |this| is potentially deleted. Do not add code after this.
}

void RenderFrameHostImpl::UndoCommitNavigation(RenderFrameProxyHost& proxy,
                                               bool is_loading) {
  TRACE_EVENT("navigation", "RenderFrameHostImpl::UndoCommitNavigation",
              "render_frame_host", this);

  DCHECK_EQ(lifecycle_state_, LifecycleStateImpl::kPendingCommit);

  if (IsRenderFrameLive()) {
    // By definition, the browser process has not received the
    // `DidCommitNavgation()`, so the RenderFrameProxyHost endpoints are still
    // bound. Resetting now means any queued IPCs that are still in-flight will
    // be dropped. This is a bit problematic, but it is still less problematic
    // than just crashing the renderer for being in an inconsistent state.
    proxy.TearDownMojoConnection();

    GetMojomFrameInRenderer()->UndoCommitNavigation(
        is_loading,
        proxy.frame_tree_node()->current_replication_state().Clone(),
        proxy.GetFrameToken(), proxy.CreateAndBindRemoteFrameInterfaces(),
        proxy.CreateAndBindRemoteMainFrameInterfaces());
  }

  SetLifecycleState(LifecycleStateImpl::kReadyToBeDeleted);
}

void RenderFrameHostImpl::MaybeDispatchDidFinishLoadOnPrerenderActivation() {
  // Don't dispatch notification if DidFinishLoad has not yet been invoked for
  // `rfh` i.e., when the url is nullopt.
  if (!document_associated_data_
           ->pending_did_finish_load_url_for_prerendering())
    return;

  delegate_->OnDidFinishLoad(
      this, *document_associated_data_
                 ->pending_did_finish_load_url_for_prerendering());

  // Set to nullopt to avoid calling DidFinishLoad twice.
  document_associated_data_
      ->reset_pending_did_finish_load_url_for_prerendering();
}

void RenderFrameHostImpl::MaybeDispatchDOMContentLoadedOnPrerenderActivation() {
  // Don't send a notification if DOM content is not yet loaded.
  if (!document_associated_data_->dom_content_loaded())
    return;

  delegate_->DOMContentLoaded(this);
}

void RenderFrameHostImpl::SwapOuterDelegateFrame(RenderFrameProxyHost* proxy) {
  GetMojomFrameInRenderer()->Unload(
      /*is_loading=*/false,
      browsing_context_state_->current_replication_state().Clone(),
      proxy->GetFrameToken(), proxy->CreateAndBindRemoteFrameInterfaces(),
      proxy->CreateAndBindRemoteMainFrameInterfaces());
}

void RenderFrameHostImpl::DetachFromProxy() {
  if (IsPendingDeletion())
    return;

  // Start pending deletion on this frame and its children.
  DeleteRenderFrame(mojom::FrameDeleteIntention::kNotMainFrame);
  StartPendingDeletionOnSubtree(PendingDeletionReason::kFrameDetach);
  frame_tree()->FrameUnloading(GetFrameTreeNodeForUnload());

  // Some children with no unload handler may be eligible for immediate
  // deletion. Cut the dead branches now. This is a performance optimization.
  PendingDeletionCheckCompletedOnSubtree();  // May delete |this|.
  // |this| is potentially deleted. Do not add code after this.
}

void RenderFrameHostImpl::ProcessBeforeUnloadCompleted(
    bool proceed,
    bool treat_as_final_completion_callback,
    const base::TimeTicks& renderer_before_unload_start_time,
    const base::TimeTicks& renderer_before_unload_end_time,
    bool for_legacy) {
  TRACE_EVENT_NESTABLE_ASYNC_END1(
      "navigation", "RenderFrameHostImpl BeforeUnload", TRACE_ID_LOCAL(this),
      "render_frame_host", this);
  // If this renderer navigated while the beforeunload request was in flight, we
  // may have cleared this state in DidCommitProvisionalLoad, in which case we
  // can ignore this message.
  // However renderer might also be swapped out but we still want to proceed
  // with navigation, otherwise it would block future navigations. This can
  // happen when pending cross-site navigation is canceled by a second one just
  // before DidCommitProvisionalLoad while current RVH is waiting for commit
  // but second navigation is started from the beginning.
  RenderFrameHostImpl* initiator = GetBeforeUnloadInitiator();
  if (!initiator)
    return;

  // Continue processing the ACK in the frame that triggered beforeunload in
  // this frame.  This could be either this frame itself or an ancestor frame.
  initiator->ProcessBeforeUnloadCompletedFromFrame(
      proceed, treat_as_final_completion_callback, this,
      /*is_frame_being_destroyed=*/false, renderer_before_unload_start_time,
      renderer_before_unload_end_time, for_legacy);
}

RenderFrameHostImpl* RenderFrameHostImpl::GetBeforeUnloadInitiator() {
  for (RenderFrameHostImpl* frame = this; frame; frame = frame->GetParent()) {
    if (frame->is_waiting_for_beforeunload_completion_)
      return frame;
  }
  return nullptr;
}

void RenderFrameHostImpl::ProcessBeforeUnloadCompletedFromFrame(
    bool proceed,
    bool treat_as_final_completion_callback,
    RenderFrameHostImpl* frame,
    bool is_frame_being_destroyed,
    const base::TimeTicks& renderer_before_unload_start_time,
    const base::TimeTicks& renderer_before_unload_end_time,
    bool for_legacy) {
  // Check if we need to wait for more beforeunload completion callbacks. If
  // |proceed| is false, we know the navigation or window close will be aborted,
  // so we don't need to wait for beforeunload completion callbacks from any
  // other frames. |treat_as_final_completion_callback| also indicates that we
  // shouldn't wait for any other ACKs (e.g., when a beforeunload timeout
  // fires).
  if (!proceed || treat_as_final_completion_callback) {
    beforeunload_pending_replies_.clear();
  } else {
    beforeunload_pending_replies_.erase(frame);
    if (!beforeunload_pending_replies_.empty())
      return;
  }

  DCHECK(!send_before_unload_start_time_.is_null());

  // Sets a default value for before_unload_end_time so that the browser
  // survives a hacked renderer.
  base::TimeTicks before_unload_end_time = renderer_before_unload_end_time;
  base::TimeDelta browser_to_renderer_ipc_time_delta;
  if (!renderer_before_unload_start_time.is_null() &&
      !renderer_before_unload_end_time.is_null()) {
    base::TimeTicks before_unload_completed_time = base::TimeTicks::Now();

    if (!base::TimeTicks::IsConsistentAcrossProcesses() && !for_legacy) {
      // TimeTicks is not consistent across processes and we are passing
      // TimeTicks across process boundaries so we need to compensate for any
      // skew between the processes. Here we are converting the renderer's
      // notion of before_unload_end_time to TimeTicks in the browser process.
      // See comments in inter_process_time_ticks_converter.h for more.
      blink::InterProcessTimeTicksConverter converter(
          blink::LocalTimeTicks::FromTimeTicks(send_before_unload_start_time_),
          blink::LocalTimeTicks::FromTimeTicks(before_unload_completed_time),
          blink::RemoteTimeTicks::FromTimeTicks(
              renderer_before_unload_start_time),
          blink::RemoteTimeTicks::FromTimeTicks(
              renderer_before_unload_end_time));
      const base::TimeTicks browser_before_unload_start_time =
          converter
              .ToLocalTimeTicks(blink::RemoteTimeTicks::FromTimeTicks(
                  renderer_before_unload_start_time))
              .ToTimeTicks();
      const base::TimeTicks browser_before_unload_end_time =
          converter
              .ToLocalTimeTicks(blink::RemoteTimeTicks::FromTimeTicks(
                  renderer_before_unload_end_time))
              .ToTimeTicks();
      before_unload_end_time = browser_before_unload_end_time;
      browser_to_renderer_ipc_time_delta =
          browser_before_unload_start_time - send_before_unload_start_time_;
    } else {
      browser_to_renderer_ipc_time_delta =
          (renderer_before_unload_start_time - send_before_unload_start_time_);
    }

    if (for_legacy) {
      base::UmaHistogramTimes(
          "Navigation.OnBeforeUnloadLegacyPostTaskTime",
          before_unload_completed_time - renderer_before_unload_end_time);
      // When `for_legacy` is true callers should supply
      // `send_before_unload_start_time_` as the value for
      // `renderer_before_unload_start_time`, which means
      // `browser_to_renderer_ipc_time_delta` should be 0.
      DCHECK(browser_to_renderer_ipc_time_delta.is_zero());
    } else {
      base::UmaHistogramTimes(
          "Navigation.OnBeforeUnloadBrowserToRendererIpcTime",
          browser_to_renderer_ipc_time_delta);
    }

    base::TimeDelta on_before_unload_overhead_time =
        (before_unload_completed_time - send_before_unload_start_time_) -
        (renderer_before_unload_end_time - renderer_before_unload_start_time);
    base::UmaHistogramTimes("Navigation.OnBeforeUnloadOverheadTime",
                            on_before_unload_overhead_time);

    frame_tree_node_->navigator().LogBeforeUnloadTime(
        renderer_before_unload_start_time, renderer_before_unload_end_time,
        send_before_unload_start_time_, for_legacy);
  }

  // Resets beforeunload waiting state.
  is_waiting_for_beforeunload_completion_ = false;
  has_shown_beforeunload_dialog_ = false;
  if (beforeunload_timeout_)
    beforeunload_timeout_->Stop();
  send_before_unload_start_time_ = base::TimeTicks();

  // We could reach this from a subframe destructor for |frame| while we're in
  // the middle of closing the current tab. In that case, dispatch the ACK to
  // prevent re-entrancy and a potential nested attempt to free the current
  // frame. See https://crbug.com/866382 and https://crbug.com/1147567.
  base::OnceClosure task = base::BindOnce(
      [](base::WeakPtr<RenderFrameHostImpl> self,
         const base::TimeTicks& before_unload_end_time, bool proceed,
         bool unload_ack_is_for_navigation) {
        if (!self)
          return;
        FrameTreeNode* frame = self->frame_tree_node();
        // If the ACK is for a navigation, send it to the Navigator to have the
        // current navigation stop/proceed. Otherwise, send it to the
        // RenderFrameHostManager which handles closing.
        if (unload_ack_is_for_navigation) {
          frame->navigator().BeforeUnloadCompleted(frame, proceed,
                                                   before_unload_end_time);
        } else {
          frame->render_manager()->BeforeUnloadCompleted(proceed);
        }
      },
      // The overhead of the browser->renderer IPC may be non trivial. Account
      // for it here. Ideally this would also include the time to execute the
      // JS, but we would need to exclude the time spent waiting for a dialog,
      // which is tricky.
      weak_ptr_factory_.GetWeakPtr(),
      before_unload_end_time - browser_to_renderer_ipc_time_delta, proceed,
      unload_ack_is_for_navigation_);

  if (is_frame_being_destroyed) {
    DCHECK(proceed);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(task));
  } else {
    std::move(task).Run();
  }

  // If canceled, notify the delegate to cancel its pending navigation entry.
  // This is usually redundant with the dialog closure code in WebContentsImpl's
  // OnDialogClosed, but there may be some cases that Blink returns !proceed
  // without showing the dialog. We also update the address bar here to be safe.
  if (!proceed)
    frame_tree_->DidCancelLoading();
}

bool RenderFrameHostImpl::IsWaitingForUnloadACK() const {
  return page_close_state_ == PageCloseState::kRunningUnloadHandlers ||
         is_waiting_for_unload_ack_;
}

bool RenderFrameHostImpl::BeforeUnloadTimedOut() const {
  return beforeunload_timeout_ &&
         (send_before_unload_start_time_ != base::TimeTicks()) &&
         (base::TimeTicks::Now() - send_before_unload_start_time_) >
             beforeunload_timeout_delay_;
}

void RenderFrameHostImpl::OnUnloadACK() {
  // Give the tests a chance to override this sequence.
  if (unload_ack_callback_ && unload_ack_callback_.Run()) {
    return;
  }

  // This method should be called in the pending deletion state except the
  // placeholder RFHs for an inner WebContents that use unload without changing
  // lifecycle states. When attaching a GuestView as the inner WebContents,
  // `RFH::SwapOuterDelegateFrame()` is called for the placeholder RFH so that
  // it makes its renderer send this message. `owner_` is non null since this
  // attachment can only happen for subframes and pending deletion is the only
  // case where subframes may have a null `owner_`.
  RenderFrameHostOwner* owner =
      IsPendingDeletion() ? GetFrameTreeNodeForUnload() : owner_;
  if (!is_main_frame() &&
      owner->GetRenderFrameHostManager().is_attaching_inner_delegate()) {
    // This RFH was unloaded while attaching an inner delegate. The RFH
    // will stay around but it will no longer be associated with a RenderFrame.
    RenderFrameDeleted();
    return;
  }

  // Ignore spurious unload ack.
  if (!is_waiting_for_unload_ack_)
    return;

  // Ignore OnUnloadACK if the RenderFrameHost should be left in pending
  // deletion state.
  if (do_not_delete_for_testing_)
    return;

  DCHECK_EQ(LifecycleStateImpl::kRunningUnloadHandlers, lifecycle_state());
  SetLifecycleState(LifecycleStateImpl::kReadyToBeDeleted);
  PendingDeletionCheckCompleted();  // Can delete |this|.
  // |this| is potentially deleted. Do not add code after this.
}

void RenderFrameHostImpl::MaybeLogMissingUnloadAck() {
  // If you are seeing this logging appear in a flaky test, the test may be
  // dependent on an unload handler or other sudden-termination disabling event
  // handler. If so, the test should probably
  // - call DisableUnloadTimerForTesting() to avoid timeouts on slow devices
  // - wait for the frame to be deleted e.g. via
  //   RenderFrameHostWrapper::WaitUntilRenderFrameDeleted
  // See https://crbug.com/1489568 for more information.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kLogMissingUnloadACK)) {
    LOG(ERROR) << "Missing unload ACK for "
               << (IsOutermostMainFrame() ? "main frame" : "subframe") << ": "
               << GetLastCommittedURL();
  }
}

void RenderFrameHostImpl::OnNavigationUnloadTimeout() {
  MaybeLogMissingUnloadAck();
  OnUnloaded();
}

void RenderFrameHostImpl::OnUnloaded() {
  DCHECK(is_waiting_for_unload_ack_);

  TRACE_EVENT_NESTABLE_ASYNC_END0("navigation", "RenderFrameHostImpl::Unload",
                                  TRACE_ID_LOCAL(this));
  if (unload_event_monitor_timeout_)
    unload_event_monitor_timeout_->Stop();

  base::WeakPtr<RenderFrameHostImpl> self = GetWeakPtr();
  ClearWebUI();
  // See https://crbug.com/1308391. Calling `ClearWebUI()` indirectly call
  // content's embedders via a chain of destructors. Some might destroy the
  // whole WebContents.
  if (!self) {
    return;
  }

  bool deleted =
      GetFrameTreeNodeForUnload()->render_manager()->DeleteFromPendingList(
          this);
  CHECK(deleted);
}

void RenderFrameHostImpl::DisableUnloadTimerForTesting() {
  unload_event_monitor_timeout_.reset();
}

bool RenderFrameHostImpl::IsBackForwardCacheEvictionTimeRunningForTesting()
    const {
  return back_forward_cache_eviction_timer_.IsRunning();
}

void RenderFrameHostImpl::SetSubframeUnloadTimeoutForTesting(
    const base::TimeDelta& timeout) {
  subframe_unload_timeout_ = timeout;
}

#if BUILDFLAG(IS_ANDROID)
void RenderFrameHostImpl::RequestSmartClipExtract(
    ExtractSmartClipDataCallback callback,
    gfx::Rect rect) {
  int32_t callback_id = smart_clip_callbacks_.Add(
      std::make_unique<ExtractSmartClipDataCallback>(std::move(callback)));
  GetAssociatedLocalFrame()->ExtractSmartClipData(
      rect, base::BindOnce(&RenderFrameHostImpl::OnSmartClipDataExtracted,
                           base::Unretained(this), callback_id));
}

void RenderFrameHostImpl::OnSmartClipDataExtracted(int32_t callback_id,
                                                   const std::u16string& text,
                                                   const std::u16string& html,
                                                   const gfx::Rect& clip_rect) {
  std::move(*smart_clip_callbacks_.Lookup(callback_id))
      .Run(text, html, clip_rect);
  smart_clip_callbacks_.Remove(callback_id);
}
#endif  // BUILDFLAG(IS_ANDROID)

void RenderFrameHostImpl::RunModalAlertDialog(
    const std::u16string& alert_message,
    bool disable_third_party_subframe_suppresion,
    RunModalAlertDialogCallback response_callback) {
  auto dialog_closed_callback = base::BindOnce(
      [](RunModalAlertDialogCallback response_callback, bool success,
         const std::u16string& response) {
        // The response string is unused but we use a generic mechanism for
        // closing the javascript dialog that returns two arguments.
        std::move(response_callback).Run();
      },
      std::move(response_callback));
  RunJavaScriptDialog(alert_message, std::u16string(),
                      JAVASCRIPT_DIALOG_TYPE_ALERT,
                      disable_third_party_subframe_suppresion,
                      std::move(dialog_closed_callback));
}

void RenderFrameHostImpl::RunModalConfirmDialog(
    const std::u16string& alert_message,
    bool disable_third_party_subframe_suppresion,
    RunModalConfirmDialogCallback response_callback) {
  auto dialog_closed_callback = base::BindOnce(
      [](RunModalConfirmDialogCallback response_callback, bool success,
         const std::u16string& response) {
        // The response string is unused but we use a generic mechanism for
        // closing the javascript dialog that returns two arguments.
        std::move(response_callback).Run(success);
      },
      std::move(response_callback));
  RunJavaScriptDialog(alert_message, std::u16string(),
                      JAVASCRIPT_DIALOG_TYPE_CONFIRM,
                      disable_third_party_subframe_suppresion,
                      std::move(dialog_closed_callback));
}

void RenderFrameHostImpl::RunModalPromptDialog(
    const std::u16string& alert_message,
    const std::u16string& default_value,
    bool disable_third_party_subframe_suppresion,
    RunModalPromptDialogCallback response_callback) {
  RunJavaScriptDialog(
      alert_message, default_value, JAVASCRIPT_DIALOG_TYPE_PROMPT,
      disable_third_party_subframe_suppresion, std::move(response_callback));
}

void RenderFrameHostImpl::RunJavaScriptDialog(
    const std::u16string& message,
    const std::u16string& default_prompt,
    JavaScriptDialogType dialog_type,
    bool disable_third_party_subframe_suppresion,
    JavaScriptDialogCallback ipc_response_callback) {
  // Don't show the dialog if it's triggered on a non-active or non-primary
  // RenderFrameHost. This happens when the RenderFrameHost is pending deletion,
  // or is a non-primary MPArch page (Fenced Frame, in BFCache, etc.)..
  // TODO(https://crbug.com/1262022): Have to check fenced frames explicitly
  // since they are not yet implemented with MPArch. Once the transition from
  // shadow DOM to MPArch is complete, remove the last part of the condition.
  // TODO(crbug.com/1244137): We have to check portals explicitly as they are
  // considered primary. Remove check after we migrate portals to MPArch.
  if (!IsActive() || !GetPage().IsPrimary() || IsNestedWithinFencedFrame() ||
      frame_tree()->delegate()->IsPortal()) {
    std::move(ipc_response_callback).Run(/*success=*/false, std::u16string());
    return;
  }

  // While a JS message dialog is showing, tabs in the same process shouldn't
  // process input events.
  GetProcess()->SetBlocked(true);

  delegate_->RunJavaScriptDialog(
      this, message, default_prompt, dialog_type,
      disable_third_party_subframe_suppresion,
      base::BindOnce(&RenderFrameHostImpl::JavaScriptDialogClosed,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(ipc_response_callback)));
}

void RenderFrameHostImpl::RunBeforeUnloadConfirm(
    bool is_reload,
    RunBeforeUnloadConfirmCallback ipc_response_callback) {
  TRACE_EVENT1("navigation", "RenderFrameHostImpl::OnRunBeforeUnloadConfirm",
               "render_frame_host", this);

  // Don't show the dialog if it's triggered on a non-active or non-primary
  // RenderFrameHost. This happens when the RenderFrameHost is pending deletion,
  // or is a non-primary MPArch page (Fenced Frame, in BFCache, etc.)..
  // TODO(https://crbug.com/1262022): Have to check fenced frames explicitly
  // since they are not yet implemented with MPArch. Once the transition from
  // shadow DOM to MPArch is complete, remove the last part of the condition.
  if (!IsActive() || !GetPage().IsPrimary() || IsNestedWithinFencedFrame() ||
      frame_tree()->delegate()->IsPortal()) {
    std::move(ipc_response_callback).Run(/*success=*/false);
    return;
  }

  // Allow at most one attempt to show a beforeunload dialog per navigation.
  RenderFrameHostImpl* beforeunload_initiator = GetBeforeUnloadInitiator();
  if (beforeunload_initiator) {
    // If the running beforeunload handler wants to display a dialog and the
    // before-unload type wants to ignore it, then short-circuit the request and
    // respond as if the user decided to stay on the page, canceling the unload.
    if (beforeunload_initiator->beforeunload_dialog_request_cancels_unload_) {
      std::move(ipc_response_callback).Run(/*success=*/false);
      return;
    }

    if (beforeunload_initiator->has_shown_beforeunload_dialog_) {
      // TODO(alexmos): Pass enough data back to renderer to record histograms
      // for Document.BeforeUnloadDialog and add the intervention console
      // message to match renderer-side behavior in
      // Document::DispatchBeforeUnloadEvent().
      std::move(ipc_response_callback).Run(/*success=*/true);
      return;
    }
    beforeunload_initiator->has_shown_beforeunload_dialog_ = true;
  } else {
    // TODO(alexmos): If a renderer-initiated beforeunload shows a dialog, it
    // won't find a |beforeunload_initiator|. This can happen for a
    // renderer-initiated navigation or window.close().  We should ensure that
    // when the browser process later kicks off subframe unload handlers (if
    // any), they won't be able to show additional dialogs. However, we can't
    // just set |has_shown_beforeunload_dialog_| because we don't know which
    // frame is navigating/closing here.  Plumb enough information here to fix
    // this.
  }

  // While a JS beforeunload dialog is showing, tabs in the same process
  // shouldn't process input events.
  GetProcess()->SetBlocked(true);

  // The beforeunload dialog for this frame may have been triggered by a
  // browser-side request to this frame or a frame up in the frame hierarchy.
  // Stop any timers that are waiting.
  for (RenderFrameHostImpl* frame = this; frame; frame = frame->GetParent()) {
    if (frame->beforeunload_timeout_)
      frame->beforeunload_timeout_->Stop();
  }

  auto ipc_callback_wrapper = base::BindOnce(
      [](RunBeforeUnloadConfirmCallback response_callback, bool success,
         const std::u16string& response) {
        // The response string is unused but we use a generic mechanism for
        // closing the javascript dialog that returns two arguments.
        std::move(response_callback).Run(success);
      },
      std::move(ipc_response_callback));
  auto dialog_closed_callback = base::BindOnce(
      &RenderFrameHostImpl::JavaScriptDialogClosed,
      weak_ptr_factory_.GetWeakPtr(), std::move(ipc_callback_wrapper));

  delegate_->RunBeforeUnloadConfirm(this, is_reload,
                                    std::move(dialog_closed_callback));
}

void RenderFrameHostImpl::MaybeStartOutermostMainFrameNavigation(
    const std::vector<GURL>& urls) {
  const bool kStartupEnabled =
      base::FeatureList::IsEnabled(features::kSpeculativeServiceWorkerStartup);
  const bool kWarmUpEnabled =
      base::FeatureList::IsEnabled(
          blink::features::kSpeculativeServiceWorkerWarmUp) &&
      !blink::features::kSpeculativeServiceWorkerWarmUpDryRun.Get();

  if (!kStartupEnabled && !kWarmUpEnabled) {
    return;
  }

  TRACE_EVENT0("navigation",
               "RenderFrameHostImpl::MaybeStartOutermostMainFrameNavigation");

  // Disallow service worker start-up nor warm-up when there are other
  // windows that might script with this frame to mitigate security and
  // privacy concerns.
  if (site_instance_->GetRelatedActiveContentsCount() > 1u) {
    return;
  }

  ServiceWorkerContextWrapper* context =
      GetStoragePartition()->GetServiceWorkerContext();

  if (!context) {
    return;
  }

  for (const auto& url : urls) {
    GURL filtered_url(url);

    if (GetProcess()->FilterURL(/*empty_allowed=*/false, &filtered_url) ==
        RenderProcessHost::FilterURLResult::kBlocked) {
      continue;
    }

    if (!OriginCanAccessServiceWorkers(filtered_url)) {
      continue;
    }

    const blink::StorageKey key =
        blink::StorageKey::CreateFirstParty(url::Origin::Create(filtered_url));

    if (!context->MaybeHasRegistrationForStorageKey(key)) {
      continue;
    }

    // Ask the service worker context to speculatively start a service worker
    // for the request URL if necessary for optimization purposes. There are
    // cases where we have already started the service worker (e.g, Prerendering
    // or the previous navigation already started the service worker), but this
    // call does nothing if the service worker already started for the URL.
    if (kStartupEnabled) {
      context->StartServiceWorkerForNavigationHint(filtered_url, key,
                                                   base::DoNothing());
    }

    // Ask the service worker context to speculatively warm-up a service worker
    // for the request URL if necessary for optimization purposes. There are
    // cases where we have already started the service worker (e.g, Prerendering
    // or the previous navigation already started the service worker), but this
    // call does nothing if the service worker already started for the URL.
    if (kWarmUpEnabled) {
      context->WarmUpServiceWorker(filtered_url, key, base::DoNothing());
    }
  }
}

// TODO(crbug.com/1213863): Move this method to content::PageImpl.
void RenderFrameHostImpl::UpdateFaviconURL(
    std::vector<blink::mojom::FaviconURLPtr> favicon_urls) {
  DCHECK(!GetParent());
  GetPage().set_favicon_urls(std::move(favicon_urls));
  delegate_->UpdateFaviconURL(this, GetPage().favicon_urls());
}

float RenderFrameHostImpl::GetPageScaleFactor() const {
  DCHECK(!GetParent());
  return page_scale_factor_;
}

void RenderFrameHostImpl::ScaleFactorChanged(float scale) {
  DCHECK(!GetParent());
  page_scale_factor_ = scale;
  delegate_->OnPageScaleFactorChanged(GetPage());
}

void RenderFrameHostImpl::ContentsPreferredSizeChanged(
    const gfx::Size& pref_size) {
  // Do not try to handle the message in inactive RenderFrameHosts for
  // simplicity. If this RenderFrameHost belongs to a bfcached or prerendered
  // page, the page will be deleted. We predict that it will not significantly
  // impact coverage because renderers only send this message when running in
  // `PreferredSizeChanged` mode.
  if (IsInactiveAndDisallowActivation(
          DisallowActivationReasonId::kContentsPreferredSizeChanged)) {
    return;
  }

  // Ignore the request if we are aren't the outermost main frame.
  if (GetParentOrOuterDocument())
    return;

  delegate_->UpdateWindowPreferredSize(pref_size);
}

void RenderFrameHostImpl::TextAutosizerPageInfoChanged(
    blink::mojom::TextAutosizerPageInfoPtr page_info) {
  GetPage().OnTextAutosizerPageInfoChanged(std::move(page_info));
}

void RenderFrameHostImpl::FocusPage() {
  render_view_host_->OnFocus();
}

void RenderFrameHostImpl::TakeFocus(bool reverse) {
  // TODO(crbug.com/1225366): Consider moving this to PageImpl.
  DCHECK(is_main_frame());

  // Do not update the parent on behalf of the inactive document.
  if (IsInactiveAndDisallowActivation(
          DisallowActivationReasonId::kDispatchLoad)) {
    return;
  }

  // If we are representing an inner frame tree call advance on our outer
  // delegate's parent's RenderFrameHost.
  RenderFrameHostImpl* parent_or_outer_document =
      GetParentOrOuterDocumentOrEmbedder();
  if (parent_or_outer_document) {
    RenderFrameProxyHost* proxy_host = GetProxyToOuterDelegate();
    DCHECK(proxy_host);

    if (HasTransientUserActivation() ||
        FocusSourceHasTransientUserActivation()) {
      parent_or_outer_document->ActivateFocusSourceUserActivation();
      focus_source_user_activation_state_.Deactivate();
    }

    parent_or_outer_document->DidFocusFrame();
    parent_or_outer_document->AdvanceFocus(
        reverse ? blink::mojom::FocusType::kBackward
                : blink::mojom::FocusType::kForward,
        proxy_host);
    return;
  }

  render_view_host_->OnTakeFocus(reverse);
}

void RenderFrameHostImpl::UpdateTargetURL(
    const GURL& url,
    blink::mojom::LocalMainFrameHost::UpdateTargetURLCallback callback) {
  // An inactive document should ignore to update the target url.
  if (!IsActive()) {
    std::move(callback).Run();
    return;
  }

  delegate_->UpdateTargetURL(this, url);
  std::move(callback).Run();
}

void RenderFrameHostImpl::RequestClose() {
  // The renderer already ensures that this can only be called on an outermost
  // main frame - see DOMWindow::Close().  Terminate the renderer if this is
  // not the case.
  if (!IsOutermostMainFrame()) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFH_WINDOW_CLOSE_ON_NON_OUTERMOST_FRAME);
    return;
  }

  // If the renderer is telling us to close, it has already run the unload
  // events, and we can take the fast path.
  ClosePageIgnoringUnloadEvents(ClosePageSource::kRenderer);
}

bool RenderFrameHostImpl::HasStickyUserActivation() const {
  return user_activation_state_.HasBeenActive();
}

bool RenderFrameHostImpl::IsActiveUserActivation() const {
  return user_activation_state_.IsActive();
}

void RenderFrameHostImpl::ClearUserActivation() {
  user_activation_state_.Clear();
  history_user_activation_state_.Clear();
}

void RenderFrameHostImpl::ConsumeTransientUserActivation() {
  user_activation_state_.ConsumeIfActive();
}

void RenderFrameHostImpl::ActivateUserActivation(
    blink::mojom::UserActivationNotificationType notification_type) {
  user_activation_state_.Activate(notification_type);
  history_user_activation_state_.Activate();
}

void RenderFrameHostImpl::ActivateFocusSourceUserActivation() {
  focus_source_user_activation_state_.Activate();
}

bool RenderFrameHostImpl::IsHistoryUserActivationActive() const {
  return history_user_activation_state_.IsActive();
}

void RenderFrameHostImpl::ConsumeHistoryUserActivation() {
  history_user_activation_state_.Consume();
}

void RenderFrameHostImpl::DeactivateFocusSourceUserActivation() {
  focus_source_user_activation_state_.Deactivate();
}

void RenderFrameHostImpl::ClosePage(ClosePageSource source) {
  // This path is taken when tab/window close is initiated by either the
  // browser process or via a window.close() call through a proxy. In both
  // cases, we need to tell the main frame's renderer process to run unload
  // handlers and prepare for page close.
  //
  // This should only be called on outermost main frames. If this
  // RenderFrameHost is no longer a primary main frame (e.g., if it was placed
  // into back-forward cache or became pending deletion just before getting
  // here), we should not close the active tab if the request to close came from
  // the renderer, so return early in that case. We proceed with closing
  // regardless if the request came from the browser so that renderers can't
  // avoid closing via navigation.
  DCHECK(is_main_frame());
  // TODO(crbug.com/1254770): Orphaned portals use this code path. Revisit how
  // portals handle unload when migrating off of inner WebContents.
  DCHECK(IsOutermostMainFrame() || frame_tree()->delegate()->IsPortal());
  if (!IsInPrimaryMainFrame() && source == ClosePageSource::kRenderer) {
    return;
  }

  page_close_state_ = PageCloseState::kRunningUnloadHandlers;

  if (IsRenderFrameLive() && !IsPageReadyToBeClosed()) {
    close_timeout_ = std::make_unique<TimeoutMonitor>(
        base::BindRepeating(&RenderFrameHostImpl::ClosePageTimeout,
                            weak_ptr_factory_.GetWeakPtr(), source));
    close_timeout_->Start(kUnloadTimeout);

    GetAssociatedLocalMainFrame()->ClosePage(
        base::BindOnce(&RenderFrameHostImpl::ClosePageIgnoringUnloadEvents,
                       weak_ptr_factory_.GetWeakPtr(), source));
  } else {
    // This RenderFrameHost doesn't have a live renderer (or has already run
    // unload handlers), so just skip the close event and close the page.
    ClosePageIgnoringUnloadEvents(source);
  }
}

void RenderFrameHostImpl::ClosePageIgnoringUnloadEvents(
    ClosePageSource source) {
  if (close_timeout_) {
    close_timeout_->Stop();
    close_timeout_.reset();
  }

  // If this RenderFrameHost is no longer the primary main frame (e.g., if it
  // was replaced by another frame while waiting for the ClosePage ACK or
  // timeout), there's no need to close the active tab if the request to close
  // came from the renderer, so return early in that case. We proceed with
  // closing regardless if the request came from the browser so that renderers
  // can't avoid closing via navigation.
  if (!IsInPrimaryMainFrame() && source == ClosePageSource::kRenderer) {
    page_close_state_ = PageCloseState::kNotClosing;
    return;
  }

  page_close_state_ = PageCloseState::kReadyToBeClosed;
  delegate_->Close();
}

bool RenderFrameHostImpl::IsPageReadyToBeClosed() {
  DCHECK(IsInPrimaryMainFrame());
  // If there is a JavaScript dialog up, don't bother sending the renderer the
  // close event because it is known unresponsive, waiting for the reply from
  // the dialog.
  return page_close_state_ == PageCloseState::kReadyToBeClosed ||
         delegate_->IsJavaScriptDialogShowing() || BeforeUnloadTimedOut();
}

void RenderFrameHostImpl::ClosePageTimeout(ClosePageSource source) {
  if (delegate_->ShouldIgnoreUnresponsiveRenderer()) {
    return;
  }

  ClosePageIgnoringUnloadEvents(source);
}

void RenderFrameHostImpl::ShowCreatedWindow(
    const blink::LocalFrameToken& opener_frame_token,
    WindowOpenDisposition disposition,
    blink::mojom::WindowFeaturesPtr window_features,
    bool user_gesture,
    ShowCreatedWindowCallback callback) {
  // This needs to be sent to the opener frame's delegate since it stores
  // the handle to this class's associated RenderWidgetHostView.
  RenderFrameHostImpl* opener_frame_host =
      FromFrameToken(GetProcess()->GetID(), opener_frame_token);

  // If |opener_frame_host| has been destroyed just return.
  // TODO(crbug.com/1150976): Get rid of having to look up the opener frame
  // to find the newly created web contents, because it is actually just
  // |delegate_|.
  if (!opener_frame_host) {
    std::move(callback).Run();
    return;
  }
  opener_frame_host->delegate()->ShowCreatedWindow(
      opener_frame_host, GetRenderWidgetHost()->GetRoutingID(), disposition,
      *window_features, user_gesture);
  std::move(callback).Run();
}

void RenderFrameHostImpl::SetWindowRect(const gfx::Rect& bounds,
                                        SetWindowRectCallback callback) {
  // Prerendering pages should not reach this code.
  if (lifecycle_state_ == LifecycleStateImpl::kPrerendering) {
    local_main_frame_host_receiver_.ReportBadMessage(
        "SetWindowRect called during prerendering.");
    return;
  }
  // Throw out SetWindowRects that are not from the outermost document.
  if (GetParentOrOuterDocument()) {
    local_main_frame_host_receiver_.ReportBadMessage(
        "SetWindowRect called from child frame.");
    return;
  }

  delegate_->SetWindowRect(bounds);
  std::move(callback).Run();
}

void RenderFrameHostImpl::DidFirstVisuallyNonEmptyPaint() {
  // TODO(crbug.com/1225366): Consider moving this to PageImpl.
  DCHECK(is_main_frame());
  GetPage().OnFirstVisuallyNonEmptyPaint();
}

void RenderFrameHostImpl::DownloadURL(
    blink::mojom::DownloadURLParamsPtr blink_parameters) {
  // TODO(crbug.com/1205359): We should defer the download until the
  // prerendering page is activated, and it will comply with the prerendering
  // spec.
  if (CancelPrerendering(
          PrerenderCancellationReason(PrerenderFinalStatus::kDownload))) {
    return;
  }

  if (!VerifyDownloadUrlParams(GetProcess(), *blink_parameters)) {
    return;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("renderer_initiated_download", R"(
        semantics {
          sender: "Download from Renderer"
          description:
            "The frame has either navigated to a URL that was determined to be "
            "a download via one of the renderer's classification mechanisms, "
            "or WebView has requested a <canvas> or <img> element at a "
            "specific location be to downloaded."
          trigger:
            "The user navigated to a destination that was categorized as a "
            "download, or WebView triggered saving a <canvas> or <img> tag."
          data: "Only the URL we are attempting to download."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "This feature cannot be disabled by settings."
          chrome_policy {
            DownloadRestrictions {
              DownloadRestrictions: 3
            }
          }
        })");
  std::unique_ptr<download::DownloadUrlParameters> parameters(
      new download::DownloadUrlParameters(blink_parameters->url,
                                          GetProcess()->GetID(), GetRoutingID(),
                                          traffic_annotation));
  parameters->set_content_initiated(!blink_parameters->is_context_menu_save);
  parameters->set_has_user_gesture(blink_parameters->has_user_gesture);
  parameters->set_suggested_name(
      blink_parameters->suggested_name.value_or(std::u16string()));
  parameters->set_prompt(blink_parameters->is_context_menu_save);
  parameters->set_cross_origin_redirects(
      blink_parameters->cross_origin_redirects);
  parameters->set_referrer(
      blink_parameters->referrer ? blink_parameters->referrer->url : GURL());
  parameters->set_referrer_policy(Referrer::ReferrerPolicyForUrlRequest(
      blink_parameters->referrer ? blink_parameters->referrer->policy
                                 : network::mojom::ReferrerPolicy::kDefault));
  parameters->set_initiator(
      blink_parameters->initiator_origin.value_or(url::Origin()));
  parameters->set_download_source(download::DownloadSource::FROM_RENDERER);

  if (blink_parameters->data_url_blob) {
    DataURLBlobReader::ReadDataURLFromBlob(
        std::move(blink_parameters->data_url_blob),
        base::BindOnce(&OnDataURLRetrieved, std::move(parameters)));
    return;
  }

  scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;
  if (blink_parameters->blob_url_token) {
    blob_url_loader_factory =
        ChromeBlobStorageContext::URLLoaderFactoryForToken(
            GetStoragePartition(), std::move(blink_parameters->blob_url_token));
  }
  devtools_instrumentation::ApplyNetworkOverridesForDownload(this,
                                                             parameters.get());

  StartDownload(std::move(parameters), std::move(blob_url_loader_factory));
}

void RenderFrameHostImpl::ReportNoBinderForInterface(const std::string& error) {
  broker_receiver_.ReportBadMessage(error + " for the frame/document scope");
}

ukm::SourceId RenderFrameHostImpl::GetPageUkmSourceId() {
  CHECK(!IsInLifecycleState(LifecycleState::kPrerendering));
  // This id for all subframes or fenced frames is the same as the id for the
  // outermost main frame. For portals, this id for frames inside a portal is
  // the same as the id for the main frame for the portal.
  RenderFrameHostImpl* main_frame =
      IsNestedWithinFencedFrame() ? GetOutermostMainFrame() : GetMainFrame();
  int64_t navigation_id =
      main_frame->last_committed_cross_document_navigation_id_;
  if (navigation_id == -1)
    return ukm::kInvalidSourceId;
  return ukm::ConvertToSourceId(navigation_id,
                                ukm::SourceIdType::NAVIGATION_ID);
}

BrowserContext* RenderFrameHostImpl::GetBrowserContext() {
  return GetProcess()->GetBrowserContext();
}

// TODO(crbug.com/1091720): Would be better to do this directly in the chrome
// layer.  See referenced bug for further details.
void RenderFrameHostImpl::ReportInspectorIssue(
    blink::mojom::InspectorIssueInfoPtr info) {
  devtools_instrumentation::BuildAndReportBrowserInitiatedIssue(
      this, std::move(info));
}

void RenderFrameHostImpl::WriteIntoTrace(
    perfetto::TracedProto<TraceProto> proto) const {
  proto.Set(TraceProto::kRenderFrameHostId, GetGlobalId());
  proto->set_frame_tree_node_id(GetFrameTreeNodeId());
  proto->set_lifecycle_state(LifecycleStateToProto());
  proto->set_frame_type(GetFrameTypeProto());
  proto->set_origin(GetLastCommittedOrigin().GetDebugString());
  proto->set_url(GetLastCommittedURL().possibly_invalid_spec());
  proto.Set(TraceProto::kProcess, GetProcess());
  proto.Set(TraceProto::kSiteInstance, GetSiteInstance());
  if (auto* parent = GetParent()) {
    proto.Set(TraceProto::kParent, parent);
  } else if (auto* outer_document = GetParentOrOuterDocument()) {
    proto.Set(TraceProto::kOuterDocument, outer_document);
    outer_document->WriteIntoTrace(proto.WriteNestedMessage(
        perfetto::protos::pbzero::RenderFrameHost::kOuterDocument));
  } else if (auto* embedder = GetParentOrOuterDocumentOrEmbedder()) {
    proto.Set(TraceProto::kEmbedder, embedder);
    embedder->WriteIntoTrace(proto.WriteNestedMessage(
        perfetto::protos::pbzero::RenderFrameHost::kEmbedder));
  }
  proto.Set(TraceProto::kBrowsingContextState, browsing_context_state_);
}

perfetto::protos::pbzero::RenderFrameHost::LifecycleState
RenderFrameHostImpl::LifecycleStateToProto() const {
  using RFHProto = perfetto::protos::pbzero::RenderFrameHost;
  switch (lifecycle_state()) {
    case LifecycleStateImpl::kSpeculative:
      return RFHProto::SPECULATIVE;
    case LifecycleStateImpl::kPendingCommit:
      return RFHProto::PENDING_COMMIT;
    case LifecycleStateImpl::kPrerendering:
      return RFHProto::PRERENDERING;
    case LifecycleStateImpl::kActive:
      return RFHProto::ACTIVE;
    case LifecycleStateImpl::kInBackForwardCache:
      return RFHProto::IN_BACK_FORWARD_CACHE;
    case LifecycleStateImpl::kRunningUnloadHandlers:
      return RFHProto::RUNNING_UNLOAD_HANDLERS;
    case LifecycleStateImpl::kReadyToBeDeleted:
      return RFHProto::READY_TO_BE_DELETED;
  }

  return RFHProto::UNSPECIFIED;
}

perfetto::protos::pbzero::FrameTreeNodeInfo::FrameType
RenderFrameHostImpl::GetFrameTypeProto() const {
  using RFHProto = perfetto::protos::pbzero::FrameTreeNodeInfo;

  if (GetParent()) {
    return RFHProto::SUBFRAME;
  }
  if (GetPage().IsPrimary()) {
    return RFHProto::PRIMARY_MAIN_FRAME;
  }
  if (lifecycle_state() == LifecycleStateImpl::kPrerendering) {
    return RFHProto::PRERENDER_MAIN_FRAME;
  }
  if (IsFencedFrameRoot()) {
    return RFHProto::FENCED_FRAME_ROOT;
  }

  // It returns a different value from FrameTreeNode::GetFrameType() when
  // - `this` is a speculative RFH or
  // - `this` is not in a frame tree (e.g., IsInBackForwardCache() or
  // IsPendingDeletion()).
  return RFHProto::UNSPECIFIED_FRAME_TYPE;
}

StoragePartitionImpl* RenderFrameHostImpl::GetStoragePartition() {
  // Both RenderProcessHostImpl and MockRenderProcessHost obtain the
  // StoragePartition instance through BrowserContext::GetStoragePartition()
  // call. That method does not support creating TestStoragePartition
  // instances and always vends StoragePartitionImpl objects. It is therefore
  // safe to static cast the result here.
  return static_cast<StoragePartitionImpl*>(
      GetProcess()->GetStoragePartition());
}

void RenderFrameHostImpl::RequestTextSurroundingSelection(
    blink::mojom::LocalFrame::GetTextSurroundingSelectionCallback callback,
    int max_length) {
  DCHECK(!callback.is_null());
  GetAssociatedLocalFrame()->GetTextSurroundingSelection(max_length,
                                                         std::move(callback));
}

bool RenderFrameHostImpl::HasCommittingNavigationRequestForOrigin(
    const url::Origin& origin,
    NavigationRequest* navigation_request_to_exclude) {
  for (const auto& it : navigation_requests_) {
    NavigationRequest* request = it.first;
    if (request != navigation_request_to_exclude &&
        request->HasCommittingOrigin(origin)) {
      return true;
    }
  }

  // Note: this function excludes |same_document_navigation_requests_|, which
  // should be ok since these cannot change the origin.
  return false;
}

void RenderFrameHostImpl::SendInterventionReport(const std::string& id,
                                                 const std::string& message) {
  GetAssociatedLocalFrame()->SendInterventionReport(id, message);
}

WebUI* RenderFrameHostImpl::GetWebUI() {
  return web_ui();
}

void RenderFrameHostImpl::AllowBindings(int bindings_flags) {
  // Never grant any bindings to browser plugin guests.
  if (GetProcess()->IsForGuestsOnly()) {
    NOTREACHED() << "Never grant bindings to a guest process.";
    return;
  }
  TRACE_EVENT2("navigation", "RenderFrameHostImpl::AllowBindings",
               "render_frame_host", this, "bindings_flags", bindings_flags);

  int webui_bindings = bindings_flags & kWebUIBindingsPolicyMask;

  // Ensure callers that specify non-zero WebUI bindings are doing so on a
  // RenderFrameHost that has WebUI associated with it. If we run the renderer
  // code in-process, the security invariant cannot be enforced, therefore it
  // should be skipped in that case.
  if (webui_bindings != BINDINGS_POLICY_NONE &&
      !RenderProcessHost::run_renderer_in_process() &&
      base::FeatureList::IsEnabled(
          features::kEnsureAllowBindingsIsAlwaysForWebUI)) {
    ProcessLock process_lock = GetProcess()->GetProcessLock();
    if (!process_lock.is_locked_to_site() ||
        !base::Contains(URLDataManagerBackend::GetWebUISchemes(),
                        process_lock.lock_url().scheme())) {
      SCOPED_CRASH_KEY_STRING256("AllowBindings", "process_lock",
                                 process_lock.ToString());
      CHECK(false) << "Calling AllowBindings for a process not locked to WebUI:"
                   << process_lock;
    }
  }

  // The bindings being granted here should not differ from the bindings that
  // the associated WebUI requires.
  if (web_ui_)
    CHECK_EQ(web_ui_->GetBindings(), webui_bindings);

  // Ensure we aren't granting WebUI bindings to a process that has already been
  // used to host other content.
  if (webui_bindings && GetProcess()->IsInitializedAndNotDead() &&
      !ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          GetProcess()->GetID())) {
    // This process has no bindings yet. Make sure it does not have any frames
    // that have committed a navigation, since bindings should always be granted
    // prior to committing the first WebUI navigation in a process.  This is a
    // defense-in-depth check complementing the site isolation process lock
    // checks above and in places like RenderProcessHostImpl::IsSuitableHost().
    // --single-process only has one renderer, so it is exempt from this check.
    size_t non_empty_frame_count = 0;
    GetProcess()->ForEachRenderFrameHost(base::BindRepeating(
        [](size_t& non_empty_frame_count, RenderFrameHost* rfh) {
          if (!static_cast<RenderFrameHostImpl*>(rfh)
                   ->is_initial_empty_document()) {
            ++non_empty_frame_count;
          }
        },
        std::ref(non_empty_frame_count)));
    if (non_empty_frame_count > 0 &&
        !base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kSingleProcess)) {
      return;
    }
  }

  if (webui_bindings) {
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantWebUIBindings(
        GetProcess()->GetID(), webui_bindings);
  }

  enabled_bindings_ |= bindings_flags;

  if (is_render_frame_created()) {
    GetFrameBindingsControl()->AllowBindings(enabled_bindings_);
    if (web_ui_ && enabled_bindings_ & BINDINGS_POLICY_WEB_UI)
      web_ui_->SetUpMojoConnection();
  }
}

int RenderFrameHostImpl::GetEnabledBindings() {
  return enabled_bindings_;
}

void RenderFrameHostImpl::SetWebUIProperty(const std::string& name,
                                           const std::string& value) {
  // WebUI allows to register SetProperties only for the outermost main frame.
  if (GetParentOrOuterDocument())
    return;

  // This is a sanity check before telling the renderer to enable the property.
  // It could lie and send the corresponding IPC messages anyway, but we will
  // not act on them if enabled_bindings_ doesn't agree. If we get here without
  // WebUI bindings, terminate the renderer process.
  if (enabled_bindings_ & BINDINGS_POLICY_WEB_UI)
    web_ui_->SetProperty(name, value);
  else
    ReceivedBadMessage(GetProcess(), bad_message::RVH_WEB_UI_BINDINGS_MISMATCH);
}

void RenderFrameHostImpl::DisableBeforeUnloadHangMonitorForTesting() {
  beforeunload_timeout_.reset();
}

bool RenderFrameHostImpl::IsBeforeUnloadHangMonitorDisabledForTesting() {
  return !beforeunload_timeout_;
}

void RenderFrameHostImpl::DoNotDeleteForTesting() {
  do_not_delete_for_testing_ = true;
}

void RenderFrameHostImpl::ResumeDeletionForTesting() {
  do_not_delete_for_testing_ = false;
}

void RenderFrameHostImpl::DetachForTesting() {
  do_not_delete_for_testing_ = false;
  RenderFrameHostImpl::Detach();
}

bool RenderFrameHostImpl::IsFeatureEnabled(
    blink::mojom::PermissionsPolicyFeature feature) {
  return permissions_policy_ && permissions_policy_->IsFeatureEnabledForOrigin(
                                    feature, GetLastCommittedOrigin());
}

const blink::PermissionsPolicy* RenderFrameHostImpl::GetPermissionsPolicy() {
  return permissions_policy_.get();
}

const blink::ParsedPermissionsPolicy&
RenderFrameHostImpl::GetPermissionsPolicyHeader() {
  return permissions_policy_header_;
}

void RenderFrameHostImpl::ViewSource() {
  delegate_->ViewSource(this);
}

void RenderFrameHostImpl::FlushNetworkAndNavigationInterfacesForTesting(
    bool do_nothing_if_no_network_service_connection) {
  if (do_nothing_if_no_network_service_connection &&
      !network_service_disconnect_handler_holder_) {
    return;
  }
  DCHECK(network_service_disconnect_handler_holder_);
  network_service_disconnect_handler_holder_.FlushForTesting();  // IN-TEST

  DCHECK(IsRenderFrameLive());
  DCHECK(frame_);
  frame_.FlushForTesting();  // IN-TEST
}

void RenderFrameHostImpl::PrepareForInnerWebContentsAttach(
    PrepareForInnerWebContentsAttachCallback callback) {
  // TODO(https://crbug.com/1405759) Explain why `owner_` exists.
  CHECK(owner_);
  owner_->GetRenderFrameHostManager().PrepareForInnerDelegateAttach(
      std::move(callback));
}

// UpdateSubresourceLoaderFactories may be called (internally/privately), when
// RenderFrameHostImpl detects a NetworkService crash after pushing a
// NetworkService-based factory to the renderer process.  It may also be called
// when DevTools wants to send to the renderer process a fresh factory bundle
// (e.g. after injecting DevToolsURLLoaderInterceptor) - the latter scenario may
// happen even if `this` RenderFrameHostImpl has not pushed any NetworkService
// factories to the renderer process (DevTools is agnostic to this).
void RenderFrameHostImpl::UpdateSubresourceLoaderFactories() {
  // Disregard this if frame is being destroyed.
  if (!frame_)
    return;

  // Disregard if the IPC payload would have been empty.
  //
  // Examples of when this can happen:
  // 1. DevTools tries to inject its proxies by calling this method on *all*
  //    frames, regardless of whether a frame has actually used one or more
  //    NetworkService-based URLLoaderFactory.  See also
  //    RenderFrameDevToolsAgentHost::UpdateResourceLoaderFactories.
  // 2. The RenderFrameHostImpl::CreateNetworkServiceDefaultFactory method
  //    (exposed via //content/public) is called on a frame that doesn't
  //    actually use a NetworkService-backed URLLoaderFactory.  This means
  //    that CreateNetworkServiceDefaultFactoryAndObserve sets up
  //    `network_service_disconnect_handler_holder_`, but
  //    `recreate_default_url_loader_factory_after_network_service_crash_` is
  //    false.
  if (!recreate_default_url_loader_factory_after_network_service_crash_ &&
      isolated_worlds_requiring_separate_url_loader_factory_.empty()) {
    return;
  }

  // TODO(https://crbug.com/1382971): Remove the crash key logging after the
  // ad-hoc bug investigation is no longer needed.
  SCOPED_CRASH_KEY_STRING256("rfhi-uslf", "frame->ToDebugString",
                             ToDebugString());

  // The `subresource_loader_factories_config` of the new factories might need
  // to depend on the pending (rather than the last committed) navigation,
  // because we can't predict if an in-flight Commit IPC might be present when
  // an extension injects a content script and MarkIsolatedWorlds... is called.
  // See also the doc comment for the ForPendingOrLastCommittedNavigation
  // method.
  auto subresource_loader_factories_config =
      SubresourceLoaderFactoriesConfig::ForPendingOrLastCommittedNavigation(
          *this);

  mojo::PendingRemote<network::mojom::URLLoaderFactory> default_factory_remote;
  bool bypass_redirect_checks = false;
  if (recreate_default_url_loader_factory_after_network_service_crash_) {
    DCHECK(!IsOutOfProcessNetworkService() ||
           network_service_disconnect_handler_holder_.is_bound());
    bypass_redirect_checks = CreateNetworkServiceDefaultFactoryAndObserve(
        CreateURLLoaderFactoryParamsForMainWorld(
            subresource_loader_factories_config,
            "RFHI::UpdateSubresourceLoaderFactories"),
        subresource_loader_factories_config.ukm_source_id(),
        default_factory_remote.InitWithNewPipeAndPassReceiver());
  }

  std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
      subresource_loader_factories =
          std::make_unique<blink::PendingURLLoaderFactoryBundle>(
              std::move(default_factory_remote),
              blink::PendingURLLoaderFactoryBundle::SchemeMap(),
              CreateURLLoaderFactoriesForIsolatedWorlds(
                  subresource_loader_factories_config,
                  isolated_worlds_requiring_separate_url_loader_factory_),
              bypass_redirect_checks);

  GetMojomFrameInRenderer()->UpdateSubresourceLoaderFactories(
      std::move(subresource_loader_factories));
}

blink::FrameOwnerElementType RenderFrameHostImpl::GetFrameOwnerElementType() {
  return frame_owner_element_type_;
}

bool RenderFrameHostImpl::HasTransientUserActivation() {
  return user_activation_state_.IsActive();
}

bool RenderFrameHostImpl::FocusSourceHasTransientUserActivation() {
  return focus_source_user_activation_state_.IsActive();
}

void RenderFrameHostImpl::NotifyUserActivation(
    blink::mojom::UserActivationNotificationType notification_type) {
  GetAssociatedLocalFrame()->NotifyUserActivation(notification_type);
}

void RenderFrameHostImpl::DidAccessInitialMainDocument() {
  frame_tree_->DidAccessInitialMainDocument();
}

void RenderFrameHostImpl::DidChangeName(const std::string& name,
                                        const std::string& unique_name) {
  // Frame name updates used to occur in the FrameTreeNode; however, as they
  // now occur in RenderFrameHostImpl (and by extension, BrowsingContextState),
  // ensure that invalid updates (i.e. when in the BackForwardCache or in a
  // pending deletion state) are not applied.
  if ((IsInBackForwardCache() || IsPendingDeletion()) &&
      base::FeatureList::IsEnabled(
          features::kDisableFrameNameUpdateOnNonCurrentRenderFrameHost)) {
    return;
  }
  if (GetParent() != nullptr) {
    // TODO(lukasza): Call ReceivedBadMessage when |unique_name| is empty.
    DCHECK(!unique_name.empty());
  }
  TRACE_EVENT2("navigation", "RenderFrameHostImpl::OnDidChangeName",
               "render_frame_host", this, "name", name);

  std::string old_name = browsing_context_state_->frame_name();
  browsing_context_state_->SetFrameName(name, unique_name);
  if (old_name.empty() && !name.empty())
    frame_tree_node_->render_manager()->CreateProxiesForNewNamedFrame(
        browsing_context_state_);
  delegate_->DidChangeName(this, name);
}

void RenderFrameHostImpl::EnforceInsecureRequestPolicy(
    blink::mojom::InsecureRequestPolicy policy) {
  browsing_context_state_->SetInsecureRequestPolicy(policy);
}

void RenderFrameHostImpl::EnforceInsecureNavigationsSet(
    const std::vector<uint32_t>& set) {
  browsing_context_state_->SetInsecureNavigationsSet(set);
}

void RenderFrameHostImpl::AddDocumentService(
    internal::DocumentServiceBase* document_service,
    base::PassKey<internal::DocumentServiceBase>) {
  document_associated_data_->services().push_back(document_service);
}

void RenderFrameHostImpl::RemoveDocumentService(
    internal::DocumentServiceBase* document_service,
    base::PassKey<internal::DocumentServiceBase>) {
  if (document_service == last_web_bluetooth_service_for_testing_) {
    last_web_bluetooth_service_for_testing_ = nullptr;
  }
  base::Erase(document_associated_data_->services(), document_service);
}

FrameTreeNode* RenderFrameHostImpl::FindAndVerifyChild(
    int32_t child_frame_routing_id,
    bad_message::BadMessageReason reason) {
  auto child_frame_or_proxy = LookupRenderFrameHostOrProxy(
      GetProcess()->GetID(), child_frame_routing_id);
  return FindAndVerifyChildInternal(child_frame_or_proxy, reason);
}

FrameTreeNode* RenderFrameHostImpl::FindAndVerifyChild(
    const blink::FrameToken& child_frame_token,
    bad_message::BadMessageReason reason) {
  auto child_frame_or_proxy =
      LookupRenderFrameHostOrProxy(GetProcess()->GetID(), child_frame_token);
  return FindAndVerifyChildInternal(child_frame_or_proxy, reason);
}

FrameTreeNode* RenderFrameHostImpl::FindAndVerifyChildInternal(
    RenderFrameHostOrProxy child_frame_or_proxy,
    bad_message::BadMessageReason reason) {
  // A race can result in |child| to be nullptr. Avoid killing the renderer in
  // that case.
  if (!child_frame_or_proxy)
    return nullptr;

  if (&child_frame_or_proxy.GetFrameTreeNode()->frame_tree() != frame_tree()) {
    // Ignore the cases when the child lives in a different frame tree.
    // This is possible when we create a proxy for inner WebContents (e.g.
    // for portals) so the |child_frame_or_proxy| points to the root frame
    // of the nested WebContents, which is in a different tree.
    // TODO(altimin, lfg): Reconsider what the correct behaviour here should be.
    return nullptr;
  }

  if (child_frame_or_proxy.GetFrameTreeNode()->parent() != this) {
    bad_message::ReceivedBadMessage(GetProcess(), reason);
    return nullptr;
  }
  return child_frame_or_proxy.GetFrameTreeNode();
}

void RenderFrameHostImpl::UpdateTitle(
    const absl::optional<::std::u16string>& title,
    base::i18n::TextDirection title_direction) {
  // This message should only be sent for top-level frames.
  if (!is_main_frame())
    return;

  std::u16string received_title;
  if (title.has_value())
    received_title = title.value();

  if (received_title.length() > blink::mojom::kMaxTitleChars) {
    mojo::ReportBadMessage("Renderer sent too many characters in title.");
    return;
  }

  delegate_->UpdateTitle(this, received_title, title_direction);
}

void RenderFrameHostImpl::DidInferColorScheme(
    blink::mojom::PreferredColorScheme color_scheme) {
  if (is_main_frame()) {
    GetPage().DidInferColorScheme(color_scheme);
  }
}

void RenderFrameHostImpl::UpdateEncoding(const std::string& encoding_name) {
  if (!is_main_frame()) {
    mojo::ReportBadMessage("Renderer sent updated encoding for a subframe.");
    return;
  }

  GetPage().UpdateEncoding(encoding_name);
}

void RenderFrameHostImpl::FullscreenStateChanged(
    bool is_fullscreen,
    blink::mojom::FullscreenOptionsPtr options) {
  if (IsInactiveAndDisallowActivation(
          DisallowActivationReasonId::kFullScreenStateChanged))
    return;
  delegate_->FullscreenStateChanged(this, is_fullscreen, std::move(options));
}

bool RenderFrameHostImpl::CanUseWindowingControls(
    base::StringPiece js_api_name) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  mojo::ReportBadMessage(
      base::StrCat({js_api_name,
                    " API is only supported on Desktop platforms. This "
                    "excludes mobile platforms."}));
  return false;
#else
  if (!base::FeatureList::IsEnabled(
          blink::features::kDesktopPWAsAdditionalWindowingControls)) {
    mojo::ReportBadMessage(base::StrCat(
        {"API called without Additional Windowing Controls feature enabled: ",
         js_api_name}));
    return false;
  }
  if (!IsInPrimaryMainFrame()) {
    mojo::ReportBadMessage(base::StrCat(
        {"API called from a non-primary-main frame: ", js_api_name}));
    return false;
  }
  if (!IsActive()) {
    mojo::ReportBadMessage(
        base::StrCat({"API called from a non-active frame: ", js_api_name}));
    return false;
  }

  // These checks don't kill RFH, instead they log to the developer console.
  if (!IsWindowManagementGranted(this)) {
    AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning,
        base::StrCat({"API called without `window-management` permission "
                      "being granted: ",
                      js_api_name}));
    return false;
  }
  if (WebContents* web_contents = WebContents::FromRenderFrameHost(this)) {
    return web_contents->GetDelegate()->CanUseWindowingControls(this);
  }
  return false;
#endif
}

void RenderFrameHostImpl::Maximize() {
  const std::string js_api_name = "window.maximize";
#if !defined(USE_AURA)
  // TODO(laurila, crbug.com/1466851): Enable on Mac, when it's supported.
  mojo::ReportBadMessage(
      base::StrCat({js_api_name, " API is not supported on MacOS yet."}));
#endif

  if (!CanUseWindowingControls(js_api_name)) {
    return;
  }

  delegate_->Maximize();
}

void RenderFrameHostImpl::Minimize() {
  const std::string js_api_name = "window.minimize";

#if !defined(USE_AURA)
  // TODO(laurila, crbug.com/1466851): Enable on Mac, when it's supported.
  mojo::ReportBadMessage(
      base::StrCat({js_api_name, " API is not supported on MacOS yet."}));
#endif

  if (!CanUseWindowingControls(js_api_name)) {
    return;
  }

  delegate_->Minimize();
}

void RenderFrameHostImpl::Restore() {
  const std::string js_api_name = "window.restore";

#if !defined(USE_AURA)
  // TODO(laurila, crbug.com/1466851): Enable on Mac, when it's supported.
  mojo::ReportBadMessage(
      base::StrCat({js_api_name, " API is not supported on MacOS yet."}));
#endif

  if (!CanUseWindowingControls("window.restore")) {
    return;
  }

  delegate_->Restore();
}

void RenderFrameHostImpl::SetResizable(bool resizable) {
  if (!CanUseWindowingControls("window.setResizable")) {
    return;
  }
  if (WebContents* web_contents = WebContents::FromRenderFrameHost(this)) {
    if (web_contents->GetDelegate()) {
      web_contents->GetDelegate()->SetCanResizeFromWebAPI(resizable);
    }
  }
}

void RenderFrameHostImpl::RegisterProtocolHandler(const std::string& scheme,
                                                  const GURL& url,
                                                  bool user_gesture) {
  delegate_->RegisterProtocolHandler(this, scheme, url, user_gesture);
}

void RenderFrameHostImpl::UnregisterProtocolHandler(const std::string& scheme,
                                                    const GURL& url,
                                                    bool user_gesture) {
  delegate_->UnregisterProtocolHandler(this, scheme, url, user_gesture);
}

void RenderFrameHostImpl::DidDisplayInsecureContent() {
  frame_tree_->controller().ssl_manager()->DidDisplayMixedContent();
}

void RenderFrameHostImpl::DidContainInsecureFormAction() {
  frame_tree_->controller().ssl_manager()->DidContainInsecureFormAction();
}

void RenderFrameHostImpl::MainDocumentElementAvailable(
    bool uses_temporary_zoom_level) {
  if (!is_main_frame()) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFH_INVALID_CALL_FROM_NOT_MAIN_FRAME);
    return;
  }

  GetPage().set_is_main_document_element_available(true);
  GetPage().set_uses_temporary_zoom_level(uses_temporary_zoom_level);

  // Don't dispatch PrimaryMainDocumentElementAvailable for non-primary
  // RenderFrameHosts. As most of the observers are interested only in taking
  // into account and can interact with or send IPCs to only the current
  // document in the primary main frame. Since the WebContents could be hosting
  // more than one main frame (e.g., fenced frame, prerender pages or pending
  // delete RFHs), return early for other cases.
  if (!IsInPrimaryMainFrame())
    return;

  delegate_->PrimaryMainDocumentElementAvailable();

  if (base::FeatureList::IsEnabled(blink::features::kFullscreenPopupWindows) &&
      fullscreen_document_on_document_element_ready_ &&
      (*fullscreen_document_on_document_element_ready_) == GetDocumentToken()) {
    fullscreen_document_on_document_element_ready_ = absl::nullopt;
    // Create a fullscreen request token to waive the user activation
    // requirement when the request is received back to the browser in
    // `EnterFullscreen`.
    fullscreen_request_token_.Activate();
    GetAssociatedLocalFrame()->RequestFullscreenDocumentElement();
  }

  if (!uses_temporary_zoom_level)
    return;

#if !BUILDFLAG(IS_ANDROID)
  HostZoomMapImpl* host_zoom_map =
      static_cast<HostZoomMapImpl*>(HostZoomMap::Get(GetSiteInstance()));
  host_zoom_map->SetTemporaryZoomLevel(GetGlobalId(),
                                       host_zoom_map->GetDefaultZoomLevel());
#endif  // !BUILDFLAG(IS_ANDROID)
}

void RenderFrameHostImpl::SetNeedsOcclusionTracking(bool needs_tracking) {
  // Do not update the parent on behalf of inactive RenderFrameHost. See also
  // https://crbug.com/972566.
  if (IsInactiveAndDisallowActivation(
          DisallowActivationReasonId::kSetNeedsOcclusionTracking))
    return;

  RenderFrameProxyHost* proxy = GetProxyToParent();
  if (!proxy) {
    bad_message::ReceivedBadMessage(GetProcess(),
                                    bad_message::RFH_NO_PROXY_TO_PARENT);
    return;
  }

  if (proxy->is_render_frame_proxy_live()) {
    proxy->GetAssociatedRemoteFrame()->SetNeedsOcclusionTracking(
        needs_tracking);
  }
}

void RenderFrameHostImpl::SetVirtualKeyboardMode(
    ui::mojom::VirtualKeyboardMode mode) {
  // TODO(crbug.com/1225366): Consider moving this to PageImpl.
  if (GetOutermostMainFrame() != this) {
    bad_message::ReceivedBadMessage(
        GetProcess(),
        bad_message::RFHI_SET_OVERLAYS_CONTENT_NOT_OUTERMOST_FRAME);
    return;
  }
  GetPage().SetVirtualKeyboardMode(mode);
}

#if BUILDFLAG(IS_ANDROID)
void RenderFrameHostImpl::UpdateUserGestureCarryoverInfo() {
  // This should not occur for prerenders but may occur for pages in
  // the BackForwardCache depending on timing.
  if (!IsActive())
    return;
  delegate_->UpdateUserGestureCarryoverInfo();
}
#endif

void RenderFrameHostImpl::VisibilityChanged(
    blink::mojom::FrameVisibility visibility) {
  visibility_ = visibility;
  delegate_->OnFrameVisibilityChanged(this, visibility_);
}

void RenderFrameHostImpl::DidChangeThemeColor(
    absl::optional<SkColor> theme_color) {
  // TODO(crbug.com/1225366): Consider moving this to PageImpl.
  DCHECK(is_main_frame());
  GetPage().OnThemeColorChanged(theme_color);
}

void RenderFrameHostImpl::DidChangeBackgroundColor(
    const SkColor4f& background_color,
    bool color_adjust) {
  // TODO(crbug.com/1225366): Consider moving this to PageImpl.
  DCHECK(is_main_frame());
  GetPage().DidChangeBackgroundColor(background_color, color_adjust);
}

void RenderFrameHostImpl::SetCommitCallbackInterceptorForTesting(
    CommitCallbackInterceptor* interceptor) {
  // This DCHECK's aims to avoid unexpected replacement of an interceptor.
  // If this becomes a legitimate use case, feel free to remove.
  DCHECK(!commit_callback_interceptor_ || !interceptor);
  commit_callback_interceptor_ = interceptor;
}

void RenderFrameHostImpl::SetCreateNewPopupCallbackForTesting(
    const CreateNewPopupWidgetCallbackForTesting& callback) {
  // This DCHECK aims to avoid unexpected replacement of a callback.
  DCHECK(!create_new_popup_widget_callback_ || !callback);
  create_new_popup_widget_callback_ = callback;
}

void RenderFrameHostImpl::SetUnloadACKCallbackForTesting(
    const UnloadACKCallbackForTesting& callback) {
  // This DCHECK aims to avoid unexpected replacement of a callback.
  DCHECK(!unload_ack_callback_ || !callback);
  unload_ack_callback_ = callback;
}

const net::HttpResponseHeaders* RenderFrameHostImpl::GetLastResponseHeaders() {
  // This shouldn't be called before committing the document as this value is
  // set during call to RenderFrameHostImpl::DidNavigate which happens after
  // commit.
  DCHECK_NE(lifecycle_state(), LifecycleStateImpl::kSpeculative);
  DCHECK_NE(lifecycle_state(), LifecycleStateImpl::kPendingCommit);
  return last_response_head_ ? last_response_head_->headers.get() : nullptr;
}

void RenderFrameHostImpl::DidBlockNavigation(
    const GURL& blocked_url,
    const GURL& initiator_url,
    blink::mojom::NavigationBlockedReason reason) {
  // Cross-origin navigations are not allowed in prerendering so we can not
  // reach here while prerendering.
  DCHECK_NE(lifecycle_state(), LifecycleStateImpl::kPrerendering);
  delegate_->OnDidBlockNavigation(blocked_url, initiator_url, reason);
}

void RenderFrameHostImpl::DidChangeLoadProgress(double load_progress) {
  if (!is_main_frame())
    return;

  if (load_progress < GetPage().load_progress())
    return;

  GetPage().set_load_progress(load_progress);

  // Only dispatch LoadProgressChanged for the primary main frame.
  if (IsInPrimaryMainFrame())
    delegate_->DidChangeLoadProgressForPrimaryMainFrame();
}

void RenderFrameHostImpl::DidFinishLoad(const GURL& validated_url) {
  // In case of prerendering, we dispatch DidFinishLoad on activation. This is
  // done to avoid notifying observers about a load event triggered from a
  // inactive RenderFrameHost.
  if (lifecycle_state() == LifecycleStateImpl::kPrerendering) {
    document_associated_data_->set_pending_did_finish_load_url_for_prerendering(
        validated_url);
    return;
  }

  delegate_->OnDidFinishLoad(this, validated_url);
}

void RenderFrameHostImpl::DispatchLoad() {
  TRACE_EVENT1("navigation", "RenderFrameHostImpl::DispatchLoad",
               "render_frame_host", this);

  // Only active and prerendered documents are allowed to dispatch load events
  // to the parent.
  if (lifecycle_state() != LifecycleStateImpl::kPrerendering) {
    // Don't forward the load event to the parent on behalf of inactive
    // RenderFrameHost. This can happen in a race where this inactive
    // RenderFrameHost finishes loading just after the frame navigates away.
    // See https://crbug.com/626802.
    if (IsInactiveAndDisallowActivation(
            DisallowActivationReasonId::kDispatchLoad))
      return;
  }

  DCHECK(lifecycle_state() == LifecycleStateImpl::kActive ||
         lifecycle_state() == LifecycleStateImpl::kPrerendering);

  // Only frames with an out-of-process parent frame should be sending this
  // message.
  RenderFrameProxyHost* proxy = GetProxyToParent();
  if (!proxy) {
    bad_message::ReceivedBadMessage(GetProcess(),
                                    bad_message::RFH_NO_PROXY_TO_PARENT);
    return;
  }

  if (proxy->is_render_frame_proxy_live())
    proxy->GetAssociatedRemoteFrame()->DispatchLoadEventForFrameOwner();
}

void RenderFrameHostImpl::GoToEntryAtOffset(
    int32_t offset,
    bool has_user_gesture,
    absl::optional<blink::scheduler::TaskAttributionId>
        soft_navigation_heuristics_task_id) {
  OPTIONAL_TRACE_EVENT2("content", "RenderFrameHostImpl::GoToEntryAtOffset",
                        "render_frame_host", this, "offset", offset);

  // Non-user initiated navigations coming from the renderer should be ignored
  // if there is an ongoing browser-initiated navigation.
  // See https://crbug.com/879965.
  // TODO(arthursonzogni): See if this should check for ongoing navigations in
  // the frame(s) affected by the session history navigation, rather than just
  // the main frame.
  if (Navigator::ShouldIgnoreIncomingRendererRequest(
          frame_tree_->root()->navigation_request(), has_user_gesture)) {
    return;
  }

  // All frames are allowed to navigate the global history.
  if (delegate_->IsAllowedToGoToEntryAtOffset(offset)) {
    frame_tree_->controller().GoToOffsetFromRenderer(
        offset, this, soft_navigation_heuristics_task_id);
  }
}

void RenderFrameHostImpl::NavigateToNavigationApiKey(
    const std::string& key,
    bool has_user_gesture,
    absl::optional<blink::scheduler::TaskAttributionId> task_id) {
  // Non-user initiated navigations coming from the renderer should be ignored
  // if there is an ongoing browser-initiated navigation.
  // See https://crbug.com/879965.
  // TODO(arthursonzogni): See if this should check for ongoing navigations in
  // the frame(s) affected by the session history navigation, rather than just
  // the main frame.
  if (Navigator::ShouldIgnoreIncomingRendererRequest(
          frame_tree_->root()->navigation_request(), has_user_gesture)) {
    return;
  }
  frame_tree_->controller().NavigateToNavigationApiKey(this, task_id, key);
}

void RenderFrameHostImpl::NavigateEventHandlerPresenceChanged(bool present) {
  DCHECK_NE(has_navigate_event_handler_, present);
  has_navigate_event_handler_ = present;
}

void RenderFrameHostImpl::HandleAccessibilityFindInPageResult(
    blink::mojom::FindInPageResultAXParamsPtr params) {
  // Only update FindInPageResult on active RenderFrameHost. Note that, it is
  // safe to ignore this call for BackForwardCache, as we terminate the
  // FindInPage session once the page enters BackForwardCache.
  if (lifecycle_state() != LifecycleStateImpl::kActive)
    return;

  BrowserAccessibilityManager* manager =
      GetOrCreateBrowserAccessibilityManager();
  if (manager) {
    manager->OnFindInPageResult(params->request_id, params->match_index,
                                params->start_id, params->start_offset,
                                params->end_id, params->end_offset);
  }
}

void RenderFrameHostImpl::HandleAccessibilityFindInPageTermination() {
  // Only update FindInPageTermination on active RenderFrameHost. Note that, it
  // is safe to ignore this call for BackForwardCache, as we terminate the
  // FindInPage session once the page enters BackForwardCache.
  if (lifecycle_state() != LifecycleStateImpl::kActive)
    return;

  BrowserAccessibilityManager* manager =
      GetOrCreateBrowserAccessibilityManager();
  if (manager)
    manager->OnFindInPageTermination();
}

// TODO(crbug.com/1213863): Move this method to content::PageImpl.
void RenderFrameHostImpl::DocumentOnLoadCompleted() {
  if (!is_main_frame()) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFH_INVALID_CALL_FROM_NOT_MAIN_FRAME);
    return;
  }

  GetPage().set_is_on_load_completed_in_main_document(true);

  // Don't dispatch DocumentOnLoadCompletedInPrimaryMainFrame for non-primary
  // main frames. As most of the observers are interested only in the onload
  // completion of the current document in the primary main frame. Since the
  // WebContents could be hosting more than one main frame (e.g., fenced frames,
  // prerender pages or pending delete RFHs), return early for other cases. In
  // case of prerendering, we dispatch DocumentOnLoadCompletedInPrimaryMainFrame
  // on activation.
  if (!IsInPrimaryMainFrame())
    return;

  // This message is only sent for top-level frames.
  //
  // TODO(avi): when frame tree mirroring works correctly, add a check here
  // to enforce it.
  delegate_->DocumentOnLoadCompleted(this);
}

void RenderFrameHostImpl::ForwardResourceTimingToParent(
    blink::mojom::ResourceTimingInfoPtr timing) {
  // Only active and prerendered documents are allowed to forward the resource
  // timing information to the parent.
  if (lifecycle_state() != LifecycleStateImpl::kPrerendering) {
    // Don't forward the resource timing of the parent on behalf of inactive
    // RenderFrameHost. This can happen in a race where this RenderFrameHost
    // finishes loading just after the frame navigates away. See
    // https://crbug.com/626802.
    if (IsInactiveAndDisallowActivation(
            DisallowActivationReasonId::kForwardResourceTimingToParent))
      return;
  }

  DCHECK(lifecycle_state() == LifecycleStateImpl::kActive ||
         lifecycle_state() == LifecycleStateImpl::kPrerendering);

  RenderFrameProxyHost* proxy = GetProxyToParent();
  if (!proxy) {
    bad_message::ReceivedBadMessage(GetProcess(),
                                    bad_message::RFH_NO_PROXY_TO_PARENT);
    return;
  }
  if (proxy->is_render_frame_proxy_live()) {
    proxy->GetAssociatedRemoteFrame()->AddResourceTimingFromChild(
        std::move(timing));
  }
}

bool RenderFrameHostImpl::Reload() {
  // Reloading the document frame will delete the currently active one.
  // We expected this function to be used only when this RenderFrameHost
  // is the currently active one. RenderFrameHost pending deletion, or
  // in the BackForwardCache are not expected to have side effect on the
  // current page.
  CHECK(IsActive());

  return owner_->Reload();
}

void RenderFrameHostImpl::SendAccessibilityEventsToManager(
    const AXEventNotificationDetails& details) {
  if (!browser_accessibility_manager_) {
    return;
  }

  DCHECK(delegate_->GetAccessibilityMode().has_mode(ui::AXMode::kNativeAPIs));
  if (!browser_accessibility_manager_->OnAccessibilityEvents(details)) {
    // OnAccessibilityEvents returns false in IPC error conditions.
    AccessibilityFatalError();
  }
}

bool RenderFrameHostImpl::IsInactiveAndDisallowActivation(uint64_t reason) {
  TRACE_EVENT1("navigation",
               "RenderFrameHostImpl::IsInactiveAndDisallowActivation",
               "render_frame_host", this);
  switch (lifecycle_state_) {
    case LifecycleStateImpl::kRunningUnloadHandlers:
    case LifecycleStateImpl::kReadyToBeDeleted:
      return true;
    case LifecycleStateImpl::kInBackForwardCache: {
      // This function should not be called with kAXEvent when the page is in
      // back/forward cache, because |HandleAXevents()| will continue to process
      // accessibility events without evicting unless the kEvictOnAXEvents flag
      // is on.
      if (!base::FeatureList::IsEnabled(features::kEvictOnAXEvents))
        DCHECK_NE(reason, kAXEvent);
      BackForwardCacheCanStoreDocumentResult can_store_flat;
      can_store_flat.NoDueToDisallowActivation(reason);
      EvictFromBackForwardCacheWithFlattenedReasons(can_store_flat);
    }
      return true;
    case LifecycleStateImpl::kPrerendering:
      // Since the page in prerendering state is able to handle IndexedDB event,
      // the prerendering should not be cancelled because of that.
      DCHECK_NE(reason, DisallowActivationReasonId::kIndexedDBEvent);
      CancelPrerendering(
          PrerenderCancellationReason::BuildForDisallowActivationState(reason));
      return true;
    case LifecycleStateImpl::kSpeculative:
      // We do not expect speculative or pending commit RenderFrameHosts to
      // generate events that require an active/inactive check. Don't crash the
      // browser process in case it comes from a compromised renderer, but kill
      // the renderer to avoid further confusion.
      bad_message::ReceivedBadMessage(
          GetProcess(), bad_message::RFH_INACTIVE_CHECK_FROM_SPECULATIVE_RFH);
      return false;
    case LifecycleStateImpl::kPendingCommit:
      // TODO(https://crbug.com/1191469): Understand the expected behaviour to
      // disallow activation for kPendingCommit RenderFrameHosts and update
      // accordingly.
      bad_message::ReceivedBadMessage(
          GetProcess(),
          bad_message::RFH_INACTIVE_CHECK_FROM_PENDING_COMMIT_RFH);
      return false;
    case LifecycleStateImpl::kActive:
      return false;
  }
}

bool RenderFrameHostImpl::IsInactiveAndDisallowActivationForAXEvents(
    const std::vector<ui::AXEvent>& events) {
  DCHECK(base::FeatureList::IsEnabled(features::kEvictOnAXEvents));
  if (lifecycle_state_ != LifecycleStateImpl::kInBackForwardCache) {
    return IsInactiveAndDisallowActivation(
        DisallowActivationReasonId::kAXEvent);
  }
  // If the lifecycle state is |LifecycleStateImpl::kInBackForwardCache|, we
  // cannot handle accessibility events any more. We should evict the entry.
  BackForwardCacheCanStoreDocumentResult can_store_flat;
  can_store_flat.NoDueToAXEvents(events);
  EvictFromBackForwardCacheWithFlattenedReasons(can_store_flat);
  return true;
}

void RenderFrameHostImpl::EvictFromBackForwardCache(
    blink::mojom::RendererEvictionReason reason) {
  EvictFromBackForwardCacheWithReason(
      RendererEvictionReasonToNotRestoredReason(reason));
}

void RenderFrameHostImpl::EvictFromBackForwardCacheWithReason(
    BackForwardCacheMetrics::NotRestoredReason reason) {
  // kIgnoreEventAndEvict should never be a reason on its own without further
  // details.
  DCHECK_NE(reason,
            BackForwardCacheMetrics::NotRestoredReason::kIgnoreEventAndEvict);

  BackForwardCacheCanStoreDocumentResult flattened_reasons;
  flattened_reasons.No(reason);
  EvictFromBackForwardCacheWithFlattenedReasons(flattened_reasons);
}

void RenderFrameHostImpl::EvictFromBackForwardCacheWithFlattenedReasons(
    BackForwardCacheCanStoreDocumentResult can_store_flat) {
  // Create a NotRestoredReasons tree that has |can_store_flat| as a reason
  // for |this| RenderFrameHost.
  auto can_store =
      BackForwardCacheImpl::CreateEvictionBackForwardCacheCanStoreTreeResult(
          *this, can_store_flat);
  EvictFromBackForwardCacheWithFlattenedAndTreeReasons(can_store);
}

void RenderFrameHostImpl::EvictFromBackForwardCacheWithFlattenedAndTreeReasons(
    BackForwardCacheCanStoreDocumentResultWithTree& can_store) {
  TRACE_EVENT2("navigation", "RenderFrameHostImpl::EvictFromBackForwardCache",
               "can_store", can_store.flattened_reasons.ToString(), "rfh",
               static_cast<void*>(this));
  TRACE_EVENT("navigation",
              "RenderFrameHostImpl::"
              "EvictFromBackForwardCacheWithFlattenedAndTreeReasons",
              ChromeTrackEvent::kBackForwardCacheCanStoreDocumentResult,
              can_store.flattened_reasons);
  DCHECK(IsBackForwardCacheEnabled());

  RenderFrameHostImpl* top_document = GetOutermostMainFrame();

  if (top_document->is_evicted_from_back_forward_cache_)
    return;

  bool in_back_forward_cache = IsInBackForwardCache();

  // TODO(hajimehoshi): Record the 'race condition' by JavaScript execution when
  // |in_back_forward_cache| is false.
  BackForwardCacheMetrics* metrics = top_document->GetBackForwardCacheMetrics();
  if (in_back_forward_cache && metrics) {
    metrics->SetNotRestoredReasons(can_store);
  }

  if (!in_back_forward_cache) {
    TRACE_EVENT0("navigation", "BackForwardCache_EvictAfterDocumentRestored");
    // TODO(crbug.com/1163843): We should no longer get into this branch thanks
    // to https://crrev.com/c/2563674 but we do. We should eventually replace
    // this with a CHECK.
    BackForwardCacheMetrics::RecordEvictedAfterDocumentRestored(
        BackForwardCacheMetrics::EvictedAfterDocumentRestoredReason::
            kByJavaScript);
    CaptureTraceForNavigationDebugScenario(
        DebugScenario::kDebugBackForwardCacheEvictRestoreRace);

    // A document is evicted from the BackForwardCache, but it has already been
    // restored. The current document should be reloaded, because it is not
    // salvageable.
    top_document->frame_tree()->controller().Reload(ReloadType::NORMAL, false);
    return;
  }

  // Check if there is an in-flight navigation restoring the document that is
  // being evicted.
  NavigationRequest* in_flight_navigation_request =
      top_document->frame_tree_node()->navigation_request();

  if (in_flight_navigation_request &&
      in_flight_navigation_request
              ->GetRenderFrameHostRestoredFromBackForwardCache() ==
          top_document) {
    // If we are currently navigating to the document that was just evicted, we
    // must restart the navigation. This is important because restarting the
    // navigation deletes the `NavigationRequest` associated with the evicted
    // document (preventing use-after-free).
    // Restarting the navigation should also happen asynchronously as eviction
    // might happen in the middle of another navigation - we should not try to
    // restart the navigation in that case.
    top_document->frame_tree_node()->RestartBackForwardCachedNavigationAsync(
        in_flight_navigation_request->nav_entry_id());
  }

  // Evict the frame and schedule it to be destroyed. Eviction happens
  // immediately, but destruction is delayed, so that callers don't have to
  // worry about use-after-free of |this|.
  top_document->is_evicted_from_back_forward_cache_ = true;
  GetBackForwardCache().PostTaskToDestroyEvictedFrames();
}

void RenderFrameHostImpl::
    UseDummyStickyBackForwardCacheDisablingFeatureForTesting() {
  OnBackForwardCacheDisablingFeatureUsed(
      BackForwardCacheDisablingFeature::kDummy);
}

bool RenderFrameHostImpl::HasSeenRecentXrOverlaySetup() {
  static constexpr base::TimeDelta kMaxInterval = base::Seconds(1);
  base::TimeDelta delta = base::TimeTicks::Now() - last_xr_overlay_setup_time_;
  DVLOG(2) << __func__ << ": return " << (delta <= kMaxInterval);
  return delta <= kMaxInterval;
}

void RenderFrameHostImpl::SetIsXrOverlaySetup() {
  DVLOG(2) << __func__;
  last_xr_overlay_setup_time_ = base::TimeTicks::Now();
}

// TODO(alexmos): When the allowFullscreen flag is known in the browser
// process, use it to double-check that fullscreen can be entered here.
void RenderFrameHostImpl::EnterFullscreen(
    blink::mojom::FullscreenOptionsPtr options,
    EnterFullscreenCallback callback) {
  const bool had_fullscreen_token = fullscreen_request_token_.IsActive();

  // Frames (possibly a subframe) that are not active nor belonging to a primary
  // page should not enter fullscreen.
  if (!IsActive() || !GetPage().IsPrimary()) {
    std::move(callback).Run(/*granted=*/false);
    return;
  }

  // Entering fullscreen requires a transient user activation, a fullscreen
  // capability delegation token, a user-generated screen orientation change, or
  // another feature-specific transient allowance.
  // CanEnterFullscreenWithoutUserActivation is only ever true in tests, to
  // allow fullscreen when mocking screen orientation changes.
  if (!delegate_->HasSeenRecentScreenOrientationChange() &&
      !WindowManagementAllowsFullscreen() && !HasSeenRecentXrOverlaySetup() &&
      !GetContentClient()
           ->browser()
           ->CanEnterFullscreenWithoutUserActivation()) {
    // Consume any transient user activation and delegated fullscreen token.
    // Reject requests made without transient user activation or a token.
    // TODO(lanwei): Investigate whether we can terminate the renderer when
    // transient user activation and the delegated token are both inactive.
    CHECK(owner_);  // See `owner_` invariants about `IsActive()`.
    const bool consumed_activation = owner_->UpdateUserActivationState(
        blink::mojom::UserActivationUpdateType::kConsumeTransientActivation,
        blink::mojom::UserActivationNotificationType::kNone);
    const bool consumed_token = fullscreen_request_token_.ConsumeIfActive();
    if (!consumed_activation && !consumed_token) {
      DLOG(ERROR) << "Cannot enter fullscreen because there is no transient "
                  << "user activation, orientation change, XR overlay, nor "
                  << "capability delegation.";
      std::move(callback).Run(/*granted=*/false);
      return;
    }
  }

  if (!delegate_->CanEnterFullscreenMode(this, *options)) {
    std::move(callback).Run(/*granted=*/false);
    return;
  }

  // Allow sites with the Window Management permission to open a popup window
  // after requesting fullscreen on a specific screen of a multi-screen device.
  // This enables multi-screen content experiences from a single user gesture.
  const display::Screen* screen = display::Screen::GetScreen();
  display::Display display;
  if (screen && screen->GetNumDisplays() > 1 &&
      screen->GetDisplayWithDisplayId(options->display_id, &display) &&
      IsWindowManagementGranted(this)) {
    transient_allow_popup_.Activate();
  }

  std::move(callback).Run(/*granted=*/true);

  // Entering fullscreen from a cross-process subframe also affects all
  // renderers for ancestor frames, which will need to apply fullscreen CSS to
  // appropriate ancestor <iframe> elements, fire fullscreenchange events, etc.
  // Thus, walk through the ancestor chain of this frame and for each (parent,
  // child) pair, send a message about the pending fullscreen change to the
  // child's proxy in parent's SiteInstanceGroup. The renderer process will use
  // this to find the <iframe> element in the parent frame that will need
  // fullscreen styles. This is done at most once per SiteInstanceGroup: for
  // example, with a A-B-A-B hierarchy, if the bottom frame goes fullscreen,
  // this only needs to notify its parent, and Blink-side logic will take care
  // of applying necessary changes to the other two ancestors.
  std::set<SiteInstance*> notified_instances;
  notified_instances.insert(GetSiteInstance());
  for (RenderFrameHostImpl* rfh = this; rfh->GetParent();
       rfh = rfh->GetParent()) {
    SiteInstance* parent_site_instance = rfh->GetParent()->GetSiteInstance();
    if (base::Contains(notified_instances, parent_site_instance))
      continue;

    RenderFrameProxyHost* child_proxy =
        rfh->browsing_context_state()->GetRenderFrameProxyHost(
            static_cast<SiteInstanceImpl*>(parent_site_instance)->group());
    if (child_proxy->is_render_frame_proxy_live()) {
      child_proxy->GetAssociatedRemoteFrame()->WillEnterFullscreen(
          options.Clone());
      notified_instances.insert(parent_site_instance);
    }
  }

  // Focus the window if another frame may have delegated the capability.
  if (had_fullscreen_token && !GetView()->HasFocus())
    GetView()->Focus();
  delegate_->EnterFullscreenMode(this, *options);
  delegate_->FullscreenStateChanged(this, /*is_fullscreen=*/true,
                                    std::move(options));

  // The previous call might change the fullscreen state. We need to make sure
  // the renderer is aware of that, which is done via the resize message.
  // Typically, this will be sent as part of the call on the |delegate_| above
  // when resizing the native windows, but sometimes fullscreen can be entered
  // without causing a resize, so we need to ensure that the resize message is
  // sent in that case. We always send this to the main frame's widget, and if
  // there are any OOPIF widgets, this will also trigger them to resize via
  // frameRectsChanged.
  render_view_host_->GetWidget()->SynchronizeVisualProperties();
}

// TODO(alexmos): When the allowFullscreen flag is known in the browser
// process, use it to double-check that fullscreen can be entered here.
void RenderFrameHostImpl::ExitFullscreen() {
  delegate_->ExitFullscreenMode(/*will_cause_resize=*/true);

  // The previous call might change the fullscreen state. We need to make sure
  // the renderer is aware of that, which is done via the resize message.
  // Typically, this will be sent as part of the call on the |delegate_| above
  // when resizing the native windows, but sometimes fullscreen can be entered
  // without causing a resize, so we need to ensure that the resize message is
  // sent in that case. We always send this to the main frame's widget, and if
  // there are any OOPIF widgets, this will also trigger them to resize via
  // frameRectsChanged.
  render_view_host_->GetWidget()->SynchronizeVisualProperties();
}

void RenderFrameHostImpl::SuddenTerminationDisablerChanged(
    bool present,
    blink::mojom::SuddenTerminationDisablerType disabler_type) {
  switch (disabler_type) {
    case blink::mojom::SuddenTerminationDisablerType::kBeforeUnloadHandler:
      DCHECK_NE(has_before_unload_handler_, present);
      if (IsNestedWithinFencedFrame()) {
        bad_message::ReceivedBadMessage(
            GetProcess(),
            bad_message::RFH_BEFOREUNLOAD_HANDLER_NOT_ALLOWED_IN_FENCED_FRAME);
        return;
      }
      has_before_unload_handler_ = present;
      break;
    case blink::mojom::SuddenTerminationDisablerType::kPageHideHandler:
      DCHECK_NE(has_pagehide_handler_, present);
      has_pagehide_handler_ = present;
      break;
    case blink::mojom::SuddenTerminationDisablerType::kUnloadHandler:
      DCHECK_NE(has_unload_handler_, present);
      if (IsNestedWithinFencedFrame()) {
        bad_message::ReceivedBadMessage(
            GetProcess(),
            bad_message::RFH_UNLOAD_HANDLER_NOT_ALLOWED_IN_FENCED_FRAME);
        return;
      }
      has_unload_handler_ = present;
      break;
    case blink::mojom::SuddenTerminationDisablerType::kVisibilityChangeHandler:
      DCHECK_NE(has_visibilitychange_handler_, present);
      has_visibilitychange_handler_ = present;
      break;
  }
}

bool RenderFrameHostImpl::GetSuddenTerminationDisablerState(
    blink::mojom::SuddenTerminationDisablerType disabler_type) {
  if (do_not_delete_for_testing_ &&
      disabler_type !=
          blink::mojom::SuddenTerminationDisablerType::kBeforeUnloadHandler) {
    return true;
  }
  switch (disabler_type) {
    case blink::mojom::SuddenTerminationDisablerType::kBeforeUnloadHandler:
      return has_before_unload_handler_;
    case blink::mojom::SuddenTerminationDisablerType::kPageHideHandler:
      return has_pagehide_handler_;
    case blink::mojom::SuddenTerminationDisablerType::kUnloadHandler:
      return has_unload_handler_;
    case blink::mojom::SuddenTerminationDisablerType::kVisibilityChangeHandler:
      return has_visibilitychange_handler_;
  }
}

void RenderFrameHostImpl::DidDispatchDOMContentLoadedEvent() {
  document_associated_data_->MarkDomContentLoaded();

  // In case of prerendering, we dispatch DOMContentLoaded on activation. This
  // is done to avoid notifying observers about a load event triggered from a
  // inactive RenderFrameHost.
  if (lifecycle_state() == LifecycleStateImpl::kPrerendering)
    return;

  delegate_->DOMContentLoaded(this);
}

void RenderFrameHostImpl::FocusedElementChanged(
    bool is_editable_element,
    bool is_richly_editable_element,
    const gfx::Rect& bounds_in_frame_widget,
    blink::mojom::FocusType focus_type) {
  if (!GetView())
    return;

  has_focused_editable_element_ = is_editable_element;
  has_focused_richly_editable_element_ = is_richly_editable_element;

  // First convert the bounds to root view.
  delegate_->OnFocusedElementChangedInFrame(
      this,
      gfx::Rect(GetView()->TransformPointToRootCoordSpace(
                    bounds_in_frame_widget.origin()),
                bounds_in_frame_widget.size()),
      focus_type);
}

void RenderFrameHostImpl::TextSelectionChanged(const std::u16string& text,
                                               uint32_t offset,
                                               const gfx::Range& range) {
  RecordAction(base::UserMetricsAction("TextSelectionChanged"));
  has_selection_ = !text.empty();
  GetRenderWidgetHost()->SelectionChanged(text, offset, range);
}

void RenderFrameHostImpl::DidReceiveUserActivation() {
  delegate_->DidReceiveUserActivation(this);
}

void RenderFrameHostImpl::WebAuthnAssertionRequestSucceeded() {
  delegate_->WebAuthnAssertionRequestSucceeded(this);
}

void RenderFrameHostImpl::MaybeIsolateForUserActivation() {
  // If user activation occurs in a frame that previously triggered a site
  // isolation hint based on the Cross-Origin-Opener-Policy header, isolate the
  // corresponding site for all future BrowsingInstances.  We also do this for
  // user activation on any same-origin subframe of such COOP main frames.
  //
  // Note that without user activation, COOP-triggered site isolation is scoped
  // only to the current BrowsingInstance.  This prevents malicious sites from
  // silently loading COOP sites to put them on the isolation list and later
  // querying that state.
  if (GetMainFrame()
          ->GetSiteInstance()
          ->GetSiteInfo()
          .does_site_request_dedicated_process_for_coop()) {
    // The SiteInfo flag above should guarantee that we've already passed all
    // the isolation eligibility checks, such as having the corresponding
    // feature enabled or satisfying memory requirements.
    DCHECK(base::FeatureList::IsEnabled(
        features::kSiteIsolationForCrossOriginOpenerPolicy));

    bool is_same_origin_activation =
        GetParent() ? GetMainFrame()->GetLastCommittedOrigin().IsSameOriginWith(
                          GetLastCommittedOrigin())
                    : true;
    if (is_same_origin_activation) {
      SiteInstance::StartIsolatingSite(
          GetSiteInstance()->GetBrowserContext(),
          GetMainFrame()->GetLastCommittedURL(),
          ChildProcessSecurityPolicy::IsolatedOriginSource::WEB_TRIGGERED,
          SiteIsolationPolicy::ShouldPersistIsolatedCOOPSites());
    }
  }
}

void RenderFrameHostImpl::UpdateUserActivationState(
    blink::mojom::UserActivationUpdateType update_type,
    blink::mojom::UserActivationNotificationType notification_type) {
  // Don't update UserActivationState for non-active RenderFrameHost. In case
  // of BackForwardCache, this is only called for tests and it is safe to ignore
  // such requests.
  if (lifecycle_state() != LifecycleStateImpl::kActive)
    return;

  CHECK(owner_);  // See `owner_` invariants about `lifecycle_state_`.
  owner_->UpdateUserActivationState(update_type, notification_type);
}

void RenderFrameHostImpl::DidConsumeHistoryUserActivation() {
  // owner_ may be null for IsPendingDeletion() or IsInBackForwardCache(), in
  // which case the history user activation is managed by a different active
  // RenderFrameHost.
  if (owner_) {
    owner_->DidConsumeHistoryUserActivation();
  }
}

void RenderFrameHostImpl::HadStickyUserActivationBeforeNavigationChanged(
    bool value) {
  browsing_context_state_->OnSetHadStickyUserActivationBeforeNavigation(value);
}

void RenderFrameHostImpl::ScrollRectToVisibleInParentFrame(
    const gfx::RectF& rect_to_scroll,
    blink::mojom::ScrollIntoViewParamsPtr params) {
  // Do not update the parent on behalf of inactive RenderFrameHost.
  if (IsInactiveAndDisallowActivation(
          DisallowActivationReasonId::kDispatchLoad)) {
    return;
  }

  RenderFrameProxyHost* proxy = nullptr;

  if (IsFencedFrameRoot()) {
    // TODO(bokan): This is overly trusting of the renderer. We'll need some way
    // to verify that the browser is the one that initiated this
    // ScrollFocusedEditable process.
    // https://crbug.com/1123606. I&S tracker row 191.
    if (!params->for_focused_editable) {
      local_frame_host_receiver_.ReportBadMessage(
          "ScrollRectToVisibleInParentFrame can only be used for "
          "is_for_editable from a fenced frame");
      return;
    }

    proxy = GetProxyToOuterDelegate();
  } else {
    proxy = GetProxyToParent();
  }

  if (!proxy)
    return;

  proxy->ScrollRectToVisible(rect_to_scroll, std::move(params));
}

void RenderFrameHostImpl::BubbleLogicalScrollInParentFrame(
    blink::mojom::ScrollDirection direction,
    ui::ScrollGranularity granularity) {
  // Do not update the parent on behalf of inactive RenderFrameHost.
  if (IsInactiveAndDisallowActivation(
          DisallowActivationReasonId::kDispatchLoad)) {
    return;
  }

  // TODO(bokan): This is overly trusting of the renderer. Ideally we'd check
  // that a keyboard event was recently sent. https://crbug.com/1123606. I&S
  // tracker row 191.
  RenderFrameProxyHost* proxy =
      IsFencedFrameRoot() ? GetProxyToOuterDelegate() : GetProxyToParent();

  if (!proxy) {
    // Only frames with an out-of-process parent frame should be sending this
    // message.
    bad_message::ReceivedBadMessage(GetProcess(),
                                    bad_message::RFH_NO_PROXY_TO_PARENT);
    return;
  }

  if (proxy->is_render_frame_proxy_live()) {
    proxy->GetAssociatedRemoteFrame()->BubbleLogicalScroll(direction,
                                                           granularity);
  }
}

void RenderFrameHostImpl::ShowPopupMenu(
    mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client,
    const gfx::Rect& bounds,
    int32_t item_height,
    double font_size,
    int32_t selected_item,
    std::vector<blink::mojom::MenuItemPtr> menu_items,
    bool right_aligned,
    bool allow_multiple_selection) {
#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
  // Do not open popups in an inactive document.
  if (!IsActive()) {
    // Sending this message requires user activation, which is impossible
    // for a prerendering document, so the renderer process should be
    // terminated. See
    // https://html.spec.whatwg.org/multipage/interaction.html#tracking-user-activation.
    if (lifecycle_state() == LifecycleStateImpl::kPrerendering) {
      bad_message::ReceivedBadMessage(
          GetProcess(), bad_message::RFH_POPUP_REQUEST_WHILE_PRERENDERING);
    }
    return;
  }

  if (delegate()->ShowPopupMenu(this, bounds)) {
    return;
  }

  auto* view = render_view_host()->delegate_->GetDelegateView();
  if (!view)
    return;

  gfx::Point original_point(bounds.x(), bounds.y());
  gfx::Point transformed_point =
      static_cast<RenderWidgetHostViewBase*>(GetView())
          ->TransformPointToRootCoordSpace(original_point);
  gfx::Rect transformed_bounds(transformed_point.x(), transformed_point.y(),
                               bounds.width(), bounds.height());
  view->ShowPopupMenu(this, std::move(popup_client), transformed_bounds,
                      item_height, font_size, selected_item,
                      std::move(menu_items), right_aligned,
                      allow_multiple_selection);
#endif
}

void RenderFrameHostImpl::ShowContextMenu(
    mojo::PendingAssociatedRemote<blink::mojom::ContextMenuClient>
        context_menu_client,
    const blink::UntrustworthyContextMenuParams& params) {
  if (IsInactiveAndDisallowActivation(
          DisallowActivationReasonId::kShowContextMenu))
    return;

  // Validate the URLs in |params|.  If the renderer can't request the URLs
  // directly, don't show them in the context menu.
  ContextMenuParams validated_params(params);
  // Freshly constructed ContextMenuParams have empty `page_url` and `frame_url`
  // - populate them based on trustworthy, browser-side data.
  validated_params.page_url = GetOutermostMainFrame()->GetLastCommittedURL();
  validated_params.frame_url = GetLastCommittedURL();
  validated_params.frame_origin = GetLastCommittedOrigin();
  validated_params.is_subframe = !!GetParentOrOuterDocument();

  // We don't validate |unfiltered_link_url| so that this field can be used
  // when users want to copy the original link URL.
  RenderProcessHost* process = GetProcess();
  process->FilterURL(true, &validated_params.link_url);
  process->FilterURL(true, &validated_params.src_url);
  // In theory `page_url` and `frame_url` come from a trustworthy data source
  // (from the browser process) and therefore don't need to be validated via
  // FilterURL below.  In practice, some scenarios depend on clearing of the
  // `page_url` - see https://crbug.com/1285312.
  process->FilterURL(false, &validated_params.page_url);
  process->FilterURL(true, &validated_params.frame_url);

  // It is necessary to transform the coordinates to account for nested
  // RenderWidgetHosts, such as with out-of-process iframes.
  gfx::Point original_point(validated_params.x, validated_params.y);
  gfx::Point transformed_point =
      static_cast<RenderWidgetHostViewBase*>(GetView())
          ->TransformPointToRootCoordSpace(original_point);
  validated_params.x = transformed_point.x();
  validated_params.y = transformed_point.y();

  if (validated_params.selection_start_offset < 0) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFH_NEGATIVE_SELECTION_START_OFFSET);
    return;
  }

  delegate_->ShowContextMenu(*this, std::move(context_menu_client),
                             validated_params);
}

void RenderFrameHostImpl::DidLoadResourceFromMemoryCache(
    const GURL& url,
    const std::string& http_method,
    const std::string& mime_type,
    network::mojom::RequestDestination request_destination,
    bool include_credentials) {
  delegate_->DidLoadResourceFromMemoryCache(this, url, http_method, mime_type,
                                            request_destination,
                                            include_credentials);
}

void RenderFrameHostImpl::DidChangeFrameOwnerProperties(
    const blink::FrameToken& child_frame_token,
    blink::mojom::FrameOwnerPropertiesPtr properties) {
  auto* child =
      FindAndVerifyChild(child_frame_token, bad_message::RFH_OWNER_PROPERTY);
  if (!child)
    return;

  bool has_display_none_property_changed =
      properties->is_display_none !=
      child->frame_owner_properties().is_display_none;

  child->set_frame_owner_properties(*properties);

  child->render_manager()->OnDidUpdateFrameOwnerProperties(*properties);
  if (has_display_none_property_changed) {
    delegate_->DidChangeDisplayState(child->current_frame_host(),
                                     properties->is_display_none);
  }
}

void RenderFrameHostImpl::DidChangeOpener(
    const absl::optional<blink::LocalFrameToken>& opener_frame_token) {
  // `owner_` could be null when we get this message asynchronously from the
  // renderer in pending deletion state.
  if (!owner_)
    return;

  owner_->GetRenderFrameHostManager().DidChangeOpener(
      opener_frame_token, GetSiteInstance()->group());
}

void RenderFrameHostImpl::DidChangeIframeAttributes(
    const blink::FrameToken& child_frame_token,
    blink::mojom::IframeAttributesPtr attributes) {
  if (attributes->parsed_csp_attribute &&
      !ValidateCSPAttribute(
          attributes->parsed_csp_attribute->header->header_value)) {
    bad_message::ReceivedBadMessage(GetProcess(),
                                    bad_message::RFH_CSP_ATTRIBUTE);
    return;
  }

  if (attributes->browsing_topics &&
      !base::FeatureList::IsEnabled(blink::features::kBrowsingTopics)) {
    bad_message::ReceivedBadMessage(
        GetProcess(),
        bad_message::RFH_RECEIVED_INVALID_BROWSING_TOPICS_ATTRIBUTE);
    return;
  }

  if (attributes->shared_storage_writable &&
      (!base::FeatureList::IsEnabled(blink::features::kSharedStorageAPIM118))) {
    bad_message::ReceivedBadMessage(
        GetProcess(),
        bad_message::RFH_RECEIVED_INVALID_SHARED_STORAGE_WRITABLE_ATTRIBUTE);
    return;
  }

  auto* child = FindAndVerifyChild(
      child_frame_token, bad_message::RFH_DID_CHANGE_IFRAME_ATTRIBUTE);
  if (!child)
    return;

  child->SetAttributes(std::move(attributes));
}

void RenderFrameHostImpl::DidChangeFramePolicy(
    const blink::FrameToken& child_frame_token,
    const blink::FramePolicy& frame_policy) {
  // Ensure that a frame can only update sandbox flags or permissions policy for
  // its immediate children.  If this is not the case, the renderer is
  // considered malicious and is killed.
  FrameTreeNode* child = FindAndVerifyChild(
      // TODO(iclelland): Rename this message
      child_frame_token, bad_message::RFH_SANDBOX_FLAGS);
  if (!child)
    return;

  child->SetPendingFramePolicy(frame_policy);

  // Notify the RenderFrame if it lives in a different process from its parent.
  // The frame's proxies in other processes also need to learn about the updated
  // flags and policy, but these notifications are sent later in
  // RenderFrameHostManager::CommitPendingFramePolicy(), when the frame
  // navigates and the new policies take effect.
  if (child->current_frame_host()->GetSiteInstance() != GetSiteInstance()) {
    child->current_frame_host()
        ->GetAssociatedLocalFrame()
        ->DidUpdateFramePolicy(frame_policy);
  }
}

void RenderFrameHostImpl::CapturePaintPreviewOfSubframe(
    const gfx::Rect& clip_rect,
    const base::UnguessableToken& guid) {
  if (IsInactiveAndDisallowActivation(
          DisallowActivationReasonId::kCapturePaintPreview))
    return;
  // This should only be called on a subframe.
  if (IsOutermostMainFrame()) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFH_SUBFRAME_CAPTURE_ON_MAIN_FRAME);
    return;
  }

  delegate()->CapturePaintPreviewOfCrossProcessSubframe(clip_rect, guid, this);
}

void RenderFrameHostImpl::SetCloseListener(
    mojo::PendingRemote<blink::mojom::CloseListener> listener) {
  CloseListenerHost::GetOrCreateForCurrentDocument(this)->SetListener(
      std::move(listener));
}

void RenderFrameHostImpl::BindBrowserInterfaceBrokerReceiver(
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> receiver) {
  DCHECK(receiver.is_valid());
  if (frame_tree()->is_prerendering()) {
    // RenderFrameHostImpl will rebind the receiver end of
    // BrowserInterfaceBroker if it receives a new one sent from renderer
    // processes. It happens when renderer processes navigate to a new document,
    // see RenderFrameImpl::DidCommitNavigation() and
    // RenderFrameHostImpl::DidCommitNavigation(). So before binding a new
    // receiver end of BrowserInterfaceBroker, RenderFrameHostImpl should drop
    // all deferred binders to avoid connecting Mojo pipes with old documents.
    DCHECK(mojo_binder_policy_applier_)
        << "prerendering pages should have a policy applier";
    mojo_binder_policy_applier_->DropDeferredBinders();
  }
  broker_receiver_.Bind(std::move(receiver));
  broker_receiver_.SetFilter(
      std::make_unique<internal::ActiveUrlMessageFilter>(this));
}

void RenderFrameHostImpl::BindAssociatedInterfaceProviderReceiver(
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterfaceProvider>
        receiver) {
  DCHECK(receiver.is_valid());
  associated_interface_provider_receiver_.Bind(std::move(receiver));
}

void RenderFrameHostImpl::BindDomOperationControllerHostReceiver(
    mojo::PendingAssociatedReceiver<mojom::DomAutomationControllerHost>
        receiver) {
  DCHECK(receiver.is_valid());
  // In the renderer side, the remote is document-associated so the receiver on
  // the browser side can be reused after a cross-document navigation.
  // TODO(dcheng): Make this document-associated?
  dom_automation_controller_receiver_.reset();
  dom_automation_controller_receiver_.Bind(std::move(receiver));
  dom_automation_controller_receiver_.SetFilter(
      CreateMessageFilterForAssociatedReceiver(
          mojom::DomAutomationControllerHost::Name_));
}

void RenderFrameHostImpl::SetKeepAliveTimeoutForTesting(
    base::TimeDelta timeout) {
  keep_alive_handle_factory_.set_timeout(timeout);
}

void RenderFrameHostImpl::UpdateState(const blink::PageState& state) {
  OPTIONAL_TRACE_EVENT1("content", "RenderFrameHostImpl::UpdateState",
                        "render_frame_host", this);
  // TODO(creis): Verify the state's ISN matches the last committed FNE.

  // Without this check, the renderer can trick the browser into using
  // filenames it can't access in a future session restore.
  if (!CanAccessFilesOfPageState(state)) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFH_CAN_ACCESS_FILES_OF_PAGE_STATE);
    return;
  }

  frame_tree_->controller().UpdateStateForFrame(this, state);
}

void RenderFrameHostImpl::OpenURL(blink::mojom::OpenURLParamsPtr params) {
  // Verify and unpack the Mojo payload.
  GURL validated_url;
  scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;
  if (!VerifyOpenURLParams(this, GetProcess(), params, &validated_url,
                           &blob_url_loader_factory)) {
    return;
  }

  // If the flag `is_unfenced_top_navigation` is set, this is a special code
  // path for MPArch fenced frames. The target frame doesn't have a handle
  // inside the MPArch renderer process, so we need to set it here.
  // TODO(crbug.com/1315802): Refactor _unfencedTop handling.
  if (params->is_unfenced_top_navigation) {
    GURL validated_params_url = params->url;

    // Check that the IPC parameters are valid and that the navigation
    // is allowed.
    // TODO(crbug.com/1315802): When this handling is refactored into a separate
    // IPC, make sure that the checks from VerifyOpenURLParams above are not
    // unintentionally weakened.
    if (!ValidateUnfencedTopNavigation(this, validated_params_url,
                                       GetProcess()->GetID(), params->post_body,
                                       params->user_gesture)) {
      return;
    }

    TRACE_EVENT1("navigation", "RenderFrameHostImpl::OpenURL(_unfencedTop)",
                 "url", validated_params_url.possibly_invalid_spec());

    // There are some relevant parameter changes to note:
    // Change the navigation target to the outermost frame.
    // This escapes Portals but not GuestViews.
    // - We don't especially care about Portals, because the frame won't be
    //   user activated until it's no longer in a portal
    // - We don't want _unfencedTop navigations to escape a GuestView
    //   (<webview>) and affect their embedder.
    RenderFrameHostImpl* target_frame = GetOutermostMainFrame();

    // Change `should_replace_current_entry` to false.
    // Fenced frames are enforced to have a history of length 1. Because the
    // renderer thinks this navigation is to the fenced frame root, it sets
    // `should_replace_current_entry` to true, but we do not want this
    // restriction for navigations outside the fenced frame.
    // TODO(crbug.com/1315802): Make sure that the browser doesn't rely on
    // whether the renderer says we should replace the current entry, i.e.
    // make sure there are no situations where we should actually replace the
    // current entry but don't, due to this line.
    bool should_replace_current_entry = false;

    // TODO(crbug.com/1315802): Null out the initiator origin, frame token, and
    // site instance.
    // We use an opaque `initiator_origin` in order to avoid leaking
    // information from the fenced frame to its embedder. (The navigation will
    // be treated as cross-origin unconditionally.) We don't need to provide a
    // `source_site_instance`.
    // url::Origin initiator_origin;

    // TODO(crbug.com/1237552): Resolve the discussion of download policy.
    blink::NavigationDownloadPolicy download_policy;

    target_frame->frame_tree_node()->navigator().NavigateFromFrameProxy(
        target_frame, validated_params_url,
        base::OptionalToPtr(params->initiator_frame_token),
        GetProcess()->GetID(), params->initiator_origin,
        params->initiator_base_url, GetSiteInstance(), content::Referrer(),
        ui::PAGE_TRANSITION_LINK, should_replace_current_entry, download_policy,
        "GET",
        /*post_body=*/nullptr, params->extra_headers,
        /*blob_url_loader_factory=*/nullptr,
        network::mojom::SourceLocation::New(), /*has_user_gesture=*/false,
        params->is_form_submission, params->impression,
        params->initiator_activation_and_ad_status, base::TimeTicks::Now(),
        /*is_embedder_initiated_fenced_frame_navigation=*/false,
        /*is_unfenced_top_navigation=*/true,
        /*force_new_browsing_instance=*/true);
    return;
  }

  TRACE_EVENT1("navigation", "RenderFrameHostImpl::OpenURL", "url",
               validated_url.possibly_invalid_spec());

  RenderFrameHostOwner* owner = owner_;
  // Inactive documents are not allowed to initiate navigations.
  // Also, see a similar check in RenderFrameHostImpl::BeginNavigation at
  // https://source.chromium.org/chromium/chromium/src/+/main:content/browser/renderer_host/render_frame_host_impl.cc;l=7761-7769;drc=6dc39d60fea45c003424272efdb4c366119a9d7f
  if (!owner) {
    return;
  }
  owner->GetCurrentNavigator().RequestOpenURL(
      this, validated_url, base::OptionalToPtr(params->initiator_frame_token),
      GetProcess()->GetID(), params->initiator_origin,
      params->initiator_base_url, params->post_body, params->extra_headers,
      params->referrer.To<content::Referrer>(), params->disposition,
      params->should_replace_current_entry, params->user_gesture,
      params->triggering_event_info, params->href_translate,
      std::move(blob_url_loader_factory), params->impression);
}

void RenderFrameHostImpl::GetAssociatedInterface(
    const std::string& name,
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterface>
        receiver) {
  // `associated_interface_provider_receiver_` and `associated_registry_` are
  // both reset at the same time, so we should never get a request for an
  // associated interface when `associated_registry_` is not valid.
  DCHECK(associated_registry_);

  // Perform Mojo capability control if `mojo_binder_policy_applier_` exists.
  if (mojo_binder_policy_applier_ &&
      !mojo_binder_policy_applier_->ApplyPolicyToAssociatedBinder(name)) {
    return;
  }

  mojo::ScopedInterfaceEndpointHandle handle = receiver.PassHandle();
  if (associated_registry_->TryBindInterface(name, &handle))
    return;
}

void RenderFrameHostImpl::DidStopLoading() {
  TRACE_EVENT("navigation", "RenderFrameHostImpl::DidStopLoading",
              ChromeTrackEvent::kRenderFrameHost, this);
  if (did_stop_loading_callback_)
    std::move(did_stop_loading_callback_).Run();

  // This may be called for newly created frames when the frame is not loading
  // that navigate to about:blank, as well as history navigations during
  // BeforeUnload or Unload events.
  // TODO(fdegans): Change this to a DCHECK after LoadEventProgress has been
  // refactored in Blink. See crbug.com/466089
  if (!is_loading()) {
    return;
  }

  was_discarded_ = false;
  loading_state_ = LoadingState::NONE;

  // If we have a PeakGpuMemoryTrack, close it as loading as stopped. It will
  // asynchronously receive the statistics from the GPU process, and update
  // UMA stats.
  if (loading_mem_tracker_)
    loading_mem_tracker_.reset();

  // Only inform the FrameTreeNode of a change in load state if the load state
  // of this RenderFrameHost is being tracked.
  // This async Mojo method can be called from the renderer before entering
  // BFCache but the message can arrive here after it.
  if (!IsPendingDeletion() && !IsInBackForwardCache()) {
    DCHECK(owner_);  // See `owner_` invariants about IsPendingDeletion() and
                     // IsInBackForwardCache().
    owner_->DidStopLoading();
  }
}

void RenderFrameHostImpl::GetSavableResourceLinksCallback(
    blink::mojom::GetSavableResourceLinksReplyPtr reply) {
  if (!reply) {
    delegate_->SavableResourceLinksError(this);
    return;
  }

  delegate_->SavableResourceLinksResponse(this, reply->resources_list,
                                          std::move(reply->referrer),
                                          reply->subframes);
}

void RenderFrameHostImpl::DomOperationResponse(const std::string& json_string) {
  delegate_->DomOperationResponse(this, json_string);
}

std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
RenderFrameHostImpl::CreateCrossOriginPrefetchLoaderFactoryBundle() {
  // IPCs coming from the renderer talk on behalf of the last-committed
  // navigation.  This also applies to IPCs asking for a prefetch factory
  // bundle.
  auto subresource_loader_factories_config =
      SubresourceLoaderFactoriesConfig::ForLastCommittedNavigation(*this);
  network::mojom::URLLoaderFactoryParamsPtr factory_params =
      URLLoaderFactoryParamsHelper::CreateForPrefetch(
          this, subresource_loader_factories_config.GetClientSecurityState(),
          subresource_loader_factories_config.cookie_setting_overrides());

  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_default_factory;
  bool bypass_redirect_checks = false;
  // Passing an empty IsolationInfo ensures the factory is not initialized with
  // a IsolationInfo. This is necessary for a cross-origin prefetch factory
  // because the factory must use the value provided by requests going through
  // it.
  bypass_redirect_checks = CreateNetworkServiceDefaultFactoryAndObserve(
      std::move(factory_params),
      subresource_loader_factories_config.ukm_source_id(),
      pending_default_factory.InitWithNewPipeAndPassReceiver());

  return std::make_unique<blink::PendingURLLoaderFactoryBundle>(
      std::move(pending_default_factory),
      blink::PendingURLLoaderFactoryBundle::SchemeMap(),
      CreateURLLoaderFactoriesForIsolatedWorlds(
          subresource_loader_factories_config,
          isolated_worlds_requiring_separate_url_loader_factory_),
      bypass_redirect_checks);
}

base::WeakPtr<RenderFrameHostImpl> RenderFrameHostImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

base::SafeRef<RenderFrameHostImpl> RenderFrameHostImpl::GetSafeRef() const {
  return weak_ptr_factory_.GetSafeRef();
}

void RenderFrameHostImpl::CreateNewWindow(
    mojom::CreateNewWindowParamsPtr params,
    CreateNewWindowCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT2("navigation", "RenderFrameHostImpl::CreateNewWindow",
               "render_frame_host", this, "url", params->target_url);

  // Only top-most frames can open picture-in-picture windows.
  if (params->disposition == WindowOpenDisposition::NEW_PICTURE_IN_PICTURE &&
      GetParentOrOuterDocumentOrEmbedder()) {
    frame_host_associated_receiver_.ReportBadMessage(
        "Only top-most frames can open picture-in-picture windows.");
    return;
  }

  bool no_javascript_access = false;

  // Filter out URLs to which navigation is disallowed from this context.
  GetProcess()->FilterURL(false, &params->target_url);

  bool effective_transient_activation_state =
      params->allow_popup || HasTransientUserActivation() ||
      (transient_allow_popup_.IsActive() &&
       params->disposition == WindowOpenDisposition::NEW_POPUP);

  // Ignore window creation when sent from a frame that's not active or
  // created.
  bool can_create_window =
      IsActive() && is_render_frame_created() &&
      GetContentClient()->browser()->CanCreateWindow(
          this, GetLastCommittedURL(),
          GetOutermostMainFrame()->GetLastCommittedURL(),
          last_committed_origin_, params->window_container_type,
          params->target_url, params->referrer.To<Referrer>(),
          params->frame_name, params->disposition, *params->features,
          effective_transient_activation_state, params->opener_suppressed,
          &no_javascript_access);

  // If this frame isn't allowed to create a window, return early (before we
  // consume transient user activation).
  if (!can_create_window) {
    std::move(callback).Run(mojom::CreateNewWindowStatus::kBlocked, nullptr);
    return;
  }

  // Otherwise, consume user activation before we proceed. In particular, it is
  // important to do this before we return from the |opener_suppressed| case
  // below.
  // NB: This call will consume activations in the browser and the remote frame
  // proxies for this frame. The initiating renderer will consume its view of
  // the activations after we return.

  // See `owner_` invariants about `IsActive()`, which is implied by
  // `can_create_window`.
  CHECK(owner_);
  bool was_consumed = owner_->UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kConsumeTransientActivation,
      blink::mojom::UserActivationNotificationType::kNone);

  // For Android WebView, we support a pop-up like behavior for window.open()
  // even if the embedding app doesn't support multiple windows. In this case,
  // window.open() will return "window" and navigate it to whatever URL was
  // passed.
  if (!GetOrCreateWebPreferences().supports_multiple_windows) {
    std::move(callback).Run(mojom::CreateNewWindowStatus::kReuse, nullptr);
    return;
  }

  // This will clone the sessionStorage for namespace_id_to_clone.
  StoragePartition* storage_partition = GetStoragePartition();
  DOMStorageContextWrapper* dom_storage_context =
      static_cast<DOMStorageContextWrapper*>(
          storage_partition->GetDOMStorageContext());

  scoped_refptr<SessionStorageNamespaceImpl> cloned_namespace;
  if (!params->clone_from_session_storage_namespace_id.empty()) {
    cloned_namespace = SessionStorageNamespaceImpl::CloneFrom(
        dom_storage_context, params->session_storage_namespace_id,
        params->clone_from_session_storage_namespace_id);
  } else {
    cloned_namespace = SessionStorageNamespaceImpl::Create(
        dom_storage_context, params->session_storage_namespace_id);
  }

  if (IsCredentialless() || IsNestedWithinFencedFrame() ||
      CoopSuppressOpener(/*opener=*/this)) {
    params->opener_suppressed = true;
    // TODO(https://crbug.com/1060691) This should be applied to all
    // popups opened with noopener.
    params->frame_name.clear();
  }

  RenderFrameHostImpl* top_level_opener = GetMainFrame();
  int popup_virtual_browsing_context_group =
      params->opener_suppressed
          ? CrossOriginOpenerPolicyAccessReportManager::
                GetNewVirtualBrowsingContextGroup()
          : top_level_opener->virtual_browsing_context_group();
  int popup_soap_by_default_virtual_browsing_context_group =
      params->opener_suppressed
          ? CrossOriginOpenerPolicyAccessReportManager::
                GetNewVirtualBrowsingContextGroup()
          : top_level_opener->soap_by_default_virtual_browsing_context_group();

  // If the opener is suppressed or script access is disallowed, we should
  // open the window in a new BrowsingInstance, and thus a new process. That
  // means the current renderer process will not be able to route messages to
  // it. Because of this, we will immediately show and navigate the window
  // in OnCreateNewWindowOnUI, using the params provided here.
  bool is_new_browsing_instance =
      params->opener_suppressed || no_javascript_access;

  DCHECK(IsRenderFrameLive());

  // The non-owning pointer |new_frame_tree| is valid in this stack frame since
  // nothing can delete it until this thread is freed up again.
  FrameTree* new_frame_tree =
      delegate_->CreateNewWindow(this, *params, is_new_browsing_instance,
                                 was_consumed, cloned_namespace.get());

  transient_allow_popup_.Deactivate();

  if (is_new_browsing_instance || !new_frame_tree) {
    // Opener suppressed, Javascript access disabled, or delegate did not
    // provide a handle to any windows it created. In these cases, never tell
    // the renderer about the new window.
    std::move(callback).Run(mojom::CreateNewWindowStatus::kIgnore, nullptr);
    return;
  }

  DCHECK(!params->opener_suppressed);

  RenderFrameHostImpl* new_main_rfh =
      new_frame_tree->root()->current_frame_host();

  new_main_rfh->virtual_browsing_context_group_ =
      popup_virtual_browsing_context_group;
  new_main_rfh->soap_by_default_virtual_browsing_context_group_ =
      popup_soap_by_default_virtual_browsing_context_group;

  // COOP and COOP reporter are inherited from the opener to the popup's initial
  // empty document.
  if (IsOpenerSameOriginFrame(/*opener=*/this) &&
      GetMainFrame()->coop_access_report_manager()->coop_reporter()) {
    new_main_rfh->SetCrossOriginOpenerPolicyReporter(
        std::make_unique<CrossOriginOpenerPolicyReporter>(
            GetProcess()->GetStoragePartition(), GetLastCommittedURL(),
            params->referrer->url,
            // TODO(https://crbug.com/1385827): See if we need to send the
            // origin to reporters as well.
            new_main_rfh->cross_origin_opener_policy(), GetReportingSource(),
            isolation_info_.network_anonymization_key()));
  }

  mojo::PendingAssociatedRemote<mojom::Frame> pending_frame_remote;
  mojo::PendingAssociatedReceiver<mojom::Frame> pending_frame_receiver =
      pending_frame_remote.InitWithNewEndpointAndPassReceiver();
  new_main_rfh->SetMojomFrameRemote(std::move(pending_frame_remote));

  mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
      browser_interface_broker;
  new_main_rfh->BindBrowserInterfaceBrokerReceiver(
      browser_interface_broker.InitWithNewPipeAndPassReceiver());

  mojo::PendingAssociatedRemote<blink::mojom::AssociatedInterfaceProvider>
      pending_associated_interface_provider;
  new_main_rfh->BindAssociatedInterfaceProviderReceiver(
      pending_associated_interface_provider
          .InitWithNewEndpointAndPassReceiver());

  // With this path, RenderViewHostImpl::CreateRenderView is never called
  // because `blink::WebView` is already created on the renderer side. Thus we
  // need to establish the connection here.
  mojo::PendingAssociatedRemote<blink::mojom::PageBroadcast> page_broadcast;
  mojo::PendingAssociatedReceiver<blink::mojom::PageBroadcast>
      page_broadcast_receiver =
          page_broadcast.InitWithNewEndpointAndPassReceiver();

  auto widget_params =
      new_main_rfh->GetLocalRenderWidgetHost()
          ->BindAndGenerateCreateFrameWidgetParamsForNewWindow();

  new_main_rfh->render_view_host()->BindPageBroadcast(
      std::move(page_broadcast));

  bool wait_for_debugger =
      devtools_instrumentation::ShouldWaitForDebuggerInWindowOpen();

  mojom::CreateNewWindowReplyPtr reply = mojom::CreateNewWindowReply::New(
      new_main_rfh->GetFrameToken(), new_main_rfh->GetRoutingID(),
      std::move(pending_frame_receiver), std::move(widget_params),
      std::move(page_broadcast_receiver), std::move(browser_interface_broker),
      std::move(pending_associated_interface_provider), cloned_namespace->id(),
      new_main_rfh->GetDevToolsFrameToken(), wait_for_debugger,
      new_main_rfh->GetDocumentToken(),
      new_main_rfh->policy_container_host()->CreatePolicyContainerForBlink(),
      blink::BrowsingContextGroupInfo(
          new_main_rfh->GetSiteInstance()->browsing_instance_token(),
          new_main_rfh->GetSiteInstance()->coop_related_group_token()));

  std::move(callback).Run(mojom::CreateNewWindowStatus::kSuccess,
                          std::move(reply));

  // The mojom reply callback with kSuccess causes the renderer to create the
  // renderer-side objects.
  new_main_rfh->render_view_host()->RenderViewCreated(new_main_rfh);
}

void RenderFrameHostImpl::SendLegacyTechEvent(
    const std::string& type,
    blink::mojom::LegacyTechEventCodeLocationPtr code_location) {
  GetContentClient()->browser()->ReportLegacyTechEvent(
      this, type,
      base::FeatureList::IsEnabled(features::kLegacyTechReportTopLevelUrl)
          ? GetOutermostMainFrameOrEmbedder()->GetLastCommittedURL()
          : GetLastCommittedURL(),
      code_location->filename, code_location->line, code_location->column);
}

void RenderFrameHostImpl::SendPrivateAggregationRequestsForFencedFrameEvent(
    const std::string& event_type) {
  if (!base::FeatureList::IsEnabled(blink::features::kPrivateAggregationApi) ||
      !blink::features::kPrivateAggregationApiEnabledInProtectedAudience
           .Get() ||
      !blink::features::kPrivateAggregationApiProtectedAudienceExtensionsEnabled
           .Get()) {
    mojo::ReportBadMessage(
        "FLEDGE extensions must be enabled to use reportEvent() for private "
        "aggregation events.");
    return;
  }
  // We only care if the event type starts with "reserved." - We allow event
  // types like "myevent.reserved.name".
  if (base::StartsWith(event_type, blink::kFencedFrameReservedPAEventPrefix)) {
    mojo::ReportBadMessage("Reserved events cannot be triggered manually.");
    return;
  }
  // Get the reporting metadata associated with the fenced frame.
  const absl::optional<FencedFrameProperties>& fenced_frame_properties =
      frame_tree_node_->GetFencedFrameProperties();
  if (!fenced_frame_properties.has_value() ||
      !fenced_frame_properties->fenced_frame_reporter_) {
    // No associated fenced frame reporter. This should have been captured
    // in the renderer process at `Fence::reportEvent`.
    // This implies there is an inconsistency between the browser and the
    // renderer.
    mojo::ReportBadMessage(
        "This frame had reporting metadata registered in its renderer process"
        "but not in its browser process. The reporting metadata should be"
        "consistent between the two.");
    return;
  }
  if (!fenced_frame_properties->mapped_url_.has_value() ||
      !GetLastCommittedOrigin().IsSameOriginWith(
          url::Origin::Create(fenced_frame_properties->mapped_url_
                                  ->GetValueIgnoringVisibility()))) {
    mojo::ReportBadMessage(
        "This frame is cross-origin to the mapped url of its fenced frame "
        "config, so the renderer should not be able to call reportEvent.");
    return;
  }

  fenced_frame_properties->fenced_frame_reporter_
      ->SendPrivateAggregationRequestsForEvent(event_type);
}

void RenderFrameHostImpl::CreatePortal(
    mojo::PendingAssociatedReceiver<blink::mojom::Portal> pending_receiver,
    mojo::PendingAssociatedRemote<blink::mojom::PortalClient> client,
    blink::mojom::RemoteFrameInterfacesFromRendererPtr remote_frame_interfaces,
    CreatePortalCallback callback) {
  if (!Portal::IsEnabled()) {
    frame_host_associated_receiver_.ReportBadMessage(
        "blink.mojom.Portal can only be used if the Portals feature is "
        "enabled.");
    return;
  }

  // If a Portal is nested inside a prerendered page, its owning RFH is also
  // |kPrerendering|. In this case the creation of the Portal is deferred from
  // the renderer.
  if (GetLifecycleState() == RenderFrameHost::LifecycleState::kPrerendering) {
    frame_host_associated_receiver_.ReportBadMessage(
        "RenderFrameHostImpl::CreatePortal cannot be called when the RFH is "
        "being prerendered.");
    return;
  }

  // We don't support attaching a portal inside a nested browsing context or
  // inside a fenced frame.
  if (!is_main_frame() || IsFencedFrameRoot()) {
    frame_host_associated_receiver_.ReportBadMessage(
        "RFHI::CreatePortal called in a nested browsing context");
    return;
  }

  // TODO(crbug.com/1051639): We need to find a long term solution to when/how
  // portals should work in sandboxed documents.
  if (active_sandbox_flags() != network::mojom::WebSandboxFlags::kNone) {
    frame_host_associated_receiver_.ReportBadMessage(
        "RFHI::CreatePortal called in a sandboxed browsing context");
    return;
  }

  // Note that we don't check |GetLastCommittedOrigin|, since that is inherited
  // by the initial empty document of a new frame.
  // TODO(1008989): Once issue 1008989 is fixed we could move this check into
  // |Portal::Create|.
  if (!GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
    frame_host_associated_receiver_.ReportBadMessage(
        "Portal creation is restricted to the HTTP family.");
    return;
  }

  auto portal = std::make_unique<Portal>(this);
  portal->Bind(std::move(pending_receiver), std::move(client));
  auto it = portals_.insert(std::move(portal)).first;

  RenderFrameProxyHost* proxy_host =
      (*it)->CreateProxyAndAttachPortal(std::move(remote_frame_interfaces));

  if (!proxy_host) {
    frame_host_associated_receiver_.ReportBadMessage(
        "Trying to attach a portal that has already been attached.");
    return;
  }

  // Since the portal is newly created and has yet to commit a navigation, this
  // state is default-constructed.
  const blink::mojom::FrameReplicationState& initial_replicated_state =
      proxy_host->frame_tree_node()->current_replication_state();
  DCHECK(initial_replicated_state.origin.opaque());

  std::move(callback).Run(initial_replicated_state.Clone(),
                          (*it)->portal_token(), proxy_host->GetFrameToken(),
                          (*it)->GetDevToolsFrameToken());
}

void RenderFrameHostImpl::AdoptPortal(
    const blink::PortalToken& portal_token,
    blink::mojom::RemoteFrameInterfacesFromRendererPtr remote_frame_interfaces,
    AdoptPortalCallback callback) {
  Portal* portal = FindPortalByToken(portal_token);
  if (!portal) {
    frame_host_associated_receiver_.ReportBadMessage(
        "Unknown portal_token when adopting portal.");
    return;
  }
  DCHECK_EQ(portal->owner_render_frame_host(), this);
  RenderFrameProxyHost* proxy_host =
      portal->CreateProxyAndAttachPortal(std::move(remote_frame_interfaces));

  if (!proxy_host) {
    frame_host_associated_receiver_.ReportBadMessage(
        "Trying to attach a portal that has already been attached.");
    return;
  }

  // |frame_sink_id| should be set to the associated frame. See
  // https://crbug.com/966119 for details.
  viz::FrameSinkId frame_sink_id = proxy_host->frame_tree_node()
                                       ->render_manager()
                                       ->GetRenderWidgetHostView()
                                       ->GetFrameSinkId();
  proxy_host->GetAssociatedRemoteFrame()->SetFrameSinkId(frame_sink_id);

  std::move(callback).Run(
      proxy_host->frame_tree_node()->current_replication_state().Clone(),
      proxy_host->GetFrameToken(), portal->GetDevToolsFrameToken());
}

std::vector<FencedFrame*> RenderFrameHostImpl::GetFencedFrames() const {
  std::vector<FencedFrame*> result;
  for (const std::unique_ptr<FencedFrame>& fenced_frame : fenced_frames_)
    result.push_back(fenced_frame.get());
  return result;
}

void RenderFrameHostImpl::DestroyFencedFrame(FencedFrame& fenced_frame) {
  auto it = base::ranges::find_if(fenced_frames_,
                                  base::MatchesUniquePtr(&fenced_frame));
  CHECK(it != fenced_frames_.end());
  fenced_frames_.erase(it);
}

void RenderFrameHostImpl::CreateFencedFrame(
    mojo::PendingAssociatedReceiver<blink::mojom::FencedFrameOwnerHost>
        pending_receiver,
    blink::mojom::RemoteFrameInterfacesFromRendererPtr remote_frame_interfaces,
    const blink::RemoteFrameToken& frame_token,
    const base::UnguessableToken& devtools_frame_token) {
  // We should defer fenced frame creation during prerendering, so creation at
  // this point is an error.
  if (GetLifecycleState() == RenderFrameHost::LifecycleState::kPrerendering) {
    bad_message::ReceivedBadMessage(GetProcess(),
                                    bad_message::FF_CREATE_WHILE_PRERENDERING);
    return;
  }
  if (!blink::features::IsFencedFramesEnabled()) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFH_FENCED_FRAME_MOJO_WHEN_DISABLED);
    return;
  }
  // Cannot create a fenced frame in a sandbox iframe which doesn't allow
  // features that need to be allowed in the fenced frame.
  if (IsSandboxed(blink::kFencedFrameMandatoryUnsandboxedFlags)) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFH_CREATE_FENCED_FRAME_IN_SANDBOXED_FRAME);
    return;
  }

  // Check that we have a unique `frame_token`.
  if (RenderFrameProxyHost::IsFrameTokenInUse(frame_token)) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFHI_CREATE_FENCED_FRAME_BAD_FRAME_TOKEN);
    return;
  }

  // Ensure the devtools frame token doesn't exist in the FrameTree for
  // this tab.
  for (FrameTreeNode* node :
       GetOutermostMainFrame()->frame_tree()->NodesIncludingInnerTreeNodes()) {
    if (node->current_frame_host()->devtools_frame_token() ==
        devtools_frame_token) {
      bad_message::ReceivedBadMessage(
          GetProcess(),
          bad_message::RFHI_CREATE_FENCED_FRAME_BAD_DEVTOOLS_FRAME_TOKEN);
      return;
    }
  }

  // Inactive pages cannot create fenced frames. If the page is in the BFCache,
  // it will be evicted.
  if (IsInactiveAndDisallowActivation(
          DisallowActivationReasonId::kCreateFencedFrame)) {
    return;
  }

  fenced_frames_.push_back(std::make_unique<FencedFrame>(
      weak_ptr_factory_.GetSafeRef(), was_discarded_));
  FencedFrame* fenced_frame = fenced_frames_.back().get();
  RenderFrameProxyHost* proxy_host =
      fenced_frame->InitInnerFrameTreeAndReturnProxyToOuterFrameTree(
          std::move(remote_frame_interfaces), frame_token,
          devtools_frame_token);
  fenced_frame->Bind(std::move(pending_receiver));

  // Since the fenced frame is newly created and has yet to commit a navigation,
  // this state is default-constructed.
  const blink::mojom::FrameReplicationState& initial_replicated_state =
      proxy_host->frame_tree_node()->current_replication_state();
  // Note that a default-constructed `FrameReplicationState` always has an
  // opaque origin, simply because the frame hasn't had any navigations yet.
  // Fenced frames (after their first navigation) do not have opaque origins,
  // and this default-constructed FRS does not impact that.
  DCHECK(initial_replicated_state.origin.opaque());
}

// TODO(crbug.com/1400992): Move SendFencedFrameReportingBeacon into a separate
// refcounted class, so that pending beacons can outlive the RFHI.
void RenderFrameHostImpl::SendFencedFrameReportingBeacon(
    const std::string& event_data,
    const std::string& event_type,
    const std::vector<blink::FencedFrame::ReportingDestination>& destinations,
    network::AttributionReportingRuntimeFeatures
        attribution_reporting_runtime_features) {
  if (!blink::features::IsFencedFramesEnabled()) {
    mojo::ReportBadMessage(
        "SendFencedFrameReportingBeacon() received while FencedFrames not "
        "enabled.");
    return;
  }

  if (event_data.length() > blink::kFencedFrameMaxBeaconLength) {
    mojo::ReportBadMessage(
        "The data provided to SendFencedFrameReportingBeacon() exceeds the "
        "maximum length, which is 64KB.");
    return;
  }

  for (const blink::FencedFrame::ReportingDestination& destination :
       destinations) {
    SendFencedFrameReportingBeaconInternal(
        DestinationEnumEvent(event_type, event_data), destination,
        /*from_renderer=*/true, attribution_reporting_runtime_features);
  }
}

// TODO(crbug.com/1400992): Move SendFencedFrameReportingBeaconToCustomURL into
// a separate refcounted class, so that pending beacons can outlive the RFHI.
void RenderFrameHostImpl::SendFencedFrameReportingBeaconToCustomURL(
    const GURL& destination_url,
    network::AttributionReportingRuntimeFeatures
        attribution_reporting_runtime_features) {
  if (!blink::features::IsFencedFramesEnabled()) {
    mojo::ReportBadMessage(
        "SendFencedFrameReportingBeaconToCustomURL() received while "
        "FencedFrames not enabled.");
    return;
  }

  if (!base::FeatureList::IsEnabled(
          blink::features::kAdAuctionReportingWithMacroApi)) {
    mojo::ReportBadMessage(
        "SendFencedFrameReportingBeaconToCustomURL() received while "
        "AdAuctionReportingWithMacroApi not enabled.");
    return;
  }

  if (!destination_url.is_valid() ||
      !destination_url.SchemeIs(url::kHttpsScheme)) {
    mojo::ReportBadMessage(
        "SendFencedFrameReportingBeaconToCustomURL() received an invalid or "
        "non-HTTPS url, which should have been checked in the renderer.");
    return;
  }

  SendFencedFrameReportingBeaconInternal(
      DestinationURLEvent(destination_url),
      blink::FencedFrame::ReportingDestination::kBuyer,
      /*from_renderer=*/true, attribution_reporting_runtime_features);
}

void RenderFrameHostImpl::MaybeSendFencedFrameAutomaticReportingBeacon(
    NavigationRequest& navigation_request,
    blink::mojom::AutomaticBeaconType event_type) {
  // The automatic beacon only cares about top-frame navigations.
  if (!IsOutermostMainFrame()) {
    return;
  }

  if (!navigation_request.GetInitiatorFrameToken().has_value()) {
    return;
  }

  // Treat the automatic beacon as if it's being sent by the document that
  // initiated the top-level navigation. (You can think of it like a
  // reportEvent call from that document.)
  RenderFrameHostImpl* initiator_rfh = RenderFrameHostImpl::FromFrameToken(
      navigation_request.GetInitiatorProcessId(),
      navigation_request.GetInitiatorFrameToken().value());
  if (!initiator_rfh) {
    return;
  }

  // Beacons can only be sent from inside a fenced frame/urn iframe tree, where
  // there is a fenced frame reporter.
  const absl::optional<FencedFrameProperties>& properties =
      initiator_rfh->frame_tree_node()->GetFencedFrameProperties();
  if (properties.has_value() &&
      event_type == blink::mojom::AutomaticBeaconType::kTopNavigationCommit) {
    base::UmaHistogramEnumeration(blink::kFencedFrameTopNavigationHistogram,
                                  blink::FencedFrameNavigationState::kCommit);
  }
  if (!properties.has_value() || !properties->fenced_frame_reporter_) {
    return;
  }

  // If there is no automatic beacon declared, there is nothing to send.
  absl::optional<AutomaticBeaconInfo> info =
      properties->GetAutomaticBeaconInfo(event_type);
  if (!info.has_value()) {
    return;
  }

  // Automatic beacons can only be sent if the initiating frame had transient
  // user activation when it navigated. For navigations originating from the
  // contextual menu (i.e. "Open Link in X"), or for navigations originating
  // from clicking a link directly, the navigation initiator activation status
  // will not be set, so we check the initiator frame's user activation directly
  // through the navigation request's common parameters.
  // It is safe to check both values at once. If one is not properly set, it
  // will always be set to a false negative and not a false positive, so there
  // is no way for that to cause an accidental beacon to be sent.
  if (navigation_request.GetNavigationInitiatorActivationAndAdStatus() ==
          blink::mojom::NavigationInitiatorActivationAndAdStatus::
              kDidNotStartWithTransientActivation &&
      !navigation_request.common_params().has_user_gesture) {
    RecordAutomaticBeaconOutcome(
        blink::AutomaticBeaconOutcome::kNoUserActivation);
    return;
  }

  // Beacons can only be sent when the initiator frame is same-origin with the
  // fenced frame config's mapped url.
  if (!properties->mapped_url_.has_value() ||
      !initiator_rfh->GetLastCommittedOrigin().IsSameOriginWith(
          url::Origin::Create(
              properties->mapped_url_->GetValueIgnoringVisibility()))) {
    RecordAutomaticBeaconOutcome(blink::AutomaticBeaconOutcome::kNotSameOrigin);
    return;
  }

  // Before M120: Only destinations explicitly registered when calling
  //              setReportEventDataForAutomaticBeacons() will have beacons sent
  //              to their endpoints.
  // After M120: Any destination registered in a Protected Audience/Shared
  //             Storage worklet will have a beacon sent to its endpoint.
  if (base::FeatureList::IsEnabled(
          blink::features::kFencedFramesM120FeaturesPart1)) {
    RecordAutomaticBeaconOutcome(blink::AutomaticBeaconOutcome::kSuccess);

    network::AttributionReportingRuntimeFeatures
        attribution_reporting_features =
            info ? info->attribution_reporting_runtime_features
                 : network::AttributionReportingRuntimeFeatures();

    for (const auto& destination :
         properties->fenced_frame_reporter_->ReportingDestinations()) {
      std::string data;
      // For data to be sent in the automatic beacon, it must be specified in
      // the event's "destination" for setReportEventDataForAutomaticBeacons().
      if (info &&
          std::find(info->destinations.begin(), info->destinations.end(),
                    destination) != info->destinations.end()) {
        data = info->data;
      }
      initiator_rfh->SendFencedFrameReportingBeaconInternal(
          AutomaticBeaconEvent(event_type, data), destination,
          /*from_renderer=*/false, attribution_reporting_features,
          navigation_request.GetNavigationId());
    }
  } else {
    if (!info->destinations.empty()) {
      RecordAutomaticBeaconOutcome(blink::AutomaticBeaconOutcome::kSuccess);
    }

    for (blink::FencedFrame::ReportingDestination destination :
         info->destinations) {
      initiator_rfh->SendFencedFrameReportingBeaconInternal(
          AutomaticBeaconEvent(event_type, info->data), destination,
          /*from_renderer=*/false, info->attribution_reporting_runtime_features,
          navigation_request.GetNavigationId());
    }
  }

  initiator_rfh->frame_tree_node()
      ->MaybeResetFencedFrameAutomaticBeaconReportEventData(event_type);
}

void RenderFrameHostImpl::SendFencedFrameReportingBeaconInternal(
    const FencedFrameReporter::DestinationVariant& event_variant,
    blink::FencedFrame::ReportingDestination destination,
    bool from_renderer,
    network::AttributionReportingRuntimeFeatures
        attribution_reporting_runtime_features,
    absl::optional<int64_t> navigation_id) {
  if (!IsActive()) {
    // reportEvent is not allowed when this RenderFrameHost or one of its
    // ancestors is not active.
    return;
  }

  // Get the reporting metadata associated with the fenced frame.
  const absl::optional<FencedFrameProperties>& fenced_frame_properties =
      frame_tree_node_->GetFencedFrameProperties();
  if (fenced_frame_properties.has_value() &&
      fenced_frame_properties->is_ad_component_) {
    if (from_renderer) {
      // Direct invocation of fence.reportEvent from an ad component is
      // disallowed.
      AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kError,
          "This frame is an ad component. It is not allowed to call "
          "fence.reportEvent.");
      return;
    }
    // Only automatic beacon events are allowed from ad components.
    CHECK(absl::holds_alternative<AutomaticBeaconEvent>(event_variant));
  }

  if (!fenced_frame_properties.has_value() ||
      !fenced_frame_properties->fenced_frame_reporter_) {
    // No associated fenced frame reporter. This should have been captured
    // in the renderer process at `Fence::reportEvent`.
    // This implies there is an inconsistency between the browser and the
    // renderer.
    mojo::ReportBadMessage(
        "This frame has no fenced frame reporter registered in the browser.");
    return;
  }

  if (!fenced_frame_properties->mapped_url_.has_value() ||
      !GetLastCommittedOrigin().IsSameOriginWith(
          url::Origin::Create(fenced_frame_properties->mapped_url_
                                  ->GetValueIgnoringVisibility()))) {
    mojo::ReportBadMessage(
        "This frame is cross-origin to the mapped url of its fenced frame "
        "config, so the renderer should not be able to call reportEvent.");
    return;
  }

  std::string error_message;
  // By default, log w/ error severity. Can be overwritten to lower severity
  // depending on the error.
  blink::mojom::ConsoleMessageLevel console_message_level =
      blink::mojom::ConsoleMessageLevel::kError;
  if (!fenced_frame_properties->fenced_frame_reporter_->SendReport(
          event_variant, destination,
          /*request_initiator_frame=*/this,
          attribution_reporting_runtime_features, error_message,
          console_message_level, GetFrameTreeNodeId(), navigation_id)) {
    AddMessageToConsole(console_message_level, error_message);
  }
}

void RenderFrameHostImpl::SetFencedFrameAutomaticBeaconReportEventData(
    blink::mojom::AutomaticBeaconType event_type,
    const std::string& event_data,
    const std::vector<blink::FencedFrame::ReportingDestination>& destinations,
    network::AttributionReportingRuntimeFeatures
        attribution_reporting_runtime_features,
    bool once) {
  if (!blink::features::IsFencedFramesEnabled()) {
    mojo::ReportBadMessage(
        "SetFencedFrameAutomaticBeaconReportEventData() received while "
        "FencedFrames not enabled.");
    return;
  }

  if (event_data.length() > blink::kFencedFrameMaxBeaconLength) {
    mojo::ReportBadMessage(
        "The data provided to SetFencedFrameAutomaticBeaconReportEventData() "
        "exceeds the maximum length, which is 64KB.");
    return;
  }

  // The call is ignored if the RenderFrameHost is not the currently active one
  // in the FrameTreeNode. For instance, this is ignored when it is pending
  // deletion or if it entered the BackForwardCache.
  //
  // Note: The renderer process already tests the document is not detached from
  // the frame tree before sending the IPC, but this might race with frame
  // deletion IPC sent from other processes.
  if (!IsActive()) {
    return;
  }
  CHECK(owner_);  // See `owner_` invariants about `IsActive()`.

  owner_->SetFencedFrameAutomaticBeaconReportEventData(
      event_type, event_data, destinations,
      attribution_reporting_runtime_features, once);
}

void RenderFrameHostImpl::OnViewTransitionOptInChanged(
    blink::mojom::ViewTransitionSameOriginOptIn view_transition_opt_in) {
  ViewTransitionOptInState::GetOrCreateForCurrentDocument(this)
      ->set_same_origin_opt_in(view_transition_opt_in);
}

void RenderFrameHostImpl::StartDragging(
    blink::mojom::DragDataPtr drag_data,
    blink::DragOperationsMask drag_operations_mask,
    const SkBitmap& unsafe_bitmap,
    const gfx::Vector2d& cursor_offset_in_dip,
    const gfx::Rect& drag_obj_rect_in_dip,
    blink::mojom::DragEventSourceInfoPtr event_info) {
  GetRenderWidgetHost()->StartDragging(
      std::move(drag_data), GetLastCommittedOrigin(), drag_operations_mask,
      unsafe_bitmap, cursor_offset_in_dip, drag_obj_rect_in_dip,
      std::move(event_info));
}

void RenderFrameHostImpl::CreateNewPopupWidget(
    mojo::PendingAssociatedReceiver<blink::mojom::PopupWidgetHost>
        blink_popup_widget_host,
    mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost> blink_widget_host,
    mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget) {
  // Do not open popups in an inactive document.
  if (!IsActive()) {
    // Sending this message requires user activation, which is impossible
    // for a prerendering document, so the renderer process should be
    // terminated. See
    // https://html.spec.whatwg.org/multipage/interaction.html#tracking-user-activation.
    if (lifecycle_state() == LifecycleStateImpl::kPrerendering) {
      bad_message::ReceivedBadMessage(
          GetProcess(), bad_message::RFH_POPUP_REQUEST_WHILE_PRERENDERING);
    }
    return;
  }

  // We still need to allocate a widget routing id. Even though the renderer
  // doesn't receive it, the browser side still uses routing ids to track
  // widgets in various global tables.
  int32_t widget_route_id = GetProcess()->GetNextRoutingID();
  RenderWidgetHostImpl* widget = delegate_->CreateNewPopupWidget(
      site_instance_->group()->GetSafeRef(), widget_route_id,
      std::move(blink_popup_widget_host), std::move(blink_widget_host),
      std::move(blink_widget));
  if (!widget)
    return;
  // The renderer-owned widget was created before sending the IPC received here.
  widget->RendererWidgetCreated(/*for_frame_widget=*/false);

  if (create_new_popup_widget_callback_)
    create_new_popup_widget_callback_.Run(widget);
}

void RenderFrameHostImpl::GetKeepAliveHandleFactory(
    mojo::PendingReceiver<blink::mojom::KeepAliveHandleFactory> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (GetProcess()->AreRefCountsDisabled())
    return;

  keep_alive_handle_factory_.Bind(std::move(receiver));
}

void RenderFrameHostImpl::DidChangeSrcDoc(
    const blink::FrameToken& child_frame_token,
    const std::string& srcdoc_value) {
  auto* child =
      FindAndVerifyChild(child_frame_token, bad_message::RFH_OWNER_PROPERTY);
  if (!child)
    return;

  child->SetSrcdocValue(srcdoc_value);
}

void RenderFrameHostImpl::ReceivedDelegatedCapability(
    blink::mojom::DelegatedCapability delegated_capability) {
  // Every delegated capability that is checked or consumed on the browser side
  // needs to be (a) activated here and (b) consumed when RFH handles the
  // capability.
  if (delegated_capability ==
      blink::mojom::DelegatedCapability::kFullscreenRequest) {
    fullscreen_request_token_.Activate();
  }
}

void RenderFrameHostImpl::BeginNavigation(
    blink::mojom::CommonNavigationParamsPtr unvalidated_common_params,
    blink::mojom::BeginNavigationParamsPtr begin_params,
    mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token,
    mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client,
    mojo::PendingRemote<blink::mojom::PolicyContainerHostKeepAliveHandle>
        initiator_policy_container_host_keep_alive_handle,
    mojo::PendingReceiver<mojom::NavigationRendererCancellationListener>
        renderer_cancellation_listener) {
  TRACE_EVENT("navigation", "RenderFrameHostImpl::BeginNavigation",
              ChromeTrackEvent::kRenderFrameHost, this, "url",
              unvalidated_common_params->url.possibly_invalid_spec());

  // Only active and prerendered documents are allowed to start navigation in
  // their frame.
  if (lifecycle_state() != LifecycleStateImpl::kPrerendering) {
    // If this is reached in case the RenderFrameHost is in BackForwardCache
    // evict the document from BackForwardCache.
    if (IsInactiveAndDisallowActivation(
            DisallowActivationReasonId::kBeginNavigation)) {
      return;
    }
  }

  // See `owner_` invariants about `lifecycle_state_`.
  // `IsInactiveAndDisallowActivation()` check cause both pending deletion and
  // bfcached states to return early.
  DCHECK(owner_);
  if (owner_->GetRenderFrameHostManager().is_attaching_inner_delegate()) {
    // Avoid starting any new navigations since this frame is in the process of
    // attaching an inner delegate.
    return;
  }

  DCHECK(navigation_client.is_valid());

  blink::mojom::CommonNavigationParamsPtr validated_common_params =
      unvalidated_common_params.Clone();
  if (!VerifyBeginNavigationCommonParams(*this, &*validated_common_params,
                                         begin_params->initiator_frame_token)) {
    return;
  }

  // BeginNavigation() should only be triggered when the navigation is
  // initiated by a document in the same process.
  int initiator_process_id = GetProcess()->GetID();
  if (!VerifyNavigationInitiator(this, begin_params->initiator_frame_token,
                                 initiator_process_id)) {
    return;
  }

  // Container-initiated navigations must come from the same process as the
  // parent.
  if (begin_params->is_container_initiated) {
    if (!GetParent() ||
        (initiator_process_id != GetParent()->GetProcess()->GetID())) {
      mojo::ReportBadMessage(
          "container initiated navigation from non-parent process");
      return;
    }
  }

  // If the request is bearing Private State Tokens parameters:
  // - it must not be a main-frame navigation, and
  // - for redemption and signing operations, the frame's parent needs the
  //   trust-token-redemption Permissions Policy feature,
  // - for issue operation, the frame's parent needs the
  //   private-state-token-issuance Permission Policy.
  if (begin_params->trust_token_params) {
    // For Fenced Frame trust_token_params shouldn't be populated since that is
    // driven by the iframe specific attribute as defined here:
    // https://github.com/WICG/trust-token-api#extension-iframe-activation". If
    // this changes, the code below using `GetParent()` will need to be changed.
    // TODO(crbug.com/1254770): For portals implemented with mparch, we'll have
    // a similar problem with `GetParent()` calls as with Fenced Frame.
    if (IsFencedFrameRoot()) {
      mojo::ReportBadMessage(
          "RFHI: Private State Token params in fenced frame nav");
      return;
    }
    if (!GetParent()) {
      mojo::ReportBadMessage(
          "RFHI: Private State Token params in main frame nav");
      return;
    }
    RenderFrameHostImpl* parent = GetParent();
    bool is_right_operation_policy_enabled = false;
    const network::mojom::TrustTokenOperationType& operation =
        begin_params->trust_token_params->operation;
    switch (operation) {
      case network::mojom::TrustTokenOperationType::kRedemption:
      case network::mojom::TrustTokenOperationType::kSigning:
        is_right_operation_policy_enabled = parent->IsFeatureEnabled(
            blink::mojom::PermissionsPolicyFeature::kTrustTokenRedemption);
        break;
      case network::mojom::TrustTokenOperationType::kIssuance:
        is_right_operation_policy_enabled = parent->IsFeatureEnabled(
            blink::mojom::PermissionsPolicyFeature::kPrivateStateTokenIssuance);
        break;
    }

    if (!is_right_operation_policy_enabled) {
      mojo::ReportBadMessage(
          "RFHI: Mandatory Private State Tokens Permissions Policy feature "
          "is absent");
      return;
    }
  }

  if (begin_params->is_fullscreen_requested) {
    // Fullscreen requests on navigation are only allowed from initial empty
    // documents that are the outermost main frame.
    if (!is_initial_empty_document()) {
      bad_message::ReceivedBadMessage(
          GetProcess(),
          bad_message::RFHI_FULLSCREEN_NAV_INVALID_INITIAL_DOCUMENT);
      return;
    }
    if (!IsOutermostMainFrame()) {
      bad_message::ReceivedBadMessage(
          GetProcess(),
          bad_message::RFHI_FULLSCREEN_NAV_NOT_OUTERMOST_MAIN_FRAME);
      return;
    }
    RenderFrameHostImpl* initiator_render_frame_host =
        begin_params->initiator_frame_token
            ? RenderFrameHostImpl::FromFrameToken(
                  initiator_process_id,
                  begin_params->initiator_frame_token.value())
            : nullptr;
    // The initiator must have window-management permission, permission
    // policy and `fullscreen` permission policy granted and the navigation must
    // be from a user gesture, otherwise the fullscreen bit is dropped.
    if (!initiator_render_frame_host ||
        !validated_common_params->has_user_gesture ||
        !IsWindowManagementGranted(initiator_render_frame_host) ||
        !initiator_render_frame_host->permissions_policy()->IsFeatureEnabled(
            blink::mojom::PermissionsPolicyFeature::kFullscreen) ||
        !initiator_render_frame_host->permissions_policy()->IsFeatureEnabled(
            blink::mojom::PermissionsPolicyFeature::kWindowManagement)) {
      if (initiator_render_frame_host) {
        initiator_render_frame_host->AddMessageToConsole(
            blink::mojom::ConsoleMessageLevel::kWarning,
            "Fullscreen request ignored: Insufficient permissions "
            "or user activation.");
      }
      begin_params->is_fullscreen_requested = false;
    }
  }

  GetProcess()->FilterURL(true, &begin_params->searchable_form_url);

  // If the request was for a blob URL, but the validated URL is no longer a
  // blob URL, reset the blob_url_token to prevent hitting the ReportBadMessage
  // below, and to make sure we don't incorrectly try to use the blob_url_token.
  if (unvalidated_common_params->url.SchemeIsBlob() &&
      !validated_common_params->url.SchemeIsBlob()) {
    blob_url_token = mojo::NullRemote();
  }

  if (blob_url_token && !validated_common_params->url.SchemeIsBlob()) {
    mojo::ReportBadMessage("Blob URL Token, but not a blob: URL");
    return;
  }
  scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;
  if (blob_url_token) {
    blob_url_loader_factory =
        ChromeBlobStorageContext::URLLoaderFactoryForToken(
            GetStoragePartition(), std::move(blob_url_token));
  }

  // If the request was for a blob URL, but no blob_url_token was passed in this
  // is not necessarily an error. Renderer initiated reloads for example are one
  // situation where a renderer currently doesn't have an easy way of resolving
  // the blob URL. For those situations resolve the blob URL here, as we don't
  // care about ordering with other blob URL manipulation anyway.
  if (validated_common_params->url.SchemeIsBlob() && !blob_url_loader_factory) {
    blob_url_loader_factory = ChromeBlobStorageContext::URLLoaderFactoryForUrl(
        GetStoragePartition(), validated_common_params->url);
  }

  if (waiting_for_init_) {
    pending_navigate_ = std::make_unique<PendingNavigation>(
        std::move(validated_common_params), std::move(begin_params),
        std::move(blob_url_loader_factory), std::move(navigation_client),
        std::move(renderer_cancellation_listener));
    return;
  }

  // We can discard the parameter
  // |initiator_policy_container_host_keep_alive_handle|. This is just needed to
  // ensure that the PolicyContainerHost of the initiator RenderFrameHost
  // can still be retrieved even if the RenderFrameHost has been deleted in
  // between. Since the NavigationRequest will be created synchronously as a
  // result of this function's execution, we don't need to pass
  // |initiator_policy_container_host_keep_alive_handle| along.

  owner_->GetCurrentNavigator().OnBeginNavigation(
      frame_tree_node(), std::move(validated_common_params),
      std::move(begin_params), std::move(blob_url_loader_factory),
      std::move(navigation_client), EnsurePrefetchedSignedExchangeCache(),
      initiator_process_id, std::move(renderer_cancellation_listener));
}

void RenderFrameHostImpl::SubresourceResponseStarted(
    const url::SchemeHostPort& final_response_url,
    net::CertStatus cert_status) {
  OPTIONAL_TRACE_EVENT1("content",
                        "RenderFrameHostImpl::SubresourceResponseStarted",
                        "url", final_response_url.GetURL());
  bool was_processed =
      frame_tree_->controller().ssl_manager()->DidStartResourceResponse(
          final_response_url, cert_status);
  UMA_HISTOGRAM_ENUMERATION("SSL.Experimental.SubresourceResponse",
                            was_processed
                                ? SSLSubresourceResponseType::kProcessed
                                : SSLSubresourceResponseType::kIgnored);
}
void RenderFrameHostImpl::ResourceLoadComplete(
    blink::mojom::ResourceLoadInfoPtr resource_load_info) {
  GlobalRequestID global_request_id;
  const bool is_frame_request =
      blink::IsRequestDestinationFrame(resource_load_info->request_destination);
  if (main_frame_request_ids_.first == resource_load_info->request_id) {
    global_request_id = main_frame_request_ids_.second;
  } else if (is_frame_request) {
    // The load complete message for the main resource arrived before
    // |DidCommitProvisionalLoad()|. We save the load info so
    // |ResourceLoadComplete()| can be called later in
    // |DidCommitProvisionalLoad()| when we can map to the global request ID.
    deferred_main_frame_load_info_ = std::move(resource_load_info);
    return;
  }
  delegate_->ResourceLoadComplete(this, global_request_id,
                                  std::move(resource_load_info));
}

void RenderFrameHostImpl::HandleAXEvents(
    const ui::AXTreeID& tree_id,
    blink::mojom::AXUpdatesAndEventsPtr updates_and_events,
    uint32_t reset_token) {
  TRACE_EVENT0("accessibility", "RenderFrameHostImpl::HandleAXEvents");
  SCOPED_UMA_HISTOGRAM_TIMER("Accessibility.Performance.HandleAXEvents");

  if (tree_id != GetAXTreeID()) {
    // The message has arrived after the frame has navigated which means its
    // events are no longer relevant and can be discarded.
    if (!accessibility_testing_callback_.is_null()) {
      // The callback must still run, otherwise dump event tests can hang.
      accessibility_testing_callback_.Run(this, ax::mojom::Event::kNone, 0);
    }
    return;
  }

  // Don't process this IPC if either we're waiting on a reset and this IPC
  // doesn't have the matching token ID, or if we're not waiting on a reset but
  // this message includes a reset token.
  // The token prevents obsolete data from being processed.
  CHECK(accessibility_reset_token_);
  if (*accessibility_reset_token_ != reset_token) {
    DVLOG(1) << "Ignoring obsolete accessibility data.";
    return;
  }

  ui::AXMode accessibility_mode = delegate_->GetAccessibilityMode();

  if (accessibility_mode.is_mode_off()) {
    return;
  }
  if (base::FeatureList::IsEnabled(features::kEvictOnAXEvents)) {
    // If the flag is on, evict the bfcache entry now that AX events are
    // received.
    if (IsInactiveAndDisallowActivationForAXEvents(updates_and_events->events))
      return;
  } else {
    // If the page is in back/forward cache, do not return early and continue to
    // apply AX tree updates.
    // TODO(https://crbug.com/1328126): Define and implement the behavior for
    // when the page is prerendering, too.
    if (!IsInBackForwardCache() &&
        IsInactiveAndDisallowActivation(DisallowActivationReasonId::kAXEvent)) {
      return;
    }
  }

  GetOrCreateBrowserAccessibilityManager();

  AXEventNotificationDetails details;
  details.ax_tree_id = tree_id;
  details.events = std::move(updates_and_events->events);
  details.updates = std::move(updates_and_events->updates);
  for (auto& update : details.updates) {
    if (update.has_tree_data) {
      DCHECK_EQ(tree_id, update.tree_data.tree_id);
      ax_tree_data_ = update.tree_data;
      update.tree_data = GetAXTreeData();
    }
  }

  if (needs_ax_root_id_) {
    // This is the first update after the tree id changed. AXTree must be sent
    // a new root id, otherwise crashes are likely to result.
    DCHECK(!details.updates.empty());
    DCHECK_NE(ui::kInvalidAXNodeID, details.updates[0].root_id);
    needs_ax_root_id_ = false;
  }

  SendAccessibilityEventsToManager(details);

  delegate_->AccessibilityEventReceived(details);

  // For testing only.
  if (!accessibility_testing_callback_.is_null()) {
    if (details.events.empty()) {
      // Objects were marked dirty but no events were provided.
      // The callback must still run, otherwise dump event tests can hang.
      accessibility_testing_callback_.Run(this, ax::mojom::Event::kNone, 0);
    } else {
      // Call testing callback functions for each event to fire.
      for (auto& event : details.events) {
        if (static_cast<int>(event.event_type) < 0)
          continue;

        accessibility_testing_callback_.Run(this, event.event_type, event.id);
      }
    }
  }
}

void RenderFrameHostImpl::HandleAXLocationChanges(
    const ui::AXTreeID& tree_id,
    std::vector<blink::mojom::LocationChangesPtr> changes,
    uint32_t reset_token) {
  if (tree_id != GetAXTreeID()) {
    // The message has arrived after the frame has navigated which means its
    // changes are no longer relevant and can be discarded.
    return;
  }

  CHECK(accessibility_reset_token_);
  if (*accessibility_reset_token_ != reset_token) {
    DVLOG(1) << "Ignoring obsolete accessibility data.";
    return;
  }

  if (IsInactiveAndDisallowActivation(
          DisallowActivationReasonId::kAXLocationChange)) {
    return;
  }

  BrowserAccessibilityManager* manager =
      GetOrCreateBrowserAccessibilityManager();
  if (manager)
    manager->OnLocationChanges(changes);

  // Send the updates to the automation extension API.
  std::vector<AXLocationChangeNotificationDetails> details;
  details.reserve(changes.size());
  for (auto& change : changes) {
    AXLocationChangeNotificationDetails detail;
    detail.id = change->id;
    detail.ax_tree_id = GetAXTreeID();
    detail.new_location = change->new_location;
    details.push_back(detail);
  }
  delegate_->AccessibilityLocationChangesReceived(details);
}

media::MediaMetricsProvider::RecordAggregateWatchTimeCallback
RenderFrameHostImpl::GetRecordAggregateWatchTimeCallback() {
  // The URL used for UKM must always be the top level frame.
  return delegate_->GetRecordAggregateWatchTimeCallback(
      GetOutermostMainFrame()->GetLastCommittedURL());
}

void RenderFrameHostImpl::ResetWaitingState() {
  // We don't allow resetting waiting state when the RenderFrameHost is either
  // in BackForwardCache or in pending deletion state, as we don't allow
  // navigations from either of these two states.
  DCHECK(!IsInBackForwardCache());
  DCHECK(!IsPendingDeletion());

  // Whenever we reset the RFH state, we should not be waiting for beforeunload
  // or close acks.  We clear them here to be safe, since they can cause
  // navigations to be ignored in DidCommitProvisionalLoad.
  if (is_waiting_for_beforeunload_completion_) {
    is_waiting_for_beforeunload_completion_ = false;
    if (beforeunload_timeout_)
      beforeunload_timeout_->Stop();
    has_shown_beforeunload_dialog_ = false;
    beforeunload_pending_replies_.clear();
  }
  send_before_unload_start_time_ = base::TimeTicks();
  page_close_state_ = PageCloseState::kNotClosing;
}

CanCommitStatus RenderFrameHostImpl::CanCommitOriginAndUrl(
    const url::Origin& origin,
    const GURL& url,
    bool is_same_document_navigation,
    bool is_pdf,
    bool is_sandboxed) {
  // Note that callers are responsible for avoiding this function in modes that
  // can bypass these rules, such as --disable-web-security or certain Android
  // WebView features like universal access from file URLs.

  // Renderer-debug URLs can never be committed.
  if (blink::IsRendererDebugURL(url)) {
    LogCanCommitOriginAndUrlFailureReason("is_renderer_debug_url");
    return CanCommitStatus::CANNOT_COMMIT_URL;
  }

  // Verify that if this RenderFrameHost is for a WebUI it is not committing a
  // URL which is not allowed in a WebUI process. As we are at the commit stage,
  // set OriginIsolationRequest to kNone (this is implicitly done by the
  // UrlInfoInit constructor).
  if (!Navigator::CheckWebUIRendererDoesNotDisplayNormalURL(
          this, UrlInfo(UrlInfoInit(url).WithOrigin(origin).WithIsPdf(is_pdf)),
          /*is_renderer_initiated_check=*/true)) {
    return CanCommitStatus::CANNOT_COMMIT_URL;
  }

  // MHTML subframes can supply URLs at commit time that do not match the
  // process lock. For example, it can be either "cid:..." or arbitrary URL at
  // which the frame was at the time of generating the MHTML
  // (e.g. "http://localhost"). In such cases, don't verify the URL, but require
  // the URL to commit in the process of the main frame.
  if (!is_main_frame()) {
    RenderFrameHostImpl* main_frame = GetMainFrame();
    if (main_frame->is_mhtml_document()) {
      if (IsSameSiteInstance(main_frame))
        return CanCommitStatus::CAN_COMMIT_ORIGIN_AND_URL;

      // If an MHTML subframe commits in a different process (even one that
      // appears correct for the subframe's URL), then we aren't correctly
      // loading it from the archive and should kill the renderer.
      static auto* const oopif_in_mhtml_page_key =
          base::debug::AllocateCrashKeyString(
              "oopif_in_mhtml_page", base::debug::CrashKeySize::Size32);
      base::debug::SetCrashKeyString(
          oopif_in_mhtml_page_key,
          is_mhtml_document() ? "is_mhtml_doc" : "not_mhtml_doc");
      LogCanCommitOriginAndUrlFailureReason("oopif_in_mhtml_page");
      return CanCommitStatus::CANNOT_COMMIT_URL;
    }
  }

  // Same-document navigations cannot change origins, as long as these checks
  // aren't being bypassed in unusual modes. This check must be after the MHTML
  // check, as shown by NavigationMhtmlBrowserTest.IframeAboutBlankNotFound.
  if (is_same_document_navigation && origin != GetLastCommittedOrigin()) {
    LogCanCommitOriginAndUrlFailureReason("cross_origin_same_document");
    return CanCommitStatus::CANNOT_COMMIT_ORIGIN;
  }

  // Give the client a chance to disallow URLs from committing.
  if (!GetContentClient()->browser()->CanCommitURL(GetProcess(), url)) {
    LogCanCommitOriginAndUrlFailureReason(
        "content_client_disallowed_commit_url");
    return CanCommitStatus::CANNOT_COMMIT_URL;
  }

  // Check with ChildProcessSecurityPolicy, which enforces Site Isolation, etc.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  const CanCommitStatus can_commit_status = policy->CanCommitOriginAndUrl(
      GetProcess()->GetID(), GetSiteInstance()->GetIsolationContext(),
      UrlInfo(
          UrlInfoInit(url)
              .WithOrigin(origin)
              .WithStoragePartitionConfig(
                  GetSiteInstance()->GetSiteInfo().storage_partition_config())
              .WithWebExposedIsolationInfo(
                  GetSiteInstance()->GetWebExposedIsolationInfo())
              .WithSandbox(is_sandboxed)));
  if (can_commit_status != CanCommitStatus::CAN_COMMIT_ORIGIN_AND_URL) {
    LogCanCommitOriginAndUrlFailureReason("cpspi_disallowed_commit");
    return can_commit_status;
  }

  const auto origin_tuple_or_precursor_tuple =
      origin.GetTupleOrPrecursorTupleIfOpaque();
  if (origin_tuple_or_precursor_tuple.IsValid()) {
    // Verify that the origin/precursor is allowed to commit in this process.
    // Note: This also handles non-standard cases for |url|, such as
    // about:blank, data, and blob URLs.

    // Renderer-debug URLs can never be committed.
    if (blink::IsRendererDebugURL(origin_tuple_or_precursor_tuple.GetURL())) {
      LogCanCommitOriginAndUrlFailureReason(
          "origin_or_precursor_is_renderer_debug_url");
      return CanCommitStatus::CANNOT_COMMIT_ORIGIN;
    }

    // Give the client a chance to disallow origin URLs from committing.
    // TODO(acolwell): Fix this code to work with opaque origins. Currently
    // some opaque origin precursors, like chrome-extension schemes, can trigger
    // the commit to fail. These need to be investigated.
    if (!origin.opaque() && !GetContentClient()->browser()->CanCommitURL(
                                GetProcess(), origin.GetURL())) {
      LogCanCommitOriginAndUrlFailureReason(
          "content_client_disallowed_commit_origin");
      return CanCommitStatus::CANNOT_COMMIT_ORIGIN;
    }
  }

  return CanCommitStatus::CAN_COMMIT_ORIGIN_AND_URL;
}

bool RenderFrameHostImpl::CanSubframeCommitOriginAndUrl(
    NavigationRequest* navigation_request) {
  DCHECK(navigation_request);
  DCHECK(!is_main_frame());

  const int nav_entry_id = navigation_request->nav_entry_id();
  // No validation of the main frame's origin is needed if the subframe
  // navigation doesn't target a different existing NavigationEntry. In such
  // cases, it's unlikely the wrong main frame origin could be displayed
  // afterward.
  if (nav_entry_id == 0)
    return true;

  const int last_nav_entry_index = navigation_request->frame_tree_node()
                                       ->navigator()
                                       .controller()
                                       .GetLastCommittedEntryIndex();
  const int dest_nav_entry_index = navigation_request->frame_tree_node()
                                       ->navigator()
                                       .controller()
                                       .GetEntryIndexWithUniqueID(nav_entry_id);
  if (dest_nav_entry_index <= 0 || dest_nav_entry_index == last_nav_entry_index)
    return true;

  NavigationEntryImpl* dest_nav_entry =
      navigation_request->frame_tree_node()
          ->navigator()
          .controller()
          .GetEntryAtIndex(dest_nav_entry_index);
  auto dest_main_frame_fne = dest_nav_entry->root_node()->frame_entry;

  // A subframe navigation should never lead to a NavigationEntry that looks
  // like a cross-document navigation in the main frame, since cross-document
  // navigations destroy all subframes.
  int64_t dest_main_frame_dsn = dest_main_frame_fne->document_sequence_number();
  int64_t actual_main_frame_dsn = navigation_request->frame_tree_node()
                                      ->navigator()
                                      .controller()
                                      .GetLastCommittedEntry()
                                      ->root_node()
                                      ->frame_entry->document_sequence_number();
  if (dest_main_frame_dsn != actual_main_frame_dsn)
    return false;

  // At this point in request handling RenderFrameHostImpl may not have enough
  // information to derive an accurate value for what would be the new main
  // frame origin if this request gets committed. One case this can happen is
  // a NavigtionEntry from a restored session. To make the best of this
  // situation, skip validation of main frame origin and assume it will be valid
  // by assigning a known good value - i.e. the current main frame origin - then
  // continue so at least CanCommitOriginAndUrl can perform its checks using
  // dest_top_url.
  url::Origin dest_top_origin =
      dest_main_frame_fne->committed_origin().value_or(
          GetMainFrame()->GetLastCommittedOrigin());

  const GURL& dest_top_url = dest_nav_entry->GetURL();

  // Assume the change in main frame FrameNavigationEntry won't affect whether
  // the main frame is showing a PDF or a sandboxed document, since we don't
  // track that in FrameNavigationEntry.
  const bool is_top_pdf =
      GetMainFrame()->GetSiteInstance()->GetSiteInfo().is_pdf();
  const bool is_top_sandboxed =
      GetMainFrame()->GetSiteInstance()->GetSiteInfo().is_sandboxed();

  return GetMainFrame()->CanCommitOriginAndUrl(
             dest_top_origin, dest_top_url,
             true /* is_same_document_navigation */, is_top_pdf,
             is_top_sandboxed) == CanCommitStatus::CAN_COMMIT_ORIGIN_AND_URL;
}

void RenderFrameHostImpl::Stop() {
  TRACE_EVENT1("navigation", "RenderFrameHostImpl::Stop", "render_frame_host",
               this);
  // Don't call GetAssociatedLocalFrame before the RenderFrame is created.
  if (!IsRenderFrameLive())
    return;
  GetAssociatedLocalFrame()->StopLoading();
}

void RenderFrameHostImpl::DispatchBeforeUnload(BeforeUnloadType type,
                                               bool is_reload) {
  bool for_navigation =
      type == BeforeUnloadType::BROWSER_INITIATED_NAVIGATION ||
      type == BeforeUnloadType::RENDERER_INITIATED_NAVIGATION;
  bool for_inner_delegate_attach =
      type == BeforeUnloadType::INNER_DELEGATE_ATTACH;
  DCHECK(for_navigation || for_inner_delegate_attach || !is_reload);

  // TAB_CLOSE and DISCARD should only dispatch beforeunload on outermost main
  // frames.
  DCHECK(type == BeforeUnloadType::BROWSER_INITIATED_NAVIGATION ||
         type == BeforeUnloadType::RENDERER_INITIATED_NAVIGATION ||
         type == BeforeUnloadType::INNER_DELEGATE_ATTACH ||
         IsOutermostMainFrame());

  CHECK(owner_);  // Only active documents are subject to BeforeUnload.

  if (!for_navigation) {
    // Cancel any pending navigations, to avoid their navigation commit/fail
    // event from wiping out the is_waiting_for_beforeunload_completion_ state.
    owner_->CancelNavigation();
  }

  // In renderer-initiated navigations, don't check for beforeunload in the
  // navigating frame, as it has already run beforeunload before it sent the
  // BeginNavigation IPC.
  bool check_subframes_only =
      type == BeforeUnloadType::RENDERER_INITIATED_NAVIGATION;
  if (!ShouldDispatchBeforeUnload(check_subframes_only)) {
    // When running beforeunload for navigations, ShouldDispatchBeforeUnload()
    // is checked earlier and we would only get here if it had already returned
    // true.
    DCHECK(!for_navigation);

    // Dispatch the ACK to prevent re-entrancy.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&RenderFrameHostManager::BeforeUnloadCompleted,
                       owner_->GetRenderFrameHostManager().GetWeakPtr(),
                       /*proceed=*/true));
    return;
  }
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "navigation", "RenderFrameHostImpl BeforeUnload", TRACE_ID_LOCAL(this),
      "render_frame_host", this);

  // This may be called more than once (if the user clicks the tab close button
  // several times, or if they click the tab close button then the browser close
  // button), and we only send the message once.
  if (is_waiting_for_beforeunload_completion_) {
    // Some of our close messages could be for the tab, others for cross-site
    // transitions. We always want to think it's for closing the tab if any
    // of the messages were, since otherwise it might be impossible to close
    // (if there was a cross-site "close" request pending when the user clicked
    // the close button). We want to keep the "for cross site" flag only if
    // both the old and the new ones are also for cross site.
    unload_ack_is_for_navigation_ =
        unload_ack_is_for_navigation_ && for_navigation;
  } else {
    // Start the hang monitor in case the renderer hangs in the beforeunload
    // handler.
    is_waiting_for_beforeunload_completion_ = true;
    beforeunload_dialog_request_cancels_unload_ = false;
    unload_ack_is_for_navigation_ = for_navigation;
    send_before_unload_start_time_ = base::TimeTicks::Now();
    if (delegate_->IsJavaScriptDialogShowing()) {
      // If there is a JavaScript dialog up, don't bother sending the renderer
      // the unload event because it is known unresponsive, waiting for the
      // reply from the dialog. If this incoming request is for a DISCARD be
      // sure to reply with |proceed = false|, because the presence of a dialog
      // indicates that the page can't be discarded.
      SimulateBeforeUnloadCompleted(type != BeforeUnloadType::DISCARD);
    } else {
      // Start a timer that will be shared by all frames that need to run
      // beforeunload in the current frame's subtree.
      if (beforeunload_timeout_)
        beforeunload_timeout_->Start(beforeunload_timeout_delay_);

      beforeunload_pending_replies_.clear();
      beforeunload_dialog_request_cancels_unload_ =
          (type == BeforeUnloadType::DISCARD);

      // Run beforeunload in this frame and its cross-process descendant
      // frames, in parallel.
      CheckOrDispatchBeforeUnloadForSubtree(
          check_subframes_only,
          /*send_ipc=*/true, is_reload,
          /*no_dispatch_because_avoid_unnecessary_sync=*/nullptr);
    }
  }
}

bool RenderFrameHostImpl::CheckOrDispatchBeforeUnloadForSubtree(
    bool subframes_only,
    bool send_ipc,
    bool is_reload,
    bool* no_dispatch_because_avoid_unnecessary_sync) {
  // Beforeunload is not supported inside fenced frame trees.
  if (IsFencedFrameRoot())
    return false;

  bool found_beforeunload = false;
  bool run_beforeunload_for_legacy = false;

  ForEachRenderFrameHostWithAction(
      [this, subframes_only, send_ipc, is_reload, &found_beforeunload,
       &run_beforeunload_for_legacy](
          RenderFrameHostImpl* rfh) -> FrameIterationAction {
        return CheckOrDispatchBeforeUnloadForFrame(
            subframes_only, send_ipc, is_reload, &found_beforeunload,
            &run_beforeunload_for_legacy, rfh);
      });

  if (run_beforeunload_for_legacy) {
    DCHECK(send_ipc);
    beforeunload_pending_replies_.insert(this);
    SendBeforeUnload(is_reload, GetWeakPtr(), /*for_legacy=*/true);
  } else if (no_dispatch_because_avoid_unnecessary_sync &&
             !found_beforeunload && !subframes_only && IsRenderFrameLive() &&
             IsAvoidUnnecessaryBeforeUnloadCheckSyncEnabled()) {
    *no_dispatch_because_avoid_unnecessary_sync = true;
  }

  return found_beforeunload;
}

RenderFrameHost::FrameIterationAction
RenderFrameHostImpl::CheckOrDispatchBeforeUnloadForFrame(
    bool subframes_only,
    bool send_ipc,
    bool is_reload,
    bool* found_beforeunload,
    bool* run_beforeunload_for_legacy,
    RenderFrameHostImpl* rfh) {
  // Don't traverse into inner pages.
  if (&GetPage() != &rfh->GetPage())
    return FrameIterationAction::kSkipChildren;

  // If |subframes_only| is true, skip this frame and its same-site
  // descendants.  This happens for renderer-initiated navigations, where
  // these frames have already run beforeunload.
  if (subframes_only && rfh->GetSiteInstance() == GetSiteInstance())
    return FrameIterationAction::kContinue;

  // No need to run beforeunload if the RenderFrame isn't live.
  if (!rfh->IsRenderFrameLive())
    return FrameIterationAction::kContinue;

  // Only run beforeunload in frames that have registered a beforeunload
  // handler. See description of SendBeforeUnload() for details on simulating
  // beforeunload for legacy reasons. If
  // `kAvoidUnnecessaryBeforeUnloadCheckSync` is true and there is no
  // beforeunload handler for the navigating frame, then do not simulate a
  // beforeunload handler, and navigation can continue.
  const bool run_beforeunload_for_legacy_frame =
      rfh == this && !rfh->has_before_unload_handler_ &&
      !IsAvoidUnnecessaryBeforeUnloadCheckSyncEnabled();
  const bool should_run_beforeunload =
      rfh->has_before_unload_handler_ || run_beforeunload_for_legacy_frame;

  if (!should_run_beforeunload)
    return FrameIterationAction::kContinue;

  // If we're only checking whether there's at least one frame with
  // beforeunload, then we've just found one, so we can return now.
  *found_beforeunload = true;
  if (!send_ipc)
    return FrameIterationAction::kStop;

  // Otherwise, figure out whether we need to send the IPC, or whether this
  // beforeunload was already triggered by an ancestor frame's IPC.

  // Only send beforeunload to local roots, and let Blink handle any
  // same-site frames under them. That is, if a frame has a beforeunload
  // handler, ask its local root to run it. If we've already sent the message
  // to that local root, skip this frame. For example, in A1(A2,A3), if A2
  // and A3 contain beforeunload handlers, and all three frames are
  // same-site, we ask A1 to run beforeunload for all three frames, and only
  // ask it once.
  while (!rfh->is_local_root() && rfh != this)
    rfh = rfh->GetParent();
  if (base::Contains(beforeunload_pending_replies_, rfh))
    return FrameIterationAction::kContinue;

  // For a case like A(B(A)), it's not necessary to send an IPC for the
  // innermost frame, as Blink will walk all same-site (local)
  // descendants. Detect cases like this and skip them.
  bool has_same_site_ancestor = false;
  for (auto* added_rfh : beforeunload_pending_replies_) {
    if (rfh->IsDescendantOfWithinFrameTree(added_rfh) &&
        rfh->GetSiteInstance() == added_rfh->GetSiteInstance()) {
      has_same_site_ancestor = true;
      break;
    }
  }
  if (has_same_site_ancestor)
    return FrameIterationAction::kContinue;

  if (run_beforeunload_for_legacy_frame &&
      IsAvoidUnnecessaryBeforeUnloadCheckPostTaskEnabled()) {
    // Wait to schedule until all frames have been processed. The legacy
    // beforeunload is not needed if another frame has a beforeunload
    // handler. Note that for `kAvoidUnnecessaryBeforeUnloadCheckSync`
    // `run_beforeunload_for_legacy_frame` is never true.
    *run_beforeunload_for_legacy = true;
    return FrameIterationAction::kContinue;
  }

  *run_beforeunload_for_legacy = false;

  // Add |rfh| to the list of frames that need to receive beforeunload
  // ACKs.
  beforeunload_pending_replies_.insert(rfh);

  SendBeforeUnload(is_reload, rfh->GetWeakPtr(), /*for_legacy=*/false);
  return FrameIterationAction::kContinue;
}

void RenderFrameHostImpl::SimulateBeforeUnloadCompleted(bool proceed) {
  DCHECK(is_waiting_for_beforeunload_completion_);
  base::TimeTicks approx_renderer_start_time = send_before_unload_start_time_;

  // Dispatch the ACK to prevent re-entrancy.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&RenderFrameHostImpl::ProcessBeforeUnloadCompleted,
                     weak_ptr_factory_.GetWeakPtr(), proceed,
                     /*treat_as_final_completion_callback=*/true,
                     approx_renderer_start_time, base::TimeTicks::Now(),
                     /*for_legacy=*/false));
}

bool RenderFrameHostImpl::ShouldDispatchBeforeUnload(
    bool check_subframes_only,
    bool* no_dispatch_because_avoid_unnecessary_sync) {
  return CheckOrDispatchBeforeUnloadForSubtree(
      check_subframes_only, /*send_ipc=*/false, /*is_reload=*/false,
      no_dispatch_because_avoid_unnecessary_sync);
}

void RenderFrameHostImpl::SetBeforeUnloadTimeoutDelayForTesting(
    const base::TimeDelta& timeout) {
  beforeunload_timeout_delay_ = timeout;
}

void RenderFrameHostImpl::StartPendingDeletionOnSubtree(
    RenderFrameHostImpl::PendingDeletionReason pending_deletion_reason) {
  DCHECK(IsPendingDeletion());

  if (pending_deletion_reason == PendingDeletionReason::kFrameDetach ||
      !ShouldAvoidRedundantNavigationCancellations()) {
    // Reset all navigations happening in the FrameTreeNode only when entering
    // "pending deletion" state due to frame detach if the
    // kStopCancellingNavigationsOnCommitAndNewNavigation flag is enabled, or
    // for all pending deletion cases otherwise.
    NavigationDiscardReason reason = NavigationDiscardReason::kWillRemoveFrame;
    GetFrameTreeNodeForUnload()->CancelNavigation();
    GetFrameTreeNodeForUnload()
        ->GetRenderFrameHostManager()
        .DiscardSpeculativeRFH(reason);
    ResetOwnedNavigationRequests(reason);
  } else {
    CHECK(pending_deletion_reason == PendingDeletionReason::kSwappedOut ||
          ShouldAvoidRedundantNavigationCancellations());
    // The pending deletion state is caused by swapping out the RFH. Reset only
    // the navigations that are owned by or will be using the swapped out RFH,
    // and also reset all navigations happening in the descendant frames.
    ResetNavigationsUsingSwappedOutRFH();
  }

  // For the child frames, we should delete all ongoing navigations
  // unconditionally, because the child frames will be deleted when this RFH
  // gets unloaded. Note that we are going through the RFH's children instead of
  // the FrameTreeNode's children, as the FTN might already have children that
  // isn't shared with the detached/swapped out RFH, e.g. in case of prerender
  // activation.
  for (auto& subframe : children_) {
    subframe->ResetAllNavigationsForFrameDetach();
  }

  for (std::unique_ptr<FrameTreeNode>& child_frame : children_) {
    for (FrameTreeNode* node : frame_tree()->SubtreeNodes(child_frame.get())) {
      RenderFrameHostImpl* child = node->current_frame_host();
      if (child->IsPendingDeletion())
        continue;

      // Blink handles deletion of all same-process descendants, running their
      // unload handler if necessary. So delegate sending IPC on the topmost
      // ancestor using the same process.
      RenderFrameHostImpl* local_ancestor = child;
      for (auto* rfh = child->parent_.get(); rfh != parent_;
           rfh = rfh->parent_) {
        if (rfh->GetSiteInstance() == child->GetSiteInstance())
          local_ancestor = rfh;
      }

      local_ancestor->DeleteRenderFrame(
          mojom::FrameDeleteIntention::kNotMainFrame);
      if (local_ancestor != child) {
        // In case of BackForwardCache, page is evicted directly from the cache
        // and deleted immediately, without waiting for unload handlers.
        child->SetLifecycleState(
            child->ShouldWaitForUnloadHandlers()
                ? LifecycleStateImpl::kRunningUnloadHandlers
                : LifecycleStateImpl::kReadyToBeDeleted);
      }

      node->frame_tree().FrameUnloading(node);
    }
  }
}

void RenderFrameHostImpl::PendingDeletionCheckCompleted() {
  if (lifecycle_state() == LifecycleStateImpl::kReadyToBeDeleted &&
      children_.empty()) {
    check_deletion_for_bug_1276535_ = false;
    if (is_waiting_for_unload_ack_) {
      OnUnloaded();  // Delete |this|.
      // Do not add code after this.
    } else {
      parent_->RemoveChild(GetFrameTreeNodeForUnload());
    }
  }
}

void RenderFrameHostImpl::PendingDeletionCheckCompletedOnSubtree() {
  if (children_.empty()) {
    PendingDeletionCheckCompleted();  // This might delete |this|.
    // DO NOT add code after this.
    return;
  }

  base::WeakPtr<RenderFrameHostImpl> self = GetWeakPtr();
  check_deletion_for_bug_1276535_ = true;

  // Collect children first before calling PendingDeletionCheckCompleted(). It
  // can delete them and invalidate |children_|. When the last child has been
  // deleted, it might also delete |this|.
  //
  // https://crbug.com/1276535: We collect WeakPtr, because we suspect deleting
  // a child might delete the whole WebContent and everything it contained.
  std::vector<base::WeakPtr<RenderFrameHostImpl>> weak_children;
  for (std::unique_ptr<FrameTreeNode>& child : children_) {
    RenderFrameHostImpl* child_rfh = child->current_frame_host();
    CHECK(child_rfh);
    weak_children.push_back(child_rfh->GetWeakPtr());
  }

  // DO NOT use |this| after this line.

  for (base::WeakPtr<RenderFrameHostImpl>& child_rfh : weak_children) {
    if (child_rfh) {
      child_rfh->PendingDeletionCheckCompletedOnSubtree();
    }
  }

  if (self) {
    check_deletion_for_bug_1276535_ = false;
  }
}

void RenderFrameHostImpl::ResetNavigationsUsingSwappedOutRFH() {
  // Only delete the navigation owned by the swapped out RFH or those that
  // intend to use the swapped out RFH.
  ResetOwnedNavigationRequests(
      NavigationDiscardReason::kRenderFrameHostDestruction);
  const NavigationRequest* navigation_request =
      frame_tree_node_->navigation_request();
  if (navigation_request &&
      navigation_request->state() >= NavigationRequest::WILL_PROCESS_RESPONSE &&
      navigation_request->GetRenderFrameHost() == this) {
    // It's possible for a RenderFrameHost to already have been picked for a
    // navigation but the NavigationRequest's ownership hasn't been moved to the
    // RenderFrameHost yet, if the navigation is deferred by a
    // NavigationThrottle or CommitDeferringCondition. We need to reset the
    // NavigationRequest to prevent it from trying to commit in the pending
    // deletion RFH.
    frame_tree_node_->ResetNavigationRequest(
        NavigationDiscardReason::kRenderFrameHostDestruction);
  }
}

void RenderFrameHostImpl::OnSubframeDeletionUnloadTimeout() {
  DCHECK(IsPendingDeletion());
  MaybeLogMissingUnloadAck();
  parent_->RemoveChild(GetFrameTreeNodeForUnload());
}

void RenderFrameHostImpl::SetFocusedFrame() {
  GetAssociatedLocalFrame()->Focus();
}

void RenderFrameHostImpl::AdvanceFocus(blink::mojom::FocusType type,
                                       RenderFrameProxyHost* source_proxy) {
  DCHECK(!source_proxy ||
         (source_proxy->GetProcess()->GetID() == GetProcess()->GetID()));

  absl::optional<blink::RemoteFrameToken> frame_token;
  if (source_proxy)
    frame_token = source_proxy->GetFrameToken();

  GetAssociatedLocalFrame()->AdvanceFocusInFrame(type, frame_token);
}

void RenderFrameHostImpl::JavaScriptDialogClosed(
    JavaScriptDialogCallback dialog_closed_callback,
    bool success,
    const std::u16string& user_input) {
  GetProcess()->SetBlocked(false);
  std::move(dialog_closed_callback).Run(success, user_input);
  // If executing as part of beforeunload event handling, there may have been
  // timers stopped in this frame or a frame up in the frame hierarchy. Restart
  // any timers that were stopped in OnRunBeforeUnloadConfirm().
  for (RenderFrameHostImpl* frame = this; frame; frame = frame->GetParent()) {
    if (frame->is_waiting_for_beforeunload_completion_ &&
        frame->beforeunload_timeout_) {
      frame->beforeunload_timeout_->Start(beforeunload_timeout_delay_);
    }
  }
}

bool RenderFrameHostImpl::ShouldDispatchPagehideAndVisibilitychangeDuringCommit(
    RenderFrameHostImpl* old_frame_host,
    const UrlInfo& dest_url_info) {
  // Only return true if this is a same-site navigation and we did a proactive
  // BrowsingInstance swap but we're reusing the old page's renderer process.
  DCHECK(old_frame_host);
  if (old_frame_host->GetSiteInstance()->IsRelatedSiteInstance(
          GetSiteInstance())) {
    return false;
  }
  if (old_frame_host->GetProcess() != GetProcess()) {
    return false;
  }
  if (!old_frame_host->IsNavigationSameSite(dest_url_info)) {
    return false;
  }
  DCHECK(is_main_frame());
  DCHECK_NE(old_frame_host, this);
  DCHECK_NE(old_frame_host->GetSiteInstance(), GetSiteInstance());
  return true;
}

void RenderFrameHostImpl::SendAllPendingBeaconsOnNavigation() {
  if (auto* pending_beacon_host =
          PendingBeaconHost::GetForCurrentDocument(this)) {
    pending_beacon_host->SendAllOnNavigation();
  }
  // TODO(crbug.com/1293679): Address FencedFrame.
  for (auto& child : children_) {
    child->current_frame_host()->SendAllPendingBeaconsOnNavigation();
  }
}

uint32_t RenderFrameHostImpl::FindSuddenTerminationHandlers(bool same_origin) {
  uint32_t navigation_termination = 0;
  // Search this frame and subframes for sudden termination disablers
  ForEachRenderFrameHostWithAction([this, same_origin, &navigation_termination](
                                       RenderFrameHost* rfh) {
    if (same_origin &&
        GetLastCommittedOrigin() != rfh->GetLastCommittedOrigin()) {
      return FrameIterationAction::kSkipChildren;
    }
    if (rfh->GetSuddenTerminationDisablerState(
            blink::mojom::SuddenTerminationDisablerType::kUnloadHandler)) {
      navigation_termination = navigation_termination |
                               NavigationSuddenTerminationDisablerType::kUnload;
      // We can stop when we find the first unload handler. If we ever start
      // reporting other types of sudden termination handler, we will need to
      // continue.
      return FrameIterationAction::kStop;
    }
    return FrameIterationAction::kContinue;
  });
  return navigation_termination;
}

void RenderFrameHostImpl::RecordNavigationSuddenTerminationHandlers() {
  uint32_t navigation_termination =
      is_main_frame() ? NavigationSuddenTerminationDisablerType::kMainFrame : 0;

  if (is_initial_empty_document()) {
    navigation_termination |=
        NavigationSuddenTerminationDisablerType::kInitialEmptyDocument;
  }

  if (!GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
    navigation_termination |= NavigationSuddenTerminationDisablerType::kNotHttp;
  }

  base::UmaHistogramExactLinear(
      "Navigation.SuddenTerminationDisabler.AllOrigins",
      navigation_termination |
          FindSuddenTerminationHandlers(/*same_origin=*/false),
      NavigationSuddenTerminationDisablerType::kMaxValue * 2);
  base::UmaHistogramExactLinear(
      "Navigation.SuddenTerminationDisabler.SameOrigin",
      navigation_termination |
          FindSuddenTerminationHandlers(/*same_origin=*/true),
      NavigationSuddenTerminationDisablerType::kMaxValue * 2);
}

const absl::optional<base::UnguessableToken>&
RenderFrameHostImpl::GetDevToolsNavigationToken() {
  // We shouldn't need to call this method while a RFH is speculative or pending
  // commit - there is a navigation in progress and its value will change
  // shortly.
  CHECK_GT(lifecycle_state(), LifecycleStateImpl::kPendingCommit);
  return document_associated_data_->devtools_navigation_token();
}

void RenderFrameHostImpl::CommitNavigation(
    NavigationRequest* navigation_request,
    blink::mojom::CommonNavigationParamsPtr common_params,
    blink::mojom::CommitNavigationParamsPtr commit_params,
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    absl::optional<SubresourceLoaderParams> subresource_loader_params,
    absl::optional<std::vector<blink::mojom::TransferrableURLLoaderPtr>>
        subresource_overrides,
    blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info,
    const absl::optional<blink::DocumentToken>& document_token,
    const base::UnguessableToken& devtools_navigation_token) {
  TRACE_EVENT2("navigation", "RenderFrameHostImpl::CommitNavigation",
               "navigation_request", navigation_request, "url",
               common_params->url);
  DCHECK(!blink::IsRendererDebugURL(common_params->url));
  DCHECK(navigation_request);
  DCHECK_EQ(this, navigation_request->GetRenderFrameHost());
  AssertBrowserContextShutdownHasntStarted();

  bool is_same_document =
      NavigationTypeUtils::IsSameDocument(common_params->navigation_type);
  bool is_mhtml_subframe = navigation_request->IsForMhtmlSubframe();

  // A |response| and a |url_loader_client_endpoints| must always be provided,
  // except for edge cases, where another way to load the document exist.
  DCHECK((response_head && url_loader_client_endpoints) ||
         common_params->url.SchemeIs(url::kDataScheme) || is_same_document ||
         !IsURLHandledByNetworkStack(common_params->url) || is_mhtml_subframe);

  // All children of MHTML documents must be MHTML documents.
  // As a defensive measure, crash the browser if something went wrong.
  if (!is_main_frame()) {
    RenderFrameHostImpl* root = GetMainFrame();
    if (root->is_mhtml_document_) {
      bool loaded_from_outside_the_archive =
          response_head || url_loader_client_endpoints;
      CHECK(!loaded_from_outside_the_archive ||
            common_params->url.SchemeIs(url::kDataScheme));
      CHECK(navigation_request->IsForMhtmlSubframe());
      CHECK_EQ(GetSiteInstance(), root->GetSiteInstance());
      CHECK_EQ(GetProcess(), root->GetProcess());
    } else {
      DCHECK(!navigation_request->IsForMhtmlSubframe());
    }
  }

  bool is_srcdoc = common_params->url.IsAboutSrcdoc();
  if (is_srcdoc) {
    // TODO(wjmaclean): initialize this in NavigationRequest's constructor
    // instead.
    commit_params->srcdoc_value =
        navigation_request->frame_tree_node()->srcdoc_value();
    // Main frame srcdoc navigation are meaningless. They are blocked whenever a
    // navigation attempt is made. It shouldn't reach CommitNavigation.
    CHECK(!is_main_frame());

    // For a srcdoc iframe, we expect it to either be in its parent's
    // SiteInstance (either AreIsolatedSandboxedIframesEnabled is false or
    // both parent and child are sandboxed), or that the two are in different
    // SiteInstances when only the child is sandboxed.
    CHECK(GetSiteInstance() == parent_->GetSiteInstance() ||
          !parent_->GetSiteInstance()->GetSiteInfo().is_sandboxed() &&
              GetSiteInstance()->GetSiteInfo().is_sandboxed());
  }

  // TODO(https://crbug.com/888079): Compute the Origin to commit here.

  // If this is an attempt to commit a URL in an incompatible process, capture a
  // crash dump to diagnose why it is occurring.
  // TODO(creis): Remove this check after we've gathered enough information to
  // debug issues with browser-side security checks. https://crbug.com/931895.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  const ProcessLock process_lock =
      ProcessLock::FromSiteInfo(GetSiteInstance()->GetSiteInfo());
  auto browser_calc_origin_to_commit =
      navigation_request->GetOriginToCommitWithDebugInfo();
  if (!process_lock.is_error_page() && !is_mhtml_subframe &&
      !policy->CanAccessDataForOrigin(
          GetProcess()->GetID(), browser_calc_origin_to_commit.first.value())) {
    SCOPED_CRASH_KEY_STRING64("CommitNavigation", "lock_url",
                              process_lock.ToString());
    SCOPED_CRASH_KEY_STRING64(
        "CommitNavigation", "commit_url_origin",
        common_params->url.DeprecatedGetOriginAsURL().spec());
    SCOPED_CRASH_KEY_STRING64(
        "CommitNavigation", "browser_calc_origin",
        browser_calc_origin_to_commit.first.value().GetDebugString());
    SCOPED_CRASH_KEY_STRING64("CommitNavigation", "origin_debug_info",
                              browser_calc_origin_to_commit.second);
    SCOPED_CRASH_KEY_BOOL("CommitNavigation", "is_main_frame", is_main_frame());
    // The reason this isn't is_outermost_main_frame is so that the full name of
    // the key does not exceed the 40 character limit.
    SCOPED_CRASH_KEY_BOOL("CommitNavigation", "is_outermost_frame",
                          IsOutermostMainFrame());
    NOTREACHED() << "Commiting in incompatible process for URL: "
                 << process_lock.lock_url() << " lock vs "
                 << common_params->url.DeprecatedGetOriginAsURL();
    base::debug::DumpWithoutCrashing();
  }

  const bool is_first_navigation = !has_committed_any_navigation_;
  has_committed_any_navigation_ = true;

  UpdatePermissionsForNavigation(navigation_request);

  // Get back to a clean state, in case we start a new navigation without
  // completing an unload handler.
  ResetWaitingState();

  // The renderer can exit view source mode when any error or cancellation
  // happen. When reusing the same renderer, overwrite to recover the mode.
  if (commit_params->is_view_source && IsActive()) {
    DCHECK(!GetParentOrOuterDocument());
    GetAssociatedLocalFrame()->EnableViewSourceMode();
  }

  // TODO(lfg): The renderer is not able to handle a null response, so the
  // browser provides an empty response instead. See the DCHECK in the beginning
  // of this method for the edge cases where a response isn't provided.
  network::mojom::URLResponseHeadPtr head =
      response_head ? std::move(response_head)
                    : network::mojom::URLResponseHead::New();

  std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
      subresource_loader_factories;
  mojo::PendingRemote<blink::mojom::ResourceCache> resource_cache_remote;
  if (!is_same_document || is_first_navigation) {
    recreate_default_url_loader_factory_after_network_service_crash_ = false;
    subresource_loader_factories =
        std::make_unique<blink::PendingURLLoaderFactoryBundle>();
    BrowserContext* browser_context = GetSiteInstance()->GetBrowserContext();
    auto subresource_loader_factories_config =
        SubresourceLoaderFactoriesConfig::ForPendingNavigation(
            *navigation_request);

    // Calculate `effective_scheme` - this will be the main input for deciding
    // whether the new document should have access to special URLLoaderFactories
    // like FileURLLoaderFactory, ContentURLLoaderFactory,
    // WebUIURLLoaderFactory, etc.  We look at GetTupleOrPrecursorTupleIfOpaque
    // to make sure the old behavior of sandboxed frames is preserved - see also
    // the FileURLLoaderFactory...SubresourcesInSandboxedFileFrame test.
    const std::string& effective_scheme =
        subresource_loader_factories_config.origin()
            .GetTupleOrPrecursorTupleIfOpaque()
            .scheme();

    ContentBrowserClient::NonNetworkURLLoaderFactoryMap non_network_factories;

    // Set up the default factory.
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_default_factory;

    // See if this is for WebUI.
    const auto& webui_schemes = URLDataManagerBackend::GetWebUISchemes();
    if (base::Contains(webui_schemes, effective_scheme)) {
      mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_for_webui;
      auto factory_receiver =
          factory_for_webui.InitWithNewPipeAndPassReceiver();
      GetContentClient()->browser()->WillCreateURLLoaderFactory(
          browser_context, this, GetProcess()->GetID(),
          ContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource,
          subresource_loader_factories_config.origin(),
          /*navigation_id=*/absl::nullopt,
          subresource_loader_factories_config.ukm_source_id(),
          &factory_receiver, /*header_client=*/nullptr,
          /*bypass_redirect_checks=*/nullptr,
          /*disable_secure_dns=*/nullptr, /*factory_override=*/nullptr,
          /*navigation_response_task_runner=*/nullptr);
      mojo::Remote<network::mojom::URLLoaderFactory> direct_factory_for_webui(
          CreateWebUIURLLoaderFactory(this, effective_scheme, {}));
      direct_factory_for_webui->Clone(std::move(factory_receiver));

      // If the renderer has webui bindings, then don't give it access to
      // network loader for security reasons.
      // http://crbug.com/829412: make an exception for a small whitelist
      // of WebUIs that need to be fixed to not make network requests in JS.
      if ((enabled_bindings_ & kWebUIBindingsPolicyMask) &&
          !GetContentClient()->browser()->IsWebUIAllowedToMakeNetworkRequests(
              subresource_loader_factories_config.origin())) {
        pending_default_factory = std::move(factory_for_webui);
        // WebUIURLLoaderFactory will kill the renderer if it sees a request
        // with a non-chrome scheme. Register a URLLoaderFactory for the about
        // scheme so about:blank doesn't kill the renderer.
        non_network_factories[url::kAboutScheme] =
            AboutURLLoaderFactory::Create();
      } else {
        // This is a webui scheme that doesn't have webui bindings. Give it
        // access to the network loader as it might require it.
        subresource_loader_factories->pending_scheme_specific_factories()
            .emplace(effective_scheme, std::move(factory_for_webui));
      }
    }

    if (navigation_request->IsMhtmlOrSubframe()) {
      // MHTML frames are not allowed to make any network requests - all their
      // subresource requests should be fulfilled from the MHTML archive.
      pending_default_factory =
          network::NotImplementedURLLoaderFactory::Create();
    }

    if (!pending_default_factory) {
      // Otherwise default to a Network Service-backed loader from the
      // appropriate NetworkContext.
      recreate_default_url_loader_factory_after_network_service_crash_ = true;

      // Otherwise default to a Network Service-backed loader from the
      // appropriate NetworkContext.
      bool bypass_redirect_checks =
          CreateNetworkServiceDefaultFactoryAndObserve(
              CreateURLLoaderFactoryParamsForMainWorld(
                  subresource_loader_factories_config,
                  "RFHI::CommitNavigation"),
              subresource_loader_factories_config.ukm_source_id(),
              pending_default_factory.InitWithNewPipeAndPassReceiver());
      subresource_loader_factories->set_bypass_redirect_checks(
          bypass_redirect_checks);
    }

    DCHECK(pending_default_factory);
    subresource_loader_factories->pending_default_factory() =
        std::move(pending_default_factory);

    // Only documents from a file precursor scheme can load file subresoruces.
    if (effective_scheme == url::kFileScheme) {
      // USER_BLOCKING because this scenario is exactly one of the examples
      // given by the doc comment for USER_BLOCKING: Loading and rendering a web
      // page after the user clicks a link.
      base::TaskPriority file_factory_priority =
          base::TaskPriority::USER_BLOCKING;
      non_network_factories.emplace(
          url::kFileScheme,
          FileURLLoaderFactory::Create(
              browser_context->GetPath(),
              browser_context->GetSharedCorsOriginAccessList(),
              file_factory_priority));
    }

#if BUILDFLAG(IS_ANDROID)
    if (effective_scheme == url::kContentScheme) {
      // Only content:// URLs can load content:// subresources
      non_network_factories.emplace(url::kContentScheme,
                                    ContentURLLoaderFactory::Create());
    }
#endif

    auto* partition = GetStoragePartition();
    non_network_factories.emplace(
        url::kFileSystemScheme,
        CreateFileSystemURLLoaderFactory(
            GetProcess()->GetID(), GetFrameTreeNodeId(),
            partition->GetFileSystemContext(), partition->GetPartitionDomain(),
            commit_params->storage_key));

    non_network_factories.emplace(url::kDataScheme,
                                  DataURLLoaderFactory::Create());

    GetContentClient()
        ->browser()
        ->RegisterNonNetworkSubresourceURLLoaderFactories(
            GetProcess()->GetID(), routing_id_,
            subresource_loader_factories_config.origin(),
            &non_network_factories);

    for (auto& factory : non_network_factories) {
      const std::string& scheme = factory.first;
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          original_pending_factory = std::move(factory.second);
      // TODO(https://crbug.com/1376879): Remove the workaround below once the
      // root cause of the bug has been fixed.
      if (!original_pending_factory)
        continue;

      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_factory_proxy;
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver =
          pending_factory_proxy.InitWithNewPipeAndPassReceiver();
      WillCreateURLLoaderFactory(
          subresource_loader_factories_config.origin(), &factory_receiver,
          subresource_loader_factories_config.ukm_source_id());

      mojo::Remote<network::mojom::URLLoaderFactory> remote(
          std::move(original_pending_factory));
      remote->Clone(std::move(factory_receiver));
      subresource_loader_factories->pending_scheme_specific_factories().emplace(
          scheme, std::move(pending_factory_proxy));
    }

    subresource_loader_factories->pending_isolated_world_factories() =
        CreateURLLoaderFactoriesForIsolatedWorlds(
            subresource_loader_factories_config,
            isolated_worlds_requiring_separate_url_loader_factory_);

    ResourceCacheManager* resource_cache_manager =
        partition->GetResourceCacheManager();
    if (resource_cache_manager) {
      resource_cache_manager
          ->MaybeInitializeResourceCacheRemoteOnCommitNavigation(
              resource_cache_remote, *navigation_request);
    }
  }

  // It is imperative that cross-document navigations always provide a set of
  // subresource ULFs.
  DCHECK(is_same_document || !is_first_navigation || is_srcdoc ||
         subresource_loader_factories);

  if (is_same_document) {
    DCHECK_EQ(navigation_request->frame_tree_node()->current_frame_host(),
              this);
    const base::UnguessableToken& navigation_token =
        commit_params->navigation_token;
    DCHECK(GetSameDocumentNavigationRequest(navigation_token));
    bool should_replace_current_entry =
        common_params->should_replace_current_entry;
    GetMojomFrameInRenderer()->CommitSameDocumentNavigation(
        std::move(common_params), std::move(commit_params),
        base::BindOnce(&RenderFrameHostImpl::OnSameDocumentCommitProcessed,
                       base::Unretained(this), navigation_token,
                       should_replace_current_entry));
  } else {
    // Pass the controller service worker info if we have one.
    blink::mojom::ControllerServiceWorkerInfoPtr controller;
    mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerObject>
        remote_object;
    blink::mojom::ServiceWorkerState sent_state;
    if (subresource_loader_params &&
        subresource_loader_params->controller_service_worker_info) {
      controller =
          std::move(subresource_loader_params->controller_service_worker_info);
      if (controller->object_info) {
        controller->object_info->receiver =
            remote_object.InitWithNewEndpointAndPassReceiver();
        sent_state = controller->object_info->state;
      }
    }

    // Set up the subresource loader factory that will pass requests from the
    // renderer to the originally intended network service endpoint. To save
    // memory, this is intentionally shared for prefetch, topics, and
    // keep-alive.
    scoped_refptr<network::SharedURLLoaderFactory>
        subresource_proxying_factory_bundle;
    if (subresource_loader_factories) {
      // Clone the factory bundle for prefetch.
      auto bundle = base::MakeRefCounted<blink::URLLoaderFactoryBundle>(
          std::move(subresource_loader_factories));
      subresource_loader_factories = CloneFactoryBundle(bundle);

      subresource_proxying_factory_bundle =
          network::SharedURLLoaderFactory::Create(CloneFactoryBundle(bundle));
    }

    // Set up the subresource loader factory to be passed to the renderer. It is
    // used to proxy relevant subresoruce requests (e.g. prefetch, topics)
    // through the browser process.
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        subresource_proxying_loader_factory_for_renderer;
    if (subresource_proxying_factory_bundle) {
      if (prefetched_signed_exchange_cache_) {
        prefetched_signed_exchange_cache_->RecordHistograms();
        // Reset |prefetched_signed_exchange_cache_|, not to reuse the cached
        // signed exchange which was prefetched in the previous page.
        prefetched_signed_exchange_cache_.reset();
      }

      // Also set-up URLLoaderFactory for prefetch using the same loader
      // factories. TODO(kinuko): Consider setting this up only when relevant
      // requests are encountered. Currently we have this here to make sure we
      // have non-racy situation (https://crbug.com/849929).
      base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext>
          bind_context =
              GetStoragePartition()
                  ->GetSubresourceProxyingURLLoaderService()
                  ->GetFactory(subresource_proxying_loader_factory_for_renderer
                                   .InitWithNewPipeAndPassReceiver(),
                               navigation_request->frame_tree_node()
                                   ->frame_tree_node_id(),
                               subresource_proxying_factory_bundle,
                               weak_ptr_factory_.GetWeakPtr(),
                               EnsurePrefetchedSignedExchangeCache());

      navigation_request
          ->set_subresource_proxying_url_loader_service_bind_context(
              bind_context);
    }

    // Set up the keepalive loader factory. It is used to proxy the keepalive
    // requests, i.e. fetch(..., {keepalive: true}), via the browser process.
    // See
    // https://docs.google.com/document/d/1ZzxMMBvpqn8VZBZKnb7Go8TWjnrGcXuLS_USwVVRUvY/edit
    // Note that this loader does not depend on
    // `subresource_proxying_loader_factory_for_renderer`.
    //
    // TODO(https://crbug.com/1441113): consolidate with
    // `subresource_proxying_loader_factory_for_renderer` so that requests can
    // be properly handled when both keepalive and browsing_topics are
    // specified.
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        keep_alive_loader_factory;
    if (subresource_proxying_factory_bundle &&
        base::FeatureList::IsEnabled(
            blink::features::kKeepAliveInBrowserMigration)) {
      // Also setting up URLLoaderFactory for keepalive using the same loader
      // factories.
      GetStoragePartition()->GetKeepAliveURLLoaderService()->BindFactory(
          keep_alive_loader_factory.InitWithNewPipeAndPassReceiver(),
          subresource_proxying_factory_bundle,
          navigation_request->GetPolicyContainerHost());
    }
    // Set up the fetchlater loader factory. It is used to proxy FetchLater
    // requests via the browser process.
    // See
    // https://docs.google.com/document/d/1U8XSnICPY3j-fjzG35UVm6zjwL6LvX6ETU3T8WrzLyQ/edit
    mojo::PendingAssociatedRemote<blink::mojom::FetchLaterLoaderFactory>
        fetch_later_loader_factory;
    if (subresource_proxying_factory_bundle &&
        base::FeatureList::IsEnabled(blink::features::kFetchLaterAPI)) {
      GetStoragePartition()
          ->GetKeepAliveURLLoaderService()
          ->BindFetchLaterLoaderFactory(
              fetch_later_loader_factory.InitWithNewEndpointAndPassReceiver(),
              subresource_proxying_factory_bundle,
              navigation_request->GetPolicyContainerHost());
    }

    mojom::NavigationClient* navigation_client =
        navigation_request->GetCommitNavigationClient();

    // Record the metrics about the state of the old main frame at the moment
    // when we navigate away from it as it matters for whether the page
    // is eligible for being put into back-forward cache.
    //
    // Ideally we would do this when we are just about to swap out the old
    // render frame and swap in the new one, but we can't do this for
    // same-process navigations yet as we are reusing the RenderFrameHost and
    // as the local frame navigates it overrides the values that we are
    // interested in. The cross-process navigation case is handled in
    // RenderFrameHostManager::UnloadOldFrame.
    //
    // Here we are recording the metrics for same-process navigations at the
    // point just before the navigation commits.
    // TODO(altimin, crbug.com/933147): Remove this logic after we are done with
    // implementing back-forward cache.
    if (!GetParent() &&
        navigation_request->frame_tree_node()->current_frame_host() == this) {
      if (NavigationEntryImpl* last_committed_entry =
              NavigationEntryImpl::FromNavigationEntry(
                  navigation_request->frame_tree_node()
                      ->frame_tree()
                      .controller()
                      .GetLastCommittedEntry())) {
        if (last_committed_entry->back_forward_cache_metrics()) {
          last_committed_entry->back_forward_cache_metrics()
              ->RecordFeatureUsage(this);
        }
      }
    }

    blink::mojom::PolicyContainerPtr policy_container =
        navigation_request->CreatePolicyContainerForBlink();

    auto isolation_info = GetSiteInstance()->GetWebExposedIsolationInfo();
    RenderFrameHostImpl* parent_frame_host = GetParentOrOuterDocument();

    absl::optional<blink::ParsedPermissionsPolicy> manifest_policy;
    if (!parent_frame_host && isolation_info.is_isolated_application()) {
      if (auto isolated_web_app_permissions_policy =
              GetContentClient()
                  ->browser()
                  ->GetPermissionsPolicyForIsolatedWebApp(
                      GetBrowserContext(), isolation_info.origin())) {
        manifest_policy = std::move(isolated_web_app_permissions_policy);
      }
    }

    SendCommitNavigation(
        navigation_client, navigation_request, std::move(common_params),
        std::move(commit_params), std::move(head), std::move(response_body),
        std::move(url_loader_client_endpoints),
        std::move(subresource_loader_factories),
        std::move(subresource_overrides), std::move(controller),
        std::move(container_info),
        std::move(subresource_proxying_loader_factory_for_renderer),
        std::move(keep_alive_loader_factory),
        std::move(fetch_later_loader_factory), std::move(resource_cache_remote),
        manifest_policy, std::move(policy_container), *document_token,
        devtools_navigation_token);
    navigation_request->frame_tree_node()
        ->navigator()
        .LogCommitNavigationSent();

    // |remote_object| is an associated interface ptr, so calls can't be made on
    // it until its request endpoint is sent. Now that the request endpoint was
    // sent, it can be used, so add it to ServiceWorkerObjectHost.
    if (remote_object.is_valid()) {
      subresource_loader_params->controller_service_worker_object_host
          ->AddRemoteObjectPtrAndUpdateState(std::move(remote_object),
                                             sent_state);
    }
  }
}

void RenderFrameHostImpl::FailedNavigation(
    NavigationRequest* navigation_request,
    const blink::mojom::CommonNavigationParams& common_params,
    const blink::mojom::CommitNavigationParams& commit_params,
    bool has_stale_copy_in_cache,
    int error_code,
    int extended_error_code,
    const absl::optional<std::string>& error_page_content,
    const blink::DocumentToken& document_token) {
  TRACE_EVENT2("navigation", "RenderFrameHostImpl::FailedNavigation",
               "navigation_request", navigation_request, "error", error_code);

  DCHECK(navigation_request);

  // Update renderer permissions even for failed commits, so that for example
  // the URL bar correctly displays privileged URLs instead of filtering them.
  UpdatePermissionsForNavigation(navigation_request);

  // Get back to a clean state, in case a new navigation started without
  // completing an unload handler.
  ResetWaitingState();

  std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
      subresource_loader_factories;
  mojo::PendingRemote<network::mojom::URLLoaderFactory> default_factory_remote;
  bool bypass_redirect_checks = CreateNetworkServiceDefaultFactoryAndObserve(
      CreateURLLoaderFactoryParamsForMainWorld(
          SubresourceLoaderFactoriesConfig::ForPendingNavigation(
              *navigation_request),
          "RFHI::FailedNavigation"),
      ukm::kInvalidSourceIdObj,
      default_factory_remote.InitWithNewPipeAndPassReceiver());
  subresource_loader_factories =
      std::make_unique<blink::PendingURLLoaderFactoryBundle>(
          std::move(default_factory_remote),
          blink::PendingURLLoaderFactoryBundle::SchemeMap(),
          blink::PendingURLLoaderFactoryBundle::OriginMap(),
          bypass_redirect_checks);
  recreate_default_url_loader_factory_after_network_service_crash_ = true;

  mojom::NavigationClient* navigation_client =
      navigation_request->GetCommitNavigationClient();

  blink::mojom::PolicyContainerPtr policy_container =
      navigation_request->CreatePolicyContainerForBlink();

  SendCommitFailedNavigation(navigation_client, navigation_request,
                             common_params.Clone(), commit_params.Clone(),
                             has_stale_copy_in_cache, error_code,
                             extended_error_code, error_page_content,
                             std::move(subresource_loader_factories),
                             document_token, std::move(policy_container));

  // TODO(crbug/1129537): support UKM source creation for failed navigations
  // too.

  has_committed_any_navigation_ = true;
  DCHECK(navigation_request && navigation_request->IsNavigationStarted() &&
         navigation_request->DidEncounterError());
}

void RenderFrameHostImpl::AddResourceTimingEntryForFailedSubframeNavigation(
    FrameTreeNode* child_frame,
    base::TimeTicks start_time,
    base::TimeTicks redirect_time,
    const GURL& initial_url,
    const GURL& final_url,
    network::mojom::URLResponseHeadPtr response_head,
    bool allow_response_details,
    const network::URLLoaderCompletionStatus& completion_status) {
  uint32_t status_code = 0;
  std::string mime_type;
  std::string normalized_server_timing;

  response_head->headers->GetNormalizedHeader("Server-Timing",
                                              &normalized_server_timing);

  if (allow_response_details) {
    status_code = response_head->headers->response_code();
    mime_type = response_head->mime_type;
  }

  // To avoid cross-origin leaks, make sure to only to pass here data that
  // is OK when TAO-gated (as in, timing information only).

  absl::optional<blink::FrameToken> child_token_in_parent =
      child_frame->GetRenderFrameHostManager()
          .GetFrameTokenForSiteInstanceGroup(GetSiteInstance()->group());

  if (!child_token_in_parent) {
    return;
  }

  GetAssociatedLocalFrame()->AddResourceTimingEntryForFailedSubframeNavigation(
      child_token_in_parent.value(), initial_url, start_time, redirect_time,
      response_head->request_start, response_head->response_start, status_code,
      mime_type, response_head->load_timing, response_head->connection_info,
      response_head->alpn_negotiated_protocol,
      base::Contains(url::GetSecureSchemes(),
                     url::Origin::Create(final_url).scheme()),
      response_head->is_validated, normalized_server_timing, completion_status);
}

void RenderFrameHostImpl::HandleRendererDebugURL(const GURL& url) {
  DCHECK(blink::IsRendererDebugURL(url));

  GetAssociatedLocalFrame()->HandleRendererDebugURL(url);

  // Ensure that the renderer process is marked as used after processing a
  // renderer debug URL, since this process is now unsafe to be reused by sites
  // that require a dedicated process.  Usually this happens at ready-to-commit
  // (NavigationRequest::OnResponseStarted) time for regular navigations, but
  // renderer debug URLs don't go through that path.  This matters for initial
  // navigations to renderer debug URLs.  See https://crbug.com/1074108.
  GetProcess()->SetIsUsed();
}

void RenderFrameHostImpl::CreateBroadcastChannelProvider(
    mojo::PendingAssociatedReceiver<blink::mojom::BroadcastChannelProvider>
        receiver) {
  auto* storage_partition_impl =
      static_cast<StoragePartitionImpl*>(GetStoragePartition());

  auto* broadcast_channel_service =
      storage_partition_impl->GetBroadcastChannelService();
  broadcast_channel_service->AddAssociatedReceiver(
      std::make_unique<BroadcastChannelProvider>(broadcast_channel_service,
                                                 GetStorageKey()),
      std::move(receiver));
}

void RenderFrameHostImpl::BindBlobUrlStoreAssociatedReceiver(
    mojo::PendingAssociatedReceiver<blink::mojom::BlobURLStore> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* storage_partition_impl =
      static_cast<StoragePartitionImpl*>(GetStoragePartition());

  storage_partition_impl->GetBlobUrlRegistry()->AddReceiver(
      GetStorageKey(), std::move(receiver));
}

void RenderFrameHostImpl::BindBlobUrlStoreReceiver(
    mojo::PendingReceiver<blink::mojom::BlobURLStore> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* storage_partition_impl =
      static_cast<StoragePartitionImpl*>(GetStoragePartition());

  storage_partition_impl->GetBlobUrlRegistry()->AddReceiver(
      GetStorageKey(), std::move(receiver));
}

bool RenderFrameHostImpl::IsFocused() {
  if (!GetMainFrame()->GetRenderWidgetHost()->is_focused() ||
      !frame_tree_->GetFocusedFrame())
    return false;

  RenderFrameHostImpl* focused_rfh =
      frame_tree_->GetFocusedFrame()->current_frame_host();
  return focused_rfh == this ||
         focused_rfh->IsDescendantOfWithinFrameTree(this);
}

void RenderFrameHostImpl::SetWebUI(NavigationRequest& request) {
  std::unique_ptr<WebUIImpl> new_web_ui = request.TakeWebUI();
  // This function should only be called to set a WebUI object. To clear an
  // existing WebUI object, call `ClearWebUI()` instead.
  CHECK(new_web_ui);
  // If a WebUI has been set for a RenderFrameHost, we shouldn't overwrite it
  // with a new WebUI.
  CHECK(!web_ui_);

  // Verify expectation that WebUI should not be created for error pages.
  DCHECK(!GetSiteInstance()->GetSiteInfo().is_error_page());

  // Ensure that the RenderFrameHost's process is locked.  Usually this happens
  // as part of creating a speculative RFH for WebUI navigations, but it's also
  // possible to reuse an initial RFH in an unassigned SiteInstance for a WebUI
  // navigation. In that case, the initial RFH needs to lock its process now and
  // also mark the process as used. The AllowBindings() call below requires that
  // the process is properly locked to WebUI.
  if (base::FeatureList::IsEnabled(
          features::kReuseInitialRenderFrameHostForWebUI)) {
    if (!GetSiteInstance()->HasSite()) {
      // WebUI URLs should also require assigning a site.
      CHECK(SiteInstanceImpl::ShouldAssignSiteForUrlInfo(request.GetUrlInfo()));
      GetSiteInstance()->ConvertToDefaultOrSetSite(request.GetUrlInfo());
    }
    GetProcess()->SetIsUsed();
  }

  WebUI::TypeID new_web_ui_type =
      WebUIControllerFactoryRegistry::GetInstance()->GetWebUIType(
          GetSiteInstance()->GetBrowserContext(), request.GetURL());
  CHECK_NE(new_web_ui_type, WebUI::kNoWebUI);

  web_ui_ = std::move(new_web_ui);
  web_ui_->SetRenderFrameHost(this);

  // It is not expected for GuestView to be able to navigate to WebUI.
  DCHECK(!GetProcess()->IsForGuestsOnly());

  web_ui_type_ = new_web_ui_type;

  // WebUIs need the ability to request certain schemes.
  for (const auto& scheme : web_ui_->GetRequestableSchemes()) {
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantRequestScheme(
        GetProcess()->GetID(), scheme);
  }

  // Since this is new WebUI instance, this RenderFrameHostImpl should not
  // have had any bindings. Verify that and grant the required bindings.
  DCHECK_EQ(BINDINGS_POLICY_NONE, GetEnabledBindings());
  AllowBindings(web_ui_->GetBindings());
}

void RenderFrameHostImpl::ClearWebUI() {
  web_ui_type_ = WebUI::kNoWebUI;
  web_ui_.reset();  // This might delete `this`.
  // DO NOT ADD CODE after this.
}

const mojo::Remote<blink::mojom::ImageDownloader>&
RenderFrameHostImpl::GetMojoImageDownloader() {
  // TODO(https://crbug.com/1249933): Call AssertFrameWasCommitted() here.
  if (!mojo_image_downloader_.is_bound() && GetRemoteInterfaces()) {
    GetRemoteInterfaces()->GetInterface(
        mojo_image_downloader_.BindNewPipeAndPassReceiver());
  }
  return mojo_image_downloader_;
}

const mojo::AssociatedRemote<blink::mojom::FindInPage>&
RenderFrameHostImpl::GetFindInPage() {
  if (!find_in_page_)
    GetRemoteAssociatedInterfaces()->GetInterface(&find_in_page_);
  return find_in_page_;
}

const mojo::AssociatedRemote<blink::mojom::LocalFrame>&
RenderFrameHostImpl::GetAssociatedLocalFrame() {
  if (!local_frame_)
    GetRemoteAssociatedInterfaces()->GetInterface(&local_frame_);
  return local_frame_;
}

blink::mojom::LocalMainFrame*
RenderFrameHostImpl::GetAssociatedLocalMainFrame() {
  DCHECK(is_main_frame());
  if (!local_main_frame_)
    GetRemoteAssociatedInterfaces()->GetInterface(&local_main_frame_);
  return local_main_frame_.get();
}

const mojo::Remote<blink::mojom::HighPriorityLocalFrame>&
RenderFrameHostImpl::GetHighPriorityLocalFrame() {
  if (!high_priority_local_frame_.is_bound()) {
    GetRemoteInterfaces()->GetInterface(
        high_priority_local_frame_.BindNewPipeAndPassReceiver());
  }
  return high_priority_local_frame_;
}

const mojo::AssociatedRemote<mojom::FrameBindingsControl>&
RenderFrameHostImpl::GetFrameBindingsControl() {
  if (!frame_bindings_control_)
    GetRemoteAssociatedInterfaces()->GetInterface(&frame_bindings_control_);
  return frame_bindings_control_;
}

void RenderFrameHostImpl::ResetLoadingState() {
  if (is_loading()) {
    // When pending deletion, just set the loading state to not loading.
    // Otherwise, DidStopLoading will take care of that, as well as sending
    // notification to the FrameTreeNode about the change in loading state.
    if (IsPendingDeletion() || IsInBackForwardCache()) {
      loading_state_ = LoadingState::NONE;
    } else {
      DidStopLoading();
    }
  }
}

void RenderFrameHostImpl::ClearFocusedElement() {
  has_focused_editable_element_ = false;
  GetAssociatedLocalFrame()->ClearFocusedElement();
}

void RenderFrameHostImpl::BindDevToolsAgent(
    mojo::PendingAssociatedRemote<blink::mojom::DevToolsAgentHost> host,
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> receiver) {
  GetAssociatedLocalFrame()->BindDevToolsAgent(std::move(host),
                                               std::move(receiver));
}

bool RenderFrameHostImpl::IsSameSiteInstance(
    RenderFrameHostImpl* other_render_frame_host) {
  // As a sanity check, make sure the frame belongs to the same BrowserContext.
  CHECK_EQ(GetSiteInstance()->GetBrowserContext(),
           other_render_frame_host->GetSiteInstance()->GetBrowserContext());
  return GetSiteInstance() == other_render_frame_host->GetSiteInstance();
}

void RenderFrameHostImpl::UpdateAccessibilityMode() {
  // Don't update accessibility mode for a frame that hasn't been created yet.
  if (!IsRenderFrameLive())
    return;

  ui::AXMode ax_mode = delegate_->GetAccessibilityMode();

  // Disable BackForwardCache if ScreenReader is on.
  // TODO(crbug.com/1271450): Screen readers do not recognize a navigation when
  // the page is served from bfcache. Remove the flag and this section once the
  // fix is landed.
  if (ax_mode.has_mode(ui::AXMode::kScreenReader) &&
      !BackForwardCacheImpl::IsScreenReaderAllowed()) {
    BackForwardCache::DisableForRenderFrameHost(
        this, BackForwardCacheDisable::DisabledReason(
                  BackForwardCacheDisable::DisabledReasonId::kScreenReader));
  }

  if (ax_mode.has_mode(ui::AXMode::kWebContents)) {
    if (!render_accessibility_) {
      // Render accessibility is not enabled yet, so bind the interface first.
      GetRemoteAssociatedInterfaces()->GetInterface(&render_accessibility_);
      DCHECK(render_accessibility_);
    }
    accessibility_reset_token_ = ++g_accessibility_reset_token;
    render_accessibility_->SetMode(ax_mode, *accessibility_reset_token_);
  } else {
    // Resetting the Remote signals the renderer to shutdown accessibility
    // in the renderer.
    render_accessibility_.reset();
  }

  if (!ax_mode.has_mode(ui::kAXModeBasic.flags()) &&
      browser_accessibility_manager_) {
    // Missing either kWebContents and kNativeAPIs, so
    // BrowserAccessibilityManager is no longer necessary.
    browser_accessibility_manager_->DetachFromParentManager();
    browser_accessibility_manager_.reset();
  }
}

void RenderFrameHostImpl::SnapshotDocumentForViewTransition(
    blink::mojom::LocalFrame::SnapshotDocumentForViewTransitionCallback
        callback) {
  DCHECK(IsRenderFrameLive());
  GetAssociatedLocalFrame()->SnapshotDocumentForViewTransition(
      std::move(callback));
}

#if BUILDFLAG(ENABLE_PPAPI)
RenderFrameHostImplPpapiSupport& RenderFrameHostImpl::GetPpapiSupport() {
  if (!ppapi_support_) {
    ppapi_support_ = std::make_unique<RenderFrameHostImplPpapiSupport>(*this);
  }
  return *ppapi_support_;
}
#endif

void RenderFrameHostImpl::RequestAXTreeSnapshot(
    AXTreeSnapshotCallback callback,
    mojom::SnapshotAccessibilityTreeParamsPtr params) {
  // TODO(https://crbug.com/859110): Remove once frame_ can no longer be null.
  if (!IsRenderFrameLive())
    return;

  GetMojomFrameInRenderer()->SnapshotAccessibilityTree(
      std::move(params),
      base::BindOnce(&RenderFrameHostImpl::RequestAXTreeSnapshotCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RenderFrameHostImpl::GetSavableResourceLinksFromRenderer() {
  if (!IsRenderFrameLive())
    return;
  GetAssociatedLocalFrame()->GetSavableResourceLinks(
      base::BindOnce(&RenderFrameHostImpl::GetSavableResourceLinksCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RenderFrameHostImpl::SetAccessibilityCallbackForTesting(
    const AccessibilityCallbackForTesting& callback) {
  accessibility_testing_callback_ = callback;
}

void RenderFrameHostImpl::UpdateAXTreeData() {
  ui::AXMode accessibility_mode = delegate_->GetAccessibilityMode();
  if (accessibility_mode.is_mode_off() ||
      IsInactiveAndDisallowActivation(
          DisallowActivationReasonId::kAXUpdateTree)) {
    return;
  }

  // If `needs_ax_root_id_` is true, an AXTreeUpdate has not been sent to
  // the renderer with a valid root id. This effectively means the renderer
  // does not really know about the tree, and the renderer should not be
  // updated. The renderer will be updated once the root id is known.
  if (needs_ax_root_id_)
    return;

  if (ax_defer_scope_count_ > 0) {
    ax_update_deferred_ = true;
    return;
  }

  AXEventNotificationDetails detail;
  detail.ax_tree_id = GetAXTreeID();
  detail.updates.resize(1);
  detail.updates[0].has_tree_data = true;
  detail.updates[0].tree_data = GetAXTreeData();

  SendAccessibilityEventsToManager(detail);
  delegate_->AccessibilityEventReceived(detail);
}

RenderFrameHostImpl::UpdateAXFocusDeferScope::UpdateAXFocusDeferScope(
    RenderFrameHostImpl& rfh)
    : rfh_(rfh.GetSafeRef()) {
  ++rfh_->ax_defer_scope_count_;
}

RenderFrameHostImpl::UpdateAXFocusDeferScope::~UpdateAXFocusDeferScope() {
  DCHECK_GE(rfh_->ax_defer_scope_count_, 1);
  --rfh_->ax_defer_scope_count_;
  if (!rfh_->ax_defer_scope_count_ && rfh_->ax_update_deferred_) {
    rfh_->ax_update_deferred_ = false;
    rfh_->UpdateAXTreeData();
  }
}

BrowserAccessibilityManager*
RenderFrameHostImpl::GetOrCreateBrowserAccessibilityManager() {
  // Never create a BrowserAccessibilityManager unless needed for the AXMode.
  // At least basic mode is required; it contains kWebContents and KNativeAPIs.
  ui::AXMode accessibility_mode = delegate_->GetAccessibilityMode();
  if (!accessibility_mode.has_mode(ui::AXMode::kNativeAPIs)) {
    DCHECK(!browser_accessibility_manager_);
    return nullptr;
  }

  if (browser_accessibility_manager_ ||
      no_create_browser_accessibility_manager_for_testing_)
    return browser_accessibility_manager_.get();

  browser_accessibility_manager_.reset(
      BrowserAccessibilityManager::Create(this));
  return browser_accessibility_manager_.get();
}

void RenderFrameHostImpl::ActivateFindInPageResultForAccessibility(
    int request_id) {
  BrowserAccessibilityManager* manager =
      GetOrCreateBrowserAccessibilityManager();
  if (manager)
    manager->ActivateFindInPageResult(request_id);
}

void RenderFrameHostImpl::InsertVisualStateCallback(
    VisualStateCallback callback) {
  GetRenderWidgetHost()->InsertVisualStateCallback(std::move(callback));
}

bool RenderFrameHostImpl::IsLastCommitIPAddressPubliclyRoutable() const {
  net::IPEndPoint ip_end_point =
      last_response_head().get() ? last_response_head().get()->remote_endpoint
                                 : net::IPEndPoint();

  return ip_end_point.address().IsPubliclyRoutable();
}

bool RenderFrameHostImpl::IsRenderFrameLive() {
  bool is_live =
      GetProcess()->IsInitializedAndNotDead() && is_render_frame_created();

  // Sanity check: the `blink::WebView` should always be live if the RenderFrame
  // is.
  DCHECK(!is_live || render_view_host_->IsRenderViewLive());

  return is_live;
}

RenderFrameHost::LifecycleState RenderFrameHostImpl::GetLifecycleState() {
  return GetLifecycleStateFromImpl(lifecycle_state());
}

bool RenderFrameHostImpl::IsInLifecycleState(LifecycleState state) {
  if (lifecycle_state() == LifecycleStateImpl::kSpeculative)
    return false;
  return state == GetLifecycleState();
}

RenderFrameHost::LifecycleState RenderFrameHostImpl::GetLifecycleStateFromImpl(
    LifecycleStateImpl state) {
  switch (state) {
    case LifecycleStateImpl::kSpeculative:
      // TODO(https://crbug.com/1183639): Ensure that Speculative
      // RenderFrameHosts are not exposed to embedders.
      NOTREACHED();
      return LifecycleState::kPendingCommit;
    case LifecycleStateImpl::kPendingCommit:
      return LifecycleState::kPendingCommit;
    case LifecycleStateImpl::kPrerendering:
      return LifecycleState::kPrerendering;
    case LifecycleStateImpl::kActive:
      return LifecycleState::kActive;
    case LifecycleStateImpl::kInBackForwardCache:
      return LifecycleState::kInBackForwardCache;
    case LifecycleStateImpl::kRunningUnloadHandlers:
      return LifecycleState::kPendingDeletion;
    case LifecycleStateImpl::kReadyToBeDeleted:
      return LifecycleState::kPendingDeletion;
  }
}

bool RenderFrameHostImpl::IsActive() {
  // When the document is transitioning away from kActive/kPrerendering to a
  // yet-to-be-determined state, the RenderFrameHostManager has already
  // updated its active RenderFrameHost, and the old document is no longer
  // the active one. In that case, return false.
  if (has_pending_lifecycle_state_update_)
    return false;

  return lifecycle_state() == LifecycleStateImpl::kActive;
}

size_t RenderFrameHostImpl::GetProxyCount() {
  if (!IsActive())
    return 0;
  return browsing_context_state_->GetProxyCount();
}

bool RenderFrameHostImpl::HasSelection() {
  return has_selection_;
}

FrameTreeNode* RenderFrameHostImpl::PreviousSibling() const {
  return GetSibling(-1);
}

FrameTreeNode* RenderFrameHostImpl::NextSibling() const {
  return GetSibling(1);
}

FrameTreeNode* RenderFrameHostImpl::GetSibling(int relative_offset) const {
  if (!parent_ || !parent_->child_count())
    return nullptr;

  for (size_t i = 0; i < parent_->child_count(); ++i) {
    // Frame tree node id will only be known for subframes, and will therefore
    // be accessible in this iteration, as all children are subframes.
    if (parent_->child_at(i)->frame_tree_node_id() != GetFrameTreeNodeId()) {
      continue;
    }

    if (relative_offset < 0 && base::checked_cast<size_t>(-relative_offset) > i)
      return nullptr;
    if (i + relative_offset >= parent_->child_count())
      return nullptr;
    return parent_->child_at(i + relative_offset);
  }

  NOTREACHED() << "FrameTreeNode not found in its parent's children.";
  return nullptr;
}

RenderFrameHostImpl* RenderFrameHostImpl::GetMainFrame() {
  return const_cast<RenderFrameHostImpl*>(std::as_const(*this).GetMainFrame());
}

const RenderFrameHostImpl* RenderFrameHostImpl::GetMainFrame() const {
  // Iteration over the GetParent() chain is used below, because returning
  // |frame_tree().root()->current_frame_host()| might
  // give an incorrect result after |this| has been detached from the frame
  // tree.
  const RenderFrameHostImpl* main_frame = this;
  while (const RenderFrameHostImpl* parent = main_frame->GetParent()) {
    main_frame = parent;
  }
  return main_frame;
}

bool RenderFrameHostImpl::IsInPrimaryMainFrame() {
  return !GetParent() && GetPage().IsPrimary();
}

RenderFrameHostImpl* RenderFrameHostImpl::GetOutermostMainFrame() {
  RenderFrameHostImpl* current = this;
  while (RenderFrameHostImpl* parent_or_outer_doc =
             current->GetParentOrOuterDocument()) {
    current = parent_or_outer_doc;
  }
  return current;
}

bool RenderFrameHostImpl::CanAccessFilesOfPageState(
    const blink::PageState& state) {
  return ChildProcessSecurityPolicyImpl::GetInstance()->CanReadAllFiles(
      GetProcess()->GetID(), state.GetReferencedFiles());
}

void RenderFrameHostImpl::GrantFileAccessFromPageState(
    const blink::PageState& state) {
  GrantFileAccess(GetProcess()->GetID(), state.GetReferencedFiles());
}

void RenderFrameHostImpl::SetHasPendingLifecycleStateUpdate() {
  DCHECK(!has_pending_lifecycle_state_update_);
  for (auto& child : children_)
    child->current_frame_host()->SetHasPendingLifecycleStateUpdate();
  has_pending_lifecycle_state_update_ = true;
}

void RenderFrameHostImpl::GrantFileAccessFromResourceRequestBody(
    const network::ResourceRequestBody& body) {
  GrantFileAccess(GetProcess()->GetID(), body.GetReferencedFiles());
}

void RenderFrameHostImpl::UpdatePermissionsForNavigation(
    NavigationRequest* request) {
  // Browser plugin guests are not allowed to navigate outside web-safe schemes,
  // so do not grant them the ability to commit additional URLs.
  if (!GetProcess()->IsForGuestsOnly()) {
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantCommitURL(
        GetProcess()->GetID(), request->common_params().url);
    if (request->IsLoadDataWithBaseURL()) {
      // When there's a base URL specified for the data URL, we also need to
      // grant access to the base URL. This allows file: and other unexpected
      // schemes to be accepted at commit time and during CORS checks (e.g., for
      // font requests).
      ChildProcessSecurityPolicyImpl::GetInstance()->GrantCommitURL(
          GetProcess()->GetID(),
          request->common_params().base_url_for_data_url);
    }
  }

  // We may be returning to an existing NavigationEntry that had been granted
  // file access.  If this is a different process, we will need to grant the
  // access again.  Abuse is prevented, because the files listed in the page
  // state are validated earlier, when they are received from the renderer (in
  // RenderFrameHostImpl::CanAccessFilesOfPageState).
  blink::PageState page_state = blink::PageState::CreateFromEncodedData(
      request->commit_params().page_state);
  if (page_state.IsValid())
    GrantFileAccessFromPageState(page_state);

  // We may be here after transferring navigation to a different renderer
  // process.  In this case, we need to ensure that the new renderer retains
  // ability to access files that the old renderer could access.  Abuse is
  // prevented, because the files listed in ResourceRequestBody are validated
  // earlier, when they are received from the renderer.
  if (request->common_params().post_data)
    GrantFileAccessFromResourceRequestBody(*request->common_params().post_data);
}

bool RenderFrameHostImpl::WindowManagementAllowsFullscreen() {
  return IsWindowManagementGranted(this) &&
         delegate_->IsTransientAllowFullscreenActive();
}

mojo::AssociatedRemote<mojom::NavigationClient>
RenderFrameHostImpl::GetNavigationClientFromInterfaceProvider() {
  mojo::AssociatedRemote<mojom::NavigationClient> navigation_client_remote;
  GetRemoteAssociatedInterfaces()->GetInterface(&navigation_client_remote);
  return navigation_client_remote;
}

void RenderFrameHostImpl::NavigationRequestCancelled(
    NavigationRequest* navigation_request) {
  // Remove the requests from the list of NavigationRequests waiting to commit.
  // RenderDocument should obsolete the need for this, as always swapping RFHs
  // means that it won't be necessary to clean up the list of navigation
  // requests when the renderer aborts a navigation--instead, we'll always just
  // throw away the entire speculative RFH.
  navigation_requests_.erase(navigation_request);
}

NavigationRequest*
RenderFrameHostImpl::FindLatestNavigationRequestThatIsStillCommitting() {
  // Find the most recent NavigationRequest that has triggered a Commit IPC to
  // the renderer process.  Once the renderer process handles the IPC, it may
  // possibly change the origin from |last_committed_origin_| to another origin.
  NavigationRequest* found_request = nullptr;
  for (const auto& it : navigation_requests_) {
    NavigationRequest* candidate = it.first;
    DCHECK_EQ(candidate, it.second.get());

    if (candidate->state() < NavigationRequest::READY_TO_COMMIT)
      continue;
    if (candidate->state() >= NavigationRequest::DID_COMMIT)
      continue;

    if (!found_request ||
        found_request->NavigationStart() < candidate->NavigationStart()) {
      found_request = candidate;
    }
  }

  return found_request;
}

network::mojom::URLLoaderFactoryParamsPtr
RenderFrameHostImpl::CreateURLLoaderFactoryParamsForMainWorld(
    const SubresourceLoaderFactoriesConfig& config,
    base::StringPiece debug_tag) {
  return URLLoaderFactoryParamsHelper::CreateForFrame(
      this, config.origin(), config.isolation_info(),
      config.GetClientSecurityState(), config.GetCoepReporter(), GetProcess(),
      config.trust_token_issuance_policy(),
      config.trust_token_redemption_policy(), config.cookie_setting_overrides(),
      debug_tag);
}

bool RenderFrameHostImpl::CreateNetworkServiceDefaultFactoryAndObserve(
    network::mojom::URLLoaderFactoryParamsPtr params,
    ukm::SourceIdObj ukm_source_id,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>
        default_factory_receiver) {
  bool bypass_redirect_checks = CreateNetworkServiceDefaultFactoryInternal(
      std::move(params), ukm_source_id, std::move(default_factory_receiver));

  // Add a disconnect handler when Network Service is running
  // out-of-process.
  if (IsOutOfProcessNetworkService() &&
      (!network_service_disconnect_handler_holder_ ||
       !network_service_disconnect_handler_holder_.is_connected())) {
    network_service_disconnect_handler_holder_.reset();
    StoragePartition* storage_partition = GetStoragePartition();
    network::mojom::URLLoaderFactoryParamsPtr monitoring_factory_params =
        network::mojom::URLLoaderFactoryParams::New();
    monitoring_factory_params->process_id = GetProcess()->GetID();
    monitoring_factory_params->debug_tag = "RFHI - monitoring_factory_params";

    // This factory should never be used to issue actual requests (i.e. it
    // should only be used to monitor for Network Service crashes).  Below is an
    // attempt to enforce that the factory cannot be used in practice.
    monitoring_factory_params->request_initiator_origin_lock =
        url::Origin::Create(
            GURL("https://monitoring.url.loader.factory.invalid"));

    storage_partition->GetNetworkContext()->CreateURLLoaderFactory(
        network_service_disconnect_handler_holder_.BindNewPipeAndPassReceiver(),
        std::move(monitoring_factory_params));
    network_service_disconnect_handler_holder_.set_disconnect_handler(
        base::BindOnce(&RenderFrameHostImpl::UpdateSubresourceLoaderFactories,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  return bypass_redirect_checks;
}

bool RenderFrameHostImpl::CreateNetworkServiceDefaultFactoryInternal(
    network::mojom::URLLoaderFactoryParamsPtr params,
    ukm::SourceIdObj ukm_source_id,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>
        default_factory_receiver) {
  DCHECK(params->request_initiator_origin_lock.has_value());
  const url::Origin& request_initiator =
      params->request_initiator_origin_lock.value();

  bool bypass_redirect_checks = false;
  WillCreateURLLoaderFactory(
      request_initiator, &default_factory_receiver, ukm_source_id,
      &params->header_client, &bypass_redirect_checks,
      &params->disable_secure_dns, &params->factory_override);

  GetProcess()->CreateURLLoaderFactory(std::move(default_factory_receiver),
                                       std::move(params));

  return bypass_redirect_checks;
}

void RenderFrameHostImpl::WillCreateURLLoaderFactory(
    const url::Origin& request_initiator,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver,
    ukm::SourceIdObj ukm_source_id,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client,
    bool* bypass_redirect_checks,
    bool* disable_secure_dns,
    network::mojom::URLLoaderFactoryOverridePtr* factory_override) {
  GetContentClient()->browser()->WillCreateURLLoaderFactory(
      GetBrowserContext(), this, GetProcess()->GetID(),
      ContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource,
      request_initiator, /*navigation_id=*/absl::nullopt, ukm_source_id,
      factory_receiver, header_client, bypass_redirect_checks,
      disable_secure_dns, factory_override, /*navigation_task_runner=*/nullptr);

  // Keep DevTools proxy last, i.e. closest to the network.
  devtools_instrumentation::WillCreateURLLoaderFactory(
      this, /*is_navigation=*/false, /*is_download=*/false, factory_receiver,
      factory_override);
}

bool RenderFrameHostImpl::CanExecuteJavaScript() {
  if (g_allow_injecting_javascript)
    return true;

  return !GetLastCommittedURL().is_valid() ||
         GetLastCommittedURL().SchemeIs(kChromeDevToolsScheme) ||
         ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
             GetProcess()->GetID()) ||
         // It's possible to load about:blank in a Web UI renderer.
         // See http://crbug.com/42547
         (GetLastCommittedURL().spec() == url::kAboutBlankURL);
}

// static
int RenderFrameHost::GetFrameTreeNodeIdForRoutingId(int process_id,
                                                    int routing_id) {
  auto frame_or_proxy = LookupRenderFrameHostOrProxy(process_id, routing_id);
  if (frame_or_proxy)
    return frame_or_proxy.GetFrameTreeNode()->frame_tree_node_id();
  return kNoFrameTreeNodeId;
}

// static
int RenderFrameHost::GetFrameTreeNodeIdForFrameToken(
    int process_id,
    const ::blink::FrameToken& frame_token) {
  auto frame_or_proxy = LookupRenderFrameHostOrProxy(process_id, frame_token);
  if (frame_or_proxy)
    return frame_or_proxy.GetFrameTreeNode()->frame_tree_node_id();
  return kNoFrameTreeNodeId;
}

// static
RenderFrameHost* RenderFrameHost::FromPlaceholderToken(
    int render_process_id,
    const blink::RemoteFrameToken& placeholder_frame_token) {
  RenderFrameProxyHost* rfph = RenderFrameProxyHost::FromFrameToken(
      render_process_id, placeholder_frame_token);
  FrameTreeNode* node = rfph ? rfph->frame_tree_node() : nullptr;
  return node ? node->current_frame_host() : nullptr;
}

ui::AXTreeID RenderFrameHostImpl::GetParentAXTreeID() {
  auto* parent = GetParentOrOuterDocumentOrEmbedderExcludingProspectiveOwners();
  if (!parent) {
    DCHECK(AccessibilityIsRootFrame())
        << "Child frame requires a parent, root=" << GetLastCommittedURL();
    return ui::AXTreeIDUnknown();
  }
  // TODO(accessibility) The following check fails when running this test with
  // --force-renderer-accessibility:
  // http/tests/devtools/resource-tree/resource-tree-frame-in-crafted-frame.js
  // It seems that fabricating a frame with document.write() results in a
  // frame that has no embedding token.
  // DCHECK(parent->GetAXTreeID() != ui::AXTreeIDUnknown())
  //     << "Parent frame must have an id, child url = " <<
  //     GetLastCommittedURL()
  //     << "    parent url = " << parent->GetLastCommittedURL();
  DCHECK(!AccessibilityIsRootFrame())
      << "Root frame must not have a parent, root=" << GetLastCommittedURL()
      << "  parent=" << parent->GetLastCommittedURL();
  return parent->GetAXTreeID();
}

ui::AXTreeID RenderFrameHostImpl::GetFocusedAXTreeID() {
  // If this is not the root frame tree node, we're done.
  if (!AccessibilityIsRootFrame())
    return ui::AXTreeIDUnknown();

  RenderFrameHostImpl* focused_frame = delegate_->GetFocusedFrame();
  if (focused_frame)
    return focused_frame->GetAXTreeID();

  // It's possible for there to be no focused RenderFrameHost (e.g. the frame
  // that had focus was destroyed). Note however, that this doesn't mean that
  // keyboard events are ignored; they'd be sent by default to the root
  // RenderWidgetHost.
  return ui::AXTreeIDUnknown();
}

ui::AXTreeData RenderFrameHostImpl::GetAXTreeData() {
  // Make sure to update the locally stored |ax_tree_data_| to include
  // references to the relevant AXTreeIDs before returning its value.
  ax_tree_data_.tree_id = GetAXTreeID();
  ax_tree_data_.parent_tree_id = GetParentAXTreeID();
  ax_tree_data_.focused_tree_id = GetFocusedAXTreeID();
  return ax_tree_data_;
}

void RenderFrameHostImpl::AccessibilityHitTestCallback(
    int request_id,
    ax::mojom::Event event_to_fire,
    base::OnceCallback<void(ui::AXPlatformTreeManager* hit_manager,
                            ui::AXNodeID hit_node_id)> opt_callback,
    blink::mojom::HitTestResponsePtr hit_test_response) {
  if (!hit_test_response) {
    if (opt_callback)
      std::move(opt_callback).Run(nullptr, 0);
    return;
  }

  auto frame_or_proxy = LookupRenderFrameHostOrProxy(
      GetProcess()->GetID(), hit_test_response->hit_frame_token);
  RenderFrameHostImpl* hit_frame = frame_or_proxy.GetCurrentFrameHost();

  if (!hit_frame || hit_frame->IsInactiveAndDisallowActivation(
                        DisallowActivationReasonId::kAXHitTestCallback)) {
    if (opt_callback)
      std::move(opt_callback).Run(nullptr, 0);
    return;
  }

  // If the hit node's routing ID is the same frame, we're done. If a
  // callback was provided, call it with the information about the hit node.
  if (hit_frame->GetFrameToken() == frame_token_) {
    if (opt_callback) {
      std::move(opt_callback)
          .Run(hit_frame->browser_accessibility_manager(),
               hit_test_response->hit_node_id);
    }
    return;
  }

  // The hit node has a child frame. Do a hit test in that frame's renderer.
  hit_frame->AccessibilityHitTest(
      hit_test_response->hit_frame_transformed_point, event_to_fire, request_id,
      std::move(opt_callback));
}

void RenderFrameHostImpl::RequestAXTreeSnapshotCallback(
    AXTreeSnapshotCallback callback,
    const ui::AXTreeUpdate& snapshot) {
  // Since |snapshot| is const, we need to make a copy in order to modify the
  // tree data.
  ui::AXTreeUpdate dst_snapshot;
  CopyAXTreeUpdate(snapshot, &dst_snapshot);
  std::move(callback).Run(dst_snapshot);
}

void RenderFrameHostImpl::CopyAXTreeUpdate(const ui::AXTreeUpdate& snapshot,
                                           ui::AXTreeUpdate* snapshot_copy) {
  snapshot_copy->root_id = snapshot.root_id;
  snapshot_copy->nodes.resize(snapshot.nodes.size());
  for (size_t i = 0; i < snapshot.nodes.size(); ++i)
    snapshot_copy->nodes[i] = snapshot.nodes[i];

  if (snapshot.has_tree_data) {
    ax_tree_data_ = snapshot.tree_data;
    // Set the AXTreeData to be the last |ax_tree_data_| received from the
    // render frame.
    snapshot_copy->tree_data = GetAXTreeData();
    snapshot_copy->has_tree_data = true;
  }
}

void RenderFrameHostImpl::CreatePaymentManager(
    mojo::PendingReceiver<payments::mojom::PaymentManager> receiver) {
  if (!IsFeatureEnabled(blink::mojom::PermissionsPolicyFeature::kPayment)) {
    mojo::ReportBadMessage("Permissions policy blocks Payment");
    return;
  }
  GetProcess()->CreatePaymentManagerForOrigin(GetLastCommittedOrigin(),
                                              std::move(receiver));

  // Blocklist PaymentManager from the back-forward cache as at the moment we
  // don't cancel pending payment requests when the RenderFrameHost is stored
  // in back-forward cache.
  OnBackForwardCacheDisablingStickyFeatureUsed(
      BackForwardCacheDisablingFeature::kPaymentManager);
}

WebBluetoothServiceImpl*
RenderFrameHostImpl::GetWebBluetoothServiceForTesting() {
  if (!document_associated_data_ || !last_web_bluetooth_service_for_testing_)
    return nullptr;

  return last_web_bluetooth_service_for_testing_;
}

void RenderFrameHostImpl::CreateWebBluetoothService(
    mojo::PendingReceiver<blink::mojom::WebBluetoothService> receiver) {
  BackForwardCache::DisableForRenderFrameHost(
      this, BackForwardCacheDisable::DisabledReason(
                BackForwardCacheDisable::DisabledReasonId::kWebBluetooth));

  // Although the returned pointer is being stored for test support, this is not
  // a test-only function, and the call of WebBluetoothServiceImpl::Create below
  // is how the WebBluetoothServiceImpl instance gets created.
  last_web_bluetooth_service_for_testing_ =
      WebBluetoothServiceImpl::Create(this, std::move(receiver));
}

void RenderFrameHostImpl::CreateWebUsbService(
    mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) {
  if (!base::FeatureList::IsEnabled(features::kWebUsb)) {
    return;
  }
  if (!IsFeatureEnabled(blink::mojom::PermissionsPolicyFeature::kUsb)) {
    mojo::ReportBadMessage("Permissions policy blocks access to USB.");
    return;
  }
  if (GetOutermostMainFrame()->GetLastCommittedOrigin().opaque()) {
    mojo::ReportBadMessage(
        "WebUSB is not allowed when the top-level document has an opaque "
        "origin.");
    return;
  }
  BackForwardCache::DisableForRenderFrameHost(
      this, BackForwardCacheDisable::DisabledReason(
                BackForwardCacheDisable::DisabledReasonId::kWebUSB));
  WebUsbServiceImpl::Create(*this, std::move(receiver));
}

void RenderFrameHostImpl::ResetPermissionsPolicy() {
  if (IsFencedFrameRoot()) {
    // Fenced frames have a list of required permission policies to load and
    // can't be granted extra policies, so use the required policies instead of
    // inheriting from its parent. Note that the parent policies must allow the
    // required policies, which is checked separately in
    // NavigationRequest::CheckPermissionsPoliciesForFencedFrames.
    const absl::optional<FencedFrameProperties>& fenced_frame_properties =
        frame_tree_node()->GetFencedFrameProperties();
    base::span<const blink::mojom::PermissionsPolicyFeature> permissions;
    if (fenced_frame_properties) {
      permissions = fenced_frame_properties->effective_enabled_permissions;
    }
    permissions_policy_ = blink::PermissionsPolicy::CreateForFencedFrame(
        last_committed_origin_, permissions);
    return;
  }

  RenderFrameHostImpl* parent_frame_host = GetParent();
  auto isolation_info = GetSiteInstance()->GetWebExposedIsolationInfo();

  if (!parent_frame_host && isolation_info.is_isolated_application()) {
    // In Isolated Apps, the top level frame should use the policy declared in
    // the Web App Manifest.
    if (auto isolated_web_app_permissions_policy =
            GetContentClient()
                ->browser()
                ->GetPermissionsPolicyForIsolatedWebApp(
                    GetBrowserContext(), isolation_info.origin())) {
      permissions_policy_ = blink::PermissionsPolicy::CreateFromParsedPolicy(
          *isolated_web_app_permissions_policy, last_committed_origin_);
      return;
    }
  }

  const blink::PermissionsPolicy* parent_policy =
      parent_frame_host ? parent_frame_host->permissions_policy() : nullptr;
  blink::ParsedPermissionsPolicy container_policy =
      browsing_context_state_->effective_frame_policy().container_policy;

  permissions_policy_ = blink::PermissionsPolicy::CreateFromParentPolicy(
      parent_policy, container_policy, last_committed_origin_);
}

void RenderFrameHostImpl::CreateAudioInputStreamFactory(
    mojo::PendingReceiver<blink::mojom::RendererAudioInputStreamFactory>
        receiver) {
  BrowserMainLoop* browser_main_loop = BrowserMainLoop::GetInstance();
  DCHECK(browser_main_loop);
  MediaStreamManager* msm = browser_main_loop->media_stream_manager();
  audio_service_audio_input_stream_factory_.emplace(std::move(receiver), msm,
                                                    this);
}

void RenderFrameHostImpl::CreateAudioOutputStreamFactory(
    mojo::PendingReceiver<blink::mojom::RendererAudioOutputStreamFactory>
        receiver) {
  media::AudioSystem* audio_system =
      BrowserMainLoop::GetInstance()->audio_system();
  MediaStreamManager* media_stream_manager =
      BrowserMainLoop::GetInstance()->media_stream_manager();
  audio_service_audio_output_stream_factory_.emplace(
      this, audio_system, media_stream_manager, std::move(receiver));
}

void RenderFrameHostImpl::GetFeatureObserver(
    mojo::PendingReceiver<blink::mojom::FeatureObserver> receiver) {
  if (!feature_observer_) {
    // Lazy initialize because tests sets the overridden content client
    // after the RFHI constructor.
    auto* client = GetContentClient()->browser()->GetFeatureObserverClient();
    if (!client)
      return;
    feature_observer_ = std::make_unique<FeatureObserver>(
        client, GlobalRenderFrameHostId(GetProcess()->GetID(), routing_id_));
  }
  feature_observer_->GetFeatureObserver(std::move(receiver));
}

void RenderFrameHostImpl::BindRenderAccessibilityHost(
    mojo::PendingReceiver<blink::mojom::RenderAccessibilityHost> receiver) {
  // There must be an accessibility token as
  // RenderAccessibilityImpl::ScheduleSendPendingAccessibilityEvents will only
  // attempt to send updates once it has created one, which happens as part of
  // the commit which in turns updates the browser's token before this method
  // could be called.
  DCHECK(GetAXTreeID().token());
  // `render_accessibility_host_` is reset in `TearDownMojoConnection()`, but
  // this Mojo endpoint lives on another sequence and posts tasks back to this
  // `RenderFrameHostImpl` on the UI thread. After the reset, there may still be
  // tasks in flight: use `render_frame_scoped_weak_ptr_factory_` to ensure
  // those tasks are dropped if they arrive after the reset of their
  // corresponding RenderAccessibilityHost.
  ui::AXTreeID ax_tree_id = GetAXTreeID();
  if (!render_accessibility_host_ ||
      ax_tree_id != render_accessibility_host_ax_tree_id_) {
    render_accessibility_host_ = base::SequenceBound<RenderAccessibilityHost>(
        base::ThreadPool::CreateSequencedTaskRunner({}),
        render_frame_scoped_weak_ptr_factory_.GetWeakPtr(), ax_tree_id);
  }
  render_accessibility_host_ax_tree_id_ = ax_tree_id;
  render_accessibility_host_.AsyncCall(&RenderAccessibilityHost::Bind)
      .WithArgs(std::move(receiver));
}

void RenderFrameHostImpl::BindNonAssociatedLocalFrameHost(
    mojo::PendingReceiver<blink::mojom::NonAssociatedLocalFrameHost> receiver) {
  non_associated_local_frame_host_receiver_.Bind(std::move(receiver));
}

bool RenderFrameHostImpl::CancelPrerendering(
    const PrerenderCancellationReason& reason) {
  // A prerendered page is identified by its root FrameTreeNode id, so if this
  // RenderFrameHost is in any way embedded, we need to iterate up to the
  // prerender root.
  FrameTreeNode* outermost_frame =
      GetOutermostMainFrameOrEmbedder()->frame_tree_node();

  // We need to explicitly check that `outermost_frame` is in a prerendering
  // frame tree before accessing `GetPrerenderHostRegistry()`. Non-prerendered
  // frames may outlive the PrerenderHostRegistry during WebContents
  // destruction.
  if (outermost_frame->GetFrameType() != FrameType::kPrerenderMainFrame) {
    return false;
  }

  // If this runs during the WebContents destruction, PrerenderHostRegistry was
  // already destroyed and bound prerenderings are already cancelled.
  // We can check the FrameTree status as the tree's shutdown runs first.
  if (outermost_frame->frame_tree().IsBeingDestroyed()) {
    return false;
  }

  return delegate_->GetPrerenderHostRegistry()->CancelHost(
      outermost_frame->frame_tree_node_id(), reason);
}

void RenderFrameHostImpl::CancelPrerenderingByMojoBinderPolicy(
    const std::string& interface_name) {
  // A prerendered page is identified by its root FrameTreeNode id, so if this
  // RenderFrameHost is in any way embedded, we need to iterate up to the
  // prerender root.
  FrameTreeNode* outermost_frame =
      GetOutermostMainFrameOrEmbedder()->frame_tree_node();
  PrerenderHost* prerender_host =
      delegate_->GetPrerenderHostRegistry()->FindNonReservedHostById(
          outermost_frame->frame_tree_node_id());
  if (!prerender_host)
    return;

  bool canceled = CancelPrerendering(
      PrerenderCancellationReason::BuildForMojoBinderPolicy(interface_name));
  // This function is called from MojoBinderPolicyApplier, which should only be
  // active during prerendering. It would be an error to call this while not
  // prerendering, as it could mean an interface request is never resolved for
  // an active page.
  DCHECK(canceled);
}

void RenderFrameHostImpl::CancelPreviewByMojoBinderPolicy(
    const std::string& interface_name) {
  frame_tree_->page_delegate()->CancelPreviewByMojoBinderPolicy(interface_name);
}

void RenderFrameHostImpl::RendererWillActivateForPrerendering() {
  // Loosen the policies of the Mojo capability control during dispatching the
  // prerenderingchange event in Blink, because the page may start legitimately
  // using controlled interfaces once prerenderingchange is dispatched. We
  // cannot release policies at this point, i.e., we cannot run the deferred
  // binders, because the Mojo message pipes are not channel-associated and we
  // should ensure that ActivateForPrerendering() arrives on the renderer
  // earlier than these deferred messages.
  DCHECK(mojo_binder_policy_applier_)
      << "prerendering pages should have a policy applier";
  mojo_binder_policy_applier_->PrepareToGrantAll();
}

void RenderFrameHostImpl::BindMediaInterfaceFactoryReceiver(
    mojo::PendingReceiver<media::mojom::InterfaceFactory> receiver) {
  MediaInterfaceProxy::GetOrCreateForCurrentDocument(this)->Bind(
      std::move(receiver));
}

void RenderFrameHostImpl::BindMediaMetricsProviderReceiver(
    mojo::PendingReceiver<media::mojom::MediaMetricsProvider> receiver) {
  // Only save decode stats when BrowserContext provides a VideoPerfHistory.
  // Off-the-record contexts will internally use an ephemeral history DB.
  media::VideoDecodePerfHistory::SaveCallback save_stats_cb;
  if (GetSiteInstance()->GetBrowserContext()->GetVideoDecodePerfHistory()) {
    save_stats_cb = GetSiteInstance()
                        ->GetBrowserContext()
                        ->GetVideoDecodePerfHistory()
                        ->GetSaveCallback();
  }

  auto is_shutting_down_cb = base::BindRepeating(
      [](base::WeakPtr<RenderFrameHostImpl> rfh) {
        if (GetContentClient()->browser()->IsShuttingDown()) {
          return true;
        }
        return !rfh ||
               rfh->GetLifecycleState() == LifecycleState::kPendingDeletion;
      },
      weak_ptr_factory_.GetWeakPtr());

  media::MediaMetricsProvider::Create(
      GetProcess()->GetBrowserContext()->IsOffTheRecord()
          ? media::MediaMetricsProvider::BrowsingMode::kIncognito
          : media::MediaMetricsProvider::BrowsingMode::kNormal,
      IsOutermostMainFrame()
          ? media::MediaMetricsProvider::FrameStatus::kTopFrame
          : media::MediaMetricsProvider::FrameStatus::kNotTopFrame,
      GetPage().last_main_document_source_id(),
      media::learning::FeatureValue(GetLastCommittedOrigin().host()),
      std::move(save_stats_cb),
      base::BindRepeating(
          [](base::WeakPtr<RenderFrameHostImpl> frame)
              -> media::learning::LearningSession* {
            if (!base::FeatureList::IsEnabled(media::kMediaLearningFramework) ||
                !frame) {
              return nullptr;
            }

            return frame->GetProcess()
                ->GetBrowserContext()
                ->GetLearningSession();
          },
          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(
          &RenderFrameHostImpl::GetRecordAggregateWatchTimeCallback,
          base::Unretained(this)),
      std::move(is_shutting_down_cb), std::move(receiver));
}

void RenderFrameHostImpl::BindVideoEncoderMetricsProviderReceiver(
    mojo::PendingReceiver<media::mojom::VideoEncoderMetricsProvider> receiver) {
  // Ensure the frame is not in the prerendering state as we don't record UKM
  // while prerendering. This is ensured as the BrowserInterfaceBinders defers
  // binding until the frame's activation.
  CHECK(!IsInLifecycleState(LifecycleState::kPrerendering));
  media::MojoVideoEncoderMetricsProviderService::Create(GetPageUkmSourceId(),
                                                        std::move(receiver));
}

#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
void RenderFrameHostImpl::BindMediaRemoterFactoryReceiver(
    mojo::PendingReceiver<media::mojom::RemoterFactory> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<RemoterFactoryImpl>(GetProcess()->GetID(), routing_id_),
      std::move(receiver));
}
#endif

void RenderFrameHostImpl::CreateWebSocketConnector(
    mojo::PendingReceiver<blink::mojom::WebSocketConnector> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<WebSocketConnectorImpl>(
                                  GetProcess()->GetID(), routing_id_,
                                  last_committed_origin_, isolation_info_),
                              std::move(receiver));
}

void RenderFrameHostImpl::CreateWebTransportConnector(
    mojo::PendingReceiver<blink::mojom::WebTransportConnector> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<WebTransportConnectorImpl>(
          GetProcess()->GetID(), weak_ptr_factory_.GetWeakPtr(),
          last_committed_origin_, isolation_info_.network_anonymization_key()),
      std::move(receiver));
}

void RenderFrameHostImpl::CreateNotificationService(
    mojo::PendingReceiver<blink::mojom::NotificationService> receiver) {
  GetProcess()->CreateNotificationService(
      GetGlobalId(),
      RenderProcessHost::NotificationServiceCreatorType::kDocument,
      GetStorageKey(), std::move(receiver));
}

void RenderFrameHostImpl::CreateInstalledAppProvider(
    mojo::PendingReceiver<blink::mojom::InstalledAppProvider> receiver) {
  InstalledAppProviderImpl::Create(*this, std::move(receiver));
}

void RenderFrameHostImpl::CreateCodeCacheHostWithKeys(
    mojo::PendingReceiver<blink::mojom::CodeCacheHost> receiver,
    const net::NetworkIsolationKey& nik,
    const blink::StorageKey& storage_key) {
  // Create a new CodeCacheHostImpl and bind it to the given receiver.
  code_cache_host_receivers_.Add(GetProcess()->GetID(), nik, storage_key,
                                 std::move(receiver),
                                 GetCodeCacheHostReceiverHandler());
}

void RenderFrameHostImpl::CreateCodeCacheHost(
    mojo::PendingReceiver<blink::mojom::CodeCacheHost> receiver) {
  CreateCodeCacheHostWithKeys(std::move(receiver), GetNetworkIsolationKey(),
                              GetStorageKey());
}

void RenderFrameHostImpl::CreateDedicatedWorkerHostFactory(
    mojo::PendingReceiver<blink::mojom::DedicatedWorkerHostFactory> receiver) {
  // Allocate the worker in the same process as the creator.
  int worker_process_id = GetProcess()->GetID();

  base::WeakPtr<CrossOriginEmbedderPolicyReporter> coep_reporter;
  if (coep_reporter_) {
    coep_reporter = coep_reporter_->GetWeakPtr();
  }

  // When a dedicated worker is created from the frame script, the frame is both
  // the creator and the ancestor.
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<DedicatedWorkerHostFactoryImpl>(
          worker_process_id,
          /*creator_render_frame_host_id=*/GetGlobalId(),
          /*creator_worker_token=*/absl::nullopt,
          /*ancestor_render_frame_host_id=*/GetGlobalId(), GetStorageKey(),
          isolation_info_, BuildClientSecurityState(),
          /*creator_coep_reporter=*/coep_reporter,
          /*ancestor_coep_reporter=*/coep_reporter),
      std::move(receiver));
}

#if BUILDFLAG(IS_ANDROID)
void RenderFrameHostImpl::BindNFCReceiver(
    mojo::PendingReceiver<device::mojom::NFC> receiver) {
  delegate_->GetNFC(this, std::move(receiver));
}
#endif

#if !BUILDFLAG(IS_ANDROID)
void RenderFrameHostImpl::BindSerialService(
    mojo::PendingReceiver<blink::mojom::SerialService> receiver) {
  if (!IsFeatureEnabled(blink::mojom::PermissionsPolicyFeature::kSerial)) {
    mojo::ReportBadMessage("Permissions policy blocks access to Serial.");
    return;
  }

  // Powerful features like Serial API for FencedFrames are blocked by
  // PermissionsPolicy. But as the interface is still exposed to the renderer,
  // still good to have a secondary check per-API basis to handle compromised
  // renderers. Ignore the request and mark it as bad to kill the initiating
  // renderer if it happened for some reason.
  if (IsNestedWithinFencedFrame()) {
    mojo::ReportBadMessage("Web Serial is not allowed in fences frames.");
    return;
  }

  // Rejects using Serial API when the top-level document has an opaque origin.
  if (GetOutermostMainFrame()->GetLastCommittedOrigin().opaque()) {
    mojo::ReportBadMessage(
        "Web Serial is not allowed when the top-level document has an opaque "
        "origin.");
    return;
  }

  SerialService::GetOrCreateForCurrentDocument(this)->Bind(std::move(receiver));
}

void RenderFrameHostImpl::GetHidService(
    mojo::PendingReceiver<blink::mojom::HidService> receiver) {
  HidService::Create(this, std::move(receiver));
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
void RenderFrameHostImpl::GetSmartCardService(
    mojo::PendingReceiver<blink::mojom::SmartCardService> receiver) {
  SmartCardService::Create(this, std::move(receiver));
}
#endif

IdleManagerImpl* RenderFrameHostImpl::GetIdleManager() {
  return idle_manager_.get();
}

void RenderFrameHostImpl::BindIdleManager(
    mojo::PendingReceiver<blink::mojom::IdleManager> receiver) {
  if (!IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kIdleDetection)) {
    mojo::ReportBadMessage(
        "Permissions policy blocks access to IdleDetection.");
    return;
  }

  idle_manager_->CreateService(std::move(receiver));
  OnBackForwardCacheDisablingStickyFeatureUsed(
      BackForwardCacheDisablingFeature::kIdleManager);
}

void RenderFrameHostImpl::GetPresentationService(
    mojo::PendingReceiver<blink::mojom::PresentationService> receiver) {
  if (!presentation_service_)
    presentation_service_ = PresentationServiceImpl::Create(this);
  presentation_service_->Bind(std::move(receiver));
}

PresentationServiceImpl&
RenderFrameHostImpl::GetPresentationServiceForTesting() {
  DCHECK(presentation_service_);
  return *presentation_service_.get();
}

void RenderFrameHostImpl::GetSpeechSynthesis(
    mojo::PendingReceiver<blink::mojom::SpeechSynthesis> receiver) {
  if (!speech_synthesis_impl_) {
    speech_synthesis_impl_ = std::make_unique<SpeechSynthesisImpl>(
        GetProcess()->GetBrowserContext(), this);
  }
  speech_synthesis_impl_->AddReceiver(std::move(receiver));
}

void RenderFrameHostImpl::GetSensorProvider(
    mojo::PendingReceiver<blink::mojom::WebSensorProvider> receiver) {
  FrameSensorProviderProxy::GetOrCreateForCurrentDocument(this)->Bind(
      std::move(receiver));
}

void RenderFrameHostImpl::BindCacheStorage(
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
  BindCacheStorageInternal(
      std::move(receiver),
      storage::BucketLocator::ForDefaultBucket(GetStorageKey()));
}

void RenderFrameHostImpl::BindCacheStorageInternal(
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver,
    const storage::BucketLocator& bucket_locator) {
  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter_remote;
  if (coep_reporter_) {
    coep_reporter_->Clone(
        coep_reporter_remote.InitWithNewPipeAndPassReceiver());
  }
  GetProcess()->BindCacheStorage(cross_origin_embedder_policy(),
                                 std::move(coep_reporter_remote),
                                 bucket_locator, std::move(receiver));
}

void RenderFrameHostImpl::BindInputInjectorReceiver(
    mojo::PendingReceiver<mojom::InputInjector> receiver) {
  InputInjectorImpl::Create(weak_ptr_factory_.GetWeakPtr(),
                            std::move(receiver));
}

void RenderFrameHostImpl::BindWebOTPServiceReceiver(
    mojo::PendingReceiver<blink::mojom::WebOTPService> receiver) {
  auto* fetcher = SmsFetcher::Get(GetProcess()->GetBrowserContext());
  if (WebOTPService::Create(fetcher, this, std::move(receiver)))
    document_used_web_otp_ = true;
}

void RenderFrameHostImpl::BindFederatedAuthRequestReceiver(
    mojo::PendingReceiver<blink::mojom::FederatedAuthRequest> receiver) {
  FederatedAuthRequestImpl::Create(this, std::move(receiver));
}

void RenderFrameHostImpl::BindRestrictedCookieManager(
    mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver) {
  BindRestrictedCookieManagerWithOrigin(
      std::move(receiver), GetIsolationInfoForSubresources(),
      GetLastCommittedOrigin(), GetCookieSettingOverrides());
}

void RenderFrameHostImpl::BindRestrictedCookieManagerWithOrigin(
    mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver,
    const net::IsolationInfo& isolation_info,
    const url::Origin& origin,
    net::CookieSettingOverrides cookie_setting_overrides) {
  // CookieSettingOverrides is passesd in instead of calling
  // GetCookieSettingOverrides, because this call can happen before the frame
  // is committed.
  GetStoragePartition()->CreateRestrictedCookieManager(
      network::mojom::RestrictedCookieManagerRole::SCRIPT, origin,
      isolation_info,
      /*is_service_worker=*/false, GetProcess()->GetID(), GetRoutingID(),
      cookie_setting_overrides, std::move(receiver),
      CreateCookieAccessObserver());
}

void RenderFrameHostImpl::BindTrustTokenQueryAnswerer(
    mojo::PendingReceiver<network::mojom::TrustTokenQueryAnswerer> receiver) {
  auto top_frame_origin = ComputeTopFrameOrigin(GetLastCommittedOrigin());

  // A check at the callsite in the renderer ensures a correctly functioning
  // renderer will only request this Mojo handle if the top-frame origin is
  // potentially trustworthy and has scheme HTTP or HTTPS.
  if ((top_frame_origin.scheme() != url::kHttpScheme &&
       top_frame_origin.scheme() != url::kHttpsScheme) ||
      !network::IsOriginPotentiallyTrustworthy(top_frame_origin)) {
    mojo::ReportBadMessage(
        "Attempted to get a TrustTokenQueryAnswerer for a non-trustworthy or "
        "non-HTTP/HTTPS top-frame origin.");
    return;
  }

  // Both flags are enforced in benign renderers by the
  // RuntimeEnabled=PrivateStateTokens IDL attribute (the base::Feature's value
  // is tied to the RuntimeEnabledFeature's).
  if (!base::FeatureList::IsEnabled(network::features::kPrivateStateTokens) &&
      !base::FeatureList::IsEnabled(network::features::kFledgePst)) {
    mojo::ReportBadMessage(
        "Attempted to get a TrustTokenQueryAnswerer with Private State Tokens "
        "disabled.");
    return;
  }

  // TODO(crbug.com/1145346): Document.hasPrivateToken is restricted to
  // secure contexts, so we could additionally add a check verifying that the
  // bind request "is coming from a secure context"---but there's currently no
  // direct way to perform such a check in the browser.
  GetProcess()->GetStoragePartition()->CreateTrustTokenQueryAnswerer(
      std::move(receiver), ComputeTopFrameOrigin(GetLastCommittedOrigin()));
}

void RenderFrameHostImpl::GetAudioContextManager(
    mojo::PendingReceiver<blink::mojom::AudioContextManager> receiver) {
  AudioContextManagerImpl::Create(this, std::move(receiver));
}

void RenderFrameHostImpl::GetFileSystemManager(
    mojo::PendingReceiver<blink::mojom::FileSystemManager> receiver) {
  // This is safe because file_system_manager_ is deleted on the IO thread
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&FileSystemManagerImpl::BindReceiver,
                                base::Unretained(file_system_manager_.get()),
                                GetStorageKey(), std::move(receiver)));
}

void RenderFrameHostImpl::GetGeolocationService(
    mojo::PendingReceiver<blink::mojom::GeolocationService> receiver) {
  if (!geolocation_service_) {
    auto* geolocation_context = delegate_->GetGeolocationContext();
    if (!geolocation_context)
      return;
    geolocation_service_ =
        std::make_unique<GeolocationServiceImpl>(geolocation_context, this);
  }
  geolocation_service_->Bind(std::move(receiver));
}

void RenderFrameHostImpl::GetPendingBeaconHost(
    mojo::PendingReceiver<blink::mojom::PendingBeaconHost> receiver) {
  PendingBeaconHost::CreateForCurrentDocument(
      this, GetStoragePartition()->GetURLLoaderFactoryForBrowserProcess(),
      PendingBeaconService::GetInstance());
  PendingBeaconHost* pbh = PendingBeaconHost::GetForCurrentDocument(this);
  pbh->SetReceiver(std::move(receiver));
}

void RenderFrameHostImpl::GetDeviceInfoService(
    mojo::PendingReceiver<blink::mojom::DeviceAPIService> receiver) {
  GetContentClient()->browser()->CreateDeviceInfoService(this,
                                                         std::move(receiver));
}

void RenderFrameHostImpl::GetManagedConfigurationService(
    mojo::PendingReceiver<blink::mojom::ManagedConfigurationService> receiver) {
  GetContentClient()->browser()->CreateManagedConfigurationService(
      this, std::move(receiver));
}

void RenderFrameHostImpl::GetFontAccessManager(
    mojo::PendingReceiver<blink::mojom::FontAccessManager> receiver) {
  GetStoragePartition()->GetFontAccessManager()->BindReceiver(
      GetGlobalId(), std::move(receiver));
}

void RenderFrameHostImpl::GetFileSystemAccessManager(
    mojo::PendingReceiver<blink::mojom::FileSystemAccessManager> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* manager = GetStoragePartition()->GetFileSystemAccessManager();
  manager->BindReceiver(
      FileSystemAccessManagerImpl::BindingContext(
          GetStorageKey(), GetLastCommittedURL(), GetGlobalId()),
      std::move(receiver));
}

void RenderFrameHostImpl::CreateLockManager(
    mojo::PendingReceiver<blink::mojom::LockManager> receiver) {
  GetProcess()->CreateLockManager(GetStorageKey(), std::move(receiver));
}

void RenderFrameHostImpl::CreateIDBFactory(
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) {
  GetProcess()->BindIndexedDB(GetStorageKey(), GetGlobalId(),
                              std::move(receiver));
}

void RenderFrameHostImpl::CreateBucketManagerHost(
    mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver) {
  GetProcess()->BindBucketManagerHost(weak_ptr_factory_.GetWeakPtr(),
                                      std::move(receiver));
}

void RenderFrameHostImpl::CreatePermissionService(
    mojo::PendingReceiver<blink::mojom::PermissionService> receiver) {
  PermissionServiceContext::GetOrCreateForCurrentDocument(this)->CreateService(
      std::move(receiver));
}

void RenderFrameHostImpl::GetWebAuthenticationService(
    mojo::PendingReceiver<blink::mojom::Authenticator> receiver) {
#if !BUILDFLAG(IS_ANDROID)
  AuthenticatorImpl::Create(this, std::move(receiver));
#else
  GetJavaInterfaces()->GetInterface(std::move(receiver));
#endif  // !BUILDFLAG(IS_ANDROID)
}

void RenderFrameHostImpl::GetPushMessaging(
    mojo::PendingReceiver<blink::mojom::PushMessaging> receiver) {
  if (!push_messaging_manager_) {
    auto* rph = GetProcess();
    push_messaging_manager_ = std::make_unique<PushMessagingManager>(
        *rph, routing_id_,
        base::WrapRefCounted(GetStoragePartition()->GetServiceWorkerContext()));
  }

  push_messaging_manager_->AddPushMessagingReceiver(std::move(receiver));
}

void RenderFrameHostImpl::GetVirtualAuthenticatorManager(
    mojo::PendingReceiver<blink::test::mojom::VirtualAuthenticatorManager>
        receiver) {
#if !BUILDFLAG(IS_ANDROID)
  // VirtualAuthenticatorManagerImpl is enabled at the frame level. Inactive
  // document are detached. They don't have a frame anymore, so they can't be
  // used to enable this test-only feature.
  if (!IsActive()) {
    return;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWebAuthDeprecatedMojoTestingApi)) {
    CHECK(owner_);
    owner_->GetVirtualAuthenticatorManager(std::move(receiver));
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

bool IsInitialSynchronousAboutBlankCommit(const GURL& url,
                                          bool is_initial_empty_document) {
  return url.SchemeIs(url::kAboutScheme) && url != GURL(url::kAboutSrcdocURL) &&
         is_initial_empty_document;
}

std::unique_ptr<NavigationRequest>
RenderFrameHostImpl::CreateNavigationRequestForSynchronousRendererCommit(
    const GURL& url,
    const url::Origin& origin,
    const absl::optional<GURL>& initiator_base_url,
    blink::mojom::ReferrerPtr referrer,
    const ui::PageTransition& transition,
    bool should_replace_current_entry,
    bool has_user_gesture,
    const std::vector<GURL>& redirects,
    const GURL& original_request_url,
    bool is_same_document,
    bool is_same_document_history_api_navigation) {
  // This function must only be called when there are no NavigationRequests for
  // a navigation can be found at DidCommit time, which can only happen in two
  // cases:
  // 1) This was a synchronous renderer-initiated navigation to about:blank
  // after the initial empty document.
  // 2) This was a renderer-initiated same-document navigation.
  DCHECK(IsInitialSynchronousAboutBlankCommit(
             url, frame_tree_node_->is_on_initial_empty_document()) ||
         is_same_document);
  DCHECK(!is_same_document_history_api_navigation || is_same_document);
  DCHECK(!IsPendingDeletion());     // IPC is filtered out by the caller.
  DCHECK(!IsInBackForwardCache());  // A page in the BackForwardCache is fully
                                    // loaded and has no pending navigations.
  // See `owner_` invariants about IsPendingDeletion() and
  // IsInBackForwardCache().
  CHECK(owner_);

  net::IsolationInfo isolation_info = ComputeIsolationInfoInternal(
      origin, net::IsolationInfo::RequestType::kOther, IsCredentialless(),
      /*fenced_frame_nonce_for_navigation=*/absl::nullopt);

  std::unique_ptr<CrossOriginEmbedderPolicyReporter> coep_reporter;
  // We don't switch the COEP reporter on same-document navigations, so create
  // one only for cross-document navigations.
  if (!is_same_document) {
    auto* storage_partition =
        static_cast<StoragePartitionImpl*>(GetProcess()->GetStoragePartition());
    coep_reporter = std::make_unique<CrossOriginEmbedderPolicyReporter>(
        storage_partition->GetWeakPtr(), url,
        cross_origin_embedder_policy().reporting_endpoint,
        cross_origin_embedder_policy().report_only_reporting_endpoint,
        GetReportingSource(), isolation_info.network_anonymization_key());
  }

  std::string method = "GET";
  if (is_same_document && !is_same_document_history_api_navigation) {
    // Preserve the HTTP method used by the last navigation if this is a
    // same-document navigation that is not triggered by the history API
    // (history.replaceState/pushState). See spec:
    // https://html.spec.whatwg.org/multipage/history.html#url-and-history-update-steps
    method = last_http_method_;
  }

  // HTTP status code:
  // - For same-document navigations, we should retain the HTTP status code from
  // the last committed navigation.
  // - For initial about:blank navigation, the HTTP status code is 0.
  int http_status_code = is_same_document ? last_http_status_code_ : 0;

  // Same-document navigation should retain is_overriding_user_agent from the
  // last committed navigation.
  bool is_overriding_user_agent = is_same_document && is_overriding_user_agent_;

  return owner_->CreateNavigationRequestForSynchronousRendererCommit(
      this, is_same_document, url, origin, initiator_base_url, isolation_info,
      std::move(referrer), transition, should_replace_current_entry, method,
      has_user_gesture, is_overriding_user_agent, redirects,
      original_request_url, std::move(coep_reporter), http_status_code);
}

void RenderFrameHostImpl::BeforeUnloadTimeout() {
  if (delegate_->ShouldIgnoreUnresponsiveRenderer()) {
    return;
  }

  SimulateBeforeUnloadCompleted(/*proceed=*/true);
}

void RenderFrameHostImpl::SetLastCommittedSiteInfo(const UrlInfo& url_info) {
  BrowserContext* browser_context = GetSiteInstance()->GetBrowserContext();
  SiteInfo site_info =
      url_info.url.is_empty()
          ? SiteInfo(browser_context)
          : SiteInfo::Create(GetSiteInstance()->GetIsolationContext(),
                             url_info);

  if (last_committed_url_derived_site_info_ == site_info) {
    return;
  }

  if (lifecycle_state_ == LifecycleStateImpl::kActive) {
    // Increment the active document count only if we're committing a new
    // document by replacing an old document in an existing RenderFrameHost
    // (i.e. not when this function is called on RenderFrameHost destruction),
    // which we know is the case if the RenderFrameHost is already active at
    // this point (if it was a speculative RenderFrameHost, it wouldn't have
    // been swapped in yet).
    // Note that this only handles same-RenderFrameHost document commits,
    // while a similar code in `RenderFrameHostImpl::SetLifecycle()`
    // handles cross-RenderFrameHost commits and activeness changes.
    GetSiteInstance()->DecrementActiveDocumentCount(
        last_committed_url_derived_site_info_);
    GetSiteInstance()->IncrementActiveDocumentCount(site_info);
  }

  if (!last_committed_url_derived_site_info_.site_url().is_empty()) {
    RenderProcessHostImpl::RemoveFrameWithSite(
        browser_context, GetProcess(), last_committed_url_derived_site_info_);
  }

  last_committed_url_derived_site_info_ = site_info;

  if (!last_committed_url_derived_site_info_.site_url().is_empty()) {
    RenderProcessHostImpl::AddFrameWithSite(
        browser_context, GetProcess(), last_committed_url_derived_site_info_);
  }
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
RenderFrameHostImpl::GetJavaRenderFrameHost() {
  RenderFrameHostAndroid* render_frame_host_android =
      static_cast<RenderFrameHostAndroid*>(
          GetUserData(kRenderFrameHostAndroidKey));
  if (!render_frame_host_android) {
    render_frame_host_android = new RenderFrameHostAndroid(this);
    SetUserData(kRenderFrameHostAndroidKey,
                base::WrapUnique(render_frame_host_android));
  }
  return render_frame_host_android->GetJavaObject();
}

service_manager::InterfaceProvider* RenderFrameHostImpl::GetJavaInterfaces() {
  if (!java_interfaces_) {
    mojo::PendingRemote<service_manager::mojom::InterfaceProvider> provider;
    BindInterfaceRegistryForRenderFrameHost(
        provider.InitWithNewPipeAndPassReceiver(), this);
    java_interfaces_ = std::make_unique<service_manager::InterfaceProvider>(
        base::SingleThreadTaskRunner::GetCurrentDefault());
    java_interfaces_->Bind(std::move(provider));
  }
  return java_interfaces_.get();
}
#endif

void RenderFrameHostImpl::ForEachImmediateLocalRoot(
    base::FunctionRef<void(RenderFrameHostImpl*)> func_ref) {
  ForEachRenderFrameHostWithAction([func_ref, this](RenderFrameHostImpl* rfh) {
    if (rfh->is_local_root() && rfh != this) {
      func_ref(rfh);
      return FrameIterationAction::kSkipChildren;
    }
    return FrameIterationAction::kContinue;
  });
}

void RenderFrameHostImpl::SetVisibilityForChildViews(bool visible) {
  ForEachImmediateLocalRoot([visible](RenderFrameHostImpl* frame_host) {
    if (auto* view = frame_host->GetView())
      return visible ? view->Show() : view->Hide();
  });

  if (base::FeatureList::IsEnabled(
          network::features::kVisibilityAwareResourceScheduler) &&
      IsOutermostMainFrame()) {
    GetStoragePartition()
        ->GetNetworkContext()
        ->ResourceSchedulerClientVisibilityChanged(GetTopFrameToken().value(),
                                                   visible);
  }
}

mojom::Frame* RenderFrameHostImpl::GetMojomFrameInRenderer() {
  DCHECK(frame_);
  return frame_.get();
}

bool RenderFrameHostImpl::ShouldBypassSecurityChecksForErrorPage(
    NavigationRequest* navigation_request,
    bool* should_commit_error_page) {
  if (should_commit_error_page)
    *should_commit_error_page = false;

  if (SiteIsolationPolicy::IsErrorPageIsolationEnabled(is_main_frame())) {
    if (GetSiteInstance()->GetSiteInfo().is_error_page()) {
      if (should_commit_error_page)
        *should_commit_error_page = true;

      // With error page isolation, any URL can commit in an error page process.
      return true;
    }
  } else {
    // Without error page isolation, a blocked navigation is expected to
    // commit in the old renderer process.  This may be true for subframe
    // navigations even when error page isolation is enabled for main frames.
    if (navigation_request &&
        net::IsRequestBlockedError(navigation_request->GetNetErrorCode())) {
      return true;
    }
  }

  return false;
}

void RenderFrameHostImpl::SetAudioOutputDeviceIdForGlobalMediaControls(
    std::string hashed_device_id) {
  if (audio_service_audio_output_stream_factory_.has_value()) {
    audio_service_audio_output_stream_factory_
        ->SetAuthorizedDeviceIdForGlobalMediaControls(
            std::move(hashed_device_id));
  }
}

std::unique_ptr<mojo::MessageFilter>
RenderFrameHostImpl::CreateMessageFilterForAssociatedReceiver(
    const char* interface_name) {
  return CreateMessageFilterForAssociatedReceiverInternal(
      interface_name,
      BackForwardCacheImpl::GetChannelAssociatedMessageHandlingPolicy());
}

network::mojom::ClientSecurityStatePtr
RenderFrameHostImpl::BuildClientSecurityState() const {
  // TODO(https://crbug.com/1184150) Remove this bandaid.
  //
  // Due to a race condition, CreateCrossOriginPrefetchLoaderFactoryBundle() is
  // sometimes called on the previous document, before the new document is
  // committed. In that case, it mistakenly builds a client security state
  // based on the policies of the previous document. If no document has ever
  // committed, there is no PolicyContainerHost to source policies from. To
  // avoid crashes, this returns a maximally-restrictive value instead.
  if (!policy_container_host_) {
    // Prevent other code paths from depending on this bandaid.
    DCHECK_EQ(lifecycle_state_, LifecycleStateImpl::kSpeculative);

    // Omitted: reporting endpoint, report-only value and reporting endpoint.
    network::CrossOriginEmbedderPolicy coep;
    coep.value = network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;

    return network::mojom::ClientSecurityState::New(
        std::move(coep),
        /*is_web_secure_context=*/false,
        network::mojom::IPAddressSpace::kUnknown,
        network::mojom::PrivateNetworkRequestPolicy::kBlock);
  }

  const PolicyContainerPolicies& policies = policy_container_host_->policies();
  return network::mojom::ClientSecurityState::New(
      policies.cross_origin_embedder_policy, policies.is_web_secure_context,
      policies.ip_address_space, private_network_request_policy_);
}

network::mojom::ClientSecurityStatePtr
RenderFrameHostImpl::BuildClientSecurityStateForWorkers() const {
  auto client_security_state = BuildClientSecurityState();

  client_security_state->private_network_request_policy =
      DerivePrivateNetworkRequestPolicy(
          client_security_state->ip_address_space,
          client_security_state->is_web_secure_context,
          PrivateNetworkRequestContext::kWorker);

  return client_security_state;
}

bool RenderFrameHostImpl::IsNavigationSameSite(
    const UrlInfo& dest_url_info) const {
  if (!WebExposedIsolationInfo::AreCompatible(
          GetSiteInstance()->GetWebExposedIsolationInfo(),
          dest_url_info.web_exposed_isolation_info)) {
    return false;
  }
  return GetSiteInstance()->IsNavigationSameSite(
      last_successful_url(), GetLastCommittedOrigin(), IsOutermostMainFrame(),
      dest_url_info);
}

bool RenderFrameHostImpl::ValidateDidCommitParams(
    NavigationRequest* navigation_request,
    mojom::DidCommitProvisionalLoadParams* params,
    bool is_same_document_navigation) {
  DCHECK(params);
  RenderProcessHost* process = GetProcess();

  // Error pages may sometimes commit a URL in the wrong process, which requires
  // an exception for the CanCommitOriginAndUrl() checks.  This is ok as long
  // as the origin is opaque.
  bool should_commit_error_page = false;
  bool bypass_checks_for_error_page = ShouldBypassSecurityChecksForErrorPage(
      navigation_request, &should_commit_error_page);

  // Commits in the error page process must only be failures, otherwise
  // successful navigations could commit documents from origins different
  // than the chrome-error://chromewebdata/ one and violate expectations.
  if (should_commit_error_page &&
      (navigation_request && !navigation_request->DidEncounterError())) {
    DEBUG_ALIAS_FOR_ORIGIN(origin_debug_alias, params->origin);
    bad_message::ReceivedBadMessage(
        process, bad_message::RFH_ERROR_PROCESS_NON_ERROR_COMMIT);
    return false;
  }

  // Error pages must commit in a opaque origin. Terminate the renderer
  // process if this is violated.
  if (bypass_checks_for_error_page && !params->origin.opaque()) {
    DEBUG_ALIAS_FOR_ORIGIN(origin_debug_alias, params->origin);
    bad_message::ReceivedBadMessage(
        process, bad_message::RFH_ERROR_PROCESS_NON_UNIQUE_ORIGIN_COMMIT);
    return false;
  }

  if (!bypass_checks_for_error_page &&
      !ValidateURLAndOrigin(params->url, params->origin,
                            is_same_document_navigation, navigation_request,
                            params->origin_calculation_debug_info)) {
    return false;
  }

  // Without this check, an evil renderer can trick the browser into creating
  // a navigation entry for a banned URL.  If the user clicks the back button
  // followed by the forward button (or clicks reload, or round-trips through
  // session restore, etc), we'll think that the browser commanded the
  // renderer to load the URL and grant the renderer the privileges to request
  // the URL.  To prevent this attack, we block the renderer from inserting
  // banned URLs into the navigation controller in the first place.
  const RenderProcessHost::FilterURLResult url_filter_result =
      process->FilterURL(false, &params->url);
  process->FilterURL(true, &params->referrer->url);

  if (is_same_document_navigation &&
      url_filter_result == RenderProcessHost::FilterURLResult::kBlocked) {
    // For same-document navigations, keeping about:blank#blocked can lead to
    // some really strange results with navigating back/forward and session
    // restore. So if the URL was filtered, replace it with the current URL:
    // this still ends up not quite matching the state in the renderer, but at
    // least the current URL is in the current origin, and will result in less
    // confusion in the browser process.
    //
    // Note that this would be unsafe for cross-document navigations, which can
    // be cross-origin.
    //
    // TODO(crbug.com/1464018): It would be nice to catch and block this in the
    // renderer process, so the browser process could just treat this as a 'bad
    // message received' situation.
    params->url = GetLastCommittedURL();
  }

  // Without this check, the renderer can trick the browser into using
  // filenames it can't access in a future session restore.
  if (!CanAccessFilesOfPageState(params->page_state)) {
    bad_message::ReceivedBadMessage(
        process, bad_message::RFH_CAN_ACCESS_FILES_OF_PAGE_STATE_AT_COMMIT);
    return false;
  }

  // A cross-document navigation requires an embedding token. Navigations
  // activating an existing document do not require new embedding tokens as the
  // token is already set.
  bool is_page_activation =
      navigation_request && navigation_request->IsPageActivation();
  DCHECK(!is_page_activation || embedding_token_.has_value());
  if (!is_page_activation) {
    if (!is_same_document_navigation && !params->embedding_token.has_value()) {
      bad_message::ReceivedBadMessage(process,
                                      bad_message::RFH_MISSING_EMBEDDING_TOKEN);
      return false;
    } else if (is_same_document_navigation &&
               params->embedding_token.has_value()) {
      bad_message::ReceivedBadMessage(
          process, bad_message::RFH_UNEXPECTED_EMBEDDING_TOKEN);
      return false;
    }
  }

  // Note: document_policy_header is the document policy state used to
  // initialize |document_policy_| in SecurityContext on renderer side. It is
  // supposed to be compatible with required_document_policy. If not, kill the
  // renderer.
  if (!blink::DocumentPolicy::IsPolicyCompatible(
          browsing_context_state_->effective_frame_policy()
              .required_document_policy,
          params->document_policy_header)) {
    bad_message::ReceivedBadMessage(
        process, bad_message::RFH_BAD_DOCUMENT_POLICY_HEADER);
    return false;
  }

  // If a frame claims the navigation was same-document, it must be the current
  // frame, not a pending one.
  if (is_same_document_navigation &&
      lifecycle_state() == LifecycleStateImpl::kPendingCommit) {
    bad_message::ReceivedBadMessage(process,
                                    bad_message::NI_IN_PAGE_NAVIGATION);
    return false;
  }

  // Same document navigations should not be possible on post-commit error pages
  // and would leave the NavigationController in a weird state. Kill the
  // renderer before getting to NavigationController::RendererDidNavigate if
  // that happens.
  if (is_same_document_navigation) {
    // `owner_` must exist, because `DidCommitSameDocumentNavigation()` returns
    // early for RenderFrameHost pending deletion or in the BackForwardCache.
    CHECK(owner_);
    if (owner_->GetCurrentNavigator()
            .controller()
            .has_post_commit_error_entry()) {
      bad_message::ReceivedBadMessage(
          process, bad_message::NC_SAME_DOCUMENT_POST_COMMIT_ERROR);
      return false;
    }
  }

  // Check(s) specific to sub-frame navigation.
  if (navigation_request && !is_main_frame()) {
    if (!CanSubframeCommitOriginAndUrl(navigation_request)) {
      // Terminate the renderer if allowing this subframe navigation to commit
      // would change the origin of the main frame.
      bad_message::ReceivedBadMessage(
          process,
          bad_message::RFHI_SUBFRAME_NAV_WOULD_CHANGE_MAINFRAME_ORIGIN);
      return false;
    }
  }

  return true;
}

bool RenderFrameHostImpl::ValidateURLAndOrigin(
    const GURL& url,
    const url::Origin& origin,
    bool is_same_document_navigation,
    NavigationRequest* navigation_request,
    std::string origin_calculation_debug_info) {
  // file: URLs can be allowed to access any other origin, based on settings.
  if (origin.scheme() == url::kFileScheme) {
    auto prefs = GetOrCreateWebPreferences();
    if (prefs.allow_universal_access_from_file_urls)
      return true;
  }

  // If the --disable-web-security flag is specified, all bets are off and the
  // renderer process can send any origin it wishes.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWebSecurity)) {
    return true;
  }

  // WebView's loadDataWithBaseURL API is allowed to bypass normal commit
  // checks because it is allowed to commit anything into its unlocked process
  // and its data: URL and non-opaque origin would fail the normal commit
  // checks. We should also allow same-document navigations within pages loaded
  // with loadDataWithBaseURL. Since renderer-initiated same-document
  // navigations won't have a NavigationRequest at this point, we need to check
  // |renderer_url_info_.was_loaded_from_load_data_with_base_url|.
  DCHECK(navigation_request || is_same_document_navigation ||
         frame_tree_node_->is_on_initial_empty_document());
  RenderProcessHost* process = GetProcess();
  if ((navigation_request && navigation_request->IsLoadDataWithBaseURL()) ||
      (is_same_document_navigation &&
       renderer_url_info_.was_loaded_from_load_data_with_base_url)) {
    // Allow bypass if the process isn't locked. Otherwise run normal checks.
    if (!process->GetProcessLock().is_locked_to_site())
      return true;
  }

  // Use the value of `is_pdf` from `navigation_request` (if provided). This may
  // be needed to verify the process lock in `CanCommitOriginAndUrl()`, but
  // cannot be derived from the URL and origin alone.
  bool is_pdf = navigation_request && navigation_request->GetUrlInfo().is_pdf;
  bool is_sandboxed =
      navigation_request && navigation_request->GetUrlInfo().is_sandboxed;

  // Attempts to commit certain off-limits URL should be caught more strictly
  // than our FilterURL checks.  If a renderer violates this policy, it
  // should be killed.
  switch (CanCommitOriginAndUrl(origin, url, is_same_document_navigation,
                                is_pdf, is_sandboxed)) {
    case CanCommitStatus::CAN_COMMIT_ORIGIN_AND_URL:
      // The origin and URL are safe to commit.
      break;
    case CanCommitStatus::CANNOT_COMMIT_URL:
      DLOG(ERROR) << "CANNOT_COMMIT_URL url '" << url << "'"
                  << " origin '" << origin << "'"
                  << " lock '" << process->GetProcessLock().ToString() << "'";
      VLOG(1) << "Blocked URL " << url.spec();
      LogCannotCommitUrlCrashKeys(url, is_same_document_navigation,
                                  navigation_request,
                                  origin_calculation_debug_info);

      // Kills the process.
      bad_message::ReceivedBadMessage(process,
                                      bad_message::RFH_CAN_COMMIT_URL_BLOCKED);
      return false;
    case CanCommitStatus::CANNOT_COMMIT_ORIGIN:
      DLOG(ERROR) << "CANNOT_COMMIT_ORIGIN url '" << url << "'"
                  << " origin '" << origin << "'"
                  << " lock '" << process->GetProcessLock().ToString() << "'";
      DEBUG_ALIAS_FOR_ORIGIN(origin_debug_alias, origin);
      LogCannotCommitOriginCrashKeys(url, origin, process->GetProcessLock(),
                                     is_same_document_navigation,
                                     navigation_request);

      // Kills the process.
      bad_message::ReceivedBadMessage(
          process, bad_message::RFH_INVALID_ORIGIN_ON_COMMIT);
      return false;
  }
  return true;
}

// Simulates the calculation for DidCommitProvisionalLoadParams' `referrer`.
// This is written to preserve the behavior of the calculations that happened in
// the renderer before being moved to the browser. In the future, we might want
// to remove this function in favor of using NavigationRequest::GetReferrer()
// or CommonNavigationParam's `referrer` directly.
blink::mojom::ReferrerPtr GetReferrerForDidCommitParams(
    NavigationRequest* request) {
  if (request->DidEncounterError()) {
    // Error pages always use the referrer from CommonNavigationParams, since
    // it won't go through its server redirects in the renderer, and won't be
    // marked as a client redirect.
    // TODO(https://crbug.com/1218786): Maybe make this case just return the
    // sanitized referrer below once the client redirect bug is fixed. This
    // means GetReferrerForDidCommitParams(), NavigationRequest::GetReferrer()
    // (`sanitized_referrer_`), and CommonNavigationParams's `referrer` will all
    // return the same value, and GetReferrerForDidCommitParams() and
    // `sanitized_referrer_` can be removed, leaving only
    // CommonNavigationParams's `referrer`.
    return request->common_params().referrer.Clone();
  }

  // Otherwise, return the sanitized referrer saved in the NavigationRequest.
  // - For client redirects, this will be the the URL that initiated the
  // navigation. (Note: this will only be sanitized at the start, and not after
  // any redirects, including cross-origin ones. See https://crbug.com/1218786)
  // - For other navigations, this will be the referrer used after the final
  // redirect.
  return request->GetReferrer().Clone();
}

// static
// This function logs metrics about potentially isolatable sandboxed iframes
// that are tracked through calls to UpdateIsolatableSandboxedIframeTracking().
// In addition to reporting the number of potential OOPSIFs, it also reports the
// number of unique origins encountered (to give insight into potential
// behavior if a per-origin isolation model was implemented), and it counts the
// actual number of RenderProcessHosts isolating OOPSIFs using the current
// per-site isolation model.
void RenderFrameHost::LogSandboxedIframesIsolationMetrics() {
  RoutingIDIsolatableSandboxedIframesSet* oopsifs =
      g_routing_id_isolatable_sandboxed_iframes_set.Pointer();

  base::UmaHistogramCounts1000("SiteIsolation.IsolatableSandboxedIframes",
                               oopsifs->size());

  // Count the number of unique origins across all the isolatable sandboxed
  // iframes. This will give us a sense of the potential process overhead if we
  // chose a per-origin process model for isolating these frames instead of the
  // per-site model we plan to use. We use the precursor SchemeHostPort rather
  // than the url::Origin, which is always opaque in these cases.
  {
    std::set<SiteInfo> sandboxed_site_infos;
    std::set<url::SchemeHostPort> sandboxed_origins;
    for (auto rfh_global_id : *oopsifs) {
      auto* rfhi = RenderFrameHostImpl::FromID(rfh_global_id);
      DCHECK(rfhi->GetLastCommittedOrigin().opaque());
      sandboxed_origins.insert(
          rfhi->GetLastCommittedOrigin().GetTupleOrPrecursorTupleIfOpaque());
      sandboxed_site_infos.insert(rfhi->GetSiteInstance()->GetSiteInfo());
    }
    base::UmaHistogramCounts1000(
        "SiteIsolation.IsolatableSandboxedIframes.UniqueOrigins",
        sandboxed_origins.size());
    base::UmaHistogramCounts1000(
        "SiteIsolation.IsolatableSandboxedIframes.UniqueSites",
        sandboxed_site_infos.size());
  }

  // Walk the set and count the number of unique RenderProcessHosts. Using a set
  // allows us to accurately measure process overhead, including cases where
  // SiteInstances from multiple BrowsingInstances are coalesced into a single
  // RenderProcess.
  std::set<RenderProcessHost*> sandboxed_rphs;
  for (auto rfh_global_id : *oopsifs) {
    auto* rfhi = FromID(rfh_global_id);
    DCHECK(rfhi);
    auto* site_instance =
        static_cast<SiteInstanceImpl*>(rfhi->GetSiteInstance());
    DCHECK(site_instance->HasProcess());
    if (site_instance->GetSiteInfo().is_sandboxed())
      sandboxed_rphs.insert(site_instance->GetProcess());
  }
  // There should be no sandboxed RPHs if the feature isn't enabled.
  DCHECK(SiteIsolationPolicy::AreIsolatedSandboxedIframesEnabled() ||
         sandboxed_rphs.size() == 0);
  base::UmaHistogramCounts1000(
      "Memory.RenderProcessHost.Count.SandboxedIframeOverhead",
      sandboxed_rphs.size());
}

void RenderFrameHostImpl::UpdateIsolatableSandboxedIframeTracking(
    NavigationRequest* navigation_request) {
  RoutingIDIsolatableSandboxedIframesSet* oopsifs =
      g_routing_id_isolatable_sandboxed_iframes_set.Pointer();
  GlobalRenderFrameHostId global_id = GetGlobalId();

  // Check if the flags are correct.
  DCHECK(policy_container_host_);
  bool frame_is_isolatable =
      IsSandboxed(network::mojom::WebSandboxFlags::kOrigin);

  if (frame_is_isolatable) {
    // Limit the "isolatable" sandboxed frames to those that are either in the
    // same SiteInstance as their parent/opener (and thus could be isolated), or
    // that are already isolated due to sandbox flags.
    GURL url = GetLastCommittedURL();
    if (url.IsAboutBlank() || url.is_empty()) {
      frame_is_isolatable = false;
    } else {
      // Since this frame could be a main frame, we need to consider the
      // SiteInstance of either the parent or opener (if either exists) of this
      // frame, to see if the url can be placed in an OOPSIF, i.e. it's not
      // already isolated because of being cross-site.
      RenderFrameHost* frame_owner = GetParent();
      FrameTreeNode* opener = navigation_request->frame_tree_node()->opener();
      if (!frame_owner && opener)
        frame_owner = opener->current_frame_host();

      if (!frame_owner) {
        frame_is_isolatable = false;
      } else if (GetSiteInstance()->GetSiteInfo().is_sandboxed()) {
        DCHECK(frame_is_isolatable);
      } else if (frame_owner->GetSiteInstance() != GetSiteInstance()) {
        // If this host's SiteInstance isn't already marked as is_sandboxed
        // (with a frame owner), and yet the SiteInstance doesn't match that of
        // our parent/opener, then it is already isolated for some other reason
        // (cross-site, origin-keyed, etc.).
        frame_is_isolatable = false;
      }
    }
  }

  if (frame_is_isolatable)
    oopsifs->insert(global_id);
  else
    oopsifs->erase(global_id);
}

bool RenderFrameHostImpl::DidCommitNavigationInternal(
    std::unique_ptr<NavigationRequest> navigation_request,
    mojom::DidCommitProvisionalLoadParamsPtr params,
    mojom::DidCommitSameDocumentNavigationParamsPtr same_document_params) {
  const bool is_same_document_navigation = !!same_document_params;
  // Sanity-check the page transition for frame type. Fenced Frames
  // will set page transition to AUTO_SUBFRAME.
  DCHECK_EQ(ui::PageTransitionIsMainFrame(params->transition),
            !GetParent() && !IsFencedFrameRoot());
  if (navigation_request &&
      navigation_request->commit_params().navigation_token !=
          params->navigation_token) {
    // We should have the same navigation_token in CommitNavigationParams and
    // DidCommit's |params| for all navigations, because:
    // - Cross-document navigations use NavigationClient.
    // - Same-document navigations will have a null |navigation_request|
    //   here if the navigation_token doesn't match (checked in
    //   DidCommitSameDocumentNavigation).
    // TODO(https://crbug.com/1131832): Make this a CHECK instead once we're
    // sure we never hit this case.
    LogCannotCommitUrlCrashKeys(params->url, is_same_document_navigation,
                                navigation_request.get(),
                                params->origin_calculation_debug_info);
    base::debug::DumpWithoutCrashing();
  }

  // A matching NavigationRequest should have been found, unless in a few very
  // specific cases:
  // 1) This was a synchronous about:blank navigation triggered by browsing
  // context creation.
  // 2) This was a renderer-initiated same-document navigation.
  // In these cases, we will create a NavigationRequest by calling
  // CreateNavigationRequestForSynchronousRendererCommit() further down.
  // TODO(https://crbug.com/1131832): Make these navigation go through a
  // separate path that does not send
  // FrameHostMsg_DidCommitProvisionalLoad_Params at all.
  // TODO(https://crbug.com/1215096): Tighten the checks for case 1 so that only
  // the synchronous about:blank commit can actually go through (e.g. check
  // if the URL is exactly "about:blank", currently we allow any "about:" URL
  // except for "about:srcdoc").
  const bool is_synchronous_about_blank_commit =
      IsInitialSynchronousAboutBlankCommit(
          params->url, frame_tree_node_->is_on_initial_empty_document());
  if (!navigation_request && !is_synchronous_about_blank_commit &&
      !is_same_document_navigation) {
    LogCannotCommitUrlCrashKeys(params->url, is_same_document_navigation,
                                navigation_request.get(),
                                params->origin_calculation_debug_info);

    bad_message::ReceivedBadMessage(
        GetProcess(),
        bad_message::RFH_NO_MATCHING_NAVIGATION_REQUEST_ON_COMMIT);
    return false;
  }

  if (!ValidateDidCommitParams(navigation_request.get(), params.get(),
                               is_same_document_navigation)) {
    return false;
  }

  // NOTE: The navigation has committed in the renderer so it is safe
  // to pass ownership of the FrameSinkId to the new RFH.
  if (waiting_for_renderer_widget_creation_after_commit_) {
    RendererWidgetCreated();

    CHECK_NE(frame_tree_node_->current_frame_host(), this);
    auto* previous_rfh =
        frame_tree_node_->current_frame_host()->ShouldReuseCompositing(
            *GetSiteInstance())
            ? frame_tree_node_->current_frame_host()
            : nullptr;
    if (owned_render_widget_host_ && previous_rfh) {
      previous_rfh->GetRenderWidgetHost()->SetViewIsFrameSinkIdOwner(false);
      owned_render_widget_host_->SetViewIsFrameSinkIdOwner(true);
    }
  }

  // TODO(clamy): We should stop having a special case for same-document
  // navigation and just put them in the general map of NavigationRequests.
  if (navigation_request &&
      navigation_request->common_params().url != params->url &&
      is_same_document_navigation) {
    same_document_navigation_requests_[navigation_request->commit_params()
                                           .navigation_token] =
        std::move(navigation_request);
  }

  // Set is loading to true now if it has not been set yet. This happens for
  // renderer-initiated same-document navigations. It can also happen when a
  // racy DidStopLoading Mojo method resets the loading state that was set to
  // true in CommitNavigation.
  if (!is_loading()) {
    LoadingState previous_frame_tree_loading_state =
        frame_tree()->LoadingTree()->GetLoadingState();
    loading_state_ = is_same_document_navigation
                         ? LoadingState::LOADING_WITHOUT_UI
                         : LoadingState::LOADING_UI_REQUESTED;
    // TODO(https://crbug.com/1405759): Explain why this is true.
    CHECK(owner_);
    owner_->DidStartLoading(previous_frame_tree_loading_state);
  }

  if (navigation_request)
    was_discarded_ = navigation_request->commit_params().was_discarded;

  if (navigation_request) {
    // If the navigation went through the browser before committing, it's
    // possible to calculate the referrer only using information known by the
    // browser.
    // TODO(https://crbug.com/1131832): Get rid of params->referrer completely.
    params->referrer = GetReferrerForDidCommitParams(navigation_request.get());
  } else {
    // For renderer-initiated same-document navigations and the initial
    // about:blank navigation, the referrer policy shouldn't change.
    params->referrer->policy = policy_container_host_->referrer_policy();
  }

  if (!navigation_request) {
    // If there is no valid NavigationRequest corresponding to this commit,
    // create one in order to properly issue DidFinishNavigation calls to
    // WebContentsObservers.
    DCHECK(is_synchronous_about_blank_commit || is_same_document_navigation);

    // Fill the redirect chain for the NavigationRequest. Since this is only for
    // initial empty commits or same-document navigation, we should just push
    // the client-redirect URL (if it is a client redirect) and the final URL.
    std::vector<GURL> redirects;
    if (is_same_document_navigation &&
        same_document_params->is_client_redirect) {
      // If it is a same-document navigation, it might be a client redirect, in
      // which case we should put the previous URL at the front of the redirect
      // chain.
      redirects.push_back(GetLastCommittedURL());
    }
    redirects.push_back(params->url);

    // If this is a (renderer-initiated) same-document navigation, it might be
    // started by a transient activation. The only way to know this is from the
    // `started_with_transient_activation` value of `same_document_params`
    // (because we don't know anything about this navigation before DidCommit).
    bool started_with_transient_activation =
        (is_same_document_navigation &&
         same_document_params->started_with_transient_activation);

    // If this is a (renderer-initiated) same-document navigation, the renderer
    // will tell us whether the navigation should replace the current entry or
    // not. Otherwise, this must be a synchronously committed about:blank, which
    // should always do replacement.
    bool should_replace_current_entry =
        is_same_document_navigation
            ? same_document_params->should_replace_current_entry
            : true;

    // TODO(https://crbug.com/1131832): Do not use |params| to get the values,
    // depend on values known at commit time instead.
    navigation_request = CreateNavigationRequestForSynchronousRendererCommit(
        params->url, params->origin, params->initiator_base_url,
        params->referrer.Clone(), params->transition,
        should_replace_current_entry, started_with_transient_activation,
        redirects, params->url, is_same_document_navigation,
        same_document_params &&
            same_document_params->same_document_navigation_type ==
                blink::mojom::SameDocumentNavigationType::kHistoryApi);
  }

  DCHECK(navigation_request);
  DCHECK(navigation_request->IsNavigationStarted());
  VerifyThatBrowserAndRendererCalculatedDidCommitParamsMatch(
      navigation_request.get(), *params, same_document_params.Clone());

  // Update the page transition. For subframe navigations, the renderer process
  // only gives the correct page transition at commit time.
  // TODO(clamy): We should get the correct page transition when starting the
  // request.
  navigation_request->set_transition(params->transition);

  SetLastCommittedSiteInfo(navigation_request->DidEncounterError()
                               ? UrlInfo()
                               : navigation_request->GetUrlInfo());

  isolation_info_ = navigation_request->isolation_info_for_subresources();

  // Navigations in the same document and page activations do not create a new
  // document.
  bool created_new_document =
      !is_same_document_navigation && !navigation_request->IsPageActivation();

  // TODO(crbug.com/936696): Remove this after we have RenderDocument.
  // IsWaitingToCommit can be false inside DidCommitNavigationInternal only in
  // specific circumstances listed above, and specifically for the fake
  // initial navigations triggered by the blank window.open() and creating a
  // blank iframe. In that case we do not want to reset the per-document
  // states as we are not really creating a new Document and we want to
  // preserve the states set by WebContentsCreated delegate notification
  // (which among other things create tab helpers) or RenderFrameCreated.
  bool navigated_to_new_document =
      created_new_document && navigation_request->IsWaitingToCommit();

  if (navigated_to_new_document) {
    TRACE_EVENT("content", "DidCommitProvisionalLoad_StateResetForNewDocument",
                ChromeTrackEvent::kRenderFrameHost, this);

    last_committed_cross_document_navigation_id_ =
        navigation_request->GetNavigationId();

    if (ShouldResetDocumentAssociatedDataAtCommit()) {
      DCHECK_NE(lifecycle_state(), LifecycleStateImpl::kSpeculative);
      // The old Reporting API configuration is no longer valid, as a new
      // document is being loaded into the frame. Inform the network service
      // of this, so that it can send any queued reports and mark the source
      // as expired.
      GetStoragePartition()->GetNetworkContext()->SendReportsAndRemoveSource(
          GetReportingSource());

      // Clear all document-associated data for the non-pending commit
      // RenderFrameHosts because the navigation has created a new document.
      // Make sure the data doesn't get cleared for the cases when the
      // RenderFrameHost commits before the navigation commits. This happens
      // when the current RenderFrameHost crashes before navigating to a new
      // URL.
      document_associated_data_.emplace(*this,
                                        navigation_request->GetDocumentToken());
    } else {
      // Cross-RenderFrameHost navigations that commit into a speculative
      // RenderFrameHost do not create a new DocumentAssociatedData. Ensure that
      // the NavigationRequest was populated with the correct DocumentToken to
      // avoid a mismatched token between the browser and the renderer.
      CHECK_EQ(document_associated_data_->token(),
               navigation_request->GetDocumentToken());
    }

    document_associated_data_->set_devtools_navigation_token(
        navigation_request->devtools_navigation_token());

    const absl::optional<FencedFrameProperties>& fenced_frame_properties =
        navigation_request->ComputeFencedFrameProperties();
    // On navigations of fenced frame/urn iframe roots initiated within the
    // fenced frame/urn iframe tree, store document-scoped and page-scoped
    // metadata again.
    // TODO(crbug.com/1347953): Remove this metadata, and access the metadata
    // directly in the FencedFrameProperties.
    if (fenced_frame_properties &&
        (frame_tree_node()->IsFencedFrameRoot() ||
         !frame_tree_node()->IsInFencedFrameTree())) {
      if (fenced_frame_properties->nested_urn_config_pairs_.has_value()) {
        // Store nested ad components in the fenced frame's url map.
        // This may only be done after creating the DocumentAssociatedData for
        // the new document, if appropriate, since `fenced_frame_urls_map` hangs
        // off of that. In urn iframes, unlike in fenced frames, navigations of
        // the urn iframe root don't create a new Page (because the root of the
        // Page is the top-level frame). So this operation is a no-op.
        GetPage().fenced_frame_urls_map().ImportPendingAdComponents(
            fenced_frame_properties->nested_urn_config_pairs_
                ->GetValueIgnoringVisibility());
      }

      if (fenced_frame_properties->ad_auction_data_.has_value()) {
        AdAuctionDocumentData::CreateForCurrentDocument(
            this,
            fenced_frame_properties->ad_auction_data_
                ->GetValueIgnoringVisibility()
                .interest_group_owner,
            fenced_frame_properties->ad_auction_data_
                ->GetValueIgnoringVisibility()
                .interest_group_name);
      }
    }

    // Continue observing the events for the committed navigation.
    // This is intended to receive delayed IPC calls. If `navigation_request`
    // still has a valid receiver, `this` will receive delayed IPC calls from
    // the network service. When the remote interface in the network service is
    // destructed, `mojo::ReceiverSet` automatically removes the receiver.
    for (auto& receiver : navigation_request->TakeCookieObservers()) {
      cookie_observers_.Add(this, std::move(receiver));
    }
    for (auto& receiver : navigation_request->TakeTrustTokenObservers()) {
      trust_token_observers_.Add(this, std::move(receiver));
    }
    for (auto& receiver :
         navigation_request->TakeSharedDictionaryAccessObservers()) {
      shared_dictionary_observers_.Add(this, std::move(receiver));
    }

    // Resets when navigating to a new document. This is needed because
    // RenderFrameHost might be reused for a new document
    document_used_web_otp_ = false;

    // Get the UKM source id sent to the renderer.
    const ukm::SourceId document_ukm_source_id =
        navigation_request->commit_params().document_ukm_source_id;

    ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();

    // Associate the blink::Document source id to the URL. Only URLs on primary
    // main frames can be recorded.
    // TODO(crbug.com/1245014): For prerendering pages, record the source url
    // after activation.
    if (navigation_request->IsInPrimaryMainFrame() &&
        document_ukm_source_id != ukm::kInvalidSourceId) {
      ukm_recorder->UpdateSourceURL(document_ukm_source_id, params->url);
    }
    RecordDocumentCreatedUkmEvent(params->origin, document_ukm_source_id,
                                  ukm_recorder);

    // We only replace the `CookieChangeListener` with the one initialized by
    // the navigation request when navigating to a new document. Otherwise, the
    // existing `CookieChangeListener` will be reused.
    cookie_change_listener_ = navigation_request->TakeCookieChangeListener();
  }

  if (!is_same_document_navigation) {
    DCHECK_EQ(navigation_request->is_overriding_user_agent() && is_main_frame(),
              params->is_overriding_user_agent);
    if (navigation_request->IsPrerenderedPageActivation()) {
      // Set the NavigationStart time for
      // PerformanceNavigationTiming.activationStart.
      // https://wicg.github.io/nav-speculation/prerendering.html#performance-navigation-timing-extension
      GetPage().SetActivationStartTime(navigation_request->NavigationStart());
    }

  } else {
    DCHECK_EQ(is_overriding_user_agent_, params->is_overriding_user_agent);
  }

  if (is_main_frame()) {
    document_associated_data_->owned_page()->set_last_main_document_source_id(
        ukm::ConvertToSourceId(navigation_request->GetNavigationId(),
                               ukm::SourceIdType::NAVIGATION_ID));
  }

  // TODO(https://crbug.com/1131832): Do not pass |params| to DidNavigate().
  NavigationRequest* raw_navigation_request = navigation_request.get();
  raw_navigation_request->frame_tree_node()->navigator().DidNavigate(
      this, *params, std::move(navigation_request),
      is_same_document_navigation);

  // Reset back the state to false after navigation commits.
  // TODO(https://crbug.com/1072817): Undo this plumbing after removing the
  // early post-crash CommitPending() call.
  committed_speculative_rfh_before_navigation_commit_ = false;

  // Store the Commit params so they can be reused if the page is ever
  // restored from the BackForwardCache or a Prerender2 page is activated.
  last_commit_params_ = std::move(params);

  return true;
}

bool RenderFrameHostImpl::ShouldResetDocumentAssociatedDataAtCommit() const {
  return lifecycle_state() != LifecycleStateImpl::kPendingCommit &&
         !committed_speculative_rfh_before_navigation_commit_;
}

// TODO(arthursonzogni): Investigate what must be done when
// navigation_request->IsWaitingToCommit() is false here.
void RenderFrameHostImpl::DidCommitNewDocument(
    const mojom::DidCommitProvisionalLoadParams& params,
    NavigationRequest* navigation_request) {
  // Navigations in the same document and page activations do not create a new
  // document.
  DCHECK(!navigation_request->IsSameDocument());
  DCHECK(!navigation_request->IsPageActivation());

  const GURL& request_url = navigation_request->common_params().url;
  if (request_url.IsAboutBlank() || request_url.IsAboutSrcdoc()) {
    const absl::optional<::GURL>& initiator_base_url =
        navigation_request->common_params().initiator_base_url;
    SetInheritedBaseUrl(initiator_base_url ? initiator_base_url.value()
                                           : GURL::EmptyGURL());
  } else {
    SetInheritedBaseUrl(GURL::EmptyGURL());
  }

  navigation_id_ = navigation_request->GetNavigationId();

  // The nonce to use in credentialless iframe is a page scoped attribute. So it
  // needs to change every time the top-level document change.
  // TODO(https://crbug.com1287458): Once the ShadowDom implementation of
  // FencedFrame is gone, move this attribute back to PageImpl.
  credentialless_iframes_nonce_ = base::UnguessableToken::Create();

  // When the embedder navigates a fenced frame root, the navigation
  // stores a new set of fenced frame properties.
  // (Embedder-initiated fenced frame root navigation  will necessarily create
  // a new document.)
  // This must be done before `ResetPermissionsPolicy()` below, which looks up
  // the stored fenced frame properties.
  if (navigation_request->GetFencedFrameProperties()) {
    frame_tree_node()->set_fenced_frame_properties(
        navigation_request->GetFencedFrameProperties());
  }

  ResetPermissionsPolicy();

  permissions_policy_header_ = params.permissions_policy_header;
  auto isolation_info = GetSiteInstance()->GetWebExposedIsolationInfo();
  // Isolated Apps start with a base policy as defined by the permissions_policy
  // field in its Web App Manifest, which is an allowlist, and then have headers
  // further restrict the policy if applicable. This needs to be handled
  // differently than the normal permissions policy behavior, which uses a fully
  // permissive policy as its base permissions policy and accepts rules
  // specifying which permissions policy features should be blocked, aka a
  // blocklist.
  if (isolation_info.is_isolated_application() && IsOutermostMainFrame()) {
    permissions_policy_->SetHeaderPolicyForIsolatedApp(
        params.permissions_policy_header);
  } else {
    permissions_policy_->SetHeaderPolicy(params.permissions_policy_header);
  }
  document_policy_ = blink::DocumentPolicy::CreateWithHeaderPolicy({
      params.document_policy_header,  // document_policy_header
      {},                             // endpoint_map
  });

  // Since we're changing documents, we should reset the event handler
  // trackers.
  has_before_unload_handler_ = false;
  has_unload_handler_ = false;
  has_pagehide_handler_ = false;
  has_visibilitychange_handler_ = false;

  has_navigate_event_handler_ = false;

  DCHECK(params.embedding_token.has_value());
  SetEmbeddingToken(params.embedding_token.value());

  renderer_reported_bfcache_blocking_details_.clear();
  browser_reported_bfcache_disabling_features_counts_.clear();

  TakeNewDocumentPropertiesFromNavigation(navigation_request);

  // Set embedded documents' cross-origin-opener-policy from their top level:
  //  - Use top level's policy if they are same-origin or the policy is
  //    restrict-properties
  //  - Use the default policy otherwise.
  // This COOP value is not used to enforce anything on this frame, but will be
  // inherited to every local-scheme document created from them.
  // It will also be inherited by the initial empty document from its opener.
  // TODO(https://crbug.com/1442535): Always inherit COOP since it's now tied
  // to an origin that set it.

  // TODO(https://crbug.com/888079) Computing and assigning the
  // cross-origin-opener-policy of an embedded frame should be done in
  // |NavigationRequest::ComputePoliciesToCommit| , but this is not currently
  // possible because we need the origin for the computation. The linked bug
  // moves the origin computation earlier in the navigation request, which will
  // enable the move to |NavigationRequest::ComputePoliciesToCommit|.

  // TODO(https://crbug.com/1385827): See if the above is possible after we
  // bundle the COOP origin.
  if (parent_) {
    const network::CrossOriginOpenerPolicy& top_level_coop =
        GetMainFrame()->cross_origin_opener_policy();
    if (GetMainFrame()->GetLastCommittedOrigin().IsSameOriginWith(
            params.origin) ||
        network::IsRelatedToCoopRestrictProperties(top_level_coop.value) ||
        (top_level_coop.value ==
             network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone &&
         network::IsRelatedToCoopRestrictProperties(
             top_level_coop.report_only_value))) {
      policy_container_host_->set_cross_origin_opener_policy(top_level_coop);
    } else {
      policy_container_host_->set_cross_origin_opener_policy(
          network::CrossOriginOpenerPolicy());
    }
  }

  CrossOriginOpenerPolicyAccessReportManager::InstallAccessMonitorsIfNeeded(
      navigation_request->frame_tree_node());

  // Reset the salt so that media device IDs are reset for the new document
  // if necessary.
  media_device_id_salt_base_ = CreateRandomMediaDeviceIDSalt();

  accessibility_fatal_error_count_ = 0;

  UpdateIsolatableSandboxedIframeTracking(navigation_request);

  // After commit, the browser process's access of the features' state becomes
  // read-only. (i.e. It can only get feature state, not set)
  RuntimeFeatureStateDocumentData::CreateForCurrentDocument(
      this, navigation_request->GetRuntimeFeatureStateContext());
}

// TODO(arthursonzogni): Below, many NavigationRequest's objects are passed from
// the navigation to the new document. Consider grouping them in a single
// struct.
void RenderFrameHostImpl::TakeNewDocumentPropertiesFromNavigation(
    NavigationRequest* navigation_request) {
  // It should be kept in sync with the check in
  // NavigationRequest::DidCommitNavigation.
  is_error_document_ = navigation_request->DidEncounterError();
  // Overwrite reporter's reporting source with rfh's reporting source.
  std::unique_ptr<CrossOriginOpenerPolicyReporter> coop_reporter =
      navigation_request->coop_status().TakeCoopReporter();
  if (coop_reporter)
    coop_reporter->set_reporting_source(GetReportingSource());
  SetCrossOriginOpenerPolicyReporter(std::move(coop_reporter));
  virtual_browsing_context_group_ =
      navigation_request->coop_status().virtual_browsing_context_group();
  soap_by_default_virtual_browsing_context_group_ =
      navigation_request->coop_status()
          .soap_by_default_virtual_browsing_context_group();

  // Store the required CSP (it will be used by the AncestorThrottle if
  // this frame embeds a subframe when that subframe navigates).
  required_csp_ = navigation_request->TakeRequiredCSP();

  is_fenced_frame_root_originating_from_opaque_url_ =
      navigation_request
          ->is_target_fenced_frame_root_originating_from_opaque_url();

  RuntimeFeatureStateDocumentData::CreateForCurrentDocument(
      this, navigation_request->GetRuntimeFeatureStateContext());

  // TODO(https://crbug.com/888079): Once we are able to compute the origin to
  // commit in the browser, `navigation_request->commit_params().storage_key`
  // will contain the correct origin and it won't be necessary to override it
  // with `param.origin` anymore.
  const blink::StorageKey& provisional_storage_key =
      navigation_request->commit_params().storage_key;

  url::Origin origin = GetLastCommittedOrigin();
  blink::StorageKey storage_key_to_commit = CalculateStorageKey(
      origin, base::OptionalToPtr(provisional_storage_key.nonce()));
  SetStorageKey(storage_key_to_commit);

  coep_reporter_ = navigation_request->TakeCoepReporter();
  if (coep_reporter_) {
    // Set coep reporter to the document reporting source.
    coep_reporter_->set_reporting_source(GetReportingSource());
    mojo::PendingRemote<blink::mojom::ReportingObserver> remote;
    mojo::PendingReceiver<blink::mojom::ReportingObserver> receiver =
        remote.InitWithNewPipeAndPassReceiver();
    coep_reporter_->BindObserver(std::move(remote));
    // As some tests override the associated frame after commit, do not
    // call GetAssociatedLocalFrame now.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&RenderFrameHostImpl::BindReportingObserver,
                       weak_ptr_factory_.GetWeakPtr(), std::move(receiver)));
  }

  // Set the state whether this navigation is to an MHTML document, since there
  // are certain security checks that we cannot apply to subframes in MHTML
  // documents. Do not trust renderer data when determining that, rather use
  // the |navigation_request|, which was generated and stays browser side.
  is_mhtml_document_ = navigation_request->IsWaitingToCommit() &&
                       navigation_request->IsMhtmlOrSubframe();

  is_overriding_user_agent_ =
      navigation_request->is_overriding_user_agent() && is_main_frame();

  reload_type_ = navigation_request->GetReloadType();

  // Mark whether then navigation was intended as a loadDataWithBaseURL or not.
  // If |renderer_url_info_.was_loaded_from_load_data_with_base_url| is true, we
  // will bypass checks in VerifyDidCommitParams for same-document navigations
  // in the loaded document.
  renderer_url_info_.was_loaded_from_load_data_with_base_url =
      navigation_request->IsLoadDataWithBaseURL();

  // If we still have a PeakGpuMemoryTracker, then the loading it was observing
  // never completed. Cancel it's callback so that we don't report partial
  // loads to UMA.
  if (loading_mem_tracker_) {
    loading_mem_tracker_->Cancel();
  }

  // Main Frames will create the tracker, which will be triggered after we
  // receive DidStopLoading.
  loading_mem_tracker_ = navigation_request->TakePeakGpuMemoryTracker();

  early_hints_manager_ = navigation_request->TakeEarlyHintsManager();

  // If fullscreen was requested on the navigation, then set a signal to trigger
  // a fullscreen request when the document element is ready. If a previous
  // navigation was committed without a call to `MainDocumentElementAvailable`
  // before the current navigation then this value is overwritten by the current
  // navigation.
  fullscreen_document_on_document_element_ready_.reset();
  if (navigation_request->begin_params().is_fullscreen_requested) {
    fullscreen_document_on_document_element_ready_ =
        navigation_request->GetDocumentToken();
  }
  CHECK(!fullscreen_document_on_document_element_ready_ ||
        IsOutermostMainFrame());

  // Only take some properties if this is not the synchronous initial
  // `about:blank` navigation, because the values set at construction time
  // should remain unmodified.
  if (!navigation_request->IsWaitingToCommit()) {
    return;
  }

  private_network_request_policy_ =
      navigation_request->private_network_request_policy();

  reporting_endpoints_.clear();
  DCHECK(navigation_request);

  // Reporting API: If a Reporting-Endpoints header was received with this
  // document over secure connection, send it to the network service to
  // configure the endpoints in the reporting cache.
  if (GURL::SchemeIsCryptographic(origin.scheme()) &&
      navigation_request->response() &&
      navigation_request->response()->parsed_headers->reporting_endpoints) {
    GetStoragePartition()->GetNetworkContext()->SetDocumentReportingEndpoints(
        GetReportingSource(), origin, isolation_info_,
        *(navigation_request->response()->parsed_headers->reporting_endpoints));
  }

  // We move the PolicyContainerHost of |navigation_request| into the
  // RenderFrameHost unless this is the initial, "fake" navigation to
  // about:blank (because otherwise we would overwrite the PolicyContainerHost
  // of the new document, inherited at RenderFrameHost creation time, with an
  // empty PolicyContainerHost).
  SetPolicyContainerHost(navigation_request->TakePolicyContainerHost());

  if (navigation_request->response())
    last_response_head_ = navigation_request->response()->Clone();
}

void RenderFrameHostImpl::OnSameDocumentCommitProcessed(
    const base::UnguessableToken& navigation_token,
    bool should_replace_current_entry,
    blink::mojom::CommitResult result) {
  auto request = same_document_navigation_requests_.find(navigation_token);
  if (request == same_document_navigation_requests_.end()) {
    // OnSameDocumentCommitProcessed will be called after DidCommitNavigation on
    // successfull same-document commits, so |request| should already be deleted
    // by the time we got here.
    DCHECK_EQ(result, blink::mojom::CommitResult::Ok);
    return;
  }

  if (result == blink::mojom::CommitResult::RestartCrossDocument) {
    // The navigation could not be committed as a same-document navigation.
    // Restart the navigation cross-document.
    // TODO(https://crbug.com/1405759): Explain why `owner_` exists.
    CHECK(owner_);
    owner_->RestartNavigationAsCrossDocument(std::move(request->second));
    return;
  }

  DCHECK_EQ(result, blink::mojom::CommitResult::Aborted);
  // Note: if the commit was successful, the NavigationRequest is moved in
  // DidCommitSameDocumentNavigation.
  same_document_navigation_requests_.erase(navigation_token);
}

void RenderFrameHostImpl::MaybeGenerateCrashReport(
    base::TerminationStatus status,
    int exit_code) {
  if (!last_committed_url_.SchemeIsHTTPOrHTTPS())
    return;

  // Only generate reports for local root frames that are in a different
  // process than their parent.
  if (!is_main_frame() && !IsCrossProcessSubframe())
    return;
  DCHECK(is_local_root());

  // Check the termination status to see if a crash occurred (and potentially
  // determine the |reason| for the crash).
  std::string reason;
  switch (status) {
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
      break;
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
      if (exit_code == RESULT_CODE_HUNG)
        reason = "unresponsive";
      break;
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      if (exit_code == RESULT_CODE_HUNG)
        reason = "unresponsive";
      else
        return;
      break;
    case base::TERMINATION_STATUS_OOM:
#if BUILDFLAG(IS_CHROMEOS)
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
#endif
#if BUILDFLAG(IS_ANDROID)
    case base::TERMINATION_STATUS_OOM_PROTECTED:
#endif
      reason = "oom";
      break;
    default:
      // Other termination statuses do not indicate a crash.
      return;
  }

  // Construct the crash report.
  base::Value::Dict body;
  if (!reason.empty())
    body.Set("reason", reason);

  // Send the crash report to the Reporting API.
  GetProcess()->GetStoragePartition()->GetNetworkContext()->QueueReport(
      /*type=*/"crash", /*group=*/"default", last_committed_url_,
      GetReportingSource(), isolation_info_.network_anonymization_key(),
      absl::nullopt /* user_agent */, std::move(body));
}

void RenderFrameHostImpl::SendCommitNavigation(
    mojom::NavigationClient* navigation_client,
    NavigationRequest* navigation_request,
    blink::mojom::CommonNavigationParamsPtr common_params,
    blink::mojom::CommitNavigationParamsPtr commit_params,
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories,
    absl::optional<std::vector<blink::mojom::TransferrableURLLoaderPtr>>
        subresource_overrides,
    blink::mojom::ControllerServiceWorkerInfoPtr controller,
    blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        subresource_proxying_loader_factory,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        keep_alive_loader_factory,
    mojo::PendingAssociatedRemote<blink::mojom::FetchLaterLoaderFactory>
        fetch_later_loader_factory,
    mojo::PendingRemote<blink::mojom::ResourceCache> resource_cache_remote,
    const absl::optional<blink::ParsedPermissionsPolicy>& permissions_policy,
    blink::mojom::PolicyContainerPtr policy_container,
    const blink::DocumentToken& document_token,
    const base::UnguessableToken& devtools_navigation_token) {
  TRACE_EVENT0("navigation", "RenderFrameHostImpl::SendCommitNavigation");
  base::ElapsedTimer timer;
  DCHECK_EQ(net::OK, navigation_request->GetNetErrorCode());
  // `origin_to_commit` is currently only set only on failed navigations or
  // data: URL navigations.
  DCHECK(!commit_params->origin_to_commit ||
         common_params->url.SchemeIs(url::kDataScheme));
  IncreaseCommitNavigationCounter();
  mojo::PendingRemote<blink::mojom::CodeCacheHost> code_cache_host;
  mojom::CookieManagerInfoPtr cookie_manager_info;
  mojom::StorageInfoPtr storage_info;

  // Until the browser is able to compute the origin accurately in all cases
  // (see https://crbug.com/888079), this is actually just a provisional
  // `storage_key`. The final storage key is computed by the document loader
  // taking into account the origin computed by the renderer.
  auto& code_cache_storage_key = commit_params->storage_key;
  CreateCodeCacheHostWithKeys(
      code_cache_host.InitWithNewPipeAndPassReceiver(),
      navigation_request->isolation_info_for_subresources()
          .network_isolation_key(),
      code_cache_storage_key);

  url::Origin origin_to_commit =
      navigation_request->GetOriginToCommit().value();
  // Make sure the origin of the isolation info and origin to commit match,
  // otherwise the cookie manager will crash. Sending the cookie manager here
  // is just an optimization, so it is fine for it to be null in the case
  // where these don't match.

  if (common_params->url.SchemeIsHTTPOrHTTPS() && !origin_to_commit.opaque() &&
      navigation_request->isolation_info_for_subresources()
              .frame_origin()
              .value() == origin_to_commit) {
    cookie_manager_info = mojom::CookieManagerInfo::New();
    cookie_manager_info->origin = origin_to_commit;
    auto subresource_loader_factories_config =
        SubresourceLoaderFactoriesConfig::ForPendingNavigation(
            *navigation_request);

    BindRestrictedCookieManagerWithOrigin(
        cookie_manager_info->cookie_manager.InitWithNewPipeAndPassReceiver(),
        navigation_request->isolation_info_for_subresources(), origin_to_commit,
        subresource_loader_factories_config.cookie_setting_overrides());

    // Some tests need the StorageArea interfaces to come through DomStorage,
    // so ignore the optimizations in those cases.
    if (!RenderProcessHostImpl::HasDomStorageBinderForTesting()) {
      storage_info = mojom::StorageInfo::New();
      // Bind local storage and session storage areas.
      auto* partition = GetStoragePartition();
      int process_id = GetProcess()->GetID();
      partition->OpenLocalStorageForProcess(
          process_id, commit_params->storage_key,
          storage_info->local_storage_area.InitWithNewPipeAndPassReceiver());

      // Session storage must match the default namespace.
      const std::string& namespace_id =
          navigation_request->frame_tree_node()
              ->frame_tree()
              .controller()
              .GetSessionStorageNamespace(
                  GetSiteInstance()->GetStoragePartitionConfig())
              ->id();
      partition->BindSessionStorageAreaForProcess(
          process_id,
          navigation_request->frame_tree_node()
              ->frame_tree()
              .GetSessionStorageKey(commit_params->storage_key),
          namespace_id,
          storage_info->session_storage_area.InitWithNewPipeAndPassReceiver());
    }
  }

  // Save the last sent NotRestoredReasons value for testing, so that we can
  // verify them in tests.
  // TODO(yuzus): Remove |not_restored_reasons_for_testing_| and modify
  // |FrameNavigateParamsCapturer|.
  not_restored_reasons_for_testing_ =
      commit_params->not_restored_reasons.Clone();

  // If an automatic "top_navigation" beacon is registered in the FencedFrame
  // of the document initiator of the navigation, and the navigation
  // destination is an outermost main frame, send the beacon. We do this at
  // this point because:
  // 1. We need a handle to the initiator.
  // 2. The initiator hasn't been unloaded yet due to this navigation, and
  //    still exists at this point (unless explicitly removed from the DOM
  //    otherwise).
  MaybeSendFencedFrameAutomaticReportingBeacon(
      *navigation_request,
      blink::mojom::AutomaticBeaconType::kDeprecatedTopNavigation);
  MaybeSendFencedFrameAutomaticReportingBeacon(
      *navigation_request,
      blink::mojom::AutomaticBeaconType::kTopNavigationCommit);

  // If this commit is for a main frame in another browsing context group, warn
  // the renderer that it should update the browsing context group information
  // of the page if this frame successfully commits. Note that the
  // BrowsingContextGroupInfo in the params should only be populated at commit
  // time, and only in the case of a swap.
  CHECK(!commit_params->browsing_context_group_info.has_value());
  if (is_main_frame() &&
      navigation_request->browsing_context_group_swap().ShouldSwap()) {
    commit_params->browsing_context_group_info =
        blink::BrowsingContextGroupInfo(
            GetSiteInstance()->browsing_instance_token(),
            GetSiteInstance()->coop_related_group_token());
  }

  auto* cookie_deprecation_label_manager =
      static_cast<CookieDeprecationLabelManagerImpl*>(
          GetStoragePartition()->GetCookieDeprecationLabelManager());
  if (cookie_deprecation_label_manager) {
    url::Origin top_frame_origin =
        is_main_frame() ? origin_to_commit
                        : GetMainFrame()->GetLastCommittedOrigin();
    commit_params->cookie_deprecation_label =
        cookie_deprecation_label_manager->GetValue(top_frame_origin,
                                                   origin_to_commit);
  }

  commit_params->commit_sent = base::TimeTicks::Now();
  navigation_client->CommitNavigation(
      std::move(common_params), std::move(commit_params),
      std::move(response_head), std::move(response_body),
      std::move(url_loader_client_endpoints),
      std::move(subresource_loader_factories), std::move(subresource_overrides),
      std::move(controller), std::move(container_info),
      std::move(subresource_proxying_loader_factory),
      std::move(keep_alive_loader_factory),
      std::move(fetch_later_loader_factory), document_token,
      devtools_navigation_token, permissions_policy,
      std::move(policy_container), std::move(code_cache_host),
      std::move(resource_cache_remote), std::move(cookie_manager_info),
      std::move(storage_info),
      BuildCommitNavigationCallback(navigation_request));
  base::UmaHistogramTimes(
      base::StrCat({"Navigation.SendCommitNavigationTime.",
                    IsOutermostMainFrame() ? "MainFrame" : "Subframe"}),
      timer.Elapsed());
}

void RenderFrameHostImpl::SendCommitFailedNavigation(
    mojom::NavigationClient* navigation_client,
    NavigationRequest* navigation_request,
    blink::mojom::CommonNavigationParamsPtr common_params,
    blink::mojom::CommitNavigationParamsPtr commit_params,
    bool has_stale_copy_in_cache,
    int32_t error_code,
    int32_t extended_error_code,
    const absl::optional<std::string>& error_page_content,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories,
    const blink::DocumentToken& document_token,
    blink::mojom::PolicyContainerPtr policy_container) {
  // `origin_to_commit` must be set on failed navigations.
  DCHECK(commit_params->origin_to_commit);
  DCHECK(navigation_client && navigation_request);
  DCHECK_NE(GURL(), common_params->url);
  DCHECK_NE(net::OK, error_code);
  IncreaseCommitNavigationCounter();

  // If this commit is for a main frame in another browsing context group, warn
  // the renderer that it should update the browsing context group information
  // of the page. Note that the BrowsingContextGroupInfo in the params should
  // only be populated at commit time, and only in the case of a swap.
  CHECK(!commit_params->browsing_context_group_info.has_value());
  if (is_main_frame() &&
      navigation_request->browsing_context_group_swap().ShouldSwap()) {
    commit_params->browsing_context_group_info =
        blink::BrowsingContextGroupInfo(
            GetSiteInstance()->browsing_instance_token(),
            GetSiteInstance()->coop_related_group_token());
  }

  navigation_client->CommitFailedNavigation(
      std::move(common_params), std::move(commit_params),
      has_stale_copy_in_cache, error_code, extended_error_code,
      navigation_request->GetResolveErrorInfo(), error_page_content,
      std::move(subresource_loader_factories), document_token,
      std::move(policy_container),
      GetContentClient()->browser()->GetAlternativeErrorPageOverrideInfo(
          navigation_request->GetURL(), this, GetBrowserContext(), error_code),
      BuildCommitFailedNavigationCallback(navigation_request));
}

// Called when the renderer navigates. For every frame loaded, we'll get this
// notification containing parameters identifying the navigation.
void RenderFrameHostImpl::DidCommitNavigation(
    NavigationRequest* committing_navigation_request,
    mojom::DidCommitProvisionalLoadParamsPtr params,
    mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params) {
  DCHECK(params);

  // BackForwardCacheImpl::CanStoreRenderFrameHost prevents placing the pages
  // with in-flight navigation requests in the back-forward cache and it's not
  // possible to start/commit a new one after the RenderFrameHost is in the
  // BackForwardCache (see the check IsInactiveAndDisallowActivation in
  // RFH::DidCommitSameDocumentNavigation() and RFH::BeginNavigation()) so it
  // isn't possible to get a DidCommitNavigation IPC from the renderer in
  // kInBackForwardCache state.
  DCHECK(!IsInBackForwardCache());

  std::unique_ptr<NavigationRequest> request;
  // TODO(https://crbug.com/778318): a `committing_navigation_request` is not
  // present if and only if this is a synchronous re-navigation to about:blank
  // initiated by Blink. In all other cases it should be non-null and present in
  // the map of NavigationRequests.
  if (committing_navigation_request) {
    committing_navigation_request->IgnoreCommitInterfaceDisconnection();
    if (!MaybeInterceptCommitCallback(committing_navigation_request, &params,
                                      &interface_params)) {
      return;
    }

    auto find_request =
        navigation_requests_.find(committing_navigation_request);
    CHECK(find_request != navigation_requests_.end());

    request = std::move(find_request->second);
    navigation_requests_.erase(committing_navigation_request);

    if (request->IsNavigationStarted()) {
      main_frame_request_ids_ = {params->request_id,
                                 request->GetGlobalRequestID()};
      if (deferred_main_frame_load_info_)
        ResourceLoadComplete(std::move(deferred_main_frame_load_info_));
    }
  }

  // The commit IPC should be associated with the URL being committed (not with
  // the *last* committed URL that most other IPCs are associated with).
  // TODO(crbug.com/1179502): Investigate where the origin should come from when
  // we remove FrameTree/FrameTreeNode members of this class, and the last
  // committed origin may be incorrect.
  ScopedActiveURL scoped_active_url(params->url,
                                    frame_tree()->root()->current_origin());

  ScopedCommitStateResetter commit_state_resetter(this);
  RenderProcessHost* process = GetProcess();

  TRACE_EVENT("navigation", "RenderFrameHostImpl::DidCommitProvisionalLoad",
              ChromeTrackEvent::kRenderFrameHost, this, "params", params);

  // If we're waiting for a cross-site beforeunload completion callback from
  // this renderer and we receive a Navigate message from the main frame, then
  // the renderer was navigating already and sent it before hearing the
  // blink::mojom::LocalFrame::StopLoading() message. Treat this as an implicit
  // beforeunload completion callback to allow the pending navigation to
  // continue.
  if (is_waiting_for_beforeunload_completion_ &&
      unload_ack_is_for_navigation_ && !GetParent()) {
    base::TimeTicks approx_renderer_start_time = send_before_unload_start_time_;
    ProcessBeforeUnloadCompleted(
        /*proceed=*/true, /*treat_as_final_completion_callback=*/true,
        approx_renderer_start_time, base::TimeTicks::Now(),
        /*for_legacy=*/false);
  }

  // When a frame enters pending deletion, it waits for itself and its children
  // to properly unload. Receiving DidCommitProvisionalLoad() here while the
  // frame is not active means it comes from a navigation that reached the
  // ReadyToCommit stage just before the frame entered pending deletion.
  //
  // We should ignore this message, because we have already committed to
  // destroying this RenderFrameHost. Note that we intentionally do not ignore
  // commits that happen while the current tab is being closed - see
  // https://crbug.com/805705.
  if (IsPendingDeletion())
    return;

  if (interface_params) {
    if (broker_receiver_.is_bound()) {
      broker_receiver_.reset();
    }
    BindBrowserInterfaceBrokerReceiver(
        std::move(interface_params->browser_interface_broker_receiver));
  } else {
    // If the frame is no longer on the initial empty document, and this is not
    // a same-document navigation, then both the active document as well as the
    // global object was replaced in this browsing context. The RenderFrame
    // should have rebound its BrowserInterfaceBroker to a new pipe, but failed
    // to do so. Kill the renderer, and reset the old receiver to ensure that
    // any pending interface requests originating from the previous document,
    // hence possibly from a different security origin, will no longer be
    // dispatched.
    if (!frame_tree_node_->is_on_initial_empty_document()) {
      broker_receiver_.reset();
      bad_message::ReceivedBadMessage(
          process, bad_message::RFH_INTERFACE_PROVIDER_MISSING);
      return;
    }

    // Otherwise, it is the first real load committed, for which the RenderFrame
    // is allowed to, and will re-use the existing BrowserInterfaceBroker
    // connection if the new document is same-origin with the initial empty
    // document, and therefore the global object is not replaced.
  }

  if (!DidCommitNavigationInternal(std::move(request), std::move(params),
                                   /*same_document_params=*/nullptr)) {
    return;
  }

  // Since we didn't early return, it's safe to keep the commit state.
  commit_state_resetter.disable();

  // For a top-level frame, there are potential security concerns associated
  // with displaying graphics from a previously loaded page after the URL in
  // the omnibar has been changed. It is unappealing to clear the page
  // immediately, but if the renderer is taking a long time to issue any
  // compositor output (possibly because of script deliberately creating this
  // situation) then we clear it after a while anyway.
  // See https://crbug.com/497588.
  if (is_main_frame() && GetView()) {
    RenderWidgetHostImpl::From(GetView()->GetRenderWidgetHost())->DidNavigate();
  }

  // TODO(arthursonzogni): This can be removed when RenderDocument will be
  // implemented. See https://crbug.com/936696.
  EnsureDescendantsAreUnloading();
}

mojom::NavigationClient::CommitNavigationCallback
RenderFrameHostImpl::BuildCommitNavigationCallback(
    NavigationRequest* navigation_request) {
  DCHECK(navigation_request);
  return base::BindOnce(&RenderFrameHostImpl::DidCommitNavigation,
                        base::Unretained(this), navigation_request);
}

mojom::NavigationClient::CommitFailedNavigationCallback
RenderFrameHostImpl::BuildCommitFailedNavigationCallback(
    NavigationRequest* navigation_request) {
  DCHECK(navigation_request);
  return base::BindOnce(&RenderFrameHostImpl::DidCommitNavigation,
                        base::Unretained(this), navigation_request);
}

void RenderFrameHostImpl::SendBeforeUnload(
    bool is_reload,
    base::WeakPtr<RenderFrameHostImpl> rfh,
    bool for_legacy) {
  auto before_unload_closure = base::BindOnce(
      [](base::WeakPtr<RenderFrameHostImpl> impl, bool for_legacy, bool proceed,
         base::TimeTicks renderer_before_unload_start_time,
         base::TimeTicks renderer_before_unload_end_time) {
        if (!impl)
          return;
        impl->ProcessBeforeUnloadCompleted(
            proceed, /*treat_as_final_completion_callback=*/false,
            renderer_before_unload_start_time, renderer_before_unload_end_time,
            for_legacy);
      },
      rfh, for_legacy);
  if (for_legacy) {
    if (frame_tree_node_->navigation_request()) {
      base::UmaHistogramTimes(
          "Navigation.NavigationStartToBeforeUnloadForLegacy",
          base::TimeTicks::Now() - frame_tree_node_->navigation_request()
                                       ->common_params()
                                       .navigation_start);
    }
    // Use a high-priority task to continue the navigation. This is safe as it
    // happens early in the navigation flow and shouldn't race with any other
    // tasks associated with this navigation.
    GetUIThreadTaskRunner({BrowserTaskType::kBeforeUnloadBrowserResponse})
        ->PostTask(
            FROM_HERE,
            base::BindOnce(
                [](blink::mojom::LocalFrame::BeforeUnloadCallback callback,
                   base::TimeTicks start_time, base::TimeTicks end_time) {
                  std::move(callback).Run(/*proceed=*/true, start_time,
                                          end_time);
                },
                std::move(before_unload_closure),
                send_before_unload_start_time_, base::TimeTicks::Now()));
    return;
  }
  // Experiment to run beforeunload handlers at a higher priority in the
  // renderer. See crubug.com/1042118.
  if (base::FeatureList::IsEnabled(features::kHighPriorityBeforeUnload)) {
    rfh->GetHighPriorityLocalFrame()->DispatchBeforeUnload(
        is_reload, std::move(before_unload_closure));
  } else {
    rfh->GetAssociatedLocalFrame()->BeforeUnload(
        is_reload, std::move(before_unload_closure));
  }
}

void RenderFrameHostImpl::AddServiceWorkerContainerHost(
    const std::string& uuid,
    base::WeakPtr<content::ServiceWorkerContainerHost> host) {
  if (IsInBackForwardCache()) {
    // RenderFrameHost entered BackForwardCache before adding
    // ServiceWorkerContainerHost. In this case, evict the entry from the cache.
    EvictFromBackForwardCacheWithReason(
        BackForwardCacheMetrics::NotRestoredReason::
            kEnteredBackForwardCacheBeforeServiceWorkerHostAdded);
  }
  DCHECK(!base::Contains(service_worker_container_hosts_, uuid));
  last_committed_service_worker_host_ = host;
  service_worker_container_hosts_[uuid] = std::move(host);
}

void RenderFrameHostImpl::RemoveServiceWorkerContainerHost(
    const std::string& uuid) {
  DCHECK(!service_worker_container_hosts_.empty());
  DCHECK(base::Contains(service_worker_container_hosts_, uuid));
  service_worker_container_hosts_.erase(uuid);
}

base::WeakPtr<ServiceWorkerContainerHost>
RenderFrameHostImpl::GetLastCommittedServiceWorkerHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return last_committed_service_worker_host_;
}

bool RenderFrameHostImpl::MaybeInterceptCommitCallback(
    NavigationRequest* navigation_request,
    mojom::DidCommitProvisionalLoadParamsPtr* params,
    mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params) {
  if (commit_callback_interceptor_) {
    return commit_callback_interceptor_->WillProcessDidCommitNavigation(
        navigation_request, params, interface_params);
  }
  return true;
}

void RenderFrameHostImpl::PostMessageEvent(
    const absl::optional<blink::RemoteFrameToken>& source_token,
    const std::u16string& source_origin,
    const std::u16string& target_origin,
    blink::TransferableMessage message) {
  DCHECK(is_render_frame_created());

  if (message.delegated_capability != blink::mojom::DelegatedCapability::kNone)
    ReceivedDelegatedCapability(message.delegated_capability);

  // This is always called from either another renderer (through RemoteFrame) or
  // from the embedder itself. As such, we nullify the parent task ID here, to
  // prevent this information from leaking between renderers.
  message.parent_task_id = absl::nullopt;

  GetAssociatedLocalFrame()->PostMessageEvent(
      source_token, source_origin, target_origin, std::move(message));
}

bool RenderFrameHostImpl::IsTestRenderFrameHost() const {
  return false;
}

scoped_refptr<PrefetchedSignedExchangeCache>
RenderFrameHostImpl::EnsurePrefetchedSignedExchangeCache() {
  if (!prefetched_signed_exchange_cache_) {
    prefetched_signed_exchange_cache_ =
        base::MakeRefCounted<PrefetchedSignedExchangeCache>();
  }
  return prefetched_signed_exchange_cache_;
}

void RenderFrameHostImpl::ClearPrefetchedSignedExchangeCache() {
  if (prefetched_signed_exchange_cache_)
    prefetched_signed_exchange_cache_->Clear();
}

RenderWidgetHostImpl* RenderFrameHostImpl::GetLocalRenderWidgetHost() const {
  if (is_main_frame())
    return render_view_host_->GetWidget();
  else
    return owned_render_widget_host_.get();
}

void RenderFrameHostImpl::EnsureDescendantsAreUnloading() {
  std::vector<FrameTreeNode*> frame_to_remove;
  std::vector<RenderFrameHostImpl*> rfh_to_be_checked = {this};
  while (!rfh_to_be_checked.empty()) {
    RenderFrameHostImpl* document = rfh_to_be_checked.back();
    rfh_to_be_checked.pop_back();

    // Every child is expected to be pending deletion. If it isn't the case,
    // their FrameTreeNode is immediately removed from the tree.
    for (auto& iframe : document->children_) {
      if (iframe->current_frame_host()->IsPendingDeletion()) {
        rfh_to_be_checked.push_back(iframe->current_frame_host());
      } else {
        frame_to_remove.push_back(iframe.get());
      }
    }
  }
  for (FrameTreeNode* child : frame_to_remove) {
    child->parent()->RemoveChild(child);
  }
}

void RenderFrameHostImpl::AddMessageToConsoleImpl(
    blink::mojom::ConsoleMessageLevel level,
    const std::string& message,
    bool discard_duplicates) {
  GetAssociatedLocalFrame()->AddMessageToConsole(level, message,
                                                 discard_duplicates);
}

void RenderFrameHostImpl::LogCannotCommitUrlCrashKeys(
    const GURL& url,
    bool is_same_document_navigation,
    NavigationRequest* navigation_request,
    std::string& origin_calculation_debug_info) {
  LogRendererKillCrashKeys(GetSiteInstance()->GetSiteInfo());

  // Temporary instrumentation to debug the root cause of renderer process
  // terminations. See https://crbug.com/931895.
  auto bool_to_crash_key = [](bool b) { return b ? "true" : "false"; };

  static auto* const navigation_url_key = base::debug::AllocateCrashKeyString(
      "navigation_url", base::debug::CrashKeySize::Size256);
  base::debug::SetCrashKeyString(navigation_url_key, url.spec());

  static auto* const is_same_document_key = base::debug::AllocateCrashKeyString(
      "is_same_document", base::debug::CrashKeySize::Size32);
  base::debug::SetCrashKeyString(
      is_same_document_key, bool_to_crash_key(is_same_document_navigation));

  static auto* const is_main_frame_key = base::debug::AllocateCrashKeyString(
      "is_main_frame", base::debug::CrashKeySize::Size32);
  base::debug::SetCrashKeyString(is_main_frame_key,
                                 bool_to_crash_key(is_main_frame()));

  static auto* const is_outermost_frame_key =
      base::debug::AllocateCrashKeyString("is_outermost_frame",
                                          base::debug::CrashKeySize::Size32);
  base::debug::SetCrashKeyString(is_outermost_frame_key,
                                 bool_to_crash_key(IsOutermostMainFrame()));

  static auto* const is_cross_process_subframe_key =
      base::debug::AllocateCrashKeyString("is_cross_process_subframe",
                                          base::debug::CrashKeySize::Size32);
  base::debug::SetCrashKeyString(is_cross_process_subframe_key,
                                 bool_to_crash_key(IsCrossProcessSubframe()));

  static auto* const is_local_root_key = base::debug::AllocateCrashKeyString(
      "is_local_root", base::debug::CrashKeySize::Size32);
  base::debug::SetCrashKeyString(is_local_root_key,
                                 bool_to_crash_key(is_local_root()));

  static auto* const site_lock_key = base::debug::AllocateCrashKeyString(
      "site_lock", base::debug::CrashKeySize::Size256);
  base::debug::SetCrashKeyString(
      site_lock_key,
      ProcessLock::FromSiteInfo(GetSiteInstance()->GetSiteInfo()).ToString());

  static auto* const process_lock_key = base::debug::AllocateCrashKeyString(
      "process_lock", base::debug::CrashKeySize::Size256);
  base::debug::SetCrashKeyString(process_lock_key,
                                 GetProcess()->GetProcessLock().ToString());

  if (!GetSiteInstance()->IsDefaultSiteInstance()) {
    static auto* const original_url_origin_key =
        base::debug::AllocateCrashKeyString("original_url_origin",
                                            base::debug::CrashKeySize::Size256);
    base::debug::SetCrashKeyString(
        original_url_origin_key,
        GetSiteInstance()->original_url().DeprecatedGetOriginAsURL().spec());
  }

  static auto* const is_mhtml_document_key =
      base::debug::AllocateCrashKeyString("is_mhtml_document",
                                          base::debug::CrashKeySize::Size32);
  base::debug::SetCrashKeyString(is_mhtml_document_key,
                                 bool_to_crash_key(is_mhtml_document()));

  static auto* const last_committed_url_origin_key =
      base::debug::AllocateCrashKeyString("last_committed_url_origin",
                                          base::debug::CrashKeySize::Size256);
  base::debug::SetCrashKeyString(
      last_committed_url_origin_key,
      GetLastCommittedURL().DeprecatedGetOriginAsURL().spec());

  static auto* const last_successful_url_origin_key =
      base::debug::AllocateCrashKeyString("last_successful_url_origin",
                                          base::debug::CrashKeySize::Size256);
  base::debug::SetCrashKeyString(
      last_successful_url_origin_key,
      last_successful_url().DeprecatedGetOriginAsURL().spec());

  static auto* const is_on_initial_empty_document_key =
      base::debug::AllocateCrashKeyString("is_on_initial_empty_doc",
                                          base::debug::CrashKeySize::Size32);
  base::debug::SetCrashKeyString(
      is_on_initial_empty_document_key,
      bool_to_crash_key(frame_tree_node_->is_on_initial_empty_document()));

  static auto* const origin_calculation_debug_info_key =
      base::debug::AllocateCrashKeyString("origin_calculation_debug_info",
                                          base::debug::CrashKeySize::Size256);
  base::debug::SetCrashKeyString(origin_calculation_debug_info_key,
                                 origin_calculation_debug_info);

  if (navigation_request && navigation_request->IsNavigationStarted()) {
    static auto* const is_renderer_initiated_key =
        base::debug::AllocateCrashKeyString("is_renderer_initiated",
                                            base::debug::CrashKeySize::Size32);
    base::debug::SetCrashKeyString(
        is_renderer_initiated_key,
        bool_to_crash_key(navigation_request->IsRendererInitiated()));

    static auto* const is_server_redirect_key =
        base::debug::AllocateCrashKeyString("is_server_redirect",
                                            base::debug::CrashKeySize::Size32);
    base::debug::SetCrashKeyString(
        is_server_redirect_key,
        bool_to_crash_key(navigation_request->WasServerRedirect()));

    static auto* const is_form_submission_key =
        base::debug::AllocateCrashKeyString("is_form_submission",
                                            base::debug::CrashKeySize::Size32);
    base::debug::SetCrashKeyString(
        is_form_submission_key,
        bool_to_crash_key(navigation_request->IsFormSubmission()));

    static auto* const is_error_page_key = base::debug::AllocateCrashKeyString(
        "is_error_page", base::debug::CrashKeySize::Size32);
    base::debug::SetCrashKeyString(
        is_error_page_key,
        bool_to_crash_key(navigation_request->IsErrorPage()));

    static auto* const from_begin_navigation_key =
        base::debug::AllocateCrashKeyString("from_begin_navigation",
                                            base::debug::CrashKeySize::Size32);
    base::debug::SetCrashKeyString(
        from_begin_navigation_key,
        bool_to_crash_key(navigation_request->from_begin_navigation()));

    static auto* const net_error_key = base::debug::AllocateCrashKeyString(
        "net_error", base::debug::CrashKeySize::Size32);
    base::debug::SetCrashKeyString(
        net_error_key,
        base::NumberToString(navigation_request->GetNetErrorCode()));

    static auto* const initiator_origin_key =
        base::debug::AllocateCrashKeyString("initiator_origin",
                                            base::debug::CrashKeySize::Size64);
    base::debug::SetCrashKeyString(
        initiator_origin_key,
        navigation_request->GetInitiatorOrigin()
            ? navigation_request->GetInitiatorOrigin()->GetDebugString()
            : "none");

    static auto* const starting_site_instance_key =
        base::debug::AllocateCrashKeyString("starting_site_instance",
                                            base::debug::CrashKeySize::Size256);
    base::debug::SetCrashKeyString(starting_site_instance_key,
                                   navigation_request->GetStartingSiteInstance()
                                       ->GetSiteInfo()
                                       .GetDebugString());

    // Recompute the target SiteInstance to see if it matches the current
    // one at commit time.
    BrowsingContextGroupSwap ignored_bcg_swap_info =
        BrowsingContextGroupSwap::CreateDefault();
    scoped_refptr<SiteInstance> dest_instance =
        navigation_request->frame_tree_node()
            ->render_manager()
            ->GetSiteInstanceForNavigationRequest(navigation_request,
                                                  &ignored_bcg_swap_info);
    static auto* const does_recomputed_site_instance_match_key =
        base::debug::AllocateCrashKeyString(
            "does_recomputed_site_instance_match",
            base::debug::CrashKeySize::Size32);
    base::debug::SetCrashKeyString(
        does_recomputed_site_instance_match_key,
        bool_to_crash_key(dest_instance == GetSiteInstance()));
  }
}

int64_t CalculatePostID(
    const std::string& method,
    const scoped_refptr<network::ResourceRequestBody>& request_body,
    int64_t last_post_id,
    bool is_same_document) {
  if (method != "POST")
    return -1;
  // On same-document navigations that keep the "POST" method, use the POST ID
  // from the last navigation.
  if (is_same_document)
    return last_post_id;
  // Otherwise, this is a cross-document navigation. Use the POST ID from the
  // navigation request.
  return request_body ? request_body->identifier() : -1;
}

const std::string CalculateMethod(
    const std::string& nav_request_method,
    const std::string& last_http_method,
    bool is_same_document,
    bool is_same_document_history_api_navigation) {
  DCHECK(is_same_document || !is_same_document_history_api_navigation);
  // History API navigations are always "GET" navigations. See spec:
  // https://html.spec.whatwg.org/multipage/history.html#url-and-history-update-steps
  if (is_same_document_history_api_navigation)
    return "GET";
  // If this is a same-document navigation that isn't triggered by the history
  // API, we should preserve the HTTP method used by the last navigation.
  if (is_same_document)
    return last_http_method;
  // Otherwise, this is a cross-document navigation. Use the method specified in
  // the navigation request.
  return nav_request_method;
}

int CalculateHTTPStatusCode(NavigationRequest* request,
                            int last_http_status_code) {
  // Same-document navigations or prerender activation navigation should retain
  // the HTTP status code from the last committed navigation.
  if (request->IsSameDocument() || request->IsPrerenderedPageActivation())
    return last_http_status_code;

  // Navigations that are served from the back/forward cache will always have
  // the HTTP status code set to 200.
  if (request->IsServedFromBackForwardCache())
    return 200;

  // The HTTP status code is not set if we never received any HTTP response for
  // the navigation.
  const int request_response_code = request->commit_params().http_response_code;
  if (request_response_code == -1)
    return 0;
  // Otherwise, return the status code from |request|.
  return request_response_code;
}

// Tries to simulate WebFrameLoadType in NavigationTypeToLoadType() in
// render_frame_impl.cc and RenderFrameImpl::CommitFailedNavigation().
// TODO(https://crbug.com/1131832): This should only be here temporarily.
// Remove this once the renderer behavior at commit time is more consistent with
// what the browser instructed it to do (e.g. reloads will always be classified
// as kReload).
RendererLoadType CalculateRendererLoadType(NavigationRequest* request,
                                           bool should_replace_current_entry,
                                           const GURL& previous_document_url) {
  const bool is_history =
      NavigationTypeUtils::IsHistory(request->common_params().navigation_type);
  const bool is_reload =
      NavigationTypeUtils::IsReload(request->common_params().navigation_type);
  const bool has_valid_page_state = blink::PageState::CreateFromEncodedData(
                                        request->commit_params().page_state)
                                        .IsValid();
  const bool is_error_document = request->DidEncounterError();

  // Predict if the renderer classified the navigation as a "back/forward"
  // navigation (WebFrameLoadType::kBackForward).
  bool will_be_classified_as_back_forward_navigation = false;
  if (is_error_document) {
    // For error documents, whenever the navigation has a valid PageState, it
    // will be considered as a back/forward navigation. This includes history
    // navigations and restores. See RenderFrameImpl's CommitFailedNavigation().
    will_be_classified_as_back_forward_navigation = has_valid_page_state;
  } else {
    // For normal navigations, RenderFrameImpl's NavigationTypeToLoadType()
    // will classify a navigation as kBackForward if it's a history navigation.
    will_be_classified_as_back_forward_navigation = is_history;
  }

  if (will_be_classified_as_back_forward_navigation) {
    // If the navigation is classified as kBackForward, it can't be changed to
    // another RendererLoadType below, so we can immediately return here.
    return RendererLoadType::kBackForward;
  }

  if (!is_error_document && is_reload) {
    // For non-error documents, if the NavigationType given by the browser is
    // a reload, then the navigation will be classified as a reload.
    return RendererLoadType::kReload;
  }

  return should_replace_current_entry ? RendererLoadType::kReplaceCurrentItem
                                      : RendererLoadType::kStandard;
}

bool CalculateDidCreateNewEntry(NavigationRequest* request,
                                bool should_replace_current_entry,
                                RendererLoadType renderer_load_type) {
  // This function tries to simulate the calculation of |did_create_new_entry|
  // in RenderFrameImpl::MakeDidCommitProvisionalLoadParams().
  // Standard navigations will always create a new entry.
  if (renderer_load_type == RendererLoadType::kStandard)
    return true;

  // Back/Forward navigations won't create a new entry.
  if (renderer_load_type == RendererLoadType::kBackForward)
    return false;

  // Otherwise, |did_create_new_entry| is true only for main frame
  // cross-document replacements.
  return request->IsInMainFrame() && should_replace_current_entry &&
         !request->IsSameDocument();
}

ui::PageTransition CalculateTransition(
    NavigationRequest* request,
    RendererLoadType renderer_load_type,
    const mojom::DidCommitProvisionalLoadParams& params,
    bool is_in_fenced_frame_tree) {
  if (is_in_fenced_frame_tree) {
    // Navigations inside fenced frame trees do not add session history items
    // and must be marked with PAGE_TRANSITION_AUTO_SUBFRAME. This is set
    // regardless of the `is_main_frame` value since this is inside a fenced
    // frame tree and should behave the same as iframes. Also preserve client
    // redirects if they were set.
    ui::PageTransition transition = ui::PAGE_TRANSITION_AUTO_SUBFRAME;
    if (request->IsInMainFrame() && !request->DidEncounterError() &&
        request->common_params().transition &
            ui::PAGE_TRANSITION_CLIENT_REDIRECT) {
      transition =
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_AUTO_SUBFRAME |
                                    ui::PAGE_TRANSITION_CLIENT_REDIRECT);
    }

    return transition;
  }
  if (request->IsSameDocument())
    return params.transition;
  if (request->IsInMainFrame()) {
    // This follows GetTransitionType() in render_frame_impl.cc.
    ui::PageTransition supplied_transition =
        ui::PageTransitionFromInt(request->common_params().transition);
    if (ui::PageTransitionCoreTypeIs(supplied_transition,
                                     ui::PAGE_TRANSITION_LINK) &&
        request->common_params().post_data) {
      return ui::PAGE_TRANSITION_FORM_SUBMIT;
    }
    return supplied_transition;
  }
  // This follows RenderFrameImpl::MakeDidCommitProvisionalLoadParams().
  if (renderer_load_type == RendererLoadType::kStandard)
    return ui::PAGE_TRANSITION_MANUAL_SUBFRAME;
  return ui::PAGE_TRANSITION_AUTO_SUBFRAME;
}

// Calculates the "loading" URL for a given navigation. This tries to replicate
// RenderFrameImpl::GetLoadingUrl() and is used to predict the value of "url" in
// DidCommitProvisionalLoadParams.
GURL CalculateLoadingURL(
    NavigationRequest* request,
    const mojom::DidCommitProvisionalLoadParams& params,
    const RenderFrameHostImpl::RendererURLInfo& last_renderer_url_info,
    bool last_document_is_error_document,
    const GURL& last_committed_url) {
  if (params.url.IsAboutBlank() && params.url.ref_piece() == "blocked") {
    // Some navigations can still be blocked by the renderer during the commit,
    // changing the URL to "about:blank#blocked". Currently we have no way of
    // predicting this in the browser, so just return the URL given by the
    // renderer in this case.
    // TODO(https://crbug.com/1131832): Block the navigations in the browser
    // instead and remove |params| as a parameter to this function.
    return params.url;
  }

  if (!request->common_params().url.is_valid()) {
    // Empty URL (and invalid URLs, which are converted to the empty URL due
    // to IPC URL reparsing) will be rewritten to "about:blank" in the renderer.
    // TODO(https://crbug.com/1131832): Do the rewrite in the browser.
    return GURL(url::kAboutBlankURL);
  }

  if (request->IsSameDocument() &&
      (last_document_is_error_document ||
       last_renderer_url_info.was_loaded_from_load_data_with_base_url)) {
    // Documents that have an "override" URL (loadDataWithBaseURL navigations,
    // error documents) will continue using that URL even after same-document
    // navigations.
    return last_committed_url;
  }

  // For all other navigations, the returned URL should be the same as the URL
  // in CommonNavigationParams.
  return request->common_params().url;
}

bool ShouldVerify(const std::string& param) {
#if DCHECK_IS_ON()
  return true;
#else
  if (param != "should_replace_current_entry" &&
      param != "url_is_unreachable") {
    // All params except the two above have no known complications and should be
    // compared by default.
    return true;
  }
  return GetFieldTrialParamByFeatureAsBool(features::kVerifyDidCommitParams,
                                           param, false);
#endif
}

void LogVerifyDidCommitParamsDifference(
    VerifyDidCommitParamsDifference difference) {
  UMA_HISTOGRAM_ENUMERATION("Navigation.VerifyDidCommitParams", difference);
}

std::string GetURLTypeForCrashKey(const GURL& url) {
  if (url == kUnreachableWebDataURL)
    return "error";
  if (url == kBlockedURL)
    return "blocked";
  if (url.IsAboutBlank())
    return "about:blank";
  if (url.IsAboutSrcdoc())
    return "about:srcdoc";
  if (url.is_empty())
    return "empty";
  if (!url.is_valid())
    return "invalid";
  return url.scheme();
}

std::string GetURLRelationForCrashKey(
    const GURL& actual_url,
    const GURL& predicted_url,
    const blink::mojom::CommonNavigationParams& common_params,
    const GURL& last_committed_url,
    const RenderFrameHostImpl::RendererURLInfo& renderer_url_info) {
  if (actual_url == predicted_url)
    return "as predicted";
  if (actual_url == last_committed_url)
    return "last committed";
  if (actual_url == common_params.url)
    return "common params URL";
  if (actual_url == common_params.base_url_for_data_url)
    return "base URL";
  if (actual_url == renderer_url_info.last_document_url)
    return "last document URL";
  return "unknown";
}

void RenderFrameHostImpl::
    VerifyThatBrowserAndRendererCalculatedDidCommitParamsMatch(
        NavigationRequest* request,
        const mojom::DidCommitProvisionalLoadParams& params,
        const mojom::DidCommitSameDocumentNavigationParamsPtr
            same_document_params) {
#if !DCHECK_IS_ON()
  // Only check for the flag if DCHECK is not enabled, so that we will always
  // verify the params for tests.
  if (!base::FeatureList::IsEnabled(features::kVerifyDidCommitParams))
    return;
#endif
  // Check if these values from DidCommitProvisionalLoadParams sent by the
  // renderer can be calculated entirely in the browser side:
  // - method
  // - url_is_unreachable
  // - post_id
  // - is_overriding_user_agent
  // - http_status_code
  // - should_update_history
  // - url
  // - did_create_new_entry
  // - transition
  // - history_list_was_cleared
  // - origin
  // TODO(crbug.com/1131832): Verify more params.
  // We can know if we're going to be in an error document after this navigation
  // if the net error code is not net::OK, or if we're doing a same-document
  // navigation on an error document (only possible for renderer-initiated
  // navigations).
  const bool is_error_document =
      (request->DidEncounterError() ||
       (is_error_document_ && request->IsSameDocument()));

  const bool browser_url_is_unreachable = is_error_document;

  const bool is_same_document_navigation = !!same_document_params;
  const bool is_same_document_history_api_navigation =
      same_document_params &&
      same_document_params->same_document_navigation_type ==
          blink::mojom::SameDocumentNavigationType::kHistoryApi;
  DCHECK_EQ(is_same_document_navigation, request->IsSameDocument());

  const int64_t browser_post_id =
      CalculatePostID(params.method, request->common_params().post_data,
                      last_post_id_, is_same_document_navigation);

  const std::string& browser_method = CalculateMethod(
      request->common_params().method, last_http_method_,
      is_same_document_navigation, is_same_document_history_api_navigation);

  const bool browser_is_overriding_user_agent =
      is_same_document_navigation
          ? is_overriding_user_agent_
          : (request->commit_params().is_overriding_user_agent &&
             request->frame_tree_node()->IsMainFrame());

  const int browser_http_status_code =
      CalculateHTTPStatusCode(request, last_http_status_code_);

  // Note that this follows the calculation of should_update_history in
  // RenderFrameImpl::MakeDidCommitProvisionalLoadParams().
  // TODO(https://crbug.com/1158101): Reconsider how we calculate
  // should_update_history.
  const bool browser_should_update_history =
      !browser_url_is_unreachable && browser_http_status_code != 404;

  const bool should_replace_current_entry =
      request->common_params().should_replace_current_entry;

  const GURL browser_url =
      CalculateLoadingURL(request, params, renderer_url_info_,
                          is_error_document_, last_committed_url_);

  const RendererLoadType renderer_load_type =
      CalculateRendererLoadType(request, should_replace_current_entry,
                                renderer_url_info_.last_document_url);

  const bool browser_did_create_new_entry =
      request->is_synchronous_renderer_commit()
          ? params.did_create_new_entry
          : CalculateDidCreateNewEntry(request, should_replace_current_entry,
                                       renderer_load_type);

  const ui::PageTransition browser_transition = CalculateTransition(
      request, renderer_load_type, params, IsNestedWithinFencedFrame());

  const bool browser_history_list_was_cleared =
      request->commit_params().should_clear_history_list;

  const bool everything_except_origin_matches =
      ((!ShouldVerify("method") || browser_method == params.method) &&
       (!ShouldVerify("url_is_unreachable") ||
        browser_url_is_unreachable == params.url_is_unreachable) &&
       (!ShouldVerify("post_id") || browser_post_id == params.post_id) &&
       (!ShouldVerify("is_overriding_user_agent") ||
        browser_is_overriding_user_agent == params.is_overriding_user_agent) &&
       (!ShouldVerify("http_status_code") ||
        browser_http_status_code == params.http_status_code) &&
       (!ShouldVerify("should_update_history") ||
        browser_should_update_history == params.should_update_history) &&
       (!ShouldVerify("url") || browser_url == params.url) &&
       (!ShouldVerify("did_create_new_entry") ||
        browser_did_create_new_entry == params.did_create_new_entry) &&
       (!ShouldVerify("transition") ||
        ui::PageTransitionTypeIncludingQualifiersIs(browser_transition,
                                                    params.transition)) &&
       (!ShouldVerify("history_list_was_cleared") ||
        browser_history_list_was_cleared == params.history_list_was_cleared));
  if (everything_except_origin_matches &&
      (!ShouldVerify("origin") ||
       request->state() < NavigationRequest::WILL_PROCESS_RESPONSE ||
       request->GetOriginToCommit() == params.origin)) {
    // Don't do a DumpWithoutCrashing if everything matches. Note that we save
    // `everything_except_origin_matches` separately so that we can skip doing
    // a DumpWithoutCrashing at the end of this function if the origin turns
    // out to match (actual origin match checking is done below as it does its
    // own DumpWithoutCrashing).
    return;
  }

  SCOPED_CRASH_KEY_BOOL(
      "VerifyDidCommit", "prev_ldwb",
      renderer_url_info_.was_loaded_from_load_data_with_base_url);
  SCOPED_CRASH_KEY_STRING32(
      "VerifyDidCommit", "base_url_fdu_type",
      GetURLTypeForCrashKey(request->common_params().base_url_for_data_url));
#if BUILDFLAG(IS_ANDROID)
  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "data_url_empty",
                        request->commit_params().data_url_as_string.empty());
#endif

  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "intended_as_new_entry",
                        request->commit_params().intended_as_new_entry);

  SCOPED_CRASH_KEY_STRING32("VerifyDidCommit", "method_browser",
                            browser_method);
  SCOPED_CRASH_KEY_STRING32("VerifyDidCommit", "method_renderer",
                            params.method);
  SCOPED_CRASH_KEY_STRING32("VerifyDidCommit", "original_method",
                            request->commit_params().original_method);
  // For WebView, since we don't want to log potential PIIs.
  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "method_post_browser",
                        browser_method == "POST");
  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "method_post_renderer",
                        params.method == "POST");
  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "original_method_post",
                        request->commit_params().original_method == "POST");

  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "unreachable_browser",
                        browser_url_is_unreachable);
  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "unreachable_renderer",
                        params.url_is_unreachable);

  SCOPED_CRASH_KEY_NUMBER("VerifyDidCommit", "post_id_browser",
                          browser_post_id);
  SCOPED_CRASH_KEY_NUMBER("VerifyDidCommit", "post_id_renderer",
                          params.post_id);
  // For WebView, since we don't want to log IDs.
  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "post_id_matches",
                        browser_post_id == params.post_id);
  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "post_id_-1_browser",
                        browser_post_id == -1);
  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "post_id_-1_renderer",
                        params.post_id == -1);

  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "override_ua_browser",
                        browser_is_overriding_user_agent);
  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "override_ua_renderer",
                        params.is_overriding_user_agent);

  SCOPED_CRASH_KEY_NUMBER("VerifyDidCommit", "code_browser",
                          browser_http_status_code);
  SCOPED_CRASH_KEY_NUMBER("VerifyDidCommit", "code_renderer",
                          params.http_status_code);

  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "suh_browser",
                        browser_should_update_history);
  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "suh_renderer",
                        params.should_update_history);

  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "gesture",
                        request->common_params().has_user_gesture);

  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "replace",
                        should_replace_current_entry);

  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "create_browser",
                        browser_did_create_new_entry);
  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "create_renderer",
                        params.did_create_new_entry);

  SCOPED_CRASH_KEY_NUMBER("VerifyDidCommit", "transition_browser",
                          browser_transition);
  SCOPED_CRASH_KEY_NUMBER("VerifyDidCommit", "transition_renderer",
                          params.transition);

  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "cleared_browser",
                        browser_history_list_was_cleared);
  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "cleared_renderer",
                        params.history_list_was_cleared);

  SCOPED_CRASH_KEY_STRING256("VerifyDidCommit", "url_browser",
                             browser_url.possibly_invalid_spec());
  SCOPED_CRASH_KEY_STRING256("VerifyDidCommit", "url_renderer",
                             params.url.possibly_invalid_spec());

  SCOPED_CRASH_KEY_STRING32(
      "VerifyDidCommit", "url_relation",
      GetURLRelationForCrashKey(params.url, browser_url,
                                request->common_params(), GetLastCommittedURL(),
                                renderer_url_info_));
  SCOPED_CRASH_KEY_STRING32("VerifyDidCommit", "url_browser_type",
                            GetURLTypeForCrashKey(browser_url));
  SCOPED_CRASH_KEY_STRING32("VerifyDidCommit", "url_renderer_type",
                            GetURLTypeForCrashKey(params.url));

  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "is_same_document",
                        is_same_document_navigation);
  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "is_history_api",
                        is_same_document_history_api_navigation);
  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "renderer_initiated",
                        request->IsRendererInitiated());
  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "is_subframe",
                        !request->frame_tree_node()->IsMainFrame());
  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "is_form_submission",
                        request->IsFormSubmission());
  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "is_error_document",
                        is_error_document);
  SCOPED_CRASH_KEY_NUMBER("VerifyDidCommit", "net_error",
                          request->GetNetErrorCode());

  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "is_server_redirect",
                        request->WasServerRedirect());
  SCOPED_CRASH_KEY_NUMBER("VerifyDidCommit", "redirects_size",
                          request->GetRedirectChain().size());

  SCOPED_CRASH_KEY_NUMBER("VerifyDidCommit", "entry_offset",
                          request->GetNavigationEntryOffset());
  SCOPED_CRASH_KEY_NUMBER(
      "VerifyDidCommit", "entry_count",
      request->frame_tree_node()->frame_tree().controller().GetEntryCount());
  SCOPED_CRASH_KEY_NUMBER("VerifyDidCommit", "last_committed_index",
                          request->frame_tree_node()
                              ->frame_tree()
                              .controller()
                              .GetLastCommittedEntryIndex());

  SCOPED_CRASH_KEY_BOOL(
      "VerifyDidCommit", "is_reload",
      NavigationTypeUtils::IsReload(request->common_params().navigation_type));
  SCOPED_CRASH_KEY_BOOL(
      "VerifyDidCommit", "is_restore",
      NavigationTypeUtils::IsRestore(request->common_params().navigation_type));
  SCOPED_CRASH_KEY_BOOL(
      "VerifyDidCommit", "is_history",
      NavigationTypeUtils::IsHistory(request->common_params().navigation_type));
  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "has_valid_page_state",
                        blink::PageState::CreateFromEncodedData(
                            request->commit_params().page_state)
                            .IsValid());

  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "has_gesture",
                        request->HasUserGesture());
  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "was_click",
                        request->WasInitiatedByLinkClick());

  SCOPED_CRASH_KEY_STRING256("VerifyDidCommit", "original_req_url",
                             request->commit_params().original_url.spec());
  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "original_same_doc",
                        request->commit_params().original_url.EqualsIgnoringRef(
                            GetLastCommittedURL()));

  SCOPED_CRASH_KEY_BOOL(
      "VerifyDidCommit", "on_initial_empty_doc",
      request->frame_tree_node()->is_on_initial_empty_document());

  SCOPED_CRASH_KEY_STRING256("VerifyDidCommit", "last_committed_url",
                             GetLastCommittedURL().spec());
  SCOPED_CRASH_KEY_STRING256("VerifyDidCommit", "last_document_url",
                             renderer_url_info_.last_document_url.spec());
  SCOPED_CRASH_KEY_STRING32("VerifyDidCommit", "last_url_type",
                            GetURLTypeForCrashKey(GetLastCommittedURL()));

  SCOPED_CRASH_KEY_STRING256("VerifyDidCommit", "last_method",
                             last_http_method_);
  SCOPED_CRASH_KEY_NUMBER("VerifyDidCommit", "last_code",
                          last_http_status_code_);

  bool has_original_url =
      GetSiteInstance() && !GetSiteInstance()->IsDefaultSiteInstance();
  SCOPED_CRASH_KEY_STRING256(
      "VerifyDidCommit", "si_url",
      has_original_url
          ? GetSiteInstance()->original_url().possibly_invalid_spec()
          : "");
  SCOPED_CRASH_KEY_BOOL("VerifyDidCommit", "has_si_url", has_original_url);

  // These DCHECKs ensure that tests will fail if we got here, as
  // DumpWithoutCrashing won't fail tests.
  DCHECK_EQ(browser_method, params.method);
  DCHECK_EQ(browser_url_is_unreachable, params.url_is_unreachable);
  DCHECK_EQ(browser_post_id, params.post_id);
  DCHECK_EQ(browser_is_overriding_user_agent, params.is_overriding_user_agent);
  DCHECK_EQ(browser_http_status_code, params.http_status_code);
  DCHECK_EQ(browser_should_update_history, params.should_update_history);
  DCHECK_EQ(browser_url, params.url);
  DCHECK_EQ(browser_did_create_new_entry, params.did_create_new_entry);
  DCHECK(ui::PageTransitionTypeIncludingQualifiersIs(browser_transition,
                                                     params.transition));
  DCHECK_EQ(browser_history_list_was_cleared, params.history_list_was_cleared);

  // Log histograms to trigger Chrometto slow reports, allowing us to see traces
  // to analyze what happened in these navigations.
  if (browser_method != params.method) {
    LogVerifyDidCommitParamsDifference(
        VerifyDidCommitParamsDifference::kMethod);
  }
  if (browser_url_is_unreachable != params.url_is_unreachable) {
    LogVerifyDidCommitParamsDifference(
        VerifyDidCommitParamsDifference::kURLIsUnreachable);
  }
  if (browser_post_id != params.post_id) {
    LogVerifyDidCommitParamsDifference(
        VerifyDidCommitParamsDifference::kPostID);
  }
  if (browser_is_overriding_user_agent != params.is_overriding_user_agent) {
    LogVerifyDidCommitParamsDifference(
        VerifyDidCommitParamsDifference::kIsOverridingUserAgent);
  }
  if (browser_http_status_code != params.http_status_code) {
    LogVerifyDidCommitParamsDifference(
        VerifyDidCommitParamsDifference::kHTTPStatusCode);
  }
  if (browser_should_update_history != params.should_update_history) {
    LogVerifyDidCommitParamsDifference(
        VerifyDidCommitParamsDifference::kShouldUpdateHistory);
  }
  if (browser_url != params.url) {
    LogVerifyDidCommitParamsDifference(VerifyDidCommitParamsDifference::kURL);
  }
  if (browser_did_create_new_entry != params.did_create_new_entry) {
    LogVerifyDidCommitParamsDifference(
        VerifyDidCommitParamsDifference::kDidCreateNewEntry);
  }
  if (!ui::PageTransitionTypeIncludingQualifiersIs(browser_transition,
                                                   params.transition)) {
    LogVerifyDidCommitParamsDifference(
        VerifyDidCommitParamsDifference::kTransition);
  }
  if (browser_history_list_was_cleared != params.history_list_was_cleared) {
    LogVerifyDidCommitParamsDifference(
        VerifyDidCommitParamsDifference::kHistoryListWasCleared);
  }
  // TODO(https://crbug.com/888079): The origin computed from the browser must
  // match the one reported from the renderer process.
  if (!VerifyThatBrowserAndRendererCalculatedOriginsToCommitMatch(request,
                                                                  params)) {
    LogVerifyDidCommitParamsDifference(
        VerifyDidCommitParamsDifference::kOrigin);
  }

  if (!everything_except_origin_matches) {
    // It's possible to get here when everything except the origin matches.
    // If the origin doesn't match, we would do a DumpWithoutCrashing above.
    // So, don't do a DumpWithoutCrashing unless there's another param that
    // doesn't match.
    // Note: This is temporarily disabled on Android as there has been a recent
    // spike of reports on Android WebView.
#if !BUILDFLAG(IS_ANDROID)
    base::debug::DumpWithoutCrashing();
#endif  // !BUILDFLAG(IS_ANDROID)
  }
}

BackForwardCacheImpl& RenderFrameHostImpl::GetBackForwardCache() {
  return GetOutermostMainFrame()
      ->frame_tree()
      ->controller()
      .GetBackForwardCache();
}

FrameTreeNode* RenderFrameHostImpl::GetFrameTreeNodeForUnload() {
  DCHECK(IsPendingDeletion());
  return frame_tree_node_;
}

void RenderFrameHostImpl::MaybeEvictFromBackForwardCache() {
  if (!IsInBackForwardCache())
    return;

  RenderFrameHostImpl* outermost_main_frame = GetOutermostMainFrame();
  BackForwardCacheCanStoreDocumentResultWithTree bfcache_eligibility =
      GetBackForwardCache().GetCurrentBackForwardCacheEligibility(
          outermost_main_frame);

  TRACE_EVENT("navigation",
              "RenderFrameHostImpl::MaybeEvictFromBackForwardCache",
              "render_frame_host", this, "bfcache_eligibility",
              bfcache_eligibility.flattened_reasons.ToString());

  if (bfcache_eligibility.CanStore())
    return;
  EvictFromBackForwardCacheWithFlattenedAndTreeReasons(bfcache_eligibility);
}

void RenderFrameHostImpl::LogCannotCommitOriginCrashKeys(
    const GURL& url,
    const url::Origin& origin,
    const ProcessLock& process_lock,
    bool is_same_document_navigation,
    NavigationRequest* navigation_request) {
  LogRendererKillCrashKeys(GetSiteInstance()->GetSiteInfo());

  // Temporary instrumentation to debug the root cause of
  // https://crbug.com/923144.
  auto bool_to_crash_key = [](bool b) { return b ? "true" : "false"; };

  static auto* const target_url_key = base::debug::AllocateCrashKeyString(
      "target_url", base::debug::CrashKeySize::Size256);
  base::debug::SetCrashKeyString(target_url_key, url.spec());

  static auto* const target_origin_key = base::debug::AllocateCrashKeyString(
      "target_origin", base::debug::CrashKeySize::Size256);
  base::debug::SetCrashKeyString(target_origin_key, origin.GetDebugString());

  const url::Origin url_origin = url::Origin::Resolve(url, origin);
  const auto target_url_origin_tuple_or_precursor_tuple =
      url_origin.GetTupleOrPrecursorTupleIfOpaque();
  static auto* const target_url_origin_tuple_key =
      base::debug::AllocateCrashKeyString("target_url_origin_tuple",
                                          base::debug::CrashKeySize::Size256);
  base::debug::SetCrashKeyString(
      target_url_origin_tuple_key,
      target_url_origin_tuple_or_precursor_tuple.Serialize());

  const auto target_origin_tuple_or_precursor_tuple =
      origin.GetTupleOrPrecursorTupleIfOpaque();
  static auto* const target_origin_tuple_key =
      base::debug::AllocateCrashKeyString("target_origin_tuple",
                                          base::debug::CrashKeySize::Size256);
  base::debug::SetCrashKeyString(
      target_origin_tuple_key,
      target_origin_tuple_or_precursor_tuple.Serialize());

  static auto* const last_committed_url_key =
      base::debug::AllocateCrashKeyString("last_committed_url",
                                          base::debug::CrashKeySize::Size256);
  base::debug::SetCrashKeyString(last_committed_url_key,
                                 GetLastCommittedURL().spec());

  static auto* const last_committed_origin_key =
      base::debug::AllocateCrashKeyString("last_committed_origin",
                                          base::debug::CrashKeySize::Size256);
  base::debug::SetCrashKeyString(last_committed_origin_key,
                                 GetLastCommittedOrigin().GetDebugString());

  static auto* const process_lock_key = base::debug::AllocateCrashKeyString(
      "process_lock", base::debug::CrashKeySize::Size256);
  base::debug::SetCrashKeyString(process_lock_key, process_lock.ToString());

  static auto* const is_same_document_key = base::debug::AllocateCrashKeyString(
      "is_same_document", base::debug::CrashKeySize::Size32);
  base::debug::SetCrashKeyString(
      is_same_document_key, bool_to_crash_key(is_same_document_navigation));

  static auto* const is_subframe_key = base::debug::AllocateCrashKeyString(
      "is_subframe", base::debug::CrashKeySize::Size32);
  base::debug::SetCrashKeyString(is_subframe_key,
                                 bool_to_crash_key(GetMainFrame() != this));

  static auto* const lifecycle_state_key = base::debug::AllocateCrashKeyString(
      "lifecycle_state", base::debug::CrashKeySize::Size32);
  base::debug::SetCrashKeyString(lifecycle_state_key,
                                 LifecycleStateImplToString(lifecycle_state()));

  static auto* const is_active_key = base::debug::AllocateCrashKeyString(
      "is_active", base::debug::CrashKeySize::Size32);
  base::debug::SetCrashKeyString(is_active_key, bool_to_crash_key(IsActive()));

  static auto* const is_cross_process_subframe_key =
      base::debug::AllocateCrashKeyString("is_cross_process_subframe",
                                          base::debug::CrashKeySize::Size32);
  base::debug::SetCrashKeyString(is_cross_process_subframe_key,
                                 bool_to_crash_key(IsCrossProcessSubframe()));

  static auto* const is_local_root_key = base::debug::AllocateCrashKeyString(
      "is_local_root", base::debug::CrashKeySize::Size32);
  base::debug::SetCrashKeyString(is_local_root_key,
                                 bool_to_crash_key(is_local_root()));

  if (navigation_request && navigation_request->IsNavigationStarted()) {
    static auto* const is_renderer_initiated_key =
        base::debug::AllocateCrashKeyString("is_renderer_initiated",
                                            base::debug::CrashKeySize::Size32);
    base::debug::SetCrashKeyString(
        is_renderer_initiated_key,
        bool_to_crash_key(navigation_request->IsRendererInitiated()));

    static auto* const is_server_redirect_key =
        base::debug::AllocateCrashKeyString("is_server_redirect",
                                            base::debug::CrashKeySize::Size32);
    base::debug::SetCrashKeyString(
        is_server_redirect_key,
        bool_to_crash_key(navigation_request->WasServerRedirect()));

    static auto* const is_form_submission_key =
        base::debug::AllocateCrashKeyString("is_form_submission",
                                            base::debug::CrashKeySize::Size32);
    base::debug::SetCrashKeyString(
        is_form_submission_key,
        bool_to_crash_key(navigation_request->IsFormSubmission()));

    static auto* const is_error_page_key = base::debug::AllocateCrashKeyString(
        "is_error_page", base::debug::CrashKeySize::Size32);
    base::debug::SetCrashKeyString(
        is_error_page_key,
        bool_to_crash_key(navigation_request->IsErrorPage()));

    static auto* const initiator_origin_key =
        base::debug::AllocateCrashKeyString("initiator_origin",
                                            base::debug::CrashKeySize::Size64);
    base::debug::SetCrashKeyString(
        initiator_origin_key,
        navigation_request->GetInitiatorOrigin()
            ? navigation_request->GetInitiatorOrigin()->GetDebugString()
            : "none");

    static auto* const starting_site_instance_key =
        base::debug::AllocateCrashKeyString("starting_site_instance",
                                            base::debug::CrashKeySize::Size256);
    base::debug::SetCrashKeyString(starting_site_instance_key,
                                   navigation_request->GetStartingSiteInstance()
                                       ->GetSiteInfo()
                                       .GetDebugString());
  }
}

void RenderFrameHostImpl::EnableMojoJsBindings(
    content::mojom::ExtraMojoJsFeaturesPtr features) {
  // This method should only be called on RenderFrameHost which is for a WebUI.
  DCHECK_NE(WebUI::kNoWebUI,
            WebUIControllerFactoryRegistry::GetInstance()->GetWebUIType(
                GetSiteInstance()->GetBrowserContext(),
                site_instance_->GetSiteInfo().site_url()));

  GetFrameBindingsControl()->EnableMojoJsBindings(std::move(features));
}

void RenderFrameHostImpl::EnableMojoJsBindingsWithBroker(
    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> broker) {
  // This method should only be called on RenderFrameHost that has an associated
  // WebUI, because it needs to transfer the broker's ownership to its
  // WebUIController. EnableMojoJsBindings does this differently and can be
  // called before the WebUI object is created.
  DCHECK(GetWebUI());
  GetFrameBindingsControl()->EnableMojoJsBindingsWithBroker(std::move(broker));
}

bool RenderFrameHostImpl::IsOutermostMainFrame() const {
  return !GetParentOrOuterDocument();
}

void RenderFrameHostImpl::SetIsLoadingForRendererDebugURL() {
  LoadingState previous_frame_tree_loading_state =
      frame_tree()->LoadingTree()->GetLoadingState();
  loading_state_ = LoadingState::LOADING_UI_REQUESTED;
  owner_->DidStartLoading(previous_frame_tree_loading_state);
}

BackForwardCacheMetrics* RenderFrameHostImpl::GetBackForwardCacheMetrics() {
  NavigationEntryImpl* navigation_entry =
      frame_tree()->controller().GetEntryWithUniqueID(nav_entry_id());
  if (!navigation_entry)
    return nullptr;
  return navigation_entry->back_forward_cache_metrics();
}

bool RenderFrameHostImpl::IsBackForwardCacheDisabled() const {
  return back_forward_cache_disabled_reasons_.size();
}

bool RenderFrameHostImpl::IsDOMContentLoaded() {
  return document_associated_data_->dom_content_loaded();
}

void RenderFrameHostImpl::UpdateIsAdFrame(bool is_ad_frame) {
  browsing_context_state_->SetIsAdFrame(is_ad_frame);
}

#if BUILDFLAG(IS_ANDROID)
std::pair<blink::mojom::AuthenticatorStatus, bool>
RenderFrameHostImpl::PerformGetAssertionWebAuthSecurityChecks(
    const std::string& relying_party_id,
    const url::Origin& effective_origin,
    bool is_payment_credential_get_assertion,
    const blink::mojom::RemoteDesktopClientOverridePtr&
        remote_desktop_client_override) {
  bool is_cross_origin = true;  // Will be reset in ValidateAncestorOrigins().

  WebAuthRequestSecurityChecker::RequestType request_type =
      is_payment_credential_get_assertion
          ? WebAuthRequestSecurityChecker::RequestType::
                kGetPaymentCredentialAssertion
          : WebAuthRequestSecurityChecker::RequestType::kGetAssertion;
  blink::mojom::AuthenticatorStatus status =
      GetWebAuthRequestSecurityChecker()->ValidateAncestorOrigins(
          effective_origin, request_type, &is_cross_origin);
  if (status != blink::mojom::AuthenticatorStatus::SUCCESS) {
    return std::make_pair(status, is_cross_origin);
  }

  if (!GetContentClient()
           ->browser()
           ->GetWebAuthenticationDelegate()
           ->IsSecurityLevelAcceptableForWebAuthn(this, effective_origin)) {
    return std::make_pair(blink::mojom::AuthenticatorStatus::CERTIFICATE_ERROR,
                          is_cross_origin);
  }

  status = GetWebAuthRequestSecurityChecker()->ValidateDomainAndRelyingPartyID(
      effective_origin, relying_party_id, request_type,
      remote_desktop_client_override);
  return std::make_pair(status, is_cross_origin);
}

blink::mojom::AuthenticatorStatus
RenderFrameHostImpl::PerformMakeCredentialWebAuthSecurityChecks(
    const std::string& relying_party_id,
    const url::Origin& effective_origin,
    bool is_payment_credential_creation,
    const blink::mojom::RemoteDesktopClientOverridePtr&
        remote_desktop_client_override) {
  bool is_cross_origin;

  WebAuthRequestSecurityChecker::RequestType request_type =
      is_payment_credential_creation
          ? WebAuthRequestSecurityChecker::RequestType::kMakePaymentCredential
          : WebAuthRequestSecurityChecker::RequestType::kMakeCredential;
  blink::mojom::AuthenticatorStatus status =
      GetWebAuthRequestSecurityChecker()->ValidateAncestorOrigins(
          effective_origin, request_type, &is_cross_origin);
  if (status != blink::mojom::AuthenticatorStatus::SUCCESS) {
    return status;
  }

  if (!GetContentClient()
           ->browser()
           ->GetWebAuthenticationDelegate()
           ->IsSecurityLevelAcceptableForWebAuthn(this, effective_origin)) {
    return blink::mojom::AuthenticatorStatus::CERTIFICATE_ERROR;
  }

  status = GetWebAuthRequestSecurityChecker()->ValidateDomainAndRelyingPartyID(
      effective_origin, relying_party_id, request_type,
      remote_desktop_client_override);
  if (status != blink::mojom::AuthenticatorStatus::SUCCESS) {
    return status;
  }

  return blink::mojom::AuthenticatorStatus::SUCCESS;
}
#endif

const blink::DocumentToken& RenderFrameHostImpl::GetDocumentToken() const {
  DCHECK_NE(LifecycleStateImpl::kPendingCommit, lifecycle_state());
  DCHECK_NE(LifecycleStateImpl::kSpeculative, lifecycle_state());

  return GetDocumentTokenIgnoringSafetyRestrictions();
}

const blink::DocumentToken*
RenderFrameHostImpl::GetDocumentTokenForCrossDocumentNavigationReuse(
    base::PassKey<NavigationRequest>) {
  if (ShouldResetDocumentAssociatedDataAtCommit()) {
    return nullptr;
  }

  return &document_associated_data_->token();
}

void RenderFrameHostImpl::ReinitializeDocumentAssociatedDataForReuseAfterCrash(
    base::PassKey<RenderFrameHostManager>) {
  DCHECK(is_main_frame());
  DCHECK_EQ(RenderFrameState::kDeleted, render_frame_state_);

  // Clear all the document-associated data for this RenderFrameHost when its
  // RenderFrame is recreated after a crash. Note that the user data is
  // intentionally not cleared at the time of crash. Please refer to
  // https://crbug.com/1099237 for more details.
  //
  // Clearing of user data should be called before RenderFrameCreated to
  // ensure:
  // - a) the new state set in RenderFrameCreated doesn't get deleted.
  // - b) the old state is not leaked to a new RenderFrameHost.
  document_associated_data_.emplace(*this, blink::DocumentToken());
}

void RenderFrameHostImpl::ReinitializeDocumentAssociatedDataForTesting() {
  document_associated_data_.emplace(*this, blink::DocumentToken());
}

void RenderFrameHostImpl::IsClipboardPasteContentAllowed(
    const ui::ClipboardFormatType& data_type,
    ClipboardPasteData clipboard_paste_data,
    IsClipboardPasteContentAllowedCallback callback) {
  delegate_->IsClipboardPasteContentAllowed(GetLastCommittedURL(), data_type,
                                            std::move(clipboard_paste_data),
                                            std::move(callback));
}

RenderFrameHostImpl* RenderFrameHostImpl::GetParentOrOuterDocument() const {
  return frame_tree_node()->GetParentOrOuterDocumentHelper(
      /*escape_guest_view=*/false, /*include_prospective=*/true);
}

RenderFrameHostImpl* RenderFrameHostImpl::GetParentOrOuterDocumentOrEmbedder()
    const {
  return frame_tree_node()->GetParentOrOuterDocumentHelper(
      /*escape_guest_view=*/true, /*include_prospective=*/true);
}

RenderFrameHostImpl* RenderFrameHostImpl::GetOutermostMainFrameOrEmbedder() {
  RenderFrameHostImpl* current = this;
  while (RenderFrameHostImpl* parent =
             current->GetParentOrOuterDocumentOrEmbedder()) {
    current = parent;
  }
  return current;
}

RenderFrameHostImpl* RenderFrameHostImpl::
    GetParentOrOuterDocumentOrEmbedderExcludingProspectiveOwners() const {
  return frame_tree_node()->GetParentOrOuterDocumentHelper(
      /*escape_guest_view=*/true, /*include_prospective=*/false);
}

RenderFrameHostImpl* RenderFrameHostImpl::
    GetOutermostMainFrameOrEmbedderExcludingProspectiveOwners() {
  RenderFrameHostImpl* current = this;
  while (
      RenderFrameHostImpl* parent =
          current
              ->GetParentOrOuterDocumentOrEmbedderExcludingProspectiveOwners()) {
    current = parent;
  }
  return current;
}

scoped_refptr<WebAuthRequestSecurityChecker>
RenderFrameHostImpl::GetWebAuthRequestSecurityChecker() {
  if (!webauth_request_security_checker_)
    webauth_request_security_checker_ =
        base::MakeRefCounted<WebAuthRequestSecurityChecker>(this);

  return webauth_request_security_checker_;
}

bool RenderFrameHostImpl::IsInBackForwardCache() const {
  return lifecycle_state() == LifecycleStateImpl::kInBackForwardCache;
}

bool RenderFrameHostImpl::IsPendingDeletion() const {
  return lifecycle_state() == LifecycleStateImpl::kRunningUnloadHandlers ||
         lifecycle_state() == LifecycleStateImpl::kReadyToBeDeleted;
}

void RenderFrameHostImpl::SetLifecycleState(LifecycleStateImpl new_state) {
  TRACE_EVENT2("content", "RenderFrameHostImpl::SetLifecycleState",
               "render_frame_host", this, "new_state",
               LifecycleStateImplToString(new_state));
  // Finish the slice corresponding to the old lifecycle state and begin a new
  // slice for the lifecycle state we are transitioning to.
  TRACE_EVENT_END("navigation", perfetto::Track::FromPointer(this));
  TRACE_EVENT_BEGIN(
      "navigation",
      perfetto::StaticString{LifecycleStateImplToString(new_state)},
      perfetto::Track::FromPointer(this));
// TODO(crbug.com/1256898): Consider associating expectations with each
// transitions.
#if DCHECK_IS_ON()
  static const base::NoDestructor<base::StateTransitions<LifecycleStateImpl>>
      allowed_transitions(
          // For a graph of state transitions, see
          // https://chromium.googlesource.com/chromium/src/+/HEAD/docs/render-frame-host-lifecycle-state.png
          // To update the graph, see the corresponding .gv file.

          // RenderFrameHost is only set speculative during its creation and no
          // transitions happen to this state during its lifetime.
          base::StateTransitions<LifecycleStateImpl>({
              {LifecycleStateImpl::kSpeculative,
               {LifecycleStateImpl::kActive,
                LifecycleStateImpl::kPendingCommit}},

              {LifecycleStateImpl::kPendingCommit,
               {LifecycleStateImpl::kPrerendering, LifecycleStateImpl::kActive,
                LifecycleStateImpl::kReadyToBeDeleted}},

              {LifecycleStateImpl::kPrerendering,
               {LifecycleStateImpl::kActive,
                LifecycleStateImpl::kRunningUnloadHandlers,
                LifecycleStateImpl::kReadyToBeDeleted}},

              {LifecycleStateImpl::kActive,
               {LifecycleStateImpl::kInBackForwardCache,
                LifecycleStateImpl::kRunningUnloadHandlers,
                LifecycleStateImpl::kReadyToBeDeleted}},

              {LifecycleStateImpl::kInBackForwardCache,
               {LifecycleStateImpl::kActive,
                LifecycleStateImpl::kReadyToBeDeleted}},

              {LifecycleStateImpl::kRunningUnloadHandlers,
               {LifecycleStateImpl::kReadyToBeDeleted}},

              {LifecycleStateImpl::kReadyToBeDeleted, {}},
          }));
  DCHECK_STATE_TRANSITION(allowed_transitions,
                          /*old_state=*/lifecycle_state_,
                          /*new_state=*/new_state);
#endif  // DCHECK_IS_ON()

  // TODO(crbug.com/1256896): Use switch-case to make this more readable.
  // If the RenderFrameHost is restored from BackForwardCache or is part of a
  // prerender activation, update states of all the children to kActive.
  if (new_state == LifecycleStateImpl::kActive) {
    if (lifecycle_state_ == LifecycleStateImpl::kPendingCommit ||
        lifecycle_state_ == LifecycleStateImpl::kSpeculative) {
      // Newly-created documents shouldn't have children, as child creation
      // happens after commit.
      DCHECK(children_.empty());
    }
    if (GetOutermostMainFrameOrEmbedder() == this &&
        (lifecycle_state() == LifecycleStateImpl::kInBackForwardCache ||
         lifecycle_state() == LifecycleStateImpl::kPrerendering)) {
      // We mark all the children, including inner FrameTrees and delegate
      // nodes to have the same lifecycle state.
      FrameTree::NodeRange node_range = FrameTree::SubtreeAndInnerTreeNodes(
          this,
          /*include_delegate_nodes_for_inner_frame_trees=*/true);
      FrameTree::NodeIterator node_iter = node_range.begin();
      while (node_iter != node_range.end()) {
        FrameTreeNode* ftn = *node_iter;
        // TODO(crbug.com/1232528): Inner WebContents do not know about
        // prerendering state so skip them for now. This check does not
        // impact back/forward cache since inner WebContents aren't allowed.
        if (ftn->IsOutermostMainFrame()) {
          DCHECK(ftn->current_frame_host()->lifecycle_state() !=
                 LifecycleStateImpl::kInBackForwardCache);
          node_iter.AdvanceSkippingChildren();
        } else {
          RenderFrameHostImpl* rfh = ftn->current_frame_host();
          DCHECK_EQ(rfh->lifecycle_state(), lifecycle_state_);
          rfh->SetLifecycleState(new_state);
          ++node_iter;
        }
      }
    }
  }

  if (lifecycle_state() == LifecycleStateImpl::kInBackForwardCache)
    was_restored_from_back_forward_cache_for_debugging_ = true;

  if (new_state == LifecycleStateImpl::kPendingCommit ||
      new_state == LifecycleStateImpl::kPrerendering) {
    DCHECK(children_.empty());
  }

  LifecycleStateImpl old_state = lifecycle_state_;
  lifecycle_state_ = new_state;

  // If the state changes from or to kActive, update the active document count.
  if (old_state == LifecycleStateImpl::kActive &&
      new_state != LifecycleStateImpl::kActive) {
    GetSiteInstance()->DecrementActiveDocumentCount(
        last_committed_url_derived_site_info_);
  } else if (old_state != LifecycleStateImpl::kActive &&
             new_state == LifecycleStateImpl::kActive) {
    GetSiteInstance()->IncrementActiveDocumentCount(
        last_committed_url_derived_site_info_);
  }

  // Unset the |has_pending_lifecycle_state_update_| value once the
  // LifecycleStateImpl is updated.
  if (has_pending_lifecycle_state_update_) {
    DCHECK(lifecycle_state() == LifecycleStateImpl::kInBackForwardCache ||
           IsPendingDeletion() ||
           old_state == LifecycleStateImpl::kPrerendering)
        << "Transitioned to unexpected state with resetting "
           "|has_pending_lifecycle_state_update_|\n ";
    has_pending_lifecycle_state_update_ = false;
  }

  // As kSpeculative state is not exposed to embedders, we can ignore the
  // transitions out of kSpeculative state while notifying delegate.
  if (old_state != LifecycleStateImpl::kSpeculative) {
    LifecycleState old_lifecycle_state = GetLifecycleStateFromImpl(old_state);
    LifecycleState new_lifecycle_state = GetLifecycleState();

    // Old and new lifecycle states can be equal due to the same LifecycleState
    // representing multiple LifecycleStateImpls, for example the
    // kPendingDeletion state. Don't notify the observers in such cases.
    if (old_lifecycle_state != new_lifecycle_state) {
      delegate_->RenderFrameHostStateChanged(this, old_lifecycle_state,
                                             new_lifecycle_state);
    }
  }

  if (new_state == LifecycleStateImpl::kActive &&
      old_state == LifecycleStateImpl::kInBackForwardCache) {
    if (auto* permission_service_context =
            PermissionServiceContext::GetForCurrentDocument(this)) {
      permission_service_context->NotifyPermissionStatusChangedIfNeeded();
    }
  }
}

void RenderFrameHostImpl::RecordDocumentCreatedUkmEvent(
    const url::Origin& origin,
    const ukm::SourceId document_ukm_source_id,
    ukm::UkmRecorder* ukm_recorder,
    bool only_record_identifiability_metric) {
  DCHECK(ukm_recorder);
  if (document_ukm_source_id == ukm::kInvalidSourceId)
    return;

  // Compares the subframe origin with the main frame origin. In the case of
  // nested subframes such as A(B(A)), the bottom-most frame A is expected to
  // have |is_cross_origin_frame| set to false, even though this frame is cross-
  // origin from its parent frame B. This value is only used in manual analysis.
  bool is_cross_origin_frame =
      !IsOutermostMainFrame() &&
      !GetOutermostMainFrame()->GetLastCommittedOrigin().IsSameOriginWith(
          origin);

  // Compares the subframe site with the main frame site. In the case of
  // nested subframes such as A(B(A)), the bottom-most frame A is expected to
  // have |is_cross_site_frame| set to false, even though this frame is cross-
  // site from its parent frame B. This value is only used in manual analysis.
  bool is_cross_site_frame =
      !IsOutermostMainFrame() &&
      (net::SchemefulSite(origin) !=
       net::SchemefulSite(GetOutermostMainFrame()->GetLastCommittedOrigin()));

  bool is_main_frame = IsOutermostMainFrame();

  // Our data collection policy disallows collecting UKMs while prerendering.
  // So, assign a valid ID only when the page is not in the prerendering state.
  // See //content/browser/preloading/prerender/README.md and ask the team to
  // explore options to record data for prerendering pages.
  const ukm::SourceId navigation_ukm_source_id =
      IsInLifecycleState(LifecycleState::kPrerendering) ? ukm::kInvalidSourceId
                                                        : GetPageUkmSourceId();

  RecordIdentifiabilityDocumentCreatedMetrics(
      document_ukm_source_id, ukm_recorder, navigation_ukm_source_id,
      is_cross_origin_frame, is_cross_site_frame, is_main_frame);

  if (only_record_identifiability_metric)
    return;

  ukm::builders::DocumentCreated(document_ukm_source_id)
      .SetNavigationSourceId(navigation_ukm_source_id)
      .SetIsMainFrame(is_main_frame)
      .SetIsCrossOriginFrame(is_cross_origin_frame)
      .SetIsCrossSiteFrame(is_cross_site_frame)
      .Record(ukm_recorder);
}

void RenderFrameHostImpl::BindReportingObserver(
    mojo::PendingReceiver<blink::mojom::ReportingObserver> receiver) {
  GetAssociatedLocalFrame()->BindReportingObserver(std::move(receiver));
}

mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
RenderFrameHostImpl::CreateURLLoaderNetworkObserver() {
  return GetStoragePartition()->CreateURLLoaderNetworkObserverForFrame(
      GetProcess()->GetID(), GetRoutingID());
}

PeerConnectionTrackerHost& RenderFrameHostImpl::GetPeerConnectionTrackerHost() {
  return *PeerConnectionTrackerHost::GetOrCreateForCurrentDocument(this);
}

void RenderFrameHostImpl::BindPeerConnectionTrackerHost(
    mojo::PendingReceiver<blink::mojom::PeerConnectionTrackerHost> receiver) {
  GetPeerConnectionTrackerHost().BindReceiver(std::move(receiver));
}

void RenderFrameHostImpl::EnableWebRtcEventLogOutput(int lid,
                                                     int output_period_ms) {
  GetPeerConnectionTrackerHost().StartEventLog(lid, output_period_ms);
}

void RenderFrameHostImpl::DisableWebRtcEventLogOutput(int lid) {
  GetPeerConnectionTrackerHost().StopEventLog(lid);
}

bool RenderFrameHostImpl::IsDocumentOnLoadCompletedInMainFrame() {
  return GetPage().is_on_load_completed_in_main_document();
}

// TODO(crbug.com/1192003): Move this method to content::Page when available.
const std::vector<blink::mojom::FaviconURLPtr>&
RenderFrameHostImpl::FaviconURLs() {
  return GetPage().favicon_urls();
}

mojo::PendingRemote<network::mojom::CookieAccessObserver>
RenderFrameHostImpl::CreateCookieAccessObserver() {
  mojo::PendingRemote<network::mojom::CookieAccessObserver> remote;
  cookie_observers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

mojo::PendingRemote<network::mojom::TrustTokenAccessObserver>
RenderFrameHostImpl::CreateTrustTokenAccessObserver() {
  mojo::PendingRemote<network::mojom::TrustTokenAccessObserver> remote;
  trust_token_observers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

mojo::PendingRemote<network::mojom::SharedDictionaryAccessObserver>
RenderFrameHostImpl::CreateSharedDictionaryAccessObserver() {
  mojo::PendingRemote<network::mojom::SharedDictionaryAccessObserver> remote;
  shared_dictionary_observers_.Add(this,
                                   remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

#if BUILDFLAG(ENABLE_MDNS)
void RenderFrameHostImpl::CreateMdnsResponder(
    mojo::PendingReceiver<network::mojom::MdnsResponder> receiver) {
  GetStoragePartition()->GetNetworkContext()->CreateMdnsResponder(
      std::move(receiver));
}
#endif  // BUILDFLAG(ENABLE_MDNS)

void RenderFrameHostImpl::Clone(
    mojo::PendingReceiver<network::mojom::CookieAccessObserver> observer) {
  cookie_observers_.Add(this, std::move(observer));
}

void RenderFrameHostImpl::Clone(
    mojo::PendingReceiver<network::mojom::TrustTokenAccessObserver> observer) {
  trust_token_observers_.Add(this, std::move(observer));
}

void RenderFrameHostImpl::Clone(
    mojo::PendingReceiver<network::mojom::SharedDictionaryAccessObserver>
        observer) {
  shared_dictionary_observers_.Add(this, std::move(observer));
}

void RenderFrameHostImpl::OnCookiesAccessed(
    std::vector<network::mojom::CookieAccessDetailsPtr> details_vector) {
  UMA_HISTOGRAM_COUNTS_1M("Cookie.OnCookiesAccessed.BatchSize",
                          details_vector.size());
  size_t access_sum = 0;
  for (auto& details : details_vector) {
    access_sum += details->count * details->cookie_list.size();
    EmitCookieWarningsAndMetrics(this, details);

    CookieAccessDetails allowed;
    CookieAccessDetails blocked;
    SplitCookiesIntoAllowedAndBlocked(details, &allowed, &blocked);
    if (!allowed.cookie_list.empty()) {
      delegate_->OnCookiesAccessed(this, allowed);
    }
    if (!blocked.cookie_list.empty()) {
      delegate_->OnCookiesAccessed(this, blocked);
    }
  }
  UMA_HISTOGRAM_COUNTS_1M("Cookie.OnCookiesAccessed.TotalAccesses", access_sum);
}

void RenderFrameHostImpl::OnTrustTokensAccessed(
    network::mojom::TrustTokenAccessDetailsPtr details) {
  delegate_->OnTrustTokensAccessed(this, TrustTokenAccessDetails(details));
}

void RenderFrameHostImpl::OnSharedDictionaryAccessed(
    network::mojom::SharedDictionaryAccessDetailsPtr details) {
  delegate_->OnSharedDictionaryAccessed(this, *details);
}

void RenderFrameHostImpl::SetEmbeddingToken(
    const base::UnguessableToken& embedding_token) {
  // Everything in this method depends on whether the embedding token has
  // actually changed, including setting the AXTreeID (backed by the token).
  if (embedding_token_ == embedding_token)
    return;
  embedding_token_ = embedding_token;

  // The AXTreeID of a frame is backed by its embedding token, so we need to
  // update its AXTreeID, as well as the associated mapping in
  // AXActionHandlerRegistry.
  const ui::AXTreeID old_id = GetAXTreeID();
  ui::AXTreeID ax_tree_id = ui::AXTreeID::FromToken(embedding_token);
  DCHECK_NE(old_id, ax_tree_id);
  SetAXTreeID(ax_tree_id);
  needs_ax_root_id_ = true;
  ui::AXActionHandlerRegistry::GetInstance()->SetFrameIDForAXTreeID(
      ui::AXActionHandlerRegistry::FrameID(GetProcess()->GetID(), routing_id_),
      ax_tree_id);

  // Also important to notify the delegate so that the relevant observers can
  // adapt to the fact that the AXTreeID has changed for the primary main frame
  // (e.g. the WebView needs to update the ID tracking its child accessibility
  // tree).
  if (IsInPrimaryMainFrame())
    delegate_->AXTreeIDForMainFrameHasChanged();

  // Propagate the embedding token to the RenderFrameProxyHost representing the
  // parent frame if needed, that is, if either this is a cross-process subframe
  // or the main frame of an inner web contents (i.e. would need to send it to
  // the RenderFrameProxyHost for the outer web contents owning the inner one).
  PropagateEmbeddingTokenToParentFrame();

  // The accessibility tree for the outermost root frame contains references
  // to the focused frame via its AXTreeID, so ensure that we update that.
  // For frames in a prerendering frame tree, they should never have focus, so
  // the outermost frame does not need to update the references.
  RenderFrameHostImpl* outermost = GetOutermostMainFrameOrEmbedder();
  DCHECK(outermost);
  DCHECK(lifecycle_state_ != LifecycleStateImpl::kPrerendering ||
         outermost->GetFocusedAXTreeID() != GetAXTreeID());

  if (outermost != this &&
      lifecycle_state_ != LifecycleStateImpl::kPrerendering) {
    outermost->UpdateAXTreeData();
  }

  // Finally, since the AXTreeID changed, we have to ensure the
  // BrowserAccessibilityManager gets a new tree as well.
  if (browser_accessibility_manager_) {
    browser_accessibility_manager_->DetachFromParentManager();
    browser_accessibility_manager_.reset();
  }
}

bool RenderFrameHostImpl::DocumentUsedWebOTP() {
  return document_used_web_otp_;
}

void RenderFrameHostImpl::SetFrameTreeNode(FrameTreeNode& frame_tree_node) {
  devtools_instrumentation::WillSwapFrameTreeNode(*frame_tree_node_,
                                                  frame_tree_node);
  frame_tree_node_ = &frame_tree_node;
  SetFrameTree(frame_tree_node_->frame_tree());
  // Setting the FrameTreeNode is only done for FrameTree/FrameTreeNode swaps
  // in MPArch (specifically prerender activation). This is to ensure that
  // fields such as proxies and ReplicationState are copied over correctly. In
  // the new functionality for swapping BrowsingContext on cross
  // BrowsingInstance navigations, the BrowsingContextState is the only field
  // that will need to be swapped.
  switch (features::GetBrowsingContextMode()) {
    case (features::BrowsingContextStateImplementationType::
              kLegacyOneToOneWithFrameTreeNode):
      browsing_context_state_ = frame_tree_node_->render_manager()
                                    ->current_frame_host()
                                    ->browsing_context_state();
      break;
    case (features::BrowsingContextStateImplementationType::
              kSwapForCrossBrowsingInstanceNavigations):
      // TODO(crbug.com/1270671): implement functionality for swapping on cross
      // browsing instance navigations as needed. This will likely be removed
      // once BrowsingContextState is decoupled from FrameTreeNode.
      break;
  }
}

void RenderFrameHostImpl::SetFrameTree(FrameTree& frame_tree) {
  DCHECK_EQ(&frame_tree_node_->frame_tree(), &frame_tree);
  frame_tree_ = &frame_tree;
  render_view_host()->SetFrameTree(frame_tree);
  if (owned_render_widget_host_) {
    owned_render_widget_host_->SetFrameTree(frame_tree);
  }
}

void RenderFrameHostImpl::SetPolicyContainerForEarlyCommitAfterCrash(
    scoped_refptr<PolicyContainerHost> policy_container_host) {
  DCHECK_EQ(lifecycle_state(), LifecycleStateImpl::kSpeculative);
  DCHECK(!policy_container_host_);
  SetPolicyContainerHost(std::move(policy_container_host));
}

void RenderFrameHostImpl::OnDidRunInsecureContent(const GURL& security_origin,
                                                  const GURL& target_url) {
  OPTIONAL_TRACE_EVENT2("content", "RenderFrameHostImpl::DidRunInsecureContent",
                        "security_origin", security_origin, "target_url",
                        target_url);

  RecordAction(base::UserMetricsAction("SSL.RanInsecureContent"));
  if (base::EndsWith(security_origin.spec(), kDotGoogleDotCom,
                     base::CompareCase::INSENSITIVE_ASCII)) {
    RecordAction(base::UserMetricsAction("SSL.RanInsecureContentGoogle"));
  }
  frame_tree_->controller().ssl_manager()->DidRunMixedContent(security_origin);
}

void RenderFrameHostImpl::OnDidRunContentWithCertificateErrors() {
  OPTIONAL_TRACE_EVENT0(
      "content", "RenderFrameHostImpl::OnDidRunContentWithCertificateErrors");
  // For RenderFrameHosts that are inactive and going to be discarded, we can
  // disregard this message; there's no need to update the UI if the UI will
  // never be shown again.
  //
  // We still process this message for pending-commit RenderFrameHosts. This can
  // happen when a subframe's main resource has a certificate error. The
  // origin for the last committed navigation entry will get marked as having
  // run insecure content and that will carry over to the navigation entry for
  // the pending-commit RenderFrameHost when it commits.
  //
  // Generally our approach for active content with certificate errors follows
  // our approach for mixed content (DidRunInsecureContent): when a page loads
  // active insecure content, such as a script or iframe, the top-level origin
  // gets marked as insecure and that applies to any navigation entry using the
  // same renderer process with that same top-level origin.
  //
  // We shouldn't be receiving this message for speculative RenderFrameHosts
  // i.e., before the renderer is told to commit the navigation.
  DCHECK_NE(lifecycle_state(), LifecycleStateImpl::kSpeculative);
  if (lifecycle_state() != LifecycleStateImpl::kPendingCommit &&
      IsInactiveAndDisallowActivation(
          DisallowActivationReasonId::kCertificateErrors)) {
    return;
  }
  // To update mixed content status in a fenced frame, we should call
  // an outer frame's OnDidRunContentWithCertificateErrors.
  // Otherwise, no update can be processed from fenced frames since they have
  // their own NavigationController"
  if (IsNestedWithinFencedFrame()) {
    GetParentOrOuterDocument()->OnDidRunContentWithCertificateErrors();
    return;
  }
  frame_tree_->controller().ssl_manager()->DidRunContentWithCertErrors(
      GetMainFrame()->GetLastCommittedOrigin().GetURL());
}

void RenderFrameHostImpl::OnDidDisplayContentWithCertificateErrors() {
  OPTIONAL_TRACE_EVENT0(
      "content",
      "RenderFrameHostImpl::OnDidDisplayContentWithCertificateErrors");
  frame_tree_->controller().ssl_manager()->DidDisplayContentWithCertErrors();
}

void RenderFrameHostImpl::IncreaseCommitNavigationCounter() {
  if (commit_navigation_sent_counter_ < std::numeric_limits<int>::max())
    ++commit_navigation_sent_counter_;
  else
    commit_navigation_sent_counter_ = 0;
}

bool RenderFrameHostImpl::ShouldWaitForUnloadHandlers() const {
  return has_unload_handlers() && !IsInBackForwardCache();
}

void RenderFrameHostImpl::AssertFrameWasCommitted() const {
  if (LIKELY(lifecycle_state() != LifecycleStateImpl::kSpeculative &&
             lifecycle_state() != LifecycleStateImpl::kPendingCommit)) {
    return;
  }

  NOTREACHED();
  base::debug::DumpWithoutCrashing();
}

void RenderFrameHostImpl::AssertBrowserContextShutdownHasntStarted() {
  if (LIKELY(!GetBrowserContext()->ShutdownStarted()))
    return;

  std::string debug_string = ToDebugString();
  SCOPED_CRASH_KEY_STRING256("shutdown", "frame->ToDebugString", debug_string);
  NOTREACHED() << "BrowserContext->ShutdownStarted() without first closing all "
               << "WebContents; debug_string = " << debug_string;
  base::debug::DumpWithoutCrashing();
}

blink::StorageKey RenderFrameHostImpl::GetBucketStorageKey() {
  return storage_key_;
}

blink::mojom::PermissionStatus RenderFrameHostImpl::GetPermissionStatus(
    blink::PermissionType permission_type) {
  return GetBrowserContext()
      ->GetPermissionController()
      ->GetPermissionStatusForCurrentDocument(permission_type, this);
}

void RenderFrameHostImpl::BindCacheStorageForBucket(
    const storage::BucketInfo& bucket,
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
  BindCacheStorageInternal(std::move(receiver), bucket.ToBucketLocator());
}

void RenderFrameHostImpl::GetSandboxedFileSystemForBucket(
    const storage::BucketInfo& bucket,
    blink::mojom::BucketHost::GetDirectoryCallback callback) {
  GetStoragePartition()->GetFileSystemAccessManager()->GetSandboxedFileSystem(
      FileSystemAccessManagerImpl::BindingContext(
          GetStorageKey(), GetLastCommittedURL(), GetGlobalId()),
      bucket.ToBucketLocator(), std::move(callback));
}

GlobalRenderFrameHostId RenderFrameHostImpl::GetAssociatedRenderFrameHostId()
    const {
  return GetGlobalId();
}

RenderFrameHostImpl::DocumentAssociatedData::DocumentAssociatedData(
    RenderFrameHostImpl& document,
    const blink::DocumentToken& token)
    : token_(token), weak_factory_(&document) {
  auto [_, inserted] = GetDocumentTokenMap().insert({token_, &document});
  CHECK(inserted);

  // Only create page object for the main document as the PageImpl is 1:1 with
  // main document.
  if (!document.GetParent()) {
    PageDelegate* page_delegate = document.frame_tree()->page_delegate();
    DCHECK(page_delegate);
    owned_page_ = PageFactory::Create(document, *page_delegate);
  }
}

RenderFrameHostImpl::DocumentAssociatedData::~DocumentAssociatedData() {
  while (!services_.empty()) {
    // DocumentServiceBase unregisters itself at destruction time.
    services_.back()->WillBeDestroyed(
        DocumentServiceDestructionReason::kEndOfDocumentLifetime);
    services_.back()->ResetAndDeleteThis();
  }

  // Explicitly clear all user data here, so that the other fields of
  // DocumentAssociatedData are still valid while user data is being destroyed.
  ClearAllUserData();

  // Last in case any DocumentService / DocumentUserData service destructors try
  // to look up RenderFrameHosts by DocumentToken.
  CHECK_EQ(1u, GetDocumentTokenMap().erase(token_));
}

void RenderFrameHostImpl::DocumentAssociatedData::
    set_navigation_or_document_handle(
        scoped_refptr<NavigationOrDocumentHandle> handle) {
  navigation_or_document_handle_ = std::move(handle);
}

std::ostream& operator<<(std::ostream& o,
                         const RenderFrameHostImpl::LifecycleStateImpl& s) {
  return o << RenderFrameHostImpl::LifecycleStateImplToString(s);
}

net::CookieSettingOverrides RenderFrameHostImpl::GetCookieSettingOverrides() {
  // This shouldn't be called before committing the document.
  DCHECK_NE(lifecycle_state(), LifecycleStateImpl::kSpeculative);
  DCHECK_NE(lifecycle_state(), LifecycleStateImpl::kPendingCommit);
  auto subresource_loader_factories_config =
      SubresourceLoaderFactoriesConfig::ForLastCommittedNavigation(*this);
  return subresource_loader_factories_config.cookie_setting_overrides();
}

RenderFrameHostImpl::CookieChangeListener::CookieChangeListener(
    StoragePartition* storage_partition,
    GURL& url) {
  DCHECK(storage_partition);
  auto* cookie_manager = storage_partition->GetCookieManagerForBrowserProcess();
  cookie_manager->AddCookieChangeListener(
      url, absl::nullopt,
      cookie_change_listener_receiver_.BindNewPipeAndPassRemote());
}

RenderFrameHostImpl::CookieChangeListener::~CookieChangeListener() = default;

void RenderFrameHostImpl::CookieChangeListener::OnCookieChange(
    const net::CookieChangeInfo& change) {
  // TODO (https://crbug.com/1399741): After adding the invalidation signals
  // API, we could mark the page as ineligible for BFCache as soon as the cookie
  // change event is received after the navigation is committed.
  cookie_change_info_.cookie_modification_count_++;
  if (change.cookie.IsHttpOnly()) {
    cookie_change_info_.http_only_cookie_modification_count_++;
  }
}

RenderFrameHostImpl::CookieChangeListener::CookieChangeInfo
RenderFrameHostImpl::GetCookieChangeInfo() {
  return cookie_change_listener_ ? cookie_change_listener_->cookie_change_info()
                                 : CookieChangeListener::CookieChangeInfo{};
}

void RenderFrameHostImpl::SetResourceCacheRemote(
    mojo::PendingRemote<blink::mojom::ResourceCache> pending_remote) {
  GetMojomFrameInRenderer()->SetResourceCache(std::move(pending_remote));
}

bool RenderFrameHostImpl::LoadedWithCacheControlNoStoreHeader() {
  return GetBackForwardCacheDisablingFeatures().Has(
      blink::scheduler::WebSchedulerTrackedFeature::
          kMainResourceHasCacheControlNoStore);
}

void RenderFrameHostImpl::BindFileBackedBlobFactory(
    mojo::PendingAssociatedReceiver<blink::mojom::FileBackedBlobFactory>
        receiver) {
  FileBackedBlobFactoryFrameImpl::CreateForCurrentDocument(this,
                                                           std::move(receiver));
}

bool RenderFrameHostImpl::ShouldChangeRenderFrameHostOnSameSiteNavigation()
    const {
  return ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
      is_main_frame(), is_local_root(), has_committed_any_navigation(),
      must_be_replaced());
}

bool RenderFrameHostImpl::ShouldReuseCompositing(
    SiteInstanceImpl& speculative_site_instance) const {
  if (!ShouldChangeRenderFrameHostOnSameSiteNavigation()) {
    return false;
  }

  // This indicates that the renderer process corresponding to this frame has
  // crashed and there is no compositor to reuse.
  if (must_be_replaced_) {
    return false;
  }

  if (!base::FeatureList::IsEnabled(features::kRenderDocumentCompositorReuse)) {
    return false;
  }

  if (GetSiteInstance()->group() != speculative_site_instance.group()) {
    return false;
  }

  CHECK_EQ(GetProcess(), speculative_site_instance.GetProcess());

  // TODO(khushalsagar): We'll need this for main frames as well but the
  // code for RFH creation/init limits it to subframes for now.
  bool is_main_frame = !GetParent();
  if (is_main_frame) {
    return false;
  }

  return true;
}

}  // namespace content
