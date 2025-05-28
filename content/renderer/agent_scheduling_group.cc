// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/agent_scheduling_group.h"

#include <map>
#include <utility>

#include "base/containers/map_util.h"
#include "base/feature_list.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/types/pass_key.h"
#include "content/common/agent_scheduling_group.mojom.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sync_channel.h"
#include "third_party/blink/public/common/page/browsing_context_group_info.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/page/page.mojom.h"
#include "third_party/blink/public/mojom/page/prerender_page_param.mojom.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom.h"
#include "third_party/blink/public/mojom/worker/worklet_global_scope_creation_params.mojom.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/web/web_remote_frame.h"
#include "third_party/blink/public/web/web_shared_storage_worklet_thread.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_view_client.h"

namespace content {

using ::IPC::ChannelMojo;
using ::IPC::Listener;
using ::IPC::SyncChannel;
using ::mojo::AssociatedReceiver;
using ::mojo::AssociatedRemote;
using ::mojo::PendingAssociatedReceiver;
using ::mojo::PendingAssociatedRemote;
using ::mojo::PendingReceiver;
using ::mojo::PendingRemote;
using ::mojo::Receiver;
using ::mojo::Remote;

using PassKey = ::base::PassKey<AgentSchedulingGroup>;

namespace {

RenderThreadImpl& ToImpl(RenderThread& render_thread) {
  DCHECK(RenderThreadImpl::current());
  return static_cast<RenderThreadImpl&>(render_thread);
}

static features::MBIMode GetMBIMode() {
  return base::FeatureList::IsEnabled(features::kMBIMode)
             ? features::kMBIModeParam.Get()
             : features::MBIMode::kLegacy;
}

// Creates a main WebRemoteFrame for `web_view`.
void CreateRemoteMainFrame(
    const blink::RemoteFrameToken& frame_token,
    mojo::PendingAssociatedRemote<blink::mojom::RemoteFrameHost>
        remote_frame_host,
    mojo::PendingAssociatedReceiver<blink::mojom::RemoteFrame>
        remote_frame_receiver,
    mojo::PendingAssociatedRemote<blink::mojom::RemoteMainFrameHost>
        remote_main_frame_host,
    mojo::PendingAssociatedReceiver<blink::mojom::RemoteMainFrame>
        remote_main_frame_receiver,
    base::UnguessableToken& devtools_main_frame_token,
    blink::mojom::FrameReplicationStatePtr replication_state,
    blink::WebFrame* opener_frame,
    blink::WebView* web_view) {
  blink::WebRemoteFrame::CreateMainFrame(
      web_view, frame_token, /*is_loading=*/false, devtools_main_frame_token,
      opener_frame, std::move(remote_frame_host),
      std::move(remote_frame_receiver), std::move(replication_state));
  // Root frame proxy has no ancestors to point to their RenderWidget.

  // The WebRemoteFrame created here was already attached to the Page as its
  // main frame, so we can call WebView's DidAttachRemoteMainFrame().
  web_view->DidAttachRemoteMainFrame(std::move(remote_main_frame_host),
                                     std::move(remote_main_frame_receiver));
}

// Blink inappropriately makes decisions if there is a WebViewClient set,
// so currently we need to always create a WebViewClient.
class SelfOwnedWebViewClient : public blink::WebViewClient {
 public:
  void OnDestruct() override { delete this; }
};

}  // namespace

AgentSchedulingGroup::ReceiverData::ReceiverData(
    const std::string& name,
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterface> receiver)
    : name(std::move(name)), receiver(std::move(receiver)) {}

AgentSchedulingGroup::ReceiverData::ReceiverData(ReceiverData&& other)
    : name(std::move(other.name)), receiver(std::move(other.receiver)) {}

AgentSchedulingGroup::ReceiverData::~ReceiverData() = default;

// AgentSchedulingGroup:
AgentSchedulingGroup::AgentSchedulingGroup(
    RenderThread& render_thread,
    mojo::PendingReceiver<IPC::mojom::ChannelBootstrap> bootstrap)
    : agent_group_scheduler_(
          blink::scheduler::WebThreadScheduler::MainThreadScheduler()
              .CreateWebAgentGroupScheduler()),
      render_thread_(render_thread),
      // `receiver_` will be bound by `OnAssociatedInterfaceRequest()`.
      receiver_(this) {
  DCHECK(agent_group_scheduler_);
  DCHECK_NE(GetMBIMode(), features::MBIMode::kLegacy);

  channel_ = SyncChannel::Create(
      /*listener=*/this, /*ipc_task_runner=*/render_thread_->GetIOTaskRunner(),
      /*listener_task_runner=*/agent_group_scheduler_->DefaultTaskRunner(),
      render_thread_->GetShutdownEvent());

  channel_->SetUrgentMessageObserver(agent_group_scheduler_.get());

  // TODO(crbug.com/40142495): Add necessary filters.
  // Currently, the renderer process has these filters:
  // 1. `UnfreezableMessageFilter` - in the process of being removed,
  // 2. `PnaclTranslationResourceHost` - NaCl is going away, and
  // 3. `AutomationMessageFilter` - needs to be handled somehow.

  channel_->Init(
      ChannelMojo::CreateClientFactory(
          bootstrap.PassPipe(),
          /*ipc_task_runner=*/render_thread_->GetIOTaskRunner(),
          /*proxy_task_runner=*/agent_group_scheduler_->DefaultTaskRunner()),
      /*create_pipe_now=*/true);
}

AgentSchedulingGroup::AgentSchedulingGroup(
    RenderThread& render_thread,
    PendingAssociatedReceiver<mojom::AgentSchedulingGroup> receiver)
    : agent_group_scheduler_(
          blink::scheduler::WebThreadScheduler::MainThreadScheduler()
              .CreateWebAgentGroupScheduler()),
      render_thread_(render_thread),
      receiver_(this,
                std::move(receiver),
                agent_group_scheduler_->DefaultTaskRunner()) {
  DCHECK(agent_group_scheduler_);
  DCHECK_EQ(GetMBIMode(), features::MBIMode::kLegacy);
}

AgentSchedulingGroup::~AgentSchedulingGroup() = default;

bool AgentSchedulingGroup::OnMessageReceived(const IPC::Message& message) {
#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  DCHECK_NE(message.routing_id(), MSG_ROUTING_CONTROL);

  auto* listener = GetListener(message.routing_id());
  if (!listener)
    return false;

  return listener->OnMessageReceived(message);
#else
  return false;
#endif
}

void AgentSchedulingGroup::OnBadMessageReceived(const IPC::Message& message) {
  // Not strictly required, since we don't currently do anything with bad
  // messages in the renderer, but if we ever do then this will "just work".
  return ToImpl(*render_thread_).OnBadMessageReceived(message);
}

void AgentSchedulingGroup::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  // The ASG's channel should only be used to bootstrap the ASG mojo interface.
  DCHECK_EQ(interface_name, mojom::AgentSchedulingGroup::Name_);
  DCHECK(!receiver_.is_bound());

