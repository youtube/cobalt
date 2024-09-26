// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller_chromeos.h"

#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chromeos/ui/base/tablet_state.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/immersive/immersive_revealed_lock.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_targeter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_context.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/views/background.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/view.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/ui/ash/window_pin_util.h"
#else
#include "chrome/browser/ui/lacros/window_properties.h"
#endif

namespace {

// Converts from ImmersiveModeController::AnimateReveal to
// chromeos::ImmersiveFullscreenController::AnimateReveal.
chromeos::ImmersiveFullscreenController::AnimateReveal
ToImmersiveFullscreenControllerAnimateReveal(
    ImmersiveModeController::AnimateReveal animate_reveal) {
  switch (animate_reveal) {
    case ImmersiveModeController::ANIMATE_REVEAL_YES:
      return chromeos::ImmersiveFullscreenController::ANIMATE_REVEAL_YES;
    case ImmersiveModeController::ANIMATE_REVEAL_NO:
      return chromeos::ImmersiveFullscreenController::ANIMATE_REVEAL_NO;
  }
  NOTREACHED_NORETURN();
}

class ImmersiveRevealedLockChromeos : public ImmersiveRevealedLock {
 public:
  explicit ImmersiveRevealedLockChromeos(chromeos::ImmersiveRevealedLock* lock)
      : lock_(lock) {}

  ImmersiveRevealedLockChromeos(const ImmersiveRevealedLockChromeos&) = delete;
  ImmersiveRevealedLockChromeos& operator=(
      const ImmersiveRevealedLockChromeos&) = delete;

 private:
  std::unique_ptr<chromeos::ImmersiveRevealedLock> lock_;
};

}  // namespace

ImmersiveModeControllerChromeos::ImmersiveModeControllerChromeos() = default;

ImmersiveModeControllerChromeos::~ImmersiveModeControllerChromeos() = default;

void ImmersiveModeControllerChromeos::Init(BrowserView* browser_view) {
  browser_view_ = browser_view;
  controller_.Init(this, browser_view_->frame(),
                   browser_view_->top_container());

  window_observation_.Observe(browser_view_->GetNativeWindow());
}

void ImmersiveModeControllerChromeos::SetEnabled(bool enabled) {
  if (controller_.IsEnabled() == enabled)
    return;

  if (!fullscreen_observer_.IsObserving()) {
    fullscreen_observer_.Observe(browser_view_->browser()
                                     ->exclusive_access_manager()
                                     ->fullscreen_controller());
  }

  chromeos::ImmersiveFullscreenController::EnableForWidget(
      browser_view_->frame(), enabled);
}

bool ImmersiveModeControllerChromeos::IsEnabled() const {
  return controller_.IsEnabled();
}

bool ImmersiveModeControllerChromeos::ShouldHideTopViews() const {
  return controller_.IsEnabled() && !controller_.IsRevealed();
}

bool ImmersiveModeControllerChromeos::IsRevealed() const {
  return controller_.IsRevealed();
}

int ImmersiveModeControllerChromeos::GetTopContainerVerticalOffset(
    const gfx::Size& top_container_size) const {
  if (!IsEnabled())
    return 0;

  return static_cast<int>(top_container_size.height() *
                          (visible_fraction_ - 1));
}

std::unique_ptr<ImmersiveRevealedLock>
ImmersiveModeControllerChromeos::GetRevealedLock(AnimateReveal animate_reveal) {
  return std::make_unique<ImmersiveRevealedLockChromeos>(
      controller_.GetRevealedLock(
          ToImmersiveFullscreenControllerAnimateReveal(animate_reveal)));
}

void ImmersiveModeControllerChromeos::OnFindBarVisibleBoundsChanged(
    const gfx::Rect& new_visible_bounds_in_screen) {
  find_bar_visible_bounds_in_screen_ = new_visible_bounds_in_screen;
}

bool ImmersiveModeControllerChromeos::
    ShouldStayImmersiveAfterExitingFullscreen() {
  return !browser_view_->GetSupportsTabStrip() &&
         chromeos::TabletState::Get()->InTabletMode();
}

void ImmersiveModeControllerChromeos::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  if (browser_view_->GetSupportsTabStrip())
    return;

  if (!chromeos::TabletState::Get()->InTabletMode())
    return;

  // Don't use immersive mode as long as we are in the locked fullscreen mode
  // since immersive shows browser controls which allow exiting the mode.
  if (platform_util::IsBrowserLockedFullscreen(browser_view_->browser()))
    return;

  // TODO(sammiequon): Investigate if we can move immersive mode logic to the
  // browser non client frame view.
  DCHECK_EQ(browser_view_->frame(), widget);
  if (widget->GetNativeWindow()->GetProperty(chromeos::kWindowStateTypeKey) ==
      chromeos::WindowStateType::kFloated) {
    chromeos::ImmersiveFullscreenController::EnableForWidget(widget, false);
    return;
  }

  // Enable immersive mode if the widget is activated. Do not disable immersive
  // mode if the widget deactivates, but is not minimized.
  chromeos::ImmersiveFullscreenController::EnableForWidget(
      widget, active || !widget->IsMinimized());
}

