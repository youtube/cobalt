// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/chrome_native_app_window_views_aura_ash.h"

#include <utility>

#include "apps/ui/views/app_window_frame_view.h"
#include "ash/constants/app_types.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/window_backdrop.h"
#include "ash/public/cpp/window_properties.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/icon_standardizer.h"
#include "chrome/browser/ash/note_taking_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_context_menu.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller.h"
#include "components/app_restore/app_restore_utils.h"
#include "components/app_restore/window_properties.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/session_manager/core/session_manager.h"
#include "extensions/browser/app_window/app_delegate.h"
#include "extensions/common/constants.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/hit_test.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

using extensions::AppWindow;

ChromeNativeAppWindowViewsAuraAsh::ChromeNativeAppWindowViewsAuraAsh()
    : exclusive_access_manager_(
          std::make_unique<ExclusiveAccessManager>(this)) {
  if (ash::TabletMode::Get())
    ash::TabletMode::Get()->AddObserver(this);
}

ChromeNativeAppWindowViewsAuraAsh::~ChromeNativeAppWindowViewsAuraAsh() {
  if (ash::TabletMode::Get())
    ash::TabletMode::Get()->RemoveObserver(this);
}

///////////////////////////////////////////////////////////////////////////////
// NativeAppWindowViews implementation:
void ChromeNativeAppWindowViewsAuraAsh::InitializeWindow(
    AppWindow* app_window,
    const AppWindow::CreateParams& create_params) {
  ChromeNativeAppWindowViewsAura::InitializeWindow(app_window, create_params);
  aura::Window* window = GetNativeWindow();

  // Fullscreen doesn't always imply immersive mode (see
  // ShouldEnableImmersive()).
  window->SetProperty(chromeos::kImmersiveImpliedByFullscreen, false);
  // TODO(https://crbug.com/997480): Determine if all non-resizable windows
  // should have this behavior, or just the feedback app.
  window_observation_.Observe(window);
}

///////////////////////////////////////////////////////////////////////////////
// ChromeNativeAppWindowViews implementation:
void ChromeNativeAppWindowViewsAuraAsh::OnBeforeWidgetInit(
    const AppWindow::CreateParams& create_params,
    views::Widget::InitParams* init_params,
    views::Widget* widget) {
  ChromeNativeAppWindowViewsAura::OnBeforeWidgetInit(create_params, init_params,
                                                     widget);
  // Some windows need to be placed in special containers, for example to make
  // them visible at the login or lock screen.
  absl::optional<int> container_id;
  if (create_params.is_ime_window)
    container_id = ash::kShellWindowId_ImeWindowParentContainer;
  else if (create_params.show_on_lock_screen)
    container_id = ash::kShellWindowId_LockActionHandlerContainer;

  if (container_id.has_value()) {
    ash_util::SetupWidgetInitParamsForContainer(init_params, *container_id);
    if (!ash::IsActivatableShellWindowId(*container_id)) {
      // This ensures calls to Activate() don't attempt to activate the window
      // locally, which can have side effects that should be avoided (such as
      // changing focus). See https://crbug.com/935274 for more details.
      init_params->activatable = views::Widget::InitParams::Activatable::kNo;
    }
  }

  // Resizable lock screen apps will end up maximized by ash. Do it now to
  // save back-and-forth communication with the window manager. Right now all
  // lock screen apps either end up maximized (e.g. Keep) or are not resizable.
  if (create_params.show_on_lock_screen && create_params.resizable) {
    DCHECK_EQ(ui::SHOW_STATE_DEFAULT, init_params->show_state);
    init_params->show_state = ui::SHOW_STATE_MAXIMIZED;
  }

  const int32_t restore_window_id =
      app_restore::FetchRestoreWindowId(app_window()->extension_id());
  init_params->init_properties_container.SetProperty(
      app_restore::kWindowIdKey, app_window()->session_id().id());
  init_params->init_properties_container.SetProperty(
      app_restore::kRestoreWindowIdKey, restore_window_id);
  init_params->init_properties_container.SetProperty(
      app_restore::kAppIdKey, app_window()->extension_id());
  init_params->init_properties_container.SetProperty(
      aura::client::kAppType, static_cast<int>(ash::AppType::CHROME_APP));

  app_restore::ModifyWidgetParams(restore_window_id, init_params);
}

