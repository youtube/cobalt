// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/browser_compositor_view_mac.h"

#import <Cocoa/Cocoa.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/containers/circular_deque.h"
#include "base/lazy_instance.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/features.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/context_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/base/layout.h"
#include "ui/compositor/recyclable_compositor_mac.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace content {

namespace {

// Weak pointers to all BrowserCompositorMac instances, used to
// - Determine if a spare RecyclableCompositorMac should be kept around (one
//   should be only if there exists at least one BrowserCompositorMac).
// - Force all ui::Compositors to be destroyed at shut-down (because the NSView
//   signals to shut down will come in very late, long after things that the
//   ui::Compositor depend on have been destroyed).
//   https://crbug.com/805726
base::LazyInstance<std::set<BrowserCompositorMac*>>::Leaky
    g_browser_compositors;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// BrowserCompositorMac

BrowserCompositorMac::BrowserCompositorMac(
    ui::AcceleratedWidgetMacNSView* accelerated_widget_mac_ns_view,
    BrowserCompositorMacClient* client,
    bool render_widget_host_is_hidden,
    const viz::FrameSinkId& frame_sink_id)
    : client_(client),
      accelerated_widget_mac_ns_view_(accelerated_widget_mac_ns_view),
      weak_factory_(this) {
  g_browser_compositors.Get().insert(this);

  root_layer_ = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
  // Ensure that this layer draws nothing when it does not not have delegated
  // content (otherwise this solid color will be flashed during navigation).
  root_layer_->SetColor(SK_ColorTRANSPARENT);
  delegated_frame_host_ = std::make_unique<DelegatedFrameHost>(
      frame_sink_id, this, true /* should_register_frame_sink_id */);

  SetRenderWidgetHostIsHidden(render_widget_host_is_hidden);
}

BrowserCompositorMac::~BrowserCompositorMac() {
  // Ensure that copy callbacks completed or cancelled during further tear-down
  // do not call back into this.
  weak_factory_.InvalidateWeakPtrs();

  TransitionToState(HasNoCompositor);
  delegated_frame_host_.reset();
  root_layer_.reset();

  size_t num_erased = g_browser_compositors.Get().erase(this);
  DCHECK_EQ(1u, num_erased);
}

DelegatedFrameHost* BrowserCompositorMac::GetDelegatedFrameHost() {
  DCHECK(delegated_frame_host_);
  return delegated_frame_host_.get();
}

bool BrowserCompositorMac::ForceNewSurfaceId() {
  dfh_local_surface_id_allocator_.GenerateId();
  delegated_frame_host_->EmbedSurface(
      dfh_local_surface_id_allocator_.GetCurrentLocalSurfaceId(), dfh_size_dip_,
      cc::DeadlinePolicy::UseExistingDeadline());
  return client_->OnBrowserCompositorSurfaceIdChanged();
}

const gfx::CALayerParams* BrowserCompositorMac::GetLastCALayerParams() const {
  if (!recyclable_compositor_)
    return nullptr;
  return recyclable_compositor_->widget()->GetCALayerParams();
}

viz::FrameSinkId BrowserCompositorMac::GetRootFrameSinkId() {
  if (parent_ui_layer_)
    return parent_ui_layer_->GetCompositor()->frame_sink_id();
  if (recyclable_compositor_)
    return recyclable_compositor_->compositor()->frame_sink_id();
  return viz::FrameSinkId();
}

void BrowserCompositorMac::SetBackgroundColor(SkColor background_color) {
  background_color_ = background_color;
  if (recyclable_compositor_)
    recyclable_compositor_->compositor()->SetBackgroundColor(background_color_);
}

void BrowserCompositorMac::UpdateSurfaceFromNSView(
    const gfx::Size& new_size_dip) {
  display::ScreenInfo current = client_->GetCurrentScreenInfo();

  bool is_resize = !dfh_size_dip_.IsEmpty() && new_size_dip != dfh_size_dip_;
  bool needs_new_surface_id =
      new_size_dip != dfh_size_dip_ ||
      current.device_scale_factor != dfh_device_scale_factor_;

  dfh_size_dip_ = new_size_dip;
  dfh_device_scale_factor_ = current.device_scale_factor;

  // The device scale factor is always an integer, so the result here is also
  // an integer.
  dfh_size_pixels_ = gfx::ToRoundedSize(
      gfx::ConvertSizeToPixels(dfh_size_dip_, current.device_scale_factor));
  root_layer_->SetBounds(gfx::Rect(dfh_size_dip_));

  if (needs_new_surface_id) {
    dfh_local_surface_id_allocator_.GenerateId();
    delegated_frame_host_->EmbedSurface(
        dfh_local_surface_id_allocator_.GetCurrentLocalSurfaceId(),
        dfh_size_dip_, GetDeadlinePolicy(is_resize));
  }

  if (recyclable_compositor_) {
    recyclable_compositor_->UpdateSurface(dfh_size_pixels_,
                                          current.device_scale_factor,
                                          current.display_color_spaces);
  }
}

void BrowserCompositorMac::UpdateSurfaceFromChild(
    bool auto_resize_enabled,
    float new_device_scale_factor,
    const gfx::Size& new_size_in_pixels,
    const viz::LocalSurfaceId& child_local_surface_id) {
  if (dfh_local_surface_id_allocator_.UpdateFromChild(child_local_surface_id)) {
    if (auto_resize_enabled) {
      client_->SetCurrentDeviceScaleFactor(new_device_scale_factor);
      display::ScreenInfo current = client_->GetCurrentScreenInfo();
      // TODO(danakj): We should avoid lossy conversions to integer DIPs.
      dfh_size_dip_ = gfx::ToFlooredSize(gfx::ConvertSizeToDips(
          new_size_in_pixels, current.device_scale_factor));
      dfh_size_pixels_ = new_size_in_pixels;
      dfh_device_scale_factor_ = new_device_scale_factor;
      root_layer_->SetBounds(gfx::Rect(dfh_size_dip_));
      if (recyclable_compositor_) {
        recyclable_compositor_->UpdateSurface(dfh_size_pixels_,
                                              current.device_scale_factor,
                                              current.display_color_spaces);
      }
    }
    delegated_frame_host_->EmbedSurface(
        dfh_local_surface_id_allocator_.GetCurrentLocalSurfaceId(),
        dfh_size_dip_, GetDeadlinePolicy(true /* is_resize */));
  }
  client_->OnBrowserCompositorSurfaceIdChanged();
}

void BrowserCompositorMac::SetRenderWidgetHostIsHidden(bool hidden) {
  render_widget_host_is_hidden_ = hidden;
  UpdateState();
  if (state_ == UseParentLayerCompositor) {
    // UpdateState might not call WasShown when showing a frame using the same
    // ParentLayerCompositor, since it returns early on a no-op state
    // transition.
    delegated_frame_host_->WasShown(GetRendererLocalSurfaceId(), dfh_size_dip_,
                                    {} /* record_tab_switch_time_request */);
  }
}

void BrowserCompositorMac::SetViewVisible(bool visible) {
  root_layer_->SetVisible(visible);
}

void BrowserCompositorMac::UpdateState() {
  // Always use the parent ui::Layer's ui::Compositor if available.
  if (parent_ui_layer_) {
    TransitionToState(UseParentLayerCompositor);
    return;
  }

  // If the host is visible and a compositor is required then create one.
  if (!render_widget_host_is_hidden_) {
    TransitionToState(HasOwnCompositor);
    return;
  }

  // Otherwise put the compositor up for recycling.
  TransitionToState(HasNoCompositor);
}

void BrowserCompositorMac::TransitionToState(State new_state) {
  // Skip if there is no change to make.
  bool is_no_op = false;
  if (state_ == new_state) {
    if (state_ == UseParentLayerCompositor)
      is_no_op = parent_ui_layer_ == root_layer_->parent();
    else
      is_no_op = true;
  }
  if (is_no_op)
    return;

  // First, detach from the current compositor, if there is one.
  delegated_frame_host_->DetachFromCompositor();
  if (state_ == UseParentLayerCompositor) {
    DCHECK(root_layer_->parent());
    state_ = HasNoCompositor;
    root_layer_->parent()->RemoveObserver(this);
    root_layer_->parent()->Remove(root_layer_.get());
  }
  if (state_ == HasOwnCompositor) {
    recyclable_compositor_->widget()->ResetNSView();
    recyclable_compositor_->compositor()->SetRootLayer(nullptr);
    recyclable_compositor_.reset();
  }

  // The compositor is now detached. If this is the target state, we're done.
  state_ = HasNoCompositor;
  if (new_state == HasNoCompositor) {
    // Don't transiently hide the DelegatedFrameHost because that can cause the
    // current frame to be inappropriately evicted.
    // https://crbug.com/897156
    delegated_frame_host_->WasHidden(DelegatedFrameHost::HiddenCause::kOther);
    return;
  }

  // Attach to the new compositor.
  if (new_state == UseParentLayerCompositor) {
    DCHECK(parent_ui_layer_);
    parent_ui_layer_->Add(root_layer_.get());
    parent_ui_layer_->AddObserver(this);
    state_ = UseParentLayerCompositor;
  }
  if (new_state == HasOwnCompositor) {
    recyclable_compositor_ = std::make_unique<ui::RecyclableCompositorMac>(
        content::GetContextFactory());
    display::ScreenInfo current = client_->GetCurrentScreenInfo();
    recyclable_compositor_->UpdateSurface(dfh_size_pixels_,
                                          current.device_scale_factor,
                                          current.display_color_spaces);
    recyclable_compositor_->compositor()->SetRootLayer(root_layer_.get());
    recyclable_compositor_->compositor()->SetBackgroundColor(background_color_);
    recyclable_compositor_->widget()->SetNSView(
        accelerated_widget_mac_ns_view_);
    recyclable_compositor_->Unsuspend();
    state_ = HasOwnCompositor;
  }
  DCHECK_EQ(state_, new_state);
  delegated_frame_host_->AttachToCompositor(GetCompositor());
  delegated_frame_host_->WasShown(GetRendererLocalSurfaceId(), dfh_size_dip_,
                                  {} /* record_tab_switch_time_request */);
}

// static
void BrowserCompositorMac::DisableRecyclingForShutdown() {
  // Ensure that the client has destroyed its BrowserCompositorViewMac before
  // it dependencies are destroyed.
  // https://crbug.com/805726
  while (!g_browser_compositors.Get().empty()) {
    BrowserCompositorMac* browser_compositor =
        *g_browser_compositors.Get().begin();
    browser_compositor->client_->DestroyCompositorForShutdown();
  }
}

void BrowserCompositorMac::TakeFallbackContentFrom(
    BrowserCompositorMac* other) {
  delegated_frame_host_->TakeFallbackContentFrom(
      other->delegated_frame_host_.get());
}

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost, public:

ui::Layer* BrowserCompositorMac::DelegatedFrameHostGetLayer() const {
  return root_layer_.get();
}

bool BrowserCompositorMac::DelegatedFrameHostIsVisible() const {
  return state_ != HasNoCompositor;
}

SkColor BrowserCompositorMac::DelegatedFrameHostGetGutterColor() const {
  return client_->BrowserCompositorMacGetGutterColor();
}

void BrowserCompositorMac::OnFrameTokenChanged(
    uint32_t frame_token,
    base::TimeTicks activation_time) {
  client_->OnFrameTokenChanged(frame_token, activation_time);
}

float BrowserCompositorMac::GetDeviceScaleFactor() const {
  return dfh_device_scale_factor_;
}

void BrowserCompositorMac::InvalidateLocalSurfaceIdOnEviction() {
  dfh_local_surface_id_allocator_.Invalidate();
}

std::vector<viz::SurfaceId>
BrowserCompositorMac::CollectSurfaceIdsForEviction() {
  return client_->CollectSurfaceIdsForEviction();
}

bool BrowserCompositorMac::ShouldShowStaleContentOnEviction() {
  return false;
}

void BrowserCompositorMac::DidNavigate() {
  if (render_widget_host_is_hidden_) {
    // Navigating while hidden should not allocate a new LocalSurfaceID. Once
    // sizes are ready, or we begin to Show, we can then allocate the new
    // LocalSurfaceId.
    dfh_local_surface_id_allocator_.Invalidate();
  } else {
    // The first navigation does not need a new LocalSurfaceID. The renderer can
    // use the ID that was already provided.
    if (!is_first_navigation_) {
      dfh_local_surface_id_allocator_.GenerateId();
    }
    delegated_frame_host_->EmbedSurface(
        dfh_local_surface_id_allocator_.GetCurrentLocalSurfaceId(),
        dfh_size_dip_, cc::DeadlinePolicy::UseExistingDeadline());
    client_->OnBrowserCompositorSurfaceIdChanged();
  }

  delegated_frame_host_->DidNavigate();
  is_first_navigation_ = false;
}

void BrowserCompositorMac::SetParentUiLayer(ui::Layer* new_parent_ui_layer) {
  if (new_parent_ui_layer)
    DCHECK(new_parent_ui_layer->GetCompositor());

  // Set |parent_ui_layer_| to the new value, which potentially not match the
  // value of |root_layer_->parent()|. The call to UpdateState will re-parent
  // |root_layer_|.
  DCHECK_EQ(root_layer_->parent(), parent_ui_layer_);
  parent_ui_layer_ = new_parent_ui_layer;
  UpdateState();
  DCHECK_EQ(root_layer_->parent(), parent_ui_layer_);
}

void BrowserCompositorMac::ForceNewSurfaceForTesting() {
  float current_device_scale_factor =
      client_->GetCurrentScreenInfo().device_scale_factor;
  client_->SetCurrentDeviceScaleFactor(current_device_scale_factor * 2.0f);
  UpdateSurfaceFromNSView(dfh_size_dip_);
}

viz::ScopedSurfaceIdAllocator
BrowserCompositorMac::GetScopedRendererSurfaceIdAllocator(
    base::OnceCallback<void()> allocation_task) {
  return viz::ScopedSurfaceIdAllocator(&dfh_local_surface_id_allocator_,
                                       std::move(allocation_task));
}

const viz::LocalSurfaceId& BrowserCompositorMac::GetRendererLocalSurfaceId() {
  if (!dfh_local_surface_id_allocator_.HasValidLocalSurfaceId())
    dfh_local_surface_id_allocator_.GenerateId();

  return dfh_local_surface_id_allocator_.GetCurrentLocalSurfaceId();
}

void BrowserCompositorMac::TransformPointToRootSurface(gfx::PointF* point) {
  gfx::Transform transform_to_root;
  if (parent_ui_layer_)
    parent_ui_layer_->GetTargetTransformRelativeTo(nullptr, &transform_to_root);
  *point = transform_to_root.MapPoint(*point);
}

void BrowserCompositorMac::LayerDestroyed(ui::Layer* layer) {
  DCHECK_EQ(layer, parent_ui_layer_);
  SetParentUiLayer(nullptr);
}

ui::Compositor* BrowserCompositorMac::GetCompositor() const {
  if (parent_ui_layer_)
    return parent_ui_layer_->GetCompositor();
  if (recyclable_compositor_)
    return recyclable_compositor_->compositor();
  return nullptr;
}

cc::DeadlinePolicy BrowserCompositorMac::GetDeadlinePolicy(
    bool is_resize) const {
  // Determined empirically for smoothness. Don't wait for non-resize frames,
  // as it can cause jank at new tab creation.
  // https://crbug.com/855364
  uint32_t frames_to_wait = is_resize ? 8 : 0;

  // When using the RecyclableCompositor, never wait for frames to arrive
  // (surface sync is managed by the Suspend/Unsuspend lock).
  if (recyclable_compositor_)
    frames_to_wait = 0;

  return cc::DeadlinePolicy::UseSpecifiedDeadline(frames_to_wait);
}

}  // namespace content
