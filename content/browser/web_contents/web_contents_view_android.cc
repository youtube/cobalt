// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_android.h"

#include <memory>
#include <utility>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "cc/layers/layer.h"
#include "cc/slim/layer.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/browser/android/content_ui_event_handler.h"
#include "content/browser/android/drop_data_android.h"
#include "content/browser/android/gesture_listener_manager.h"
#include "content/browser/android/select_popup.h"
#include "content/browser/android/selection/selection_popup_controller.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/android/jar_jni/DragEvent_jni.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/drop_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/android/overscroll_refresh_handler.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/display/display_util.h"
#include "ui/display/screen.h"
#include "ui/events/android/drag_event_android.h"
#include "ui/events/android/gesture_event_android.h"
#include "ui/events/android/key_event_android.h"
#include "ui/events/android/motion_event_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/android/view_configuration.h"
#include "ui/gfx/image/image_skia.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ScopedJavaLocalRef;

namespace content {

namespace {

// Returns the minimum distance in DIPs, for drag event being considered as an
// intentional drag.
int DragMovementThresholdDip() {
  static int radius = features::kTouchDragMovementThresholdDip.Get();
  return radius;
}

// True if we want to disable Android native event batching and use
// compositor event queue.
bool ShouldRequestUnbufferedDispatch() {
  static bool should_request_unbuffered_dispatch =
      base::android::BuildInfo::GetInstance()->sdk_int() >=
          base::android::SDK_VERSION_LOLLIPOP &&
      !content::GetContentClient()->UsingSynchronousCompositing();
  return should_request_unbuffered_dispatch;
}

bool IsDragAndDropEnabled() {
  // Cache the feature flag value so it isn't queried on every drag start.
  static const bool drag_feature_enabled =
      base::FeatureList::IsEnabled(features::kTouchDragAndContextMenu);
  return drag_feature_enabled;
}

bool IsDragEnabledForDropData(const DropData& drop_data) {
  if (!IsDragAndDropEnabled()) {
    return drop_data.text.has_value();
  }
  return !drop_data.url.is_empty() || !drop_data.file_contents.empty() ||
         drop_data.text.has_value();
}
}

// static
void SynchronousCompositor::SetClientForWebContents(
    WebContents* contents,
    SynchronousCompositorClient* client) {
  DCHECK(contents);
  DCHECK(client);
  WebContentsViewAndroid* wcva = static_cast<WebContentsViewAndroid*>(
      static_cast<WebContentsImpl*>(contents)->GetView());
  DCHECK(!wcva->synchronous_compositor_client());
  wcva->set_synchronous_compositor_client(client);
  RenderWidgetHostViewAndroid* rwhv = static_cast<RenderWidgetHostViewAndroid*>(
      contents->GetRenderWidgetHostView());
  if (rwhv)
    rwhv->SetSynchronousCompositorClient(client);
}

std::unique_ptr<WebContentsView> CreateWebContentsView(
    WebContentsImpl* web_contents,
    std::unique_ptr<WebContentsViewDelegate> delegate,
    RenderViewHostDelegateView** render_view_host_delegate_view) {
  auto rv = std::make_unique<WebContentsViewAndroid>(web_contents,
                                                     std::move(delegate));
  *render_view_host_delegate_view = rv.get();
  return rv;
}

WebContentsViewAndroid::WebContentsViewAndroid(
    WebContentsImpl* web_contents,
    std::unique_ptr<WebContentsViewDelegate> delegate)
    : web_contents_(web_contents),
      delegate_(std::move(delegate)),
      view_(ui::ViewAndroid::LayoutType::NORMAL),
      synchronous_compositor_client_(nullptr) {
  view_.SetLayer(cc::slim::Layer::Create());
  view_.set_event_handler(this);
}

WebContentsViewAndroid::~WebContentsViewAndroid() {
  if (view_.GetLayer())
    view_.GetLayer()->RemoveFromParent();
  view_.set_event_handler(nullptr);
}

void WebContentsViewAndroid::SetContentUiEventHandler(
    std::unique_ptr<ContentUiEventHandler> handler) {
  content_ui_event_handler_ = std::move(handler);
}

void WebContentsViewAndroid::SetOverscrollRefreshHandler(
    std::unique_ptr<ui::OverscrollRefreshHandler> overscroll_refresh_handler) {
  overscroll_refresh_handler_ = std::move(overscroll_refresh_handler);
  auto* rwhv = web_contents_->GetRenderWidgetHostView();
  if (rwhv) {
    static_cast<RenderWidgetHostViewAndroid*>(rwhv)
        ->OnOverscrollRefreshHandlerAvailable();
  }
}

ui::OverscrollRefreshHandler*
WebContentsViewAndroid::GetOverscrollRefreshHandler() const {
  return overscroll_refresh_handler_.get();
}

gfx::NativeView WebContentsViewAndroid::GetNativeView() const {
  return const_cast<gfx::NativeView>(&view_);
}

gfx::NativeView WebContentsViewAndroid::GetContentNativeView() const {
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (rwhv)
    return rwhv->GetNativeView();

  // TODO(sievers): This should return null.
  return GetNativeView();
}

RenderWidgetHostViewAndroid*
WebContentsViewAndroid::GetRenderWidgetHostViewAndroid() {
  RenderWidgetHostView* rwhv = nullptr;
  rwhv = web_contents_->GetRenderWidgetHostView();
  return static_cast<RenderWidgetHostViewAndroid*>(rwhv);
}

gfx::NativeWindow WebContentsViewAndroid::GetTopLevelNativeWindow() const {
  return view_.GetWindowAndroid();
}

gfx::Rect WebContentsViewAndroid::GetContainerBounds() const {
  return GetViewBounds();
}

void WebContentsViewAndroid::SetPageTitle(const std::u16string& title) {
  // Do nothing.
}

void WebContentsViewAndroid::Focus() {
  auto* rwhv = web_contents_->GetRenderWidgetHostView();
  if (rwhv)
    static_cast<RenderWidgetHostViewAndroid*>(rwhv)->Focus();
}

void WebContentsViewAndroid::SetInitialFocus() {
  if (web_contents_->FocusLocationBarByDefault())
    web_contents_->SetFocusToLocationBar();
  else
    Focus();
}

void WebContentsViewAndroid::StoreFocus() {
  NOTIMPLEMENTED();
}

void WebContentsViewAndroid::RestoreFocus() {
  NOTIMPLEMENTED();
}

void WebContentsViewAndroid::FocusThroughTabTraversal(bool reverse) {
  web_contents_->GetRenderViewHost()->SetInitialFocus(reverse);
}

DropData* WebContentsViewAndroid::GetDropData() const {
  NOTIMPLEMENTED();
  return NULL;
}

gfx::Rect WebContentsViewAndroid::GetViewBounds() const {
  return gfx::Rect(view_.GetSize());
}

void WebContentsViewAndroid::CreateView(gfx::NativeView context) {}

RenderWidgetHostViewBase* WebContentsViewAndroid::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  if (render_widget_host->GetView()) {
    // During testing, the view will already be set up in most cases to the
    // test view, so we don't want to clobber it with a real one. To verify that
    // this actually is happening (and somebody isn't accidentally creating the
    // view twice), we check for the RVH Factory, which will be set when we're
    // making special ones (which go along with the special views).
    DCHECK(RenderViewHostFactory::has_factory());
    return static_cast<RenderWidgetHostViewBase*>(
        render_widget_host->GetView());
  }
  // Note that while this instructs the render widget host to reference
  // |native_view_|, this has no effect without also instructing the
  // native view (i.e. ContentView) how to obtain a reference to this widget in
  // order to paint it.
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(render_widget_host);
  auto* rwhv = new RenderWidgetHostViewAndroid(rwhi, &view_);
  rwhv->SetSynchronousCompositorClient(synchronous_compositor_client_);
  return rwhv;
}