  PendingAssociatedReceiver<mojom::AgentSchedulingGroup> pending_receiver(
      std::move(handle));
  receiver_.Bind(std::move(pending_receiver),
                 agent_group_scheduler_->DefaultTaskRunner());
}

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
bool AgentSchedulingGroup::Send(IPC::Message* message) {
  std::unique_ptr<IPC::Message> msg(message);

  if (GetMBIMode() == features::MBIMode::kLegacy)
    return render_thread_->Send(msg.release());

  // This DCHECK is too idealistic for now - messages that are handled by
  // filters are sent control messages since they are intercepted before
  // routing. It is put here as documentation for now, since this code would not
  // be reached until we activate
  // `features::MBIMode::kEnabledPerRenderProcessHost` or
  // `features::MBIMode::kEnabledPerSiteInstance`.
  DCHECK_NE(message->routing_id(), MSG_ROUTING_CONTROL);

  DCHECK(channel_);
  return channel_->Send(msg.release());
}
#endif

void AgentSchedulingGroup::AddFrameRoute(
    const blink::LocalFrameToken& frame_token,
#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
    int routing_id,
#endif
    RenderFrameImpl* render_frame,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(!base::Contains(listener_map_, frame_token));
  listener_map_.insert({frame_token, render_frame});
#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  DCHECK(!base::Contains(routing_id_map_, routing_id));
  routing_id_map_.insert({routing_id, render_frame});
  render_thread_->AddRoute(routing_id, render_frame);
#endif

  // See warning in `GetAssociatedInterface`.
  // Replay any `GetAssociatedInterface` calls for this route.
  auto range = pending_receivers_.equal_range(frame_token);
  for (auto iter = range.first; iter != range.second; ++iter) {
    ReceiverData& data = iter->second;
    render_frame->OnAssociatedInterfaceRequest(data.name,
                                               data.receiver.PassHandle());
  }
  pending_receivers_.erase(range.first, range.second);
#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  render_thread_->AttachTaskRunnerToRoute(routing_id, std::move(task_runner));
#endif
}

