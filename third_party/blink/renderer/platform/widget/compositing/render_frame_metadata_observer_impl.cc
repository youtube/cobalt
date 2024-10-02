// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/compositing/render_frame_metadata_observer_impl.h"

#include <cmath>

#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/mojom/render_frame_metadata.mojom-shared.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"

namespace blink {

namespace {
#if BUILDFLAG(IS_ANDROID)
constexpr float kEdgeThreshold = 10.0f;
#endif
}  // namespace

RenderFrameMetadataObserverImpl::RenderFrameMetadataObserverImpl(
    mojo::PendingReceiver<cc::mojom::blink::RenderFrameMetadataObserver>
        receiver,
    mojo::PendingRemote<cc::mojom::blink::RenderFrameMetadataObserverClient>
        client_remote)
    : receiver_(std::move(receiver)),
      client_remote_(std::move(client_remote)) {}

RenderFrameMetadataObserverImpl::~RenderFrameMetadataObserverImpl() {}

void RenderFrameMetadataObserverImpl::BindToCurrentSequence() {
  DCHECK(receiver_.is_valid());
  render_frame_metadata_observer_receiver_.Bind(std::move(receiver_));
  render_frame_metadata_observer_client_.Bind(std::move(client_remote_));
}

void RenderFrameMetadataObserverImpl::OnRenderFrameSubmission(
    const cc::RenderFrameMetadata& render_frame_metadata,
    viz::CompositorFrameMetadata* compositor_frame_metadata,
    bool force_send) {
  // By default only report metadata changes for fields which have a low
  // frequency of change. However if there are changes in high frequency
  // fields these can be reported while testing is enabled.
  bool send_metadata = false;
  bool needs_activation_notification = true;
  if (render_frame_metadata_observer_client_) {
    if (report_all_frame_submissions_for_testing_enabled_) {
      last_frame_token_ = compositor_frame_metadata->frame_token;
      compositor_frame_metadata->send_frame_token_to_embedder = true;
      render_frame_metadata_observer_client_->OnFrameSubmissionForTesting(
          last_frame_token_);
      send_metadata = !last_render_frame_metadata_ ||
                      *last_render_frame_metadata_ != render_frame_metadata;
    } else {
      send_metadata = !last_render_frame_metadata_ ||
                      ShouldSendRenderFrameMetadata(
                          *last_render_frame_metadata_, render_frame_metadata,
                          &needs_activation_notification);
    }
    send_metadata |= force_send;
  }

#if BUILDFLAG(IS_ANDROID)
  bool is_frequency_all_updates =
      root_scroll_offset_update_frequency_ ==
      cc::mojom::blink::RootScrollOffsetUpdateFrequency::kAllUpdates;
  const bool send_root_scroll_offset_changed =
      is_frequency_all_updates && !send_metadata &&
      render_frame_metadata_observer_client_ && last_render_frame_metadata_ &&
      last_render_frame_metadata_->root_scroll_offset !=
          render_frame_metadata.root_scroll_offset &&
      render_frame_metadata.root_scroll_offset.has_value();
#endif

  // Always cache the full metadata, so that it can correctly be sent upon
  // ReportAllFrameSubmissionsForTesting or on android, which notifies on any
  // root scroll offset change. This must only be done after we've compared the
  // two for changes.
  last_render_frame_metadata_ = render_frame_metadata;

  // If the metadata is different, updates all the observers; or the metadata is
  // generated for first time and same as the default value, update the default
  // value to all the observers.
  if (send_metadata && render_frame_metadata_observer_client_) {
    auto metadata_copy = render_frame_metadata;
#if !BUILDFLAG(IS_ANDROID)
    // On non-Android, sending |root_scroll_offset| outside of tests would
    // leave the browser process with out of date information. It is an
    // optional parameter which we clear here.
    if (!report_all_frame_submissions_for_testing_enabled_)
      metadata_copy.root_scroll_offset = absl::nullopt;
#endif

    last_frame_token_ = compositor_frame_metadata->frame_token;
    compositor_frame_metadata->send_frame_token_to_embedder =
        needs_activation_notification;
    render_frame_metadata_observer_client_->OnRenderFrameMetadataChanged(
        needs_activation_notification ? last_frame_token_ : 0u, metadata_copy);
#if BUILDFLAG(IS_ANDROID)
    last_root_scroll_offset_android_ = metadata_copy.root_scroll_offset;
#endif
    TRACE_EVENT_WITH_FLOW1(
        TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
        "RenderFrameMetadataObserverImpl::OnRenderFrameSubmission",
        metadata_copy.local_surface_id &&
                metadata_copy.local_surface_id->is_valid()
            ? metadata_copy.local_surface_id->submission_trace_id() +
                  metadata_copy.local_surface_id->embed_trace_id()
            : 0,
        TRACE_EVENT_FLAG_FLOW_OUT, "local_surface_id",
        metadata_copy.local_surface_id
            ? metadata_copy.local_surface_id->ToString()
            : "null");
  }

#if BUILDFLAG(IS_ANDROID)
  if (send_root_scroll_offset_changed) {
    DCHECK(!send_metadata);
    render_frame_metadata_observer_client_->OnRootScrollOffsetChanged(
        *render_frame_metadata.root_scroll_offset);
    last_root_scroll_offset_android_ =
        *render_frame_metadata.root_scroll_offset;
  }
#endif

  // Always cache the initial frame token, so that if a test connects later on
  // it can be notified of the initial state.
  if (!last_frame_token_) {
    last_frame_token_ = compositor_frame_metadata->frame_token;
    compositor_frame_metadata->send_frame_token_to_embedder =
        needs_activation_notification;
  }
}

#if BUILDFLAG(IS_ANDROID)
void RenderFrameMetadataObserverImpl::UpdateRootScrollOffsetUpdateFrequency(
    cc::mojom::blink::RootScrollOffsetUpdateFrequency frequency) {
  root_scroll_offset_update_frequency_ = frequency;

  if (frequency ==
      cc::mojom::blink::RootScrollOffsetUpdateFrequency::kAllUpdates) {
    SendLastRenderFrameMetadata();
  }
}
#endif

void RenderFrameMetadataObserverImpl::ReportAllFrameSubmissionsForTesting(
    bool enabled) {
  report_all_frame_submissions_for_testing_enabled_ = enabled;

  if (enabled)
    SendLastRenderFrameMetadata();
}

void RenderFrameMetadataObserverImpl::SendLastRenderFrameMetadata() {
  if (!last_frame_token_)
    return;

  // When enabled for testing send the cached metadata.
  DCHECK(render_frame_metadata_observer_client_);
  DCHECK(last_render_frame_metadata_.has_value());
  render_frame_metadata_observer_client_->OnRenderFrameMetadataChanged(
      last_frame_token_, *last_render_frame_metadata_);
}

bool RenderFrameMetadataObserverImpl::ShouldSendRenderFrameMetadata(
    const cc::RenderFrameMetadata& rfm1,
    const cc::RenderFrameMetadata& rfm2,
    bool* needs_activation_notification) const {
  if (rfm1.root_background_color != rfm2.root_background_color ||
      rfm1.is_scroll_offset_at_top != rfm2.is_scroll_offset_at_top ||
      rfm1.selection != rfm2.selection ||
      rfm1.page_scale_factor != rfm2.page_scale_factor ||
      rfm1.external_page_scale_factor != rfm2.external_page_scale_factor ||
      rfm1.is_mobile_optimized != rfm2.is_mobile_optimized ||
      rfm1.delegated_ink_metadata != rfm2.delegated_ink_metadata ||
      rfm1.device_scale_factor != rfm2.device_scale_factor ||
      rfm1.viewport_size_in_pixels != rfm2.viewport_size_in_pixels ||
      rfm1.top_controls_height != rfm2.top_controls_height ||
      rfm1.top_controls_shown_ratio != rfm2.top_controls_shown_ratio ||
      rfm1.local_surface_id != rfm2.local_surface_id ||
      rfm2.new_vertical_scroll_direction !=
          viz::VerticalScrollDirection::kNull) {
    *needs_activation_notification = true;
    return true;
  }

#if BUILDFLAG(IS_ANDROID)
  if (rfm1.bottom_controls_height != rfm2.bottom_controls_height ||
      rfm1.bottom_controls_shown_ratio != rfm2.bottom_controls_shown_ratio ||
      rfm1.top_controls_min_height_offset !=
          rfm2.top_controls_min_height_offset ||
      rfm1.bottom_controls_min_height_offset !=
          rfm2.bottom_controls_min_height_offset ||
      rfm1.min_page_scale_factor != rfm2.min_page_scale_factor ||
      rfm1.max_page_scale_factor != rfm2.max_page_scale_factor ||
      rfm1.root_overflow_y_hidden != rfm2.root_overflow_y_hidden ||
      rfm1.scrollable_viewport_size != rfm2.scrollable_viewport_size ||
      rfm1.root_layer_size != rfm2.root_layer_size ||
      rfm1.has_transparent_background != rfm2.has_transparent_background) {
    *needs_activation_notification = true;
    return true;
  }

  gfx::PointF old_root_scroll_offset =
      rfm1.root_scroll_offset.value_or(gfx::PointF());
  gfx::PointF new_root_scroll_offset =
      rfm2.root_scroll_offset.value_or(gfx::PointF());
  gfx::RectF old_viewport_rect(
      gfx::PointF(old_root_scroll_offset.x(), old_root_scroll_offset.y()),
      rfm1.scrollable_viewport_size);
  gfx::RectF new_viewport_rect(
      gfx::PointF(new_root_scroll_offset.x(), new_root_scroll_offset.y()),
      rfm2.scrollable_viewport_size);
  gfx::RectF new_root_layer_rect(rfm2.root_layer_size);
  bool at_left_or_right_edge =
      rfm2.root_layer_size.width() > rfm2.scrollable_viewport_size.width() &&
      (std::abs(new_viewport_rect.right() - new_root_layer_rect.right()) <
           kEdgeThreshold ||
       std::abs(new_viewport_rect.x() - new_root_layer_rect.x()) <
           kEdgeThreshold);

  bool at_top_or_bottom_edge =
      rfm2.root_layer_size.height() > rfm2.scrollable_viewport_size.height() &&
      (std::abs(new_viewport_rect.y() - new_root_layer_rect.y()) <
           kEdgeThreshold ||
       std::abs(new_viewport_rect.bottom() - new_root_layer_rect.bottom()) <
           kEdgeThreshold);

  if (old_viewport_rect != new_viewport_rect &&
      (at_left_or_right_edge || at_top_or_bottom_edge)) {
    *needs_activation_notification = false;
    return true;
  }
#endif

  *needs_activation_notification = false;
  return false;
}

#if BUILDFLAG(IS_ANDROID)
void RenderFrameMetadataObserverImpl::DidEndScroll() {
  if (!last_render_frame_metadata_.has_value()) {
    return;
  }

  auto root_scroll_offset = last_render_frame_metadata_->root_scroll_offset;
  if (!root_scroll_offset.has_value() ||
      root_scroll_offset == last_root_scroll_offset_android_) {
    return;
  }

  if (root_scroll_offset_update_frequency_ !=
      cc::mojom::blink::RootScrollOffsetUpdateFrequency::kOnScrollEnd) {
    return;
  }

  render_frame_metadata_observer_client_->OnRootScrollOffsetChanged(
      root_scroll_offset.value());
  last_root_scroll_offset_android_ = root_scroll_offset;
}
#endif

}  // namespace blink