std::unique_ptr<views::NonClientFrameView>
ChromeNativeAppWindowViewsAuraAsh::CreateNonStandardAppFrame() {
  auto frame = std::make_unique<apps::AppWindowFrameView>(
      widget(), this, HasFrameColor(), ActiveFrameColor(),
      InactiveFrameColor());
  frame->Init();

  // For Aura windows on the Ash desktop the sizes are different and the user
  // can resize the window from slightly outside the bounds as well.
  frame->SetResizeSizes(chromeos::kResizeInsideBoundsSize,
                        chromeos::kResizeOutsideBoundsSize,
                        chromeos::kResizeAreaCornerSize);
  return frame;
}

ui::ImageModel ChromeNativeAppWindowViewsAuraAsh::GetWindowIcon() {
  const ui::ImageModel& image = ChromeNativeAppWindowViews::GetWindowIcon();
  if (image.IsEmpty())
    return ui::ImageModel();

  DCHECK(image.IsImage());
  const gfx::ImageSkia image_skia = image.Rasterize(nullptr);
  return ui::ImageModel::FromImageSkia(
      apps::CreateStandardIconImage(image_skia));
}

bool ChromeNativeAppWindowViewsAuraAsh::ShouldRemoveStandardFrame() {
  if (IsFrameless())
    return true;

  return HasFrameColor();
}

void ChromeNativeAppWindowViewsAuraAsh::EnsureAppIconCreated() {
  LoadAppIcon(true /* allow_placeholder_icon */);
}

///////////////////////////////////////////////////////////////////////////////
// ui::BaseWindow implementation:
gfx::Rect ChromeNativeAppWindowViewsAuraAsh::GetRestoredBounds() const {
  gfx::Rect* bounds =
      GetNativeWindow()->GetProperty(ash::kRestoreBoundsOverrideKey);
  if (bounds && !bounds->IsEmpty())
    return *bounds;

  return ChromeNativeAppWindowViewsAura::GetRestoredBounds();
}

ui::WindowShowState
ChromeNativeAppWindowViewsAuraAsh::GetRestoredState() const {
  // Use kRestoreShowStateKey to get the window restore show state in case a
  // window is minimized/hidden.
  ui::WindowShowState restore_state =
      GetNativeWindow()->GetProperty(aura::client::kRestoreShowStateKey);

  bool is_fullscreen = false;
  if (GetNativeWindow()->GetProperty(ash::kRestoreBoundsOverrideKey)) {
    // If an override is given, use that restore state, unless the window is in
    // immersive fullscreen.
    restore_state = chromeos::ToWindowShowState(GetNativeWindow()->GetProperty(
        ash::kRestoreWindowStateTypeOverrideKey));
    is_fullscreen = restore_state == ui::SHOW_STATE_FULLSCREEN;
  } else {
    if (IsMaximized())
      return ui::SHOW_STATE_MAXIMIZED;
    is_fullscreen = IsFullscreen();
  }

  if (is_fullscreen) {
    if (IsImmersiveModeEnabled()) {
      // Restore windows which were previously in immersive fullscreen to their
      // pre-fullscreen state. Restoring the window to a different fullscreen
      // type makes for a bad experience.
      return GetNativeWindow()->GetProperty(aura::client::kRestoreShowStateKey);
    }
    return ui::SHOW_STATE_FULLSCREEN;
  }

  return GetRestorableState(restore_state);
}

ui::ZOrderLevel ChromeNativeAppWindowViewsAuraAsh::GetZOrderLevel() const {
  return widget()->GetZOrderLevel();
}

///////////////////////////////////////////////////////////////////////////////
// views::ContextMenuController implementation:
void ChromeNativeAppWindowViewsAuraAsh::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& p,
    ui::MenuSourceType source_type) {
  menu_model_ = CreateMultiUserContextMenu(GetNativeWindow());
  if (!menu_model_)
    return;

  // Only show context menu if point is in caption.
  gfx::Point point_in_view_coords(p);
  views::View::ConvertPointFromScreen(widget()->non_client_view(),
                                      &point_in_view_coords);
  int hit_test =
      widget()->non_client_view()->NonClientHitTest(point_in_view_coords);
  if (hit_test == HTCAPTION) {
    menu_runner_ = std::make_unique<views::MenuRunner>(
        menu_model_.get(),
        views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU,
        base::BindRepeating(&ChromeNativeAppWindowViewsAuraAsh::OnMenuClosed,
                            base::Unretained(this)));
    menu_runner_->RunMenuAt(source->GetWidget(), nullptr,
                            gfx::Rect(p, gfx::Size(0, 0)),
                            views::MenuAnchorPosition::kTopLeft, source_type);
  } else {
    menu_model_.reset();
  }
}

