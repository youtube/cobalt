// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_IOS_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_IOS_H_

#include "base/memory/raw_ptr.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-forward.h"

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "content/browser/renderer_host/browser_compositor_ios.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/common/content_export.h"
#include "ui/accelerated_widget_mac/ca_layer_frame_sink.h"
#include "ui/events/gesture_detection/filtered_gesture_provider.h"

namespace ui {
class DisplayCALayerTree;
enum class DomCode;
}  // namespace ui

namespace content {

class RenderWidgetHost;
class UIViewHolder;

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewIOS
//
//  An object representing the "View" of a rendered web page. This object is
//  responsible for displaying the content of the web page, and integrating with
//  the UIView system. It is the implementation of the RenderWidgetHostView
//  that the cross-platform RenderWidgetHost object uses
//  to display the data.
//
//  Comment excerpted from render_widget_host.h:
//
//    "The lifetime of the RenderWidgetHost* is tied to the render process.
//     If the render process dies, the RenderWidgetHost* goes away and all
//     references to it must become NULL."
//
// RenderWidgetHostView class hierarchy described in render_widget_host_view.h.
class CONTENT_EXPORT RenderWidgetHostViewIOS
    : public RenderWidgetHostViewBase,
      public BrowserCompositorIOSClient,
      public TextInputManager::Observer,
      public ui::GestureProviderClient,
      public ui::CALayerFrameSink {
 public:
  // The view will associate itself with the given widget. The native view must
  // be hooked up immediately to the view hierarchy, or else when it is
  // deleted it will delete this out from under the caller.
  RenderWidgetHostViewIOS(RenderWidgetHost* widget);
  ~RenderWidgetHostViewIOS() override;

  RenderWidgetHostViewIOS(const RenderWidgetHostViewIOS&) = delete;
  RenderWidgetHostViewIOS& operator=(const RenderWidgetHostViewIOS&) = delete;

  // RenderWidgetHostView implementation.
  void InitAsChild(gfx::NativeView parent_view) override;
  void SetSize(const gfx::Size& size) override;
  void SetBounds(const gfx::Rect& rect) override;
  gfx::NativeView GetNativeView() override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  void Focus() override;
  bool HasFocus() override;
  void Hide() override;
  bool IsShowing() override;
  gfx::Rect GetViewBounds() override;
  blink::mojom::PointerLockResult LockMouse(bool) override;
  blink::mojom::PointerLockResult ChangeMouseLock(bool) override;
  void UnlockMouse() override;
  void EnsureSurfaceSynchronizedForWebTest() override;
  uint32_t GetCaptureSequenceNumber() const override;
  void TakeFallbackContentFrom(RenderWidgetHostView* view) override;
  std::unique_ptr<SyntheticGestureTarget> CreateSyntheticGestureTarget()
      override;
  void ClearFallbackSurfaceForCommitPending() override;
  void ResetFallbackToFirstNavigationSurface() override;
  viz::FrameSinkId GetRootFrameSinkId() override;
  const viz::FrameSinkId& GetFrameSinkId() const override;
  const viz::LocalSurfaceId& GetLocalSurfaceId() const override;
  viz::SurfaceId GetCurrentSurfaceId() const override;
  void InitAsPopup(RenderWidgetHostView* parent_host_view,
                   const gfx::Rect& pos,
                   const gfx::Rect& anchor_rect) override;
  void UpdateCursor(const ui::Cursor& cursor) override;
  void SetIsLoading(bool is_loading) override;
  void RenderProcessGone() override;
  void ShowWithVisibility(PageVisibilityState page_visibility) override;
  gfx::Rect GetBoundsInRootWindow() override;
  absl::optional<DisplayFeature> GetDisplayFeature() override;
  void SetDisplayFeatureForTesting(
      const DisplayFeature* display_feature) override;
  void UpdateBackgroundColor() override;
  void NotifyHostAndDelegateOnWasShown(
      blink::mojom::RecordContentToVisibleTimeRequestPtr visible_time_request)
      override;
  void RequestSuccessfulPresentationTimeFromHostOrDelegate(
      blink::mojom::RecordContentToVisibleTimeRequestPtr visible_time_request)
      override;
  void CancelSuccessfulPresentationTimeRequestForHostAndDelegate() override;
  viz::ScopedSurfaceIdAllocator DidUpdateVisualProperties(
      const cc::RenderFrameMetadata& metadata) override;
  void DidNavigate() override;
  bool RequestRepaintForTesting() override;
  void Destroy() override;
  ui::Compositor* GetCompositor() override;

  BrowserCompositorIOS* BrowserCompositor() const {
    return browser_compositor_.get();
  }

  // BrowserCompositorIOS overrides:
  SkColor BrowserCompositorIOSGetGutterColor() override;
  void OnFrameTokenChanged(uint32_t frame_token,
                           base::TimeTicks activation_time) override;
  void DestroyCompositorForShutdown() override {}
  bool OnBrowserCompositorSurfaceIdChanged() override;
  std::vector<viz::SurfaceId> CollectSurfaceIdsForEviction() override;
  display::ScreenInfo GetCurrentScreenInfo() const override;
  void SetCurrentDeviceScaleFactor(float device_scale_factor) override;
  void UpdateScreenInfo() override;
  void TransformPointToRootSurface(gfx::PointF* point) override;
  bool TransformPointToCoordSpaceForView(
      const gfx::PointF& point,
      RenderWidgetHostViewBase* target_view,
      gfx::PointF* transformed_point) override;
  void ProcessAckedTouchEvent(
      const TouchEventWithLatencyInfo& touch,
      blink::mojom::InputEventResultState ack_result) override;

  // ui::CALayerFrameSink overrides:
  void UpdateCALayerTree(const gfx::CALayerParams& ca_layer_params) override;

  // ui::GestureProviderClient implementation.
  void OnGestureEvent(const ui::GestureEventData& gesture) override;
  bool RequiresDoubleTapGestureEvents() const override;

  // TextInputManager::Observer implementation.
  void OnUpdateTextInputStateCalled(TextInputManager* text_input_manager,
                                    RenderWidgetHostViewBase* updated_view,
                                    bool did_update_state) override;

  void SetActive(bool active);
  void OnTouchEvent(blink::WebTouchEvent event);
  void UpdateNativeViewTree(gfx::NativeView view);

  void ImeSetComposition(const std::u16string& text,
                         const std::vector<ui::ImeTextSpan>& spans,
                         const gfx::Range& replacement_range,
                         int selection_start,
                         int selection_end);
  void ImeCommitText(const std::u16string& text,
                     const gfx::Range& replacement_range,
                     int relative_position);
  void ImeFinishComposingText(bool keep_selection);
  void OnFirstResponderChanged();

 private:
  friend class MockPointerLockRenderWidgetHostView;

  RenderWidgetHostImpl* GetActiveWidget();

  void OnDidUpdateVisualPropertiesComplete(
      const cc::RenderFrameMetadata& metadata);
  bool ShouldRouteEvents() const;

  void SendGestureEvent(const blink::WebGestureEvent& event);

  // Provides gesture synthesis given a stream of touch events and touch event
  // acks. This is for generating gesture events from injected touch events.
  ui::FilteredGestureProvider gesture_provider_;
  bool is_first_responder_ = false;
  bool is_getting_focus_ = false;
  bool is_visible_ = false;

  // While the mouse is locked, the cursor is hidden from the user. Mouse events
  // are still generated. However, the position they report is the last known
  // mouse position just as mouse lock was entered; the movement they report
  // indicates what the change in position of the mouse would be had it not been
  // locked.
  bool mouse_locked_ = false;

  // Tracks whether unaccelerated mouse motion events are sent while the mouse
  // is locked.
  bool mouse_lock_unadjusted_movement_ = false;

  // Latest capture sequence number which is incremented when the caller
  // requests surfaces be synchronized via
  // EnsureSurfaceSynchronizedForWebTest().
  uint32_t latest_capture_sequence_number_ = 0u;

  std::unique_ptr<BrowserCompositorIOS> browser_compositor_;
  std::unique_ptr<UIViewHolder> ui_view_;
  std::unique_ptr<ui::DisplayCALayerTree> display_tree_;
  base::WeakPtrFactory<RenderWidgetHostViewIOS> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_IOS_H_
