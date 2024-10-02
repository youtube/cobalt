// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"

#include <tuple>
#include <utility>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/mac/foundation_util.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "components/remote_cocoa/app_shim/immersive_mode_controller.h"
#include "components/remote_cocoa/app_shim/immersive_mode_delegate_mac.h"
#include "components/remote_cocoa/app_shim/mouse_capture.h"
#include "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#include "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#include "components/remote_cocoa/browser/ns_view_ids.h"
#include "components/remote_cocoa/browser/window.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/base/cocoa/nswindow_test_util.h"
#include "ui/base/cocoa/remote_accessibility_api.h"
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/input_method.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/recyclable_compositor_mac.h"
#include "ui/display/screen.h"
#include "ui/events/cocoa/cocoa_event_utils.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/mac/coordinate_conversion.h"
#include "ui/native_theme/native_theme_mac.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/cocoa/text_input_host.h"
#include "ui/views/cocoa/tooltip_manager_mac.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/views/word_lookup_client.h"

using remote_cocoa::mojom::NativeWidgetNSWindowInitParams;
using remote_cocoa::mojom::WindowVisibilityState;

namespace views {

namespace {

// Dummy implementation of the BridgedNativeWidgetHost interface. This structure
// exists to work around a bug wherein synchronous mojo calls to an associated
// interface can hang if the interface request is unbound. This structure is
// bound to the real host's interface, and then deletes itself only once the
// underlying connection closes.
// https://crbug.com/915572
class BridgedNativeWidgetHostDummy
    : public remote_cocoa::mojom::NativeWidgetNSWindowHost {
 public:
  BridgedNativeWidgetHostDummy() = default;
  ~BridgedNativeWidgetHostDummy() override = default;

 private:
  void OnVisibilityChanged(bool visible) override {}
  void OnWindowNativeThemeChanged() override {}
  void OnViewSizeChanged(const gfx::Size& new_size) override {}
  void SetKeyboardAccessible(bool enabled) override {}
  void OnIsFirstResponderChanged(bool is_first_responder) override {}
  void OnMouseCaptureActiveChanged(bool capture_is_active) override {}
  void OnScrollEvent(std::unique_ptr<ui::Event> event) override {}
  void OnMouseEvent(std::unique_ptr<ui::Event> event) override {}
  void OnGestureEvent(std::unique_ptr<ui::Event> event) override {}
  void OnWindowGeometryChanged(
      const gfx::Rect& window_bounds_in_screen_dips,
      const gfx::Rect& content_bounds_in_screen_dips) override {}
  void OnWindowFullscreenTransitionStart(
      bool target_fullscreen_state) override {}
  void OnWindowFullscreenTransitionComplete(bool is_fullscreen) override {}
  void OnWindowMiniaturizedChanged(bool miniaturized) override {}
  void OnWindowZoomedChanged(bool zoomed) override {}
  void OnWindowDisplayChanged(const display::Display& display) override {}
  void OnWindowWillClose() override {}
  void OnWindowHasClosed() override {}
  void OnWindowKeyStatusChanged(bool is_key,
                                bool is_content_first_responder,
                                bool full_keyboard_access_enabled) override {}
  void OnWindowStateRestorationDataChanged(
      const std::vector<uint8_t>& data) override {}
  void OnWindowParentChanged(uint64_t new_parent_id) override {}
  void DoDialogButtonAction(ui::DialogButton button) override {}
  void OnFocusWindowToolbar() override {}
  void SetRemoteAccessibilityTokens(
      const std::vector<uint8_t>& window_token,
      const std::vector<uint8_t>& view_token) override {}
  void GetSheetOffsetY(GetSheetOffsetYCallback callback) override {
    float offset_y = 0;
    std::move(callback).Run(offset_y);
  }
  void DispatchKeyEventRemote(
      std::unique_ptr<ui::Event> event,
      DispatchKeyEventRemoteCallback callback) override {
    bool event_handled = false;
    std::move(callback).Run(event_handled);
  }
  void DispatchKeyEventToMenuControllerRemote(
      std::unique_ptr<ui::Event> event,
      DispatchKeyEventToMenuControllerRemoteCallback callback) override {
    ui::KeyEvent* key_event = event->AsKeyEvent();
    bool event_swallowed = false;
    std::move(callback).Run(event_swallowed, key_event->handled());
  }
  void DispatchMonitorEvent(std::unique_ptr<ui::Event> event,
                            DispatchMonitorEventCallback callback) override {
    bool event_handled = false;
    std::move(callback).Run(event_handled);
  }
  void GetHasMenuController(GetHasMenuControllerCallback callback) override {
    bool has_menu_controller = false;
    std::move(callback).Run(has_menu_controller);
  }
  void GetIsDraggableBackgroundAt(
      const gfx::Point& location_in_content,
      GetIsDraggableBackgroundAtCallback callback) override {
    bool is_draggable_background = false;
    std::move(callback).Run(is_draggable_background);
  }
  void GetTooltipTextAt(const gfx::Point& location_in_content,
                        GetTooltipTextAtCallback callback) override {
    std::u16string new_tooltip_text;
    std::move(callback).Run(new_tooltip_text);
  }
  void GetIsFocusedViewTextual(
      GetIsFocusedViewTextualCallback callback) override {
    bool is_textual = false;
    std::move(callback).Run(is_textual);
  }
  void GetWidgetIsModal(GetWidgetIsModalCallback callback) override {
    bool widget_is_modal = false;
    std::move(callback).Run(widget_is_modal);
  }
  void GetDialogButtonInfo(ui::DialogButton button,
                           GetDialogButtonInfoCallback callback) override {
    bool exists = false;
    std::u16string label;
    bool is_enabled = false;
    bool is_default = false;
    std::move(callback).Run(exists, label, is_enabled, is_default);
  }
  void GetDoDialogButtonsExist(
      GetDoDialogButtonsExistCallback callback) override {
    bool buttons_exist = false;
    std::move(callback).Run(buttons_exist);
  }
  void GetShouldShowWindowTitle(
      GetShouldShowWindowTitleCallback callback) override {
    bool should_show_window_title = false;
    std::move(callback).Run(should_show_window_title);
  }
  void GetCanWindowBecomeKey(GetCanWindowBecomeKeyCallback callback) override {
    bool can_window_become_key = false;
    std::move(callback).Run(can_window_become_key);
  }
  void GetAlwaysRenderWindowAsKey(
      GetAlwaysRenderWindowAsKeyCallback callback) override {
    bool always_render_as_key = false;
    std::move(callback).Run(always_render_as_key);
  }
  void OnWindowCloseRequested(
      OnWindowCloseRequestedCallback callback) override {
    bool can_window_close = false;
    std::move(callback).Run(can_window_close);
  }
  void GetWindowFrameTitlebarHeight(
      GetWindowFrameTitlebarHeightCallback callback) override {
    bool override_titlebar_height = false;
    float titlebar_height = 0;
    std::move(callback).Run(override_titlebar_height, titlebar_height);
  }
  void GetRootViewAccessibilityToken(
      GetRootViewAccessibilityTokenCallback callback) override {
    std::vector<uint8_t> token;
    int64_t pid = 0;
    std::move(callback).Run(pid, token);
  }
  void ValidateUserInterfaceItem(
      int32_t command,
      ValidateUserInterfaceItemCallback callback) override {
    remote_cocoa::mojom::ValidateUserInterfaceItemResultPtr result;
    std::move(callback).Run(std::move(result));
  }
  void WillExecuteCommand(int32_t command,
                          WindowOpenDisposition window_open_disposition,
                          bool is_before_first_responder,
                          ExecuteCommandCallback callback) override {
    bool will_execute = false;
    std::move(callback).Run(will_execute);
  }
  void ExecuteCommand(int32_t command,
                      WindowOpenDisposition window_open_disposition,
                      bool is_before_first_responder,
                      ExecuteCommandCallback callback) override {
    bool was_executed = false;
    std::move(callback).Run(was_executed);
  }
  void HandleAccelerator(const ui::Accelerator& accelerator,
                         bool require_priority_handler,
                         HandleAcceleratorCallback callback) override {
    bool was_handled = false;
    std::move(callback).Run(was_handled);
  }
};

std::map<uint64_t, NativeWidgetMacNSWindowHost*>& GetIdToWidgetHostImplMap() {
  static base::NoDestructor<std::map<uint64_t, NativeWidgetMacNSWindowHost*>>
      id_map;
  return *id_map;
}

uint64_t g_last_bridged_native_widget_id = 0;

NSWindow* OriginalHostingWindowFromFullScreenWindow(
    NSWindow* full_screen_window) {
  if ([full_screen_window.delegate
          conformsToProtocol:@protocol(ImmersiveModeDelegate)]) {
    return base::mac::ObjCCastStrict<NSObject<ImmersiveModeDelegate>>(
               full_screen_window.delegate)
        .originalHostingWindow;
  }
  return nullptr;
}

}  // namespace

// static
NativeWidgetMacNSWindowHost* NativeWidgetMacNSWindowHost::GetFromNativeWindow(
    gfx::NativeWindow native_window) {
  NSWindow* window = native_window.GetNativeNSWindow();

  if (NativeWidgetMacNSWindow* widget_window =
          base::mac::ObjCCast<NativeWidgetMacNSWindow>(window)) {
    return GetFromId([widget_window bridgedNativeWidgetId]);
  }

  // If the window is a system created NSToolbarFullScreenWindow we need to do
  // some additional work to find the original window.
  // TODO(mek): Figure out how to make this work with remote remote_cocoa
  // windows.
  if (remote_cocoa::IsNSToolbarFullScreenWindow(window)) {
    NSWindow* original = OriginalHostingWindowFromFullScreenWindow(window);
    if (NativeWidgetMacNSWindow* widget_window =
            base::mac::ObjCCast<NativeWidgetMacNSWindow>(original)) {
      return GetFromId([widget_window bridgedNativeWidgetId]);
    }
  }

  return nullptr;  // Not created by NativeWidgetMac.
}

// static
NativeWidgetMacNSWindowHost* NativeWidgetMacNSWindowHost::GetFromNativeView(
    gfx::NativeView native_view) {
  return GetFromNativeWindow([native_view.GetNativeNSView() window]);
}

// static
const char NativeWidgetMacNSWindowHost::kImmersiveContentNSView[] =
    "kImmersiveContentNSView";

// static
NativeWidgetMacNSWindowHost* NativeWidgetMacNSWindowHost::GetFromId(
    uint64_t bridged_native_widget_id) {
  auto found = GetIdToWidgetHostImplMap().find(bridged_native_widget_id);
  if (found == GetIdToWidgetHostImplMap().end())
    return nullptr;
  return found->second;
}

NativeWidgetMacNSWindowHost::NativeWidgetMacNSWindowHost(NativeWidgetMac* owner)
    : widget_id_(++g_last_bridged_native_widget_id),
      native_widget_mac_(owner),
      root_view_id_(remote_cocoa::GetNewNSViewId()),
      accessibility_focus_overrider_(this),
      text_input_host_(new TextInputHost(this)) {
  DCHECK(GetIdToWidgetHostImplMap().find(widget_id_) ==
         GetIdToWidgetHostImplMap().end());
  GetIdToWidgetHostImplMap().emplace(widget_id_, this);
  DCHECK(owner);
}

NativeWidgetMacNSWindowHost::~NativeWidgetMacNSWindowHost() {
  DCHECK(children_.empty());
  native_window_mapping_.reset();
  if (application_host_) {
    remote_ns_window_remote_.reset();
    application_host_->RemoveObserver(this);
    application_host_ = nullptr;
  }

  // Workaround for https://crbug.com/915572
  if (remote_ns_window_host_receiver_.is_bound()) {
    auto receiver = remote_ns_window_host_receiver_.Unbind();
    if (receiver.is_valid()) {
      mojo::MakeSelfOwnedAssociatedReceiver(
          std::make_unique<BridgedNativeWidgetHostDummy>(),
          std::move(receiver));
    }
  }

  // Ensure that |this| cannot be reached by its id while it is being destroyed.
  auto found = GetIdToWidgetHostImplMap().find(widget_id_);
  DCHECK(found != GetIdToWidgetHostImplMap().end());
  DCHECK_EQ(found->second, this);
  GetIdToWidgetHostImplMap().erase(found);

  // Destroy the bridge first to prevent any calls back into this during
  // destruction.
  // TODO(ccameron): When all communication from |bridge_| to this goes through
  // the BridgedNativeWidgetHost, this can be replaced with closing that pipe.
  in_process_ns_window_bridge_.reset();
  DestroyCompositor();
}

NativeWidgetMacNSWindow* NativeWidgetMacNSWindowHost::GetInProcessNSWindow()
    const {
  return in_process_ns_window_.get();
}

gfx::NativeViewAccessible
NativeWidgetMacNSWindowHost::GetNativeViewAccessibleForNSView() const {
  if (in_process_ns_window_bridge_)
    return in_process_ns_window_bridge_->ns_view();
  return remote_view_accessible_.get();
}

gfx::NativeViewAccessible
NativeWidgetMacNSWindowHost::GetNativeViewAccessibleForNSWindow() const {
  if (in_process_ns_window_bridge_)
    return in_process_ns_window_bridge_->ns_window();
  return remote_window_accessible_.get();
}

remote_cocoa::mojom::NativeWidgetNSWindow*
NativeWidgetMacNSWindowHost::GetNSWindowMojo() const {
  if (remote_ns_window_remote_)
    return remote_ns_window_remote_.get();
  if (in_process_ns_window_bridge_)
    return in_process_ns_window_bridge_.get();
  return nullptr;
}

void NativeWidgetMacNSWindowHost::CreateInProcessNSWindowBridge(
    base::scoped_nsobject<NativeWidgetMacNSWindow> window) {
  in_process_ns_window_ = window;
  in_process_ns_window_bridge_ =
      std::make_unique<remote_cocoa::NativeWidgetNSWindowBridge>(
          widget_id_, this, this, text_input_host_.get());
  in_process_ns_window_bridge_->SetWindow(window);
}

void NativeWidgetMacNSWindowHost::CreateRemoteNSWindow(
    remote_cocoa::ApplicationHost* application_host,
    remote_cocoa::mojom::CreateWindowParamsPtr window_create_params) {
  accessibility_focus_overrider_.SetAppIsRemote(true);
  application_host_ = application_host;
  application_host_->AddObserver(this);

  // Create a local invisible window that will be used as the gfx::NativeWindow
  // handle to track this window in this process.
  {
    auto in_process_ns_window_create_params =
        remote_cocoa::mojom::CreateWindowParams::New();
    in_process_ns_window_create_params->style_mask =
        NSWindowStyleMaskBorderless;
    in_process_ns_window_ =
        remote_cocoa::NativeWidgetNSWindowBridge::CreateNSWindow(
            in_process_ns_window_create_params.get());
    [in_process_ns_window_ setBridgedNativeWidgetId:widget_id_];
    [in_process_ns_window_ setAlphaValue:0.0];
    in_process_view_id_mapping_ =
        std::make_unique<remote_cocoa::ScopedNSViewIdMapping>(
            root_view_id_, [in_process_ns_window_ contentView]);
    [in_process_ns_window_ enforceNeverMadeVisible];
  }

  // Initialize |remote_ns_window_remote_| to point to a bridge created by
  // |factory|.
  mojo::PendingAssociatedRemote<remote_cocoa::mojom::TextInputHost>
      text_input_host_remote;
  text_input_host_->BindReceiver(
      text_input_host_remote.InitWithNewEndpointAndPassReceiver());
  application_host_->GetApplication()->CreateNativeWidgetNSWindow(
      widget_id_, remote_ns_window_remote_.BindNewEndpointAndPassReceiver(),
      remote_ns_window_host_receiver_.BindNewEndpointAndPassRemote(
          ui::WindowResizeHelperMac::Get()->task_runner()),
      std::move(text_input_host_remote));

  // Create the window in its process, and attach it to its parent window.
  GetNSWindowMojo()->CreateWindow(std::move(window_create_params));
}

void NativeWidgetMacNSWindowHost::InitWindow(
    const Widget::InitParams& params,
    const gfx::Rect& initial_bounds_in_screen) {
  native_window_mapping_ =
      std::make_unique<remote_cocoa::ScopedNativeWindowMapping>(
          gfx::NativeWindow(in_process_ns_window_.get()), application_host_,
          in_process_ns_window_bridge_.get(), GetNSWindowMojo());

  Widget* widget = native_widget_mac_->GetWidget();
  widget_type_ = params.type;
  bool is_tooltip = params.type == Widget::InitParams::TYPE_TOOLTIP;
  if (!is_tooltip)
    tooltip_manager_ = std::make_unique<TooltipManagerMac>(GetNSWindowMojo());

  if (params.workspace.length()) {
    std::string restoration_data;
    if (base::Base64Decode(params.workspace, &restoration_data)) {
      state_restoration_data_ = std::vector<uint8_t>(restoration_data.begin(),
                                                     restoration_data.end());
    } else {
      DLOG(ERROR) << "Failed to decode a window's state restoration data.";
    }
  }

  // Initialize the window.
  {
    auto window_params = NativeWidgetNSWindowInitParams::New();
    window_params->modal_type = widget->widget_delegate()->GetModalType();
    window_params->is_translucent =
        params.opacity == Widget::InitParams::WindowOpacity::kTranslucent;
    window_params->is_headless_mode_window = params.headless_mode;
    window_params->is_tooltip = is_tooltip;
    is_headless_mode_window_ = params.headless_mode;

    // OSX likes to put shadows on most things. However, frameless windows (with
    // styleMask = NSWindowStyleMaskBorderless) default to no shadow. So change
    // that. ShadowType::kDrop is used for Menus, which get the same shadow
    // style on Mac.
    switch (params.shadow_type) {
      case Widget::InitParams::ShadowType::kNone:
        window_params->has_window_server_shadow = false;
        break;
      case Widget::InitParams::ShadowType::kDefault:
        // Controls should get views shadows instead of native shadows.
        window_params->has_window_server_shadow =
            params.type != Widget::InitParams::TYPE_CONTROL;
        break;
      case Widget::InitParams::ShadowType::kDrop:
        window_params->has_window_server_shadow = true;
        break;
    }  // No default case, to pick up new types.

    // Include "regular" windows without the standard frame in the window cycle.
    // These use NSWindowStyleMaskBorderless so do not get it by default.
    window_params->force_into_collection_cycle =
        widget_type_ == Widget::InitParams::TYPE_WINDOW &&
        params.remove_standard_frame;
    window_params->state_restoration_data = state_restoration_data_;

    GetNSWindowMojo()->InitWindow(std::move(window_params));
  }

  // Set a meaningful initial bounds. Note that except for frameless widgets
  // with no WidgetDelegate, the bounds will be set again by Widget after
  // initializing the non-client view. In the former case, if bounds were not
  // set at all, the creator of the Widget is expected to call SetBounds()
  // before calling Widget::Show() to avoid a kWindowSizeDeterminedLater-sized
  // (i.e. 1x1) window appearing.
  UpdateLocalWindowFrame(initial_bounds_in_screen);
  GetNSWindowMojo()->SetInitialBounds(initial_bounds_in_screen,
                                      widget->GetMinimumSize());

  // Widgets for UI controls (usually layered above web contents) start visible.
  if (widget_type_ == Widget::InitParams::TYPE_CONTROL)
    GetNSWindowMojo()->SetVisibilityState(WindowVisibilityState::kShowInactive);
}

void NativeWidgetMacNSWindowHost::CloseWindowNow() {
  bool is_out_of_process = !in_process_ns_window_bridge_;
  // Note that CloseWindowNow may delete |this| for in-process windows.
  if (GetNSWindowMojo())
    GetNSWindowMojo()->CloseWindowNow();

  // If it is out-of-process, then simulate the calls that would have been
  // during window closure.
  if (is_out_of_process) {
    OnWindowWillClose();
    while (!children_.empty())
      children_.front()->CloseWindowNow();
    OnWindowHasClosed();
  }
}

void NativeWidgetMacNSWindowHost::SetBoundsInScreen(const gfx::Rect& bounds) {
  DCHECK(!bounds.IsEmpty() ||
         !native_widget_mac_->GetWidget()->GetMinimumSize().IsEmpty())
      << "Zero-sized windows are not supported on Mac";
  UpdateLocalWindowFrame(bounds);
  GetNSWindowMojo()->SetBounds(
      bounds, native_widget_mac_->GetWidget()->GetMinimumSize());

  if (remote_ns_window_remote_) {
    gfx::Rect window_in_screen =
        gfx::ScreenRectFromNSRect([in_process_ns_window_ frame]);
    gfx::Rect content_in_screen =
        gfx::ScreenRectFromNSRect([in_process_ns_window_
            contentRectForFrameRect:[in_process_ns_window_ frame]]);

    OnWindowGeometryChanged(window_in_screen, content_in_screen);
  }
}

void NativeWidgetMacNSWindowHost::SetFullscreen(bool fullscreen,
                                                int64_t target_display_id) {
  // Note that when the NSWindow begins a fullscreen transition, the value of
  // |target_fullscreen_state_| updates via OnWindowFullscreenTransitionStart.
  // The update here is necessary for the case where we are currently in
  // transition (and therefore OnWindowFullscreenTransitionStart will not be
  // called until the current transition completes).
  target_fullscreen_state_ = fullscreen;

  if (target_fullscreen_state_)
    GetNSWindowMojo()->EnterFullscreen(target_display_id);
  else
    GetNSWindowMojo()->ExitFullscreen();
}

void NativeWidgetMacNSWindowHost::SetRootView(views::View* root_view) {
  root_view_ = root_view;
  if (root_view_) {
    // TODO(ccameron): Drag-drop functionality does not yet run over mojo.
    if (in_process_ns_window_bridge_) {
      drag_drop_client_ = std::make_unique<DragDropClientMac>(
          in_process_ns_window_bridge_.get(), root_view_);
    }
  } else {
    drag_drop_client_.reset();
  }
}

void NativeWidgetMacNSWindowHost::CreateCompositor(
    const Widget::InitParams& params) {
  DCHECK(!compositor_);
  DCHECK(!layer());
  DCHECK(ViewsDelegate::GetInstance());

  // "Infer" must be handled by ViewsDelegate::OnBeforeWidgetInit().
  DCHECK_NE(Widget::InitParams::WindowOpacity::kInferred, params.opacity);
  bool translucent =
      params.opacity == Widget::InitParams::WindowOpacity::kTranslucent;

  // Create the layer.
  SetLayer(std::make_unique<ui::Layer>(params.layer_type));
  layer()->set_delegate(this);
  layer()->SetFillsBoundsOpaquely(!translucent);

  // Create the compositor and attach the layer to it.
  ui::ContextFactory* context_factory =
      ViewsDelegate::GetInstance()->GetContextFactory();
  DCHECK(context_factory);
  compositor_ = std::make_unique<ui::RecyclableCompositorMac>(context_factory);
  compositor_->widget()->SetNSView(this);
  compositor_->compositor()->SetBackgroundColor(
      translucent ? SK_ColorTRANSPARENT : SK_ColorWHITE);
  compositor_->compositor()->SetRootLayer(layer());

  // The compositor is locked (prevented from producing frames) until the widget
  // is made visible unless the window was created in headless mode in which
  // case it will never become visible but we want its compositor to produce
  // frames for screenshooting and screencasting.
  UpdateCompositorProperties();
  layer()->SetVisible(is_visible_);
  if (is_visible_ || is_headless_mode_window_)
    compositor_->Unsuspend();

  // Register the CGWindowID (used to identify this window for video capture)
  // when it is received. Note that this is done at this moment (as opposed to
  // as a callback to CreateWindow) so that we can associate the CGWindowID with
  // the (now existing) compositor.
  auto lambda = [](NativeWidgetMacNSWindowHost* host, uint32_t cg_window_id) {
    if (!host->compositor_)
      return;
    host->scoped_cg_window_id_ =
        std::make_unique<remote_cocoa::ScopedCGWindowID>(
            cg_window_id, host->compositor_->compositor()->frame_sink_id());
  };
  GetNSWindowMojo()->InitCompositorView(
      base::BindOnce(lambda, base::Unretained(this)));
}

void NativeWidgetMacNSWindowHost::UpdateCompositorProperties() {
  if (!compositor_)
    return;
  layer()->SetBounds(gfx::Rect(content_bounds_in_screen_.size()));
  // Mac device scale factor is always an integer so the result here is an
  // integer pixel size.
  gfx::Size content_bounds_in_pixels =
      gfx::ToRoundedSize(gfx::ConvertSizeToPixels(
          content_bounds_in_screen_.size(), display_.device_scale_factor()));
  compositor_->UpdateSurface(content_bounds_in_pixels,
                             display_.device_scale_factor(),
                             display_.color_spaces());
}

void NativeWidgetMacNSWindowHost::DestroyCompositor() {
  if (layer()) {
    // LayerOwner supports a change in ownership, e.g., to animate a closing
    // window, but that won't work as expected for the root layer in
    // NativeWidgetNSWindowBridge.
    DCHECK_EQ(this, layer()->owner());
    layer()->CompleteAllAnimations();
  }
  DestroyLayer();
  if (!compositor_)
    return;
  compositor_->widget()->ResetNSView();
  compositor_->compositor()->SetRootLayer(nullptr);
  compositor_.reset();
}

bool NativeWidgetMacNSWindowHost::SetWindowTitle(const std::u16string& title) {
  if (window_title_ == title)
    return false;
  window_title_ = title;
  GetNSWindowMojo()->SetWindowTitle(window_title_);
  return true;
}

void NativeWidgetMacNSWindowHost::OnWidgetInitDone() {
  Widget* widget = native_widget_mac_->GetWidget();
  if (DialogDelegate* dialog = widget->widget_delegate()->AsDialogDelegate())
    dialog->AddObserver(this);
}

bool NativeWidgetMacNSWindowHost::RedispatchKeyEvent(NSEvent* event) {
  // If the target window is in-process, then redispatch the event directly,
  // and give an accurate return value.
  if (in_process_ns_window_bridge_)
    return in_process_ns_window_bridge_->RedispatchKeyEvent(event);

  // If the target window is out of process then always report the event as
  // handled (because it should never be handled in this process).
  GetNSWindowMojo()->RedispatchKeyEvent(ui::EventToData(event));
  return true;
}

gfx::Rect NativeWidgetMacNSWindowHost::GetContentBoundsInScreen() const {
  return content_bounds_in_screen_;
}

gfx::Rect NativeWidgetMacNSWindowHost::GetRestoredBounds() const {
  if (target_fullscreen_state_ || in_fullscreen_transition_)
    return window_bounds_before_fullscreen_;
  return window_bounds_in_screen_;
}

const std::vector<uint8_t>&
NativeWidgetMacNSWindowHost::GetWindowStateRestorationData() const {
  return state_restoration_data_;
}

void NativeWidgetMacNSWindowHost::SetNativeWindowProperty(const char* name,
                                                          void* value) {
  if (value)
    native_window_properties_[name] = value;
  else
    native_window_properties_.erase(name);
}

void* NativeWidgetMacNSWindowHost::GetNativeWindowProperty(
    const char* name) const {
  auto found = native_window_properties_.find(name);
  if (found == native_window_properties_.end())
    return nullptr;
  return found->second;
}

void NativeWidgetMacNSWindowHost::SetParent(
    NativeWidgetMacNSWindowHost* new_parent) {
  if (new_parent == parent_)
    return;

  if (parent_) {
    auto found = base::ranges::find(parent_->children_, this);
    DCHECK(found != parent_->children_.end());
    parent_->children_.erase(found);
    parent_ = nullptr;
  }

  // We can only re-parent to another Widget if that Widget is hosted in the
  // same process that we were already hosted by. If this is not the case, just
  // close the Widget.
  // https://crbug.com/957927
  remote_cocoa::ApplicationHost* new_application_host =
      new_parent ? new_parent->application_host() : application_host_.get();
  if (new_application_host != application_host_) {
    DLOG(ERROR) << "Cannot migrate views::NativeWidget to another process, "
                   "closing it instead.";
    GetNSWindowMojo()->CloseWindow();
    return;
  }

  parent_ = new_parent;
  if (parent_) {
    parent_->children_.push_back(this);
    GetNSWindowMojo()->SetParent(parent_->bridged_native_widget_id());
  } else {
    GetNSWindowMojo()->SetParent(0);
  }
}

void NativeWidgetMacNSWindowHost::OnNativeViewHostAttach(const View* view,
                                                         NSView* native_view) {
  DCHECK_EQ(0u, attached_native_view_host_views_.count(view));
  attached_native_view_host_views_[view] = native_view;
  native_widget_mac_->GetWidget()->ReorderNativeViews();
}

void NativeWidgetMacNSWindowHost::OnNativeViewHostDetach(const View* view) {
  auto it = attached_native_view_host_views_.find(view);
  DCHECK(it != attached_native_view_host_views_.end());
  attached_native_view_host_views_.erase(it);
}

void NativeWidgetMacNSWindowHost::ReorderChildViews() {
  Widget* widget = native_widget_mac_->GetWidget();
  if (!widget->GetRootView())
    return;

  // Get the ordering for the NSViews in |attached_native_view_host_views_|.
  std::vector<NSView*> attached_subviews;
  GetAttachedNativeViewHostViewsRecursive(widget->GetRootView(),
                                          &attached_subviews);

  // Convert to NSView ids that can go over mojo. If need be, create temporary
  // NSView ids.
  std::vector<uint64_t> attached_subview_ids;
  std::list<remote_cocoa::ScopedNSViewIdMapping> temp_ids;
  for (NSView* subview : attached_subviews) {
    uint64_t ns_view_id = remote_cocoa::GetIdFromNSView(subview);
    if (!ns_view_id) {
      // Subviews that do not already have an id will not work if the target is
      // in a different process.
      DCHECK(in_process_ns_window_bridge_);
      ns_view_id = remote_cocoa::GetNewNSViewId();
      temp_ids.emplace_back(ns_view_id, subview);
    }
    attached_subview_ids.push_back(ns_view_id);
  }
  GetNSWindowMojo()->SortSubviews(attached_subview_ids);
}

void NativeWidgetMacNSWindowHost::GetAttachedNativeViewHostViewsRecursive(
    View* view,
    std::vector<NSView*>* order) const {
  auto found = attached_native_view_host_views_.find(view);
  if (found != attached_native_view_host_views_.end())
    order->push_back(found->second);
  for (View* child : view->children())
    GetAttachedNativeViewHostViewsRecursive(child, order);
}

void NativeWidgetMacNSWindowHost::UpdateLocalWindowFrame(
    const gfx::Rect& frame) {
  if (!remote_ns_window_remote_)
    return;
  [in_process_ns_window_ setFrame:gfx::ScreenRectToNSRect(frame)
                          display:NO
                          animate:NO];
}

std::unique_ptr<NativeWidgetMacEventMonitor>
NativeWidgetMacNSWindowHost::AddEventMonitor(
    NativeWidgetMacEventMonitor::Client* client) {
  // Enable the local event monitor if this is the first registered monitor.
  if (event_monitors_.empty())
    GetNSWindowMojo()->SetLocalEventMonitorEnabled(true);

  // Add the new monitor to `event_monitors_`.
  auto* monitor = new NativeWidgetMacEventMonitor(client);
  event_monitors_.push_back(monitor);

  // Set up `monitor`'s remove closure to remove it from `event_monitors_`.
  auto remove_lambda = [](base::WeakPtr<NativeWidgetMacNSWindowHost> weak_this,
                          NativeWidgetMacEventMonitor* monitor) {
    if (!weak_this)
      return;
    auto found = base::ranges::find(weak_this->event_monitors_, monitor);
    CHECK(found != weak_this->event_monitors_.end());
    weak_this->event_monitors_.erase(found);

    // If this was the last monitor to be removed, disable the local
    // event monitor.
    if (weak_this->event_monitors_.empty())
      weak_this->GetNSWindowMojo()->SetLocalEventMonitorEnabled(false);
  };
  monitor->remove_closure_runner_.ReplaceClosure(
      base::BindOnce(remove_lambda, weak_factory_.GetWeakPtr(), monitor));

  return base::WrapUnique<NativeWidgetMacEventMonitor>(monitor);
}

// static
NSView* NativeWidgetMacNSWindowHost::GetGlobalCaptureView() {
  // TODO(ccameron): This will not work across process boundaries.
  return
      [remote_cocoa::CocoaMouseCapture::GetGlobalCaptureWindow() contentView];
}

void NativeWidgetMacNSWindowHost::CanGoBack(bool can_go_back) {
  GetNSWindowMojo()->SetCanGoBack(can_go_back);
}

void NativeWidgetMacNSWindowHost::CanGoForward(bool can_go_forward) {
  GetNSWindowMojo()->SetCanGoForward(can_go_forward);
}
////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMacNSWindowHost, remote_cocoa::BridgedNativeWidgetHostHelper:

id NativeWidgetMacNSWindowHost::GetNativeViewAccessible() {
  return root_view_ ? root_view_->GetNativeViewAccessible() : nil;
}

void NativeWidgetMacNSWindowHost::DispatchKeyEvent(ui::KeyEvent* event) {
  std::ignore =
      root_view_->GetWidget()->GetInputMethod()->DispatchKeyEvent(event);
}

bool NativeWidgetMacNSWindowHost::DispatchKeyEventToMenuController(
    ui::KeyEvent* event) {
  MenuController* menu_controller = MenuController::GetActiveInstance();
  if (menu_controller && root_view_ &&
      menu_controller->owner() == root_view_->GetWidget()) {
    return menu_controller->OnWillDispatchKeyEvent(event) ==
           ui::POST_DISPATCH_NONE;
  }
  return false;
}

remote_cocoa::DragDropClient* NativeWidgetMacNSWindowHost::GetDragDropClient() {
  return drag_drop_client_.get();
}

ui::TextInputClient* NativeWidgetMacNSWindowHost::GetTextInputClient() {
  return text_input_host_->GetTextInputClient();
}

bool NativeWidgetMacNSWindowHost::MustPostTaskToRunModalSheetAnimation() const {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMacNSWindowHost, remote_cocoa::ApplicationHost::Observer:
void NativeWidgetMacNSWindowHost::OnApplicationHostDestroying(
    remote_cocoa::ApplicationHost* host) {
  DCHECK_EQ(host, application_host_);
  application_host_->RemoveObserver(this);
  application_host_ = nullptr;

  // Because the process hosting this window has ended, close the window by
  // sending the window close messages that the bridge would have sent.
  OnWindowWillClose();
  // Explicitly propagate this message to all children (they are also observers,
  // but may not be destroyed before |this| is destroyed, which would violate
  // tear-down assumptions). This would have been done by the bridge, had it
  // shut down cleanly.
  while (!children_.empty())
    children_.front()->OnApplicationHostDestroying(host);
  OnWindowHasClosed();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMacNSWindowHost,
// remote_cocoa::mojom::NativeWidgetNSWindowHost:

void NativeWidgetMacNSWindowHost::OnVisibilityChanged(bool window_visible) {
  is_visible_ = window_visible;
  if (compositor_) {
    layer()->SetVisible(window_visible);
    if (window_visible) {
      compositor_->Unsuspend();
      layer()->SchedulePaint(layer()->bounds());
    } else {
      compositor_->Suspend();
    }
  }
  native_widget_mac_->GetWidget()->OnNativeWidgetVisibilityChanged(
      window_visible);
}

void NativeWidgetMacNSWindowHost::OnWindowNativeThemeChanged() {
  ui::NativeTheme::GetInstanceForNativeUi()->NotifyOnNativeThemeUpdated();
}

void NativeWidgetMacNSWindowHost::OnScrollEvent(
    std::unique_ptr<ui::Event> event) {
  root_view_->GetWidget()->OnScrollEvent(event->AsScrollEvent());
}

void NativeWidgetMacNSWindowHost::OnMouseEvent(
    std::unique_ptr<ui::Event> event) {
  ui::MouseEvent* mouse_event = event->AsMouseEvent();
  if (scoped_cg_window_id_) {
    scoped_cg_window_id_->OnMouseMoved(mouse_event->location_f(),
                                       window_bounds_in_screen_.size());
  }
  root_view_->GetWidget()->OnMouseEvent(mouse_event);
  // Note that |this| may be destroyed by the above call to OnMouseEvent.
  // https://crbug.com/1193454
}

void NativeWidgetMacNSWindowHost::OnGestureEvent(
    std::unique_ptr<ui::Event> event) {
  root_view_->GetWidget()->OnGestureEvent(event->AsGestureEvent());
}

bool NativeWidgetMacNSWindowHost::DispatchKeyEventRemote(
    std::unique_ptr<ui::Event> event,
    bool* event_handled) {
  DispatchKeyEvent(event->AsKeyEvent());
  *event_handled = event->handled();
  return true;
}

bool NativeWidgetMacNSWindowHost::DispatchKeyEventToMenuControllerRemote(
    std::unique_ptr<ui::Event> event,
    bool* event_swallowed,
    bool* event_handled) {
  *event_swallowed = DispatchKeyEventToMenuController(event->AsKeyEvent());
  *event_handled = event->handled();
  return true;
}

bool NativeWidgetMacNSWindowHost::DispatchMonitorEvent(
    std::unique_ptr<ui::Event> event,
    bool* event_handled) {
  // The calls to NativeWidgetMacEventMonitorOnEvent can add or remove monitors,
  // so take a snapshot of `event_monitors_` before making any calls.
  auto event_monitors_snapshot = event_monitors_;

  // The calls to NativeWidgetMacEventMonitorOnEvent can delete `this`. Use
  // `weak_this` to detect that.
  auto weak_this = weak_factory_.GetWeakPtr();

  *event_handled = false;
  for (auto* event_monitor : event_monitors_snapshot) {
    // Ensure `event_monitor` was not removed from `event_monitors_` by a
    // previous call to NativeWidgetMacEventMonitorOnEvent.
    if (!base::Contains(event_monitors_, event_monitor))
      continue;
    event_monitor->client_->NativeWidgetMacEventMonitorOnEvent(event.get(),
                                                               event_handled);
    if (!weak_this)
      return true;
  }
  return true;
}

bool NativeWidgetMacNSWindowHost::GetHasMenuController(
    bool* has_menu_controller) {
  MenuController* menu_controller = MenuController::GetActiveInstance();
  *has_menu_controller = menu_controller && root_view_ &&
                         menu_controller->owner() == root_view_->GetWidget() &&
                         // The editable combobox menu does not swallow keys.
                         !menu_controller->IsEditableCombobox();
  return true;
}

void NativeWidgetMacNSWindowHost::OnViewSizeChanged(const gfx::Size& new_size) {
  root_view_->SetSize(new_size);
}

bool NativeWidgetMacNSWindowHost::GetSheetOffsetY(int32_t* offset_y) {
  *offset_y = native_widget_mac_->SheetOffsetY();
  return true;
}

void NativeWidgetMacNSWindowHost::SetKeyboardAccessible(bool enabled) {
  views::FocusManager* focus_manager =
      root_view_->GetWidget()->GetFocusManager();
  if (focus_manager)
    focus_manager->SetKeyboardAccessible(enabled);
}

void NativeWidgetMacNSWindowHost::OnIsFirstResponderChanged(
    bool is_first_responder) {
  accessibility_focus_overrider_.SetViewIsFirstResponder(is_first_responder);

  FocusManager* focus_manager = root_view_->GetWidget()->GetFocusManager();
  if (focus_manager->IsSettingFocusedView()) {
    // This first responder change is not initiated by the os,
    // but by the focus change within the browser (e.g. tab switch),
    // so skip setting the focus.
    return;
  }

  if (is_first_responder) {
    focus_manager->RestoreFocusedView();
  } else {
    // Do not call ClearNativeFocus because that will re-make the
    // BridgedNativeWidget first responder (and this is called to indicate that
    // it is no longer first responder).
    focus_manager->StoreFocusedView(false /* clear_native_focus */);
  }
}

void NativeWidgetMacNSWindowHost::OnMouseCaptureActiveChanged(bool is_active) {
  DCHECK_NE(is_mouse_capture_active_, is_active);
  is_mouse_capture_active_ = is_active;
  if (!is_mouse_capture_active_)
    native_widget_mac_->GetWidget()->OnMouseCaptureLost();
}

bool NativeWidgetMacNSWindowHost::GetIsDraggableBackgroundAt(
    const gfx::Point& location_in_content,
    bool* is_draggable_background) {
  int component =
      root_view_->GetWidget()->GetNonClientComponent(location_in_content);
  *is_draggable_background = component == HTCAPTION;
  return true;
}

bool NativeWidgetMacNSWindowHost::GetTooltipTextAt(
    const gfx::Point& location_in_content,
    std::u16string* new_tooltip_text) {
  views::View* view =
      root_view_->GetTooltipHandlerForPoint(location_in_content);
  if (view) {
    gfx::Point view_point = location_in_content;
    views::View::ConvertPointToScreen(root_view_, &view_point);
    views::View::ConvertPointFromScreen(view, &view_point);
    *new_tooltip_text = view->GetTooltipText(view_point);
  }
  return true;
}

void NativeWidgetMacNSWindowHost::GetWordAt(
    const gfx::Point& location_in_content,
    bool* found_word,
    gfx::DecoratedText* decorated_word,
    gfx::Point* baseline_point) {
  *found_word = false;

  views::View* target =
      root_view_->GetEventHandlerForPoint(location_in_content);
  if (!target)
    return;

  views::WordLookupClient* word_lookup_client = target->GetWordLookupClient();
  if (!word_lookup_client)
    return;

  gfx::Point location_in_target = location_in_content;
  views::View::ConvertPointToTarget(root_view_, target, &location_in_target);
  if (!word_lookup_client->GetWordLookupDataAtPoint(
          location_in_target, decorated_word, baseline_point)) {
    return;
  }

  // Convert |baselinePoint| to the coordinate system of |root_view_|.
  views::View::ConvertPointToTarget(target, root_view_, baseline_point);
  *found_word = true;
}

bool NativeWidgetMacNSWindowHost::GetWidgetIsModal(bool* widget_is_modal) {
  *widget_is_modal = native_widget_mac_->GetWidget()->IsModal();
  return true;
}

bool NativeWidgetMacNSWindowHost::GetIsFocusedViewTextual(bool* is_textual) {
  views::FocusManager* focus_manager =
      root_view_ ? root_view_->GetWidget()->GetFocusManager() : nullptr;
  *is_textual = focus_manager && focus_manager->GetFocusedView() &&
                focus_manager->GetFocusedView()->GetClassName() ==
                    views::Label::kViewClassName;
  return true;
}

void NativeWidgetMacNSWindowHost::OnWindowGeometryChanged(
    const gfx::Rect& new_window_bounds_in_screen,
    const gfx::Rect& new_content_bounds_in_screen) {
  UpdateLocalWindowFrame(new_window_bounds_in_screen);

  bool window_has_moved =
      new_window_bounds_in_screen.origin() != window_bounds_in_screen_.origin();
  bool content_has_resized =
      new_content_bounds_in_screen.size() != content_bounds_in_screen_.size();

  window_bounds_in_screen_ = new_window_bounds_in_screen;
  content_bounds_in_screen_ = new_content_bounds_in_screen;

  // When a window grows vertically, the AppKit origin changes, but as far as
  // tookit-views is concerned, the window hasn't moved. Suppress these.
  if (window_has_moved)
    native_widget_mac_->GetWidget()->OnNativeWidgetMove();

  // Note we can't use new_window_bounds_in_screen.size(), since it includes the
  // titlebar for the purposes of detecting a window move.
  if (content_has_resized) {
    native_widget_mac_->GetWidget()->OnNativeWidgetSizeChanged(
        content_bounds_in_screen_.size());

    // Update the compositor surface and layer size.
    UpdateCompositorProperties();
  }
}

void NativeWidgetMacNSWindowHost::OnWindowFullscreenTransitionStart(
    bool target_fullscreen_state) {
  target_fullscreen_state_ = target_fullscreen_state;
  in_fullscreen_transition_ = true;

  // If going into fullscreen, store an answer for GetRestoredBounds().
  if (target_fullscreen_state)
    window_bounds_before_fullscreen_ = window_bounds_in_screen_;

  // Notify that fullscreen state is changing.
  native_widget_mac_->OnWindowFullscreenTransitionStart();
}

void NativeWidgetMacNSWindowHost::OnWindowFullscreenTransitionComplete(
    bool actual_fullscreen_state) {
  in_fullscreen_transition_ = false;

  // Notify that fullscreen state has changed.
  native_widget_mac_->OnWindowFullscreenTransitionComplete();

  // Ensure constraints are re-applied when completing a transition.
  native_widget_mac_->OnSizeConstraintsChanged();

  ui::NSWindowFullscreenNotificationWaiter::NotifyFullscreenTransitionComplete(
      native_widget_mac_->GetNativeWindow(), actual_fullscreen_state);
}

void NativeWidgetMacNSWindowHost::OnWindowMiniaturizedChanged(
    bool miniaturized) {
  is_miniaturized_ = miniaturized;
  if (native_widget_mac_)
    native_widget_mac_->GetWidget()->OnNativeWidgetWindowShowStateChanged();
}

void NativeWidgetMacNSWindowHost::OnWindowZoomedChanged(bool zoomed) {
  is_zoomed_ = zoomed;
}

void NativeWidgetMacNSWindowHost::OnWindowDisplayChanged(
    const display::Display& new_display) {
  bool display_id_changed = display_.id() != new_display.id();
  display_ = new_display;
  if (compositor_) {
    // Mac device scale factor is always an integer so the result here is an
    // integer pixel size.
    gfx::Size content_bounds_in_pixels =
        gfx::ToRoundedSize(gfx::ConvertSizeToPixels(
            content_bounds_in_screen_.size(), display_.device_scale_factor()));
    compositor_->UpdateSurface(content_bounds_in_pixels,
                               display_.device_scale_factor(),
                               display_.color_spaces());
  }

  if (display_id_changed) {
    display_link_ = ui::DisplayLinkMac::GetForDisplay(
        base::checked_cast<CGDirectDisplayID>(display_.id()));
    if (!display_link_) {
      // Note that on some headless systems, the display link will fail to be
      // created, so this should not be a fatal error.
      LOG(ERROR) << "Failed to create display link.";
    }

    if (compositor_) {
      RequestVSyncParametersUpdate();
      compositor_->compositor()->SetVSyncDisplayID(display_.id());
    }
  }
}

void NativeWidgetMacNSWindowHost::OnWindowWillClose() {
  Widget* widget = native_widget_mac_->GetWidget();
  if (DialogDelegate* dialog = widget->widget_delegate()->AsDialogDelegate())
    dialog->RemoveObserver(this);
  native_widget_mac_->WindowDestroying();
  // Remove |this| from the parent's list of children.
  SetParent(nullptr);
}

void NativeWidgetMacNSWindowHost::OnWindowHasClosed() {
  // OnWindowHasClosed will be called only after all child windows have had
  // OnWindowWillClose called on them.
  DCHECK(children_.empty());
  native_widget_mac_->WindowDestroyed();
}

void NativeWidgetMacNSWindowHost::OnWindowKeyStatusChanged(
    bool is_key,
    bool is_content_first_responder,
    bool full_keyboard_access_enabled) {
  // Explicitly set the keyboard accessibility state on regaining key
  // window status.
  if (is_key && is_content_first_responder)
    SetKeyboardAccessible(full_keyboard_access_enabled);
  accessibility_focus_overrider_.SetWindowIsKey(is_key);
  is_window_key_ = is_key;
  native_widget_mac_->OnWindowKeyStatusChanged(is_key,
                                               is_content_first_responder);
}

void NativeWidgetMacNSWindowHost::OnWindowStateRestorationDataChanged(
    const std::vector<uint8_t>& data) {
  state_restoration_data_ = data;
  native_widget_mac_->GetWidget()->OnNativeWidgetWorkspaceChanged();
}

void NativeWidgetMacNSWindowHost::OnWindowParentChanged(
    uint64_t new_parent_id) {
  NativeWidgetMacNSWindowHost* new_parent = GetFromId(new_parent_id);

  if (new_parent == parent_) {
    return;
  }

  if (parent_) {
    auto found = base::ranges::find(parent_->children_, this);
    CHECK(found != parent_->children_.end());
    parent_->children_.erase(found);
    parent_ = nullptr;
  }

  parent_ = new_parent;
  if (parent_) {
    parent_->children_.push_back(this);
  }

  if (Widget* widget = native_widget_mac_->GetWidget()) {
    widget->OnNativeWidgetParentChanged(
        parent_ ? parent_->native_widget_mac()->GetNativeView() : nullptr);
  }
}

void NativeWidgetMacNSWindowHost::DoDialogButtonAction(
    ui::DialogButton button) {
  views::DialogDelegate* dialog =
      root_view_->GetWidget()->widget_delegate()->AsDialogDelegate();
  DCHECK(dialog);
  if (button == ui::DIALOG_BUTTON_OK) {
    dialog->AcceptDialog();
  } else {
    DCHECK_EQ(button, ui::DIALOG_BUTTON_CANCEL);
    dialog->CancelDialog();
  }
}

bool NativeWidgetMacNSWindowHost::GetDialogButtonInfo(
    ui::DialogButton button,
    bool* button_exists,
    std::u16string* button_label,
    bool* is_button_enabled,
    bool* is_button_default) {
  *button_exists = false;
  views::DialogDelegate* dialog =
      root_view_->GetWidget()->widget_delegate()->AsDialogDelegate();
  if (!dialog || !(dialog->GetDialogButtons() & button))
    return true;
  *button_exists = true;
  *button_label = dialog->GetDialogButtonLabel(button);
  *is_button_enabled = dialog->IsDialogButtonEnabled(button);
  *is_button_default = button == dialog->GetDefaultDialogButton();
  return true;
}

bool NativeWidgetMacNSWindowHost::GetDoDialogButtonsExist(bool* buttons_exist) {
  views::DialogDelegate* dialog =
      root_view_->GetWidget()->widget_delegate()->AsDialogDelegate();
  *buttons_exist = dialog && dialog->GetDialogButtons();
  return true;
}

bool NativeWidgetMacNSWindowHost::GetShouldShowWindowTitle(
    bool* should_show_window_title) {
  *should_show_window_title =
      root_view_
          ? root_view_->GetWidget()->widget_delegate()->ShouldShowWindowTitle()
          : true;
  return true;
}

bool NativeWidgetMacNSWindowHost::GetCanWindowBecomeKey(
    bool* can_window_become_key) {
  *can_window_become_key =
      root_view_ ? root_view_->GetWidget()->CanActivate() : false;
  return true;
}

bool NativeWidgetMacNSWindowHost::GetAlwaysRenderWindowAsKey(
    bool* always_render_as_key) {
  *always_render_as_key =
      root_view_ ? root_view_->GetWidget()->ShouldPaintAsActive() : false;
  return true;
}

bool NativeWidgetMacNSWindowHost::OnWindowCloseRequested(
    bool* can_window_close) {
  *can_window_close = true;
  views::NonClientView* non_client_view =
      root_view_ ? root_view_->GetWidget()->non_client_view() : nullptr;
  if (non_client_view)
    *can_window_close = non_client_view->OnWindowCloseRequested() ==
                        CloseRequestResult::kCanClose;
  return true;
}

bool NativeWidgetMacNSWindowHost::GetWindowFrameTitlebarHeight(
    bool* override_titlebar_height,
    float* titlebar_height) {
  native_widget_mac_->GetWindowFrameTitlebarHeight(override_titlebar_height,
                                                   titlebar_height);
  return true;
}

void NativeWidgetMacNSWindowHost::OnFocusWindowToolbar() {
  native_widget_mac_->OnFocusWindowToolbar();
}

void NativeWidgetMacNSWindowHost::SetRemoteAccessibilityTokens(
    const std::vector<uint8_t>& window_token,
    const std::vector<uint8_t>& view_token) {
  remote_window_accessible_ =
      ui::RemoteAccessibility::GetRemoteElementFromToken(window_token);
  remote_view_accessible_ =
      ui::RemoteAccessibility::GetRemoteElementFromToken(view_token);
  [remote_view_accessible_ setWindowUIElement:remote_window_accessible_.get()];
  [remote_view_accessible_
      setTopLevelUIElement:remote_window_accessible_.get()];
}

bool NativeWidgetMacNSWindowHost::GetRootViewAccessibilityToken(
    int64_t* pid,
    std::vector<uint8_t>* token) {
  *pid = getpid();
  id element_id = GetNativeViewAccessible();
  *token = ui::RemoteAccessibility::GetTokenForLocalElement(element_id);
  return true;
}

bool NativeWidgetMacNSWindowHost::ValidateUserInterfaceItem(
    int32_t command,
    remote_cocoa::mojom::ValidateUserInterfaceItemResultPtr* out_result) {
  *out_result = remote_cocoa::mojom::ValidateUserInterfaceItemResult::New();
  native_widget_mac_->ValidateUserInterfaceItem(command, out_result->get());
  return true;
}

bool NativeWidgetMacNSWindowHost::WillExecuteCommand(
    int32_t command,
    WindowOpenDisposition window_open_disposition,
    bool is_before_first_responder,
    bool* will_execute) {
  *will_execute = native_widget_mac_->WillExecuteCommand(
      command, window_open_disposition, is_before_first_responder);
  return true;
}

bool NativeWidgetMacNSWindowHost::ExecuteCommand(
    int32_t command,
    WindowOpenDisposition window_open_disposition,
    bool is_before_first_responder,
    bool* was_executed) {
  *was_executed = native_widget_mac_->ExecuteCommand(
      command, window_open_disposition, is_before_first_responder);
  return true;
}

bool NativeWidgetMacNSWindowHost::HandleAccelerator(
    const ui::Accelerator& accelerator,
    bool require_priority_handler,
    bool* was_handled) {
  *was_handled = false;
  if (Widget* widget = native_widget_mac_->GetWidget()) {
    if (require_priority_handler &&
        !widget->GetFocusManager()->HasPriorityHandler(accelerator)) {
      return true;
    }
    *was_handled = widget->GetFocusManager()->ProcessAccelerator(accelerator);
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMacNSWindowHost,
// remote_cocoa::mojom::NativeWidgetNSWindowHost synchronous callbacks:

void NativeWidgetMacNSWindowHost::GetSheetOffsetY(
    GetSheetOffsetYCallback callback) {
  int32_t offset_y = 0;
  GetSheetOffsetY(&offset_y);
  std::move(callback).Run(offset_y);
}

void NativeWidgetMacNSWindowHost::DispatchKeyEventRemote(
    std::unique_ptr<ui::Event> event,
    DispatchKeyEventRemoteCallback callback) {
  bool event_handled = false;
  DispatchKeyEventRemote(std::move(event), &event_handled);
  std::move(callback).Run(event_handled);
}

void NativeWidgetMacNSWindowHost::DispatchKeyEventToMenuControllerRemote(
    std::unique_ptr<ui::Event> event,
    DispatchKeyEventToMenuControllerRemoteCallback callback) {
  ui::KeyEvent* key_event = event->AsKeyEvent();
  bool event_swallowed = DispatchKeyEventToMenuController(key_event);
  std::move(callback).Run(event_swallowed, key_event->handled());
}

void NativeWidgetMacNSWindowHost::DispatchMonitorEvent(
    std::unique_ptr<ui::Event> event,
    DispatchMonitorEventCallback callback) {
  bool event_handled = false;
  DispatchMonitorEvent(std::move(event), &event_handled);
  std::move(callback).Run(event_handled);
}

void NativeWidgetMacNSWindowHost::GetHasMenuController(
    GetHasMenuControllerCallback callback) {
  bool has_menu_controller = false;
  GetHasMenuController(&has_menu_controller);
  std::move(callback).Run(has_menu_controller);
}

void NativeWidgetMacNSWindowHost::GetIsDraggableBackgroundAt(
    const gfx::Point& location_in_content,
    GetIsDraggableBackgroundAtCallback callback) {
  bool is_draggable_background = false;
  GetIsDraggableBackgroundAt(location_in_content, &is_draggable_background);
  std::move(callback).Run(is_draggable_background);
}

void NativeWidgetMacNSWindowHost::GetTooltipTextAt(
    const gfx::Point& location_in_content,
    GetTooltipTextAtCallback callback) {
  std::u16string new_tooltip_text;
  GetTooltipTextAt(location_in_content, &new_tooltip_text);
  std::move(callback).Run(new_tooltip_text);
}

void NativeWidgetMacNSWindowHost::GetIsFocusedViewTextual(
    GetIsFocusedViewTextualCallback callback) {
  bool is_textual = false;
  GetIsFocusedViewTextual(&is_textual);
  std::move(callback).Run(is_textual);
}

void NativeWidgetMacNSWindowHost::GetWidgetIsModal(
    GetWidgetIsModalCallback callback) {
  bool widget_is_modal = false;
  GetWidgetIsModal(&widget_is_modal);
  std::move(callback).Run(widget_is_modal);
}

void NativeWidgetMacNSWindowHost::GetDialogButtonInfo(
    ui::DialogButton button,
    GetDialogButtonInfoCallback callback) {
  bool exists = false;
  std::u16string label;
  bool is_enabled = false;
  bool is_default = false;
  GetDialogButtonInfo(button, &exists, &label, &is_enabled, &is_default);
  std::move(callback).Run(exists, label, is_enabled, is_default);
}

void NativeWidgetMacNSWindowHost::GetDoDialogButtonsExist(
    GetDoDialogButtonsExistCallback callback) {
  bool buttons_exist = false;
  GetDoDialogButtonsExist(&buttons_exist);
  std::move(callback).Run(buttons_exist);
}

void NativeWidgetMacNSWindowHost::GetShouldShowWindowTitle(
    GetShouldShowWindowTitleCallback callback) {
  bool should_show_window_title = false;
  GetShouldShowWindowTitle(&should_show_window_title);
  std::move(callback).Run(should_show_window_title);
}

void NativeWidgetMacNSWindowHost::GetCanWindowBecomeKey(
    GetCanWindowBecomeKeyCallback callback) {
  bool can_window_become_key = false;
  GetCanWindowBecomeKey(&can_window_become_key);
  std::move(callback).Run(can_window_become_key);
}

void NativeWidgetMacNSWindowHost::GetAlwaysRenderWindowAsKey(
    GetAlwaysRenderWindowAsKeyCallback callback) {
  bool always_render_as_key = false;
  GetAlwaysRenderWindowAsKey(&always_render_as_key);
  std::move(callback).Run(always_render_as_key);
}

void NativeWidgetMacNSWindowHost::OnWindowCloseRequested(
    OnWindowCloseRequestedCallback callback) {
  bool can_window_close = false;
  OnWindowCloseRequested(&can_window_close);
  std::move(callback).Run(can_window_close);
}

void NativeWidgetMacNSWindowHost::GetWindowFrameTitlebarHeight(
    GetWindowFrameTitlebarHeightCallback callback) {
  bool override_titlebar_height = false;
  float titlebar_height = 0;
  GetWindowFrameTitlebarHeight(&override_titlebar_height, &titlebar_height);
  std::move(callback).Run(override_titlebar_height, titlebar_height);
}

void NativeWidgetMacNSWindowHost::GetRootViewAccessibilityToken(
    GetRootViewAccessibilityTokenCallback callback) {
  std::vector<uint8_t> token;
  int64_t pid;
  GetRootViewAccessibilityToken(&pid, &token);
  std::move(callback).Run(pid, token);
}

void NativeWidgetMacNSWindowHost::ValidateUserInterfaceItem(
    int32_t command,
    ValidateUserInterfaceItemCallback callback) {
  remote_cocoa::mojom::ValidateUserInterfaceItemResultPtr result;
  ValidateUserInterfaceItem(command, &result);
  std::move(callback).Run(std::move(result));
}

void NativeWidgetMacNSWindowHost::WillExecuteCommand(
    int32_t command,
    WindowOpenDisposition window_open_disposition,
    bool is_before_first_responder,
    ExecuteCommandCallback callback) {
  bool will_execute = false;
  WillExecuteCommand(command, window_open_disposition,
                     is_before_first_responder, &will_execute);
  std::move(callback).Run(will_execute);
}

void NativeWidgetMacNSWindowHost::ExecuteCommand(
    int32_t command,
    WindowOpenDisposition window_open_disposition,
    bool is_before_first_responder,
    ExecuteCommandCallback callback) {
  bool was_executed = false;
  ExecuteCommand(command, window_open_disposition, is_before_first_responder,
                 &was_executed);
  std::move(callback).Run(was_executed);
}

void NativeWidgetMacNSWindowHost::HandleAccelerator(
    const ui::Accelerator& accelerator,
    bool require_priority_handler,
    HandleAcceleratorCallback callback) {
  bool was_handled = false;
  HandleAccelerator(accelerator, require_priority_handler, &was_handled);
  std::move(callback).Run(was_handled);
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMacNSWindowHost, DialogObserver:

void NativeWidgetMacNSWindowHost::OnDialogChanged() {
  // Note it's only necessary to clear the TouchBar. If the OS needs it again,
  // a new one will be created.
  GetNSWindowMojo()->ClearTouchBar();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMacNSWindowHost, AccessibilityFocusOverrider::Client:

id NativeWidgetMacNSWindowHost::GetAccessibilityFocusedUIElement() {
  return [GetNativeViewAccessible() accessibilityFocusedUIElement];
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMacNSWindowHost, LayerDelegate:

void NativeWidgetMacNSWindowHost::OnPaintLayer(
    const ui::PaintContext& context) {
  native_widget_mac_->GetWidget()->OnNativeWidgetPaint(context);
}

void NativeWidgetMacNSWindowHost::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {
  native_widget_mac_->GetWidget()->DeviceScaleFactorChanged(
      old_device_scale_factor, new_device_scale_factor);
}

void NativeWidgetMacNSWindowHost::UpdateVisualState() {
  native_widget_mac_->GetWidget()->LayoutRootViewIfNecessary();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMacNSWindowHost, AcceleratedWidgetMac:

void NativeWidgetMacNSWindowHost::AcceleratedWidgetCALayerParamsUpdated() {
  if (const auto* ca_layer_params = compositor_->widget()->GetCALayerParams())
    GetNSWindowMojo()->SetCALayerParams(*ca_layer_params);

  // The VSync parameters skew over time (astonishingly quickly -- 0.1 msec per
  // second). If too much time has elapsed since the last time the vsync
  // parameters were calculated, re-calculate them.
  if (base::TimeTicks::Now() >= display_link_next_update_time_) {
    RequestVSyncParametersUpdate();
  }
}

void NativeWidgetMacNSWindowHost::RequestVSyncParametersUpdate() {
  if (!display_link_ || display_link_updater_) {
    return;
  }
  display_link_updater_ = display_link_->RegisterCallback(base::BindRepeating(
      &NativeWidgetMacNSWindowHost::OnVSyncParametersUpdated,
      weak_factory_for_vsync_update_.GetWeakPtr()));
}

void NativeWidgetMacNSWindowHost::OnVSyncParametersUpdated(
    ui::VSyncParamsMac params) {
  if (compositor_ && params.display_times_valid) {
    compositor_->compositor()->SetDisplayVSyncParameters(
        params.display_timebase, params.display_interval);
    display_link_next_update_time_ = base::TimeTicks::Now() + base::Seconds(10);
  }
  display_link_updater_ = nullptr;
}

}  // namespace views