RenderWidgetHostViewBase* WebContentsViewAndroid::CreateViewForChildWidget(
    RenderWidgetHost* render_widget_host) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(render_widget_host);
  return new RenderWidgetHostViewAndroid(rwhi, nullptr);
}

void WebContentsViewAndroid::RenderViewReady() {
  if (device_orientation_ == 0)
    return;
  auto* rwhva = GetRenderWidgetHostViewAndroid();
  if (rwhva)
    rwhva->UpdateScreenInfo();

  web_contents_->OnScreenOrientationChange();
}

void WebContentsViewAndroid::RenderViewHostChanged(RenderViewHost* old_host,
                                                   RenderViewHost* new_host) {
  if (old_host) {
    auto* rwhv = old_host->GetWidget()->GetView();
    if (rwhv && rwhv->GetNativeView()) {
      static_cast<RenderWidgetHostViewAndroid*>(rwhv)->UpdateNativeViewTree(
          nullptr);
    }
  }

  auto* rwhv = new_host->GetWidget()->GetView();
  if (rwhv && rwhv->GetNativeView()) {
    static_cast<RenderWidgetHostViewAndroid*>(rwhv)->UpdateNativeViewTree(
        GetNativeView());
    SetFocus(view_.HasFocus());
  }
}

void WebContentsViewAndroid::SetFocus(bool focused) {
  auto* rwhva = GetRenderWidgetHostViewAndroid();
  if (!rwhva)
    return;
  if (focused)
    rwhva->GotFocus();
  else
    rwhva->LostFocus();
}