void ImmersiveModeControllerChromeos::LayoutBrowserRootView() {
  views::Widget* widget = browser_view_->frame();
  // Update the window caption buttons.
  widget->non_client_view()->frame_view()->ResetWindowControls();
  widget->non_client_view()->frame_view()->InvalidateLayout();
  browser_view_->InvalidateLayout();
  widget->GetRootView()->Layout();
}

void ImmersiveModeControllerChromeos::OnImmersiveRevealStarted() {
  visible_fraction_ = 0;
  for (Observer& observer : observers_)
    observer.OnImmersiveRevealStarted();
}

void ImmersiveModeControllerChromeos::OnImmersiveRevealEnded() {
  visible_fraction_ = 0;
  browser_view_->contents_web_view()->holder()->SetHitTestTopInset(0);
  for (Observer& observer : observers_)
    observer.OnImmersiveRevealEnded();
}

void ImmersiveModeControllerChromeos::OnImmersiveFullscreenEntered() {}

void ImmersiveModeControllerChromeos::OnImmersiveFullscreenExited() {
  browser_view_->contents_web_view()->holder()->SetHitTestTopInset(0);
  for (Observer& observer : observers_)
    observer.OnImmersiveFullscreenExited();
}

void ImmersiveModeControllerChromeos::SetVisibleFraction(
    double visible_fraction) {
  if (visible_fraction_ == visible_fraction)
    return;

  // Sets the top inset only when the top-of-window views is fully visible. This
  // means some gesture may not be recognized well during the animation, but
  // that's fine since a complicated gesture wouldn't be involved during the
  // animation duration. See: https://crbug.com/901544.
  if (browser_view_->GetSupportsTabStrip()) {
    if (visible_fraction == 1.0) {
      browser_view_->contents_web_view()->holder()->SetHitTestTopInset(
          browser_view_->top_container()->height());
    } else if (visible_fraction_ == 1.0) {
      browser_view_->contents_web_view()->holder()->SetHitTestTopInset(0);
    }
  }
  visible_fraction_ = visible_fraction;
  browser_view_->Layout();
}

std::vector<gfx::Rect>
ImmersiveModeControllerChromeos::GetVisibleBoundsInScreen() const {
  views::View* top_container_view = browser_view_->top_container();
  gfx::Rect top_container_view_bounds = top_container_view->GetVisibleBounds();
  // TODO(tdanderson): Implement View::ConvertRectToScreen().
  gfx::Point top_container_view_bounds_in_screen_origin(
      top_container_view_bounds.origin());
  views::View::ConvertPointToScreen(
      top_container_view, &top_container_view_bounds_in_screen_origin);
  gfx::Rect top_container_view_bounds_in_screen(
      top_container_view_bounds_in_screen_origin,
      top_container_view_bounds.size());

  std::vector<gfx::Rect> bounds_in_screen;
  bounds_in_screen.push_back(top_container_view_bounds_in_screen);
  bounds_in_screen.push_back(find_bar_visible_bounds_in_screen_);
  return bounds_in_screen;
}

void ImmersiveModeControllerChromeos::OnFullscreenStateChanged() {
  if (!controller_.IsEnabled())
    return;

  // Auto hide the shelf in immersive browser fullscreen.
  bool in_tab_fullscreen = browser_view_->browser()
                               ->exclusive_access_manager()
                               ->fullscreen_controller()
                               ->IsWindowFullscreenForTabOrPending();
  browser_view_->GetNativeWindow()->SetProperty(
      chromeos::kHideShelfWhenFullscreenKey, in_tab_fullscreen);
}

void ImmersiveModeControllerChromeos::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
  bool pin_state_transition = false;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(crbug.com/1250129): Get pin state from exo.
  pin_state_transition = key == lacros::kWindowPinTypeKey;
#else
  // Track locked fullscreen changes.
  if (key == chromeos::kWindowStateTypeKey) {
    auto old_type = static_cast<chromeos::WindowStateType>(old);
    // Check if there is a transition into or out of a pinned state.
    pin_state_transition =
        IsWindowPinned(window) || chromeos::IsPinnedWindowStateType(old_type);
  }
#endif
  if (pin_state_transition) {
    browser_view_->FullscreenStateChanging();
    return;
  }

  if (key == aura::client::kShowStateKey) {
    ui::WindowShowState new_state =
        window->GetProperty(aura::client::kShowStateKey);
    auto old_state = static_cast<ui::WindowShowState>(old);

    // Make sure the browser stays up to date with the window's state. This is
    // necessary in classic Ash if the user exits fullscreen with the restore
    // button, and it's necessary in OopAsh if the window manager initiates a
    // fullscreen mode change (e.g. due to a WM shortcut).
    if (new_state == ui::SHOW_STATE_FULLSCREEN ||
        old_state == ui::SHOW_STATE_FULLSCREEN) {
      // If the browser view initiated this state change,
      // BrowserView::ProcessFullscreen will no-op, so this call is harmless.
      browser_view_->FullscreenStateChanging();
    }
  }
}

void ImmersiveModeControllerChromeos::OnWindowDestroying(aura::Window* window) {
  // Clean up observers here rather than in the destructor because the owning
  // BrowserView has already destroyed the aura::Window.
  DCHECK(window_observation_.IsObservingSource(window));
  window_observation_.Reset();
  DCHECK(!window_observation_.IsObserving());
}