///////////////////////////////////////////////////////////////////////////////
// WidgetDelegate implementation:
std::unique_ptr<views::NonClientFrameView>
ChromeNativeAppWindowViewsAuraAsh::CreateNonClientFrameView(
    views::Widget* widget) {
  if (IsFrameless())
    return CreateNonStandardAppFrame();

  window_state_observation_.Observe(ash::WindowState::Get(GetNativeWindow()));

  auto custom_frame_view = std::make_unique<ash::NonClientFrameViewAsh>(widget);

  custom_frame_view->GetHeaderView()->set_context_menu_controller(this);

  // Enter immersive mode if the app is opened in tablet mode with the hide
  // titlebars feature enabled.
  UpdateImmersiveMode();

  if (HasFrameColor()) {
    custom_frame_view->SetFrameColors(ActiveFrameColor(),
                                      InactiveFrameColor());
  }

  return custom_frame_view;
}

///////////////////////////////////////////////////////////////////////////////
// NativeAppWindow implementation:
void ChromeNativeAppWindowViewsAuraAsh::SetFullscreen(int fullscreen_types) {
  ChromeNativeAppWindowViewsAura::SetFullscreen(fullscreen_types);
  UpdateImmersiveMode();

  // In a public session, display a toast with instructions on exiting
  // fullscreen.
  if (profiles::IsPublicSession()) {
    UpdateExclusiveAccessExitBubbleContent(
        GURL(),
        fullscreen_types & (AppWindow::FULLSCREEN_TYPE_HTML_API |
                            AppWindow::FULLSCREEN_TYPE_WINDOW_API)
            ? EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION
            : EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE,
        ExclusiveAccessBubbleHideCallback(),
        /*notify_download=*/false,
        /*force_update=*/false);
  }

  // Autohide the shelf instead of hiding the shelf completely when only in
  // OS fullscreen or when in a public session.
  const bool should_hide_shelf =
      !profiles::IsPublicSession() &&
      fullscreen_types != AppWindow::FULLSCREEN_TYPE_OS;
  widget()->GetNativeWindow()->SetProperty(
      chromeos::kHideShelfWhenFullscreenKey, should_hide_shelf);

  // Invalidate the frame to ensure that it is re-laid out (even if the bounds
  // don't change) so that the frame sets the bounds of the client view.
  widget()->non_client_view()->frame_view()->InvalidateLayout();
}

void ChromeNativeAppWindowViewsAuraAsh::SetActivateOnPointer(
    bool activate_on_pointer) {
  widget()->GetNativeWindow()->SetProperty(aura::client::kActivateOnPointerKey,
                                           activate_on_pointer);
}

///////////////////////////////////////////////////////////////////////////////
// TabletModeObserver implementation:
void ChromeNativeAppWindowViewsAuraAsh::OnTabletModeStarted() {
  OnTabletModeToggled(true);
}

void ChromeNativeAppWindowViewsAuraAsh::OnTabletModeEnded() {
  OnTabletModeToggled(false);
}

///////////////////////////////////////////////////////////////////////////////
// ui::AcceleratorProvider implementation:
bool ChromeNativeAppWindowViewsAuraAsh::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  // Normally |accelerator| is used to determine the text in the bubble;
  // however, for the fullscreen type set in SetFullscreen(), the bubble
  // currently ignores it, and will always use IDS_APP_ESC_KEY. Be explicit here
  // anyway.
  *accelerator = ui::Accelerator(ui::KeyboardCode::VKEY_ESCAPE, ui::EF_NONE);
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// ExclusiveAccessContext implementation:
Profile* ChromeNativeAppWindowViewsAuraAsh::GetProfile() {
  return Profile::FromBrowserContext(web_view()->GetBrowserContext());
}

bool ChromeNativeAppWindowViewsAuraAsh::IsFullscreen() const {
  return NativeAppWindowViews::IsFullscreen();
}

void ChromeNativeAppWindowViewsAuraAsh::EnterFullscreen(
    const GURL& url,
    ExclusiveAccessBubbleType bubble_type,
    const int64_t display_id) {
  // This codepath is never hit for Chrome Apps.
  NOTREACHED_NORETURN();
}

void ChromeNativeAppWindowViewsAuraAsh::ExitFullscreen() {
  // This codepath is never hit for Chrome Apps.
  NOTREACHED_NORETURN();
}