void WebContentsViewAndroid::SetOverscrollControllerEnabled(bool enabled) {
}

void WebContentsViewAndroid::OnCapturerCountChanged() {}

void WebContentsViewAndroid::FullscreenStateChanged(bool is_fullscreen) {
  if (is_fullscreen && select_popup_)
    select_popup_->HideMenu();
}

void WebContentsViewAndroid::UpdateWindowControlsOverlay(
    const gfx::Rect& bounding_rect) {}

void WebContentsViewAndroid::ShowContextMenu(RenderFrameHost& render_frame_host,
                                             const ContextMenuParams& params) {
  if (is_active_drag_ && drag_exceeded_movement_threshold_)
    return;

  auto* rwhv = static_cast<RenderWidgetHostViewAndroid*>(
      web_contents_->GetRenderWidgetHostView());

  // See if context menu is handled by SelectionController as a selection menu.
  // If not, use the delegate to show it.
  if (rwhv && rwhv->ShowSelectionMenu(&render_frame_host, params))
    return;

  if (delegate_)
    delegate_->ShowContextMenu(render_frame_host, params);
}

SelectPopup* WebContentsViewAndroid::GetSelectPopup() {
  if (!select_popup_)
    select_popup_ = std::make_unique<SelectPopup>(web_contents_);
  return select_popup_.get();
}

void WebContentsViewAndroid::ShowPopupMenu(
    RenderFrameHost* render_frame_host,
    mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client,
    const gfx::Rect& bounds,
    int item_height,
    double item_font_size,
    int selected_item,
    std::vector<blink::mojom::MenuItemPtr> menu_items,
    bool right_aligned,
    bool allow_multiple_selection) {
  GetSelectPopup()->ShowMenu(std::move(popup_client), bounds,
                             std::move(menu_items), selected_item,
                             allow_multiple_selection, right_aligned);
}