void AgentSchedulingGroup::RemoveFrameRoute(
    const blink::LocalFrameToken& frame_token
#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
    ,
    int routing_id
#endif
) {
  DCHECK(base::Contains(listener_map_, frame_token));
  listener_map_.erase(frame_token);
#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  DCHECK(base::Contains(routing_id_map_, routing_id));
  routing_id_map_.erase(routing_id);
  render_thread_->RemoveRoute(routing_id);
#endif
}

void AgentSchedulingGroup::DidUnloadRenderFrame(
    const blink::LocalFrameToken& frame_token) {
  host_remote_->DidUnloadRenderFrame(frame_token);
}

void AgentSchedulingGroup::CreateView(mojom::CreateViewParamsPtr params) {
  RenderThreadImpl& renderer = ToImpl(*render_thread_);
  renderer.SetScrollAnimatorEnabled(
      params->web_preferences.enable_scroll_animator, PassKey());

  CreateWebView(std::move(params),
                /*was_created_by_renderer=*/false,
                /*base_url=*/blink::WebURL());
}

blink::WebView* AgentSchedulingGroup::CreateWebView(
    mojom::CreateViewParamsPtr params,
    bool was_created_by_renderer,
    const blink::WebURL& base_url) {
  TRACE_EVENT0("navigation", "AgentSchedulingGroup::CreateWebView");
  base::ScopedUmaHistogramTimer histogram_timer(
      "Navigation.AgentSchedulingGroup.CreateWebView");
  DCHECK(RenderThread::IsMainThread());

  blink::WebFrame* opener_frame = nullptr;
  if (params->opener_frame_token)
    opener_frame =
        blink::WebFrame::FromFrameToken(params->opener_frame_token.value());

  blink::WebView* web_view = blink::WebView::Create(
      new SelfOwnedWebViewClient(), params->hidden,
      std::move(params->prerender_param),
      params->type == mojom::ViewWidgetType::kFencedFrame
          ? std::make_optional(params->fenced_frame_mode)
          : std::nullopt,
      /*compositing_enabled=*/true, params->never_composited,
      opener_frame ? opener_frame->View() : nullptr,
      std::move(params->blink_page_broadcast), agent_group_scheduler(),
      params->session_storage_namespace_id, params->base_background_color,
      params->browsing_context_group_info, &params->color_provider_colors,
      std::move(params->partitioned_popin_params));

  web_view->SetRendererPreferences(params->renderer_preferences);
  web_view->SetWebPreferences(params->web_preferences);
  web_view->SetPageAttributionSupport(params->attribution_support);

  const bool is_for_nested_main_frame =
      params->type != mojom::ViewWidgetType::kTopLevel;

  switch (params->main_frame->which()) {
    case mojom::CreateMainFrameUnion::Tag::kRemoteParams: {
      auto& remote_params = params->main_frame->get_remote_params();
      CreateRemoteMainFrame(
          remote_params->token,
          std::move(remote_params->frame_interfaces->frame_host),
          std::move(remote_params->frame_interfaces->frame_receiver),
          std::move(remote_params->main_frame_interfaces->main_frame_host),
          std::move(remote_params->main_frame_interfaces->main_frame),
          params->devtools_main_frame_token,
          std::move(params->replication_state), opener_frame, web_view);
      break;
    }
    case mojom::CreateMainFrameUnion::Tag::kLocalParams: {
      RenderFrameImpl::CreateMainFrame(
          *this, web_view, opener_frame, is_for_nested_main_frame,
          /*is_for_scalable_page=*/params->type !=
              mojom::ViewWidgetType::kFencedFrame,
          std::move(params->replication_state),
          params->devtools_main_frame_token,
          std::move(params->main_frame->get_local_params()), base_url);
      break;
    }
    case mojom::CreateMainFrameUnion::Tag::kProvisionalLocalParams: {
      // Create a provisional local main frame and a placeholder RemoteFrame as
      // the main frame for the new WebView. This is used in two instances:
      //
      // 1. A main frame navigation with RenderDocument that commits in the same
      //    renderer process as the current RenderFrame, which will result in a
      //    local -> local frame swap if the navigation commits. Note that the
      //    current RenderFrame and the new RenderFrame are in separate
      //    blink::WebViews, so a local -> local main frame swap must also
      //    handle swapping the state on the blink::WebView/blink::Page.
      //
      //  2. A main frame navigation in a prerendered page. Unlike local ->
      //     local frame swaps for RenderDocument, there is never a current
      //     RenderFrame, so there is no additional complexity to swap the state
      //     on the blink::WebView/blink::Page.
      //
      // Note: a potential remote -> local swap does not go through this path at
      // all; instead, the browser creates a new view with a RemoteFrame as the
      // main frame, and then creates a provisional main LocalFrame with a
      // second IPC.
      //
      // Additional background for local -> local main frame swap:
      // The placeholder RemoteFrame is needed because:
      // - a blink::WebView/blink::Page must have a main frame
      // - but the provisional LocalFrame must not be set as the main frame of
      //   the page yet, as it is still provisional
      //
      // Unlike a potential remote -> local swap, the main RemoteFrame does not
      // have a corresponding RenderFrameProxyHost on the browser side. This is
      // because the potential navigation is within the same SiteInstanceGroup,
      // and a single frame tree node should not have both a RenderFrameHost and
      // a RenderFrameProxyHost.
      //
      // If the potential remote -> local swap is cancelled, the provisional
      // LocalFrame is deleted first, with a separate IPC to delete the
      // blink::WebView (and its corresponding placeholder RemoteFrame).
      //
      // Finally, if the remote -> local swap commits, the frame swapping logic
      // has additional logic to swap a similar placeholder RemoteFrame in the
      // previous blink::WebView to unload the previous RenderFrame.
      //
      // Note: we create the placeholder RemoteFrame with no opener, even if we
      // have an opener_frame. This ensures that the placeholder RemoteFrame is
      // not retained beyond the navigation by the opener's OpenedFrameTracker.

      // Create the placeholder RemoteFrame.
      blink::RemoteFrameToken remote_frame_token;
      CreateRemoteMainFrame(
          remote_frame_token, mojo::NullAssociatedRemote(),
          mojo::NullAssociatedReceiver(), mojo::NullAssociatedRemote(),
          mojo::NullAssociatedReceiver(), params->devtools_main_frame_token,
          params->replication_state.Clone(), nullptr, web_view);

      auto& provisional_local_params =
          params->main_frame->get_provisional_local_params();
      auto& local_params = provisional_local_params->local_params;

      // It does not make swense to reuse the compositor if there is no previous
      // RenderFrame to take it from.
      if (local_params->widget_params->reuse_compositor) {
        DCHECK(provisional_local_params->previous_frame_token);
      }

      // Create the provisional main LocalFrame.
      // TODO(dcheng): RenderFrameImpl::CreateFrame() should probably be split
      // for clarity, but this is left as an exercise for another day.
      RenderFrameImpl::CreateFrame(
          *this, local_params->frame_token, local_params->routing_id,
          std::move(local_params->frame),
          std::move(local_params->interface_broker),
          std::move(local_params->associated_interface_provider_remote),
          provisional_local_params->previous_frame_token ? web_view : nullptr,
          provisional_local_params->previous_frame_token
              ? provisional_local_params->previous_frame_token
              : remote_frame_token,
          params->opener_frame_token,
          /*parent_frame_token=*/std::nullopt,
          /*previous_sibling_frame_token=*/std::nullopt,
          params->devtools_main_frame_token,
          blink::mojom::TreeScopeType::kDocument,
          std::move(params->replication_state),
          std::move(local_params->widget_params),
          /*frame_owner_properties=*/nullptr,
          local_params->is_on_initial_empty_document,
          local_params->document_token,
          std::move(local_params->policy_container), is_for_nested_main_frame);
      break;
    }
  }

  // TODO(davidben): Move this state from Blink into content.
  if (params->window_was_opened_by_another_window)
    web_view->SetOpenedByDOM();

  GetContentClient()->renderer()->WebViewCreated(
      web_view, was_created_by_renderer,
      params->outermost_origin ? &params->outermost_origin.value() : nullptr);
  return web_view;
}