void ChromeNativeAppWindowViewsAuraAsh::UpdateExclusiveAccessExitBubbleContent(
    const GURL& url,
    ExclusiveAccessBubbleType bubble_type,
    ExclusiveAccessBubbleHideCallback bubble_first_hide_callback,
    bool notify_download,
    bool force_update) {
  DCHECK(!notify_download || exclusive_access_bubble_);
  if (bubble_type == EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE && !notify_download) {
    exclusive_access_bubble_.reset();
    if (bubble_first_hide_callback) {
      std::move(bubble_first_hide_callback)
          .Run(ExclusiveAccessBubbleHideReason::kNotShown);
    }
    return;
  }

  if (exclusive_access_bubble_) {
    exclusive_access_bubble_->UpdateContent(
        url, bubble_type, std::move(bubble_first_hide_callback),
        notify_download, force_update);
    return;
  }

  exclusive_access_bubble_ = std::make_unique<ExclusiveAccessBubbleViews>(
      this, url, bubble_type, std::move(bubble_first_hide_callback));
}

bool ChromeNativeAppWindowViewsAuraAsh::IsExclusiveAccessBubbleDisplayed()
    const {
  return exclusive_access_bubble_ && exclusive_access_bubble_->IsShowing();
}

void ChromeNativeAppWindowViewsAuraAsh::OnExclusiveAccessUserInput() {
  if (exclusive_access_bubble_)
    exclusive_access_bubble_->OnUserInput();
}

content::WebContents*
ChromeNativeAppWindowViewsAuraAsh::GetActiveWebContents() {
  return web_view()->web_contents();
}

bool ChromeNativeAppWindowViewsAuraAsh::CanUserExitFullscreen() const {
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// ExclusiveAccessBubbleViewsContext implementation:
ExclusiveAccessManager*
ChromeNativeAppWindowViewsAuraAsh::GetExclusiveAccessManager() {
  return exclusive_access_manager_.get();
}

views::Widget* ChromeNativeAppWindowViewsAuraAsh::GetBubbleAssociatedWidget() {
  return widget();
}

ui::AcceleratorProvider*
ChromeNativeAppWindowViewsAuraAsh::GetAcceleratorProvider() {
  return this;
}

gfx::NativeView ChromeNativeAppWindowViewsAuraAsh::GetBubbleParentView() const {
  return widget()->GetNativeView();
}

gfx::Point ChromeNativeAppWindowViewsAuraAsh::GetCursorPointInParent() const {
  gfx::Point cursor_pos = display::Screen::GetScreen()->GetCursorScreenPoint();
  views::View::ConvertPointFromScreen(widget()->GetRootView(), &cursor_pos);
  return cursor_pos;
}

gfx::Rect ChromeNativeAppWindowViewsAuraAsh::GetClientAreaBoundsInScreen()
    const {
  return widget()->GetClientAreaBoundsInScreen();
}

bool ChromeNativeAppWindowViewsAuraAsh::IsImmersiveModeEnabled() const {
  return GetWidget()->GetNativeWindow()->GetProperty(
      chromeos::kImmersiveIsActive);
}

gfx::Rect ChromeNativeAppWindowViewsAuraAsh::GetTopContainerBoundsInScreen() {
  gfx::Rect* bounds = GetWidget()->GetNativeWindow()->GetProperty(
      chromeos::kImmersiveTopContainerBoundsInScreen);
  return bounds ? *bounds : gfx::Rect();
}

void ChromeNativeAppWindowViewsAuraAsh::DestroyAnyExclusiveAccessBubble() {
  exclusive_access_bubble_.reset();
}

bool ChromeNativeAppWindowViewsAuraAsh::CanTriggerOnMouse() const {
  return true;
}

void ChromeNativeAppWindowViewsAuraAsh::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  ChromeNativeAppWindowViewsAura::OnWidgetActivationChanged(widget, active);
  // In splitview, minimized windows go back into the overview grid. If we
  // minimize by using the minimize button on the immersive header, the
  // overview window will calculate the title bar offset and the window will be
  // missing its top portion. Prevent this by disabling immersive mode upon
  // minimize.
  UpdateImmersiveMode();
}

void ChromeNativeAppWindowViewsAuraAsh::OnPostWindowStateTypeChange(
    ash::WindowState* window_state,
    chromeos::WindowStateType old_type) {
  DCHECK(!IsFrameless());
  DCHECK_EQ(GetNativeWindow(), window_state->window());
  if (window_state->IsFullscreen() != app_window()->IsFullscreen()) {
    // Report OS-initiated state changes to |app_window()|. This is done in
    // OnPostWindowStateTypeChange rather than OnWindowPropertyChanged because
    // WindowState saves restore bounds *after* changing the property, and
    // enabling immersive mode will change the current bounds before the old
    // bounds can be saved.
    if (window_state->IsFullscreen())
      app_window()->OSFullscreen();
    else
      app_window()->OnNativeWindowChanged();
  }
}