void WebContentsViewAndroid::StartDragging(
    const DropData& drop_data,
    blink::DragOperationsMask allowed_ops,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& cursor_offset,
    const gfx::Rect& drag_obj_rect,
    const blink::mojom::DragEventSourceInfo& event_info,
    RenderWidgetHostImpl* source_rwh) {
  if (!IsDragEnabledForDropData(drop_data)) {
    // Need to clear drag and drop state in blink.
    OnSystemDragEnded();
    return;
  }

  gfx::NativeView native_view = GetNativeView();
  if (!native_view) {
    // Need to clear drag and drop state in blink.
    OnSystemDragEnded();
    return;
  }

  const SkBitmap* bitmap = image.bitmap();
  SkBitmap dummy_bitmap;

  if (image.size().IsEmpty()) {
    // An empty drag image is possible if the Javascript sets an empty drag
    // image on purpose.
    // Create a dummy 1x1 pixel image to avoid crashes when converting to java
    // bitmap.
    dummy_bitmap.allocN32Pixels(1, 1);
    dummy_bitmap.eraseColor(0);
    bitmap = &dummy_bitmap;
  }

  // TODO(crbug.com/1405120): Consolidate cursor_offset and drag_obj_rect with
  // drop_data.

  ScopedJavaLocalRef<jobject> jdrop_data = ToJavaDropData(drop_data);
  if (!native_view->StartDragAndDrop(
          gfx::ConvertToJavaBitmap(*bitmap), jdrop_data, cursor_offset.x(),
          cursor_offset.y(), drag_obj_rect.width(), drag_obj_rect.height())) {
    // Need to clear drag and drop state in blink.
    OnSystemDragEnded();
    return;
  }

  if (selection_popup_controller_) {
    selection_popup_controller_->HidePopupsAndPreserveSelection();
    // Hide the handles temporarily.
    auto* rwhva = GetRenderWidgetHostViewAndroid();
    if (rwhva)
      rwhva->SetTextHandlesTemporarilyHidden(true);
  }
}

void WebContentsViewAndroid::UpdateDragCursor(ui::mojom::DragOperation op) {
  // Intentional no-op because Android does not have cursor.
}

bool WebContentsViewAndroid::OnDragEvent(const ui::DragEventAndroid& event) {
  switch (event.action()) {
    case JNI_DragEvent::ACTION_DRAG_ENTERED: {
      std::vector<DropData::Metadata> metadata;
      for (const std::u16string& mime_type : event.mime_types()) {
        metadata.push_back(DropData::Metadata::CreateForMimeType(
            DropData::Kind::STRING, mime_type));
      }
      OnDragEntered(metadata, event.location(), event.screen_location());
      break;
    }
    case JNI_DragEvent::ACTION_DRAG_LOCATION:
      OnDragUpdated(event.location(), event.screen_location());
      break;
    case JNI_DragEvent::ACTION_DROP: {
      DropData drop_data;
      drop_data.did_originate_from_renderer = false;
      JNIEnv* env = AttachCurrentThread();
      std::u16string drop_content =
          ConvertJavaStringToUTF16(env, event.GetJavaContent());
      for (const std::u16string& mime_type : event.mime_types()) {
        if (base::EqualsASCII(mime_type, ui::kMimeTypeURIList)) {
          drop_data.url = GURL(drop_content);
        } else if (base::EqualsASCII(mime_type, ui::kMimeTypeText)) {
          drop_data.text = drop_content;
        } else {
          drop_data.html = drop_content;
        }
      }

      OnPerformDrop(&drop_data, event.location(), event.screen_location());
      break;
    }
    case JNI_DragEvent::ACTION_DRAG_EXITED:
      OnDragExited();
      break;
    case JNI_DragEvent::ACTION_DRAG_ENDED:
      OnDragEnded();
      break;
    case JNI_DragEvent::ACTION_DRAG_STARTED:
      // Nothing meaningful to do.
      break;
  }
  return true;
}

// TODO(crbug.com/1301905): does not work for OOPIFs. The drag-and-drop calls
// on GetRenderViewHost()->GetWidget() in the following functions will need to
// be targeted to specific RenderWidgetHosts.

void WebContentsViewAndroid::OnDragEntered(
    const std::vector<DropData::Metadata>& metadata,
    const gfx::PointF& location,
    const gfx::PointF& screen_location) {
  blink::DragOperationsMask allowed_ops =
      static_cast<blink::DragOperationsMask>(blink::kDragOperationCopy |
                                             blink::kDragOperationMove);
  web_contents_->GetRenderViewHost()
      ->GetWidget()
      ->DragTargetDragEnterWithMetaData(metadata, location, screen_location,
                                        allowed_ops, 0, base::DoNothing());
}

