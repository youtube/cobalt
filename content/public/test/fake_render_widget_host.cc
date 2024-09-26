// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/fake_render_widget_host.h"

#include "third_party/blink/public/mojom/drag/drag.mojom.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom.h"
#include "third_party/blink/public/mojom/input/touch_event.mojom.h"

namespace content {

FakeRenderWidgetHost::FakeRenderWidgetHost() = default;
FakeRenderWidgetHost::~FakeRenderWidgetHost() = default;

std::pair<mojo::PendingAssociatedRemote<blink::mojom::FrameWidgetHost>,
          mojo::PendingAssociatedReceiver<blink::mojom::FrameWidget>>
FakeRenderWidgetHost::BindNewFrameWidgetInterfaces() {
  frame_widget_host_receiver_.reset();
  frame_widget_remote_.reset();
  return std::make_pair(
      frame_widget_host_receiver_.BindNewEndpointAndPassDedicatedRemote(),
      frame_widget_remote_.BindNewEndpointAndPassDedicatedReceiver());
}

std::pair<mojo::PendingAssociatedRemote<blink::mojom::WidgetHost>,
          mojo::PendingAssociatedReceiver<blink::mojom::Widget>>
FakeRenderWidgetHost::BindNewWidgetInterfaces() {
  widget_host_receiver_.reset();
  widget_remote_.reset();
  return std::make_pair(
      widget_host_receiver_.BindNewEndpointAndPassDedicatedRemote(),
      widget_remote_.BindNewEndpointAndPassDedicatedReceiver());
}

void FakeRenderWidgetHost::AnimateDoubleTapZoomInMainFrame(
    const gfx::Point& tap_point,
    const gfx::Rect& rect_to_zoom) {}

void FakeRenderWidgetHost::ZoomToFindInPageRectInMainFrame(
    const gfx::Rect& rect_to_zoom) {}

void FakeRenderWidgetHost::SetHasTouchEventConsumers(
    blink::mojom::TouchEventConsumersPtr consumers) {}

void FakeRenderWidgetHost::IntrinsicSizingInfoChanged(
    blink::mojom::IntrinsicSizingInfoPtr sizing_info) {}

void FakeRenderWidgetHost::SetCursor(const ui::Cursor& cursor) {}

void FakeRenderWidgetHost::UpdateTooltipUnderCursor(
    const std::u16string& tooltip_text,
    base::i18n::TextDirection text_direction_hint) {}

void FakeRenderWidgetHost::UpdateTooltipFromKeyboard(
    const std::u16string& tooltip_text,
    base::i18n::TextDirection text_direction_hint,
    const gfx::Rect& bounds) {}

void FakeRenderWidgetHost::ClearKeyboardTriggeredTooltip() {}

void FakeRenderWidgetHost::TextInputStateChanged(
    ui::mojom::TextInputStatePtr state) {}

void FakeRenderWidgetHost::SelectionBoundsChanged(
    const gfx::Rect& anchor_rect,
    base::i18n::TextDirection anchor_dir,
    const gfx::Rect& focus_rect,
    base::i18n::TextDirection focus_dir,
    const gfx::Rect& bounding_box,
    bool is_anchor_first) {}

void FakeRenderWidgetHost::CreateFrameSink(
    mojo::PendingReceiver<viz::mojom::CompositorFrameSink>
        compositor_frame_sink_receiver,
    mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient>
        compositor_frame_sink_client) {}

void FakeRenderWidgetHost::RegisterRenderFrameMetadataObserver(
    mojo::PendingReceiver<cc::mojom::RenderFrameMetadataObserverClient>
        render_frame_metadata_observer_client_receiver,
    mojo::PendingRemote<cc::mojom::RenderFrameMetadataObserver>
        render_frame_metadata_observer) {}

void FakeRenderWidgetHost::RequestClosePopup() {}

void FakeRenderWidgetHost::SetPopupBounds(const gfx::Rect& bounds,
                                          SetPopupBoundsCallback callback) {}

void FakeRenderWidgetHost::ShowPopup(const gfx::Rect& initial_rect,
                                     const gfx::Rect& initial_anchor_rect,
                                     ShowPopupCallback callback) {}

void FakeRenderWidgetHost::SetTouchActionFromMain(
    cc::TouchAction touch_action) {}

void FakeRenderWidgetHost::SetPanAction(blink::mojom::PanAction pan_action) {}

void FakeRenderWidgetHost::DidOverscroll(
    blink::mojom::DidOverscrollParamsPtr params) {}

void FakeRenderWidgetHost::DidStartScrollingViewport() {}

void FakeRenderWidgetHost::ImeCancelComposition() {}

void FakeRenderWidgetHost::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& bounds) {
  last_composition_range_ = range;
  last_composition_bounds_ = bounds;
}

void FakeRenderWidgetHost::SetMouseCapture(bool capture) {}

void FakeRenderWidgetHost::RequestMouseLock(bool from_user_gesture,
                                            bool unadjusted_movement,
                                            RequestMouseLockCallback callback) {
}

void FakeRenderWidgetHost::AutoscrollStart(const gfx::PointF& position) {}

void FakeRenderWidgetHost::AutoscrollFling(const gfx::Vector2dF& position) {}

void FakeRenderWidgetHost::AutoscrollEnd() {}

void FakeRenderWidgetHost::StartDragging(
    blink::mojom::DragDataPtr drag_data,
    blink::DragOperationsMask operations_allowed,
    const SkBitmap& bitmap,
    const gfx::Vector2d& cursor_offset_in_dip,
    const gfx::Rect& drag_obj_rect_in_dip,
    blink::mojom::DragEventSourceInfoPtr event_info) {}

blink::mojom::WidgetInputHandler*
FakeRenderWidgetHost::GetWidgetInputHandler() {
  if (!widget_input_handler_) {
    widget_remote_->GetWidgetInputHandler(
        widget_input_handler_.BindNewPipeAndPassReceiver(),
        widget_input_handler_host_.BindNewPipeAndPassRemote());
  }
  return widget_input_handler_.get();
}

blink::mojom::FrameWidgetInputHandler*
FakeRenderWidgetHost::GetFrameWidgetInputHandler() {
  if (!frame_widget_input_handler_) {
    GetWidgetInputHandler()->GetFrameWidgetInputHandler(
        frame_widget_input_handler_.BindNewEndpointAndPassReceiver());
  }
  return frame_widget_input_handler_.get();
}

}  // namespace content