void ChromeNativeAppWindowViewsAuraAsh::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
  if (key != aura::client::kShowStateKey)
    return;

  auto new_state = window->GetProperty(aura::client::kShowStateKey);

  if (new_state != ui::SHOW_STATE_FULLSCREEN &&
      new_state != ui::SHOW_STATE_MINIMIZED && app_window()->IsFullscreen()) {
    app_window()->Restore();
  }

  // Usually OnNativeWindowChanged() is called when the window bounds are
  // changed as a result of a state type change. Because the change in
  // state type has already occurred, we need to call
  // OnNativeWindowChanged() explicitly.
  app_window()->OnNativeWindowChanged();
  UpdateImmersiveMode();
}

void ChromeNativeAppWindowViewsAuraAsh::OnWindowDestroying(
    aura::Window* window) {
  window_state_observation_.Reset();
  DCHECK(window_observation_.IsObservingSource(window));
  window_observation_.Reset();
}

void ChromeNativeAppWindowViewsAuraAsh::OnTabletModeToggled(bool enabled) {
  tablet_mode_enabled_ = enabled;
  UpdateImmersiveMode();
  widget()->non_client_view()->Layout();
}

void ChromeNativeAppWindowViewsAuraAsh::OnMenuClosed() {
  menu_runner_.reset();
  menu_model_.reset();
}

bool ChromeNativeAppWindowViewsAuraAsh::ShouldEnableImmersiveMode() const {
  // No immersive mode for forced fullscreen or frameless windows.
  if (app_window()->IsForcedFullscreen() || IsFrameless())
    return false;

  // Always use immersive mode when fullscreen is set by the OS.
  if (app_window()->IsOsFullscreen())
    return true;

  // Windows in tablet mode which are resizable have their title bars
  // hidden in ash for more size, so enable immersive mode so users
  // have access to window controls. Non resizable windows do not gain
  // size by hidding the title bar, so it is not hidden and thus there
  // is no need for immersive mode.
  // TODO(crbug.com/801619): This adds a little extra animation
  // when minimizing or unminimizing window.
  return ash::TabletMode::IsInTabletMode() && CanResize() && !IsMinimized() &&
         GetNativeWindow()->GetProperty(chromeos::kWindowStateTypeKey) !=
             chromeos::WindowStateType::kFloated;
}

void ChromeNativeAppWindowViewsAuraAsh::UpdateImmersiveMode() {
  chromeos::ImmersiveFullscreenController::EnableForWidget(
      widget(), ShouldEnableImmersiveMode());
}

gfx::Image ChromeNativeAppWindowViewsAuraAsh::GetCustomImage() {
  gfx::Image image = ChromeNativeAppWindowViews::GetCustomImage();
  return !image.IsEmpty()
             ? gfx::Image(apps::CreateStandardIconImage(image.AsImageSkia()))
             : gfx::Image();
}

gfx::Image ChromeNativeAppWindowViewsAuraAsh::GetAppIconImage() {
  if (!app_icon_image_skia_.isNull())
    return gfx::Image(app_icon_image_skia_);

  return ChromeNativeAppWindowViews::GetAppIconImage();
}

void ChromeNativeAppWindowViewsAuraAsh::LoadAppIcon(
    bool allow_placeholder_icon) {
  if (apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
          Profile::FromBrowserContext(app_window()->browser_context()))) {
    apps::AppServiceProxy* proxy = apps::AppServiceProxyFactory::GetForProfile(
        Profile::FromBrowserContext(app_window()->browser_context()));

    auto app_type =
        proxy->AppRegistryCache().GetAppType(app_window()->extension_id());

    if (app_type != apps::AppType::kUnknown) {
      proxy->LoadIcon(
          app_type, app_window()->extension_id(), apps::IconType::kStandard,
          app_window()->app_delegate()->PreferredIconSize(),
          allow_placeholder_icon,
          base::BindOnce(&ChromeNativeAppWindowViewsAuraAsh::OnLoadIcon,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }

  // Ensures the Chrome app icon is created to generate the default app icon.
  // Otherwise, the test cases are broken.
  ChromeNativeAppWindowViews::EnsureAppIconCreated();
}

void ChromeNativeAppWindowViewsAuraAsh::OnLoadIcon(
    apps::IconValuePtr icon_value) {
  if (!icon_value || icon_value->icon_type != apps::IconType::kStandard)
    return;

  app_icon_image_skia_ = icon_value->uncompressed;

  if (icon_value->is_placeholder_icon)
    LoadAppIcon(false /* allow_placeholder_icon */);
}