void WebContentsViewAndroid::OnDragUpdated(const gfx::PointF& location,
                                           const gfx::PointF& screen_location) {
  drag_location_ = location;
  drag_screen_location_ = screen_location;

  // When drag and drop is enabled, attempt to dismiss the context menu if drag
  // leaves start location.
  if (IsDragAndDropEnabled()) {
    // On Android DragEvent.ACTION_DRAG_ENTER does not have a valid location.
    // See https://developer.android.com/guide/topics/ui/drag-drop#table2.
    if (!is_active_drag_) {
      is_active_drag_ = true;
      drag_entered_location_ = location;
    } else if (!drag_exceeded_movement_threshold_) {
      int radius = DragMovementThresholdDip();
      if (!drag_location_.IsWithinDistance(drag_entered_location_, radius)) {
        drag_exceeded_movement_threshold_ = true;
        if (delegate_)
          delegate_->DismissContextMenu();
      }
    }
  }

  blink::DragOperationsMask allowed_ops =
      static_cast<blink::DragOperationsMask>(blink::kDragOperationCopy |
                                             blink::kDragOperationMove);
  web_contents_->GetRenderViewHost()->GetWidget()->DragTargetDragOver(
      location, screen_location, allowed_ops, 0, base::DoNothing());
}

void WebContentsViewAndroid::OnDragExited() {
  web_contents_->GetRenderViewHost()->GetWidget()->DragTargetDragLeave(
      gfx::PointF(), gfx::PointF());
}

void WebContentsViewAndroid::OnPerformDrop(DropData* drop_data,
                                           const gfx::PointF& location,
                                           const gfx::PointF& screen_location) {
  web_contents_->Focus();
  web_contents_->GetRenderViewHost()->GetWidget()->FilterDropData(drop_data);
  web_contents_->GetRenderViewHost()->GetWidget()->DragTargetDrop(
      *drop_data, location, screen_location, 0, base::DoNothing());
}

void WebContentsViewAndroid::OnSystemDragEnded() {
  web_contents_->GetRenderViewHost()->GetWidget()->DragSourceSystemDragEnded();

  // Restore the selection popups and the text handles if necessary.
  if (selection_popup_controller_) {
    selection_popup_controller_->RestoreSelectionPopupsIfNecessary();
    auto* rwhva = GetRenderWidgetHostViewAndroid();
    if (rwhva)
      rwhva->SetTextHandlesTemporarilyHidden(false);
  }
}

void WebContentsViewAndroid::OnDragEnded() {
  web_contents_->GetRenderViewHost()->GetWidget()->DragSourceEndedAt(
      drag_location_, drag_screen_location_, ui::mojom::DragOperation::kNone,
      base::DoNothing());
  OnSystemDragEnded();

  is_active_drag_ = false;
  drag_exceeded_movement_threshold_ = false;
  drag_entered_location_ = gfx::PointF();
  drag_location_ = gfx::PointF();
  drag_screen_location_ = gfx::PointF();
}

void WebContentsViewAndroid::GotFocus(
    RenderWidgetHostImpl* render_widget_host) {
  web_contents_->NotifyWebContentsFocused(render_widget_host);
}

void WebContentsViewAndroid::LostFocus(
    RenderWidgetHostImpl* render_widget_host) {
  web_contents_->NotifyWebContentsLostFocus(render_widget_host);
}

// This is called when we the renderer asks us to take focus back (i.e., it has
// iterated past the last focusable element on the page).
void WebContentsViewAndroid::TakeFocus(bool reverse) {
  if (web_contents_->GetDelegate() &&
      web_contents_->GetDelegate()->TakeFocus(web_contents_, reverse))
    return;
  web_contents_->GetRenderWidgetHostView()->Focus();
}

int WebContentsViewAndroid::GetTopControlsHeight() const {
  auto* delegate = web_contents_->GetDelegate();
  return delegate ? delegate->GetTopControlsHeight() : 0;
}

int WebContentsViewAndroid::GetTopControlsMinHeight() const {
  auto* delegate = web_contents_->GetDelegate();
  return delegate ? delegate->GetTopControlsMinHeight() : 0;
}