void AgentSchedulingGroup::CreateFrame(mojom::CreateFrameParamsPtr params) {
  TRACE_EVENT0("navigation", "AgentSchedulingGroup::CreateFrame");
  base::ScopedUmaHistogramTimer histogram_timer(
      "Navigation.AgentSchedulingGroup.CreateFrame");
  RenderFrameImpl::CreateFrame(
      *this, params->frame_token, params->routing_id, std::move(params->frame),
      std::move(params->interface_broker),
      std::move(params->associated_interface_provider_remote),
      /*web_view=*/nullptr, params->previous_frame_token,
      params->opener_frame_token, params->parent_frame_token,
      params->previous_sibling_frame_token, params->devtools_frame_token,
      params->tree_scope_type, std::move(params->replication_state),
      std::move(params->widget_params),
      std::move(params->frame_owner_properties),
      params->is_on_initial_empty_document, params->document_token,
      std::move(params->policy_container), params->is_for_nested_main_frame);
}

void AgentSchedulingGroup::CreateSharedStorageWorkletService(
    mojo::PendingReceiver<blink::mojom::SharedStorageWorkletService> receiver,
    blink::mojom::WorkletGlobalScopeCreationParamsPtr
        global_scope_creation_params) {
  blink::WebSharedStorageWorkletThread::Start(
      agent_group_scheduler_->DefaultTaskRunner(), std::move(receiver),
      std::move(global_scope_creation_params));
}