int WebContentsViewAndroid::GetBottomControlsHeight() const {
  auto* delegate = web_contents_->GetDelegate();
  return delegate ? delegate->GetBottomControlsHeight() : 0;
}

int WebContentsViewAndroid::GetBottomControlsMinHeight() const {
  auto* delegate = web_contents_->GetDelegate();
  return delegate ? delegate->GetBottomControlsMinHeight() : 0;
}

bool WebContentsViewAndroid::ShouldAnimateBrowserControlsHeightChanges() const {
  auto* delegate = web_contents_->GetDelegate();
  return delegate && delegate->ShouldAnimateBrowserControlsHeightChanges();
}

bool WebContentsViewAndroid::DoBrowserControlsShrinkRendererSize() const {
  auto* delegate = web_contents_->GetDelegate();
  return delegate &&
         delegate->DoBrowserControlsShrinkRendererSize(web_contents_);
}

bool WebContentsViewAndroid::OnlyExpandTopControlsAtPageTop() const {
  auto* delegate = web_contents_->GetDelegate();
  return delegate && delegate->OnlyExpandTopControlsAtPageTop();
}

bool WebContentsViewAndroid::OnTouchEvent(const ui::MotionEventAndroid& event) {
  if (event.GetAction() == ui::MotionEventAndroid::Action::DOWN &&
      ShouldRequestUnbufferedDispatch()) {
    view_.RequestUnbufferedDispatch(event);
  }
  return false;  // let the children handle the actual event.
}

bool WebContentsViewAndroid::OnMouseEvent(const ui::MotionEventAndroid& event) {
  // Hover events can be intercepted when in accessibility mode.
  auto action = event.GetAction();
  if (action != ui::MotionEventAndroid::Action::HOVER_ENTER &&
      action != ui::MotionEventAndroid::Action::HOVER_EXIT &&
      action != ui::MotionEventAndroid::Action::HOVER_MOVE)
    return false;

  auto* manager = static_cast<BrowserAccessibilityManagerAndroid*>(
      web_contents_->GetRootBrowserAccessibilityManager());
  return manager && manager->OnHoverEvent(event);
}

bool WebContentsViewAndroid::OnGenericMotionEvent(
    const ui::MotionEventAndroid& event) {
  if (content_ui_event_handler_)
    return content_ui_event_handler_->OnGenericMotionEvent(event);
  return false;
}

bool WebContentsViewAndroid::OnKeyUp(const ui::KeyEventAndroid& event) {
  if (content_ui_event_handler_)
    return content_ui_event_handler_->OnKeyUp(event);
  return false;
}

bool WebContentsViewAndroid::DispatchKeyEvent(
    const ui::KeyEventAndroid& event) {
  if (content_ui_event_handler_)
    return content_ui_event_handler_->DispatchKeyEvent(event);
  return false;
}

bool WebContentsViewAndroid::ScrollBy(float delta_x, float delta_y) {
  if (content_ui_event_handler_)
    content_ui_event_handler_->ScrollBy(delta_x, delta_y);
  return false;
}

bool WebContentsViewAndroid::ScrollTo(float x, float y) {
  if (content_ui_event_handler_)
    content_ui_event_handler_->ScrollTo(x, y);
  return false;
}

void WebContentsViewAndroid::OnSizeChanged() {
  auto* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv) {
    web_contents_->SendScreenRects();
    rwhv->SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                      absl::nullopt);
  }
}

void WebContentsViewAndroid::OnPhysicalBackingSizeChanged(
    absl::optional<base::TimeDelta> deadline_override) {
  if (web_contents_->GetRenderWidgetHostView())
    web_contents_->SendScreenRects();
}

void WebContentsViewAndroid::OnBrowserControlsHeightChanged() {
  auto* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                      absl::nullopt);
}

void WebContentsViewAndroid::OnControlsResizeViewChanged() {
  auto* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                      absl::nullopt);
}

void WebContentsViewAndroid::NotifyVirtualKeyboardOverlayRect(
    const gfx::Rect& keyboard_rect) {
  auto* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->NotifyVirtualKeyboardOverlayRect(keyboard_rect);
}

} // namespace content