void AgentSchedulingGroup::BindAssociatedInterfaces(
    mojo::PendingAssociatedRemote<mojom::AgentSchedulingGroupHost> remote_host,
    mojo::PendingAssociatedReceiver<mojom::RouteProvider>
        route_provider_receiever) {
  host_remote_.Bind(std::move(remote_host),
                    agent_group_scheduler_->DefaultTaskRunner());
  route_provider_receiver_.Bind(std::move(route_provider_receiever),
                                agent_group_scheduler_->DefaultTaskRunner());
}

void AgentSchedulingGroup::GetRoute(
    const blink::LocalFrameToken& frame_token,
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterfaceProvider>
        receiver) {
  DCHECK(receiver.is_valid());
  associated_interface_provider_receivers_.Add(
      this, std::move(receiver), frame_token,
      agent_group_scheduler_->DefaultTaskRunner());
}

void AgentSchedulingGroup::GetAssociatedInterface(
    const std::string& name,
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterface>
        receiver) {
  const auto& frame_token =
      associated_interface_provider_receivers_.current_context();

  if (auto* listener = GetListener(frame_token)) {
    listener->OnAssociatedInterfaceRequest(name, receiver.PassHandle());
  } else {
    // THIS IS UNSAFE!
    // Associated receivers must be bound immediately or they could drop
    // messages. This is needed short term so the browser side Remote isn't
    // broken even after the corresponding `AddRoute` happens. Browser should
    // avoid calling this before the corresponding `AddRoute`, but this is a
    // short term workaround until that happens.
    pending_receivers_.emplace(frame_token,
                               ReceiverData(name, std::move(receiver)));
  }
}

RenderFrameImpl* AgentSchedulingGroup::GetListener(
    const blink::LocalFrameToken& frame_token) {
  return base::FindPtrOrNull(listener_map_, frame_token);
}

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
RenderFrameImpl* AgentSchedulingGroup::GetListener(int32_t routing_id) {
  return base::FindPtrOrNull(routing_id_map_, routing_id);
}
#endif

}  // namespace content
