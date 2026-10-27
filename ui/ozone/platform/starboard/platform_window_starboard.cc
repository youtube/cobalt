// Copyright 2024 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ui/ozone/platform/starboard/platform_window_starboard.h"

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "starboard/event.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/starboard/platform_event_source_starboard.h"
#include "ui/ozone/platform/starboard/platform_screen_starboard.h"

#if BUILDFLAG(IS_COBALT)

namespace ui {

namespace {
constexpr char kContentShellHostWindowSize[] = "content-shell-host-window-size";

std::unique_ptr<PlatformWindowStarboard::WindowCreatedCallback>
    g_created_callback =
        std::make_unique<PlatformWindowStarboard::WindowCreatedCallback>(
            base::DoNothing());
std::unique_ptr<PlatformWindowStarboard::WindowDestroyedCallback>
    g_destroyed_callback =
        std::make_unique<PlatformWindowStarboard::WindowDestroyedCallback>(
            base::DoNothing());
}  // namespace

// static
void PlatformWindowStarboard::SetWindowCreatedCallback(
    WindowCreatedCallback cb) {
  g_created_callback = std::make_unique<WindowCreatedCallback>(std::move(cb));
}

// static
void PlatformWindowStarboard::ClearWindowCreatedCallback() {
  g_created_callback =
      std::make_unique<WindowCreatedCallback>(base::DoNothing());
}

// static
void PlatformWindowStarboard::SetWindowDestroyedCallback(
    WindowDestroyedCallback cb) {
  g_destroyed_callback =
      std::make_unique<WindowDestroyedCallback>(std::move(cb));
}

// static
void PlatformWindowStarboard::ClearWindowDestroyedCallback() {
  g_destroyed_callback =
      std::make_unique<WindowDestroyedCallback>(base::DoNothing());
}

void PlatformWindowStarboard::SetWaitingForRevealAck(bool waiting) {
  waiting_for_reveal_ack_ = waiting;
}

PlatformWindowStarboard::PlatformWindowStarboard(
    PlatformWindowDelegate* delegate,
    const gfx::Rect& bounds)
    : bounds_(bounds), delegate_(delegate) {
  DCHECK(delegate);
  if (PlatformEventSource::GetInstance()) {
    PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
    static_cast<PlatformEventSourceStarboard*>(
        PlatformEventSource::GetInstance())
        ->AddPlatformEventObserverStarboard(this);
  }

  Show(false /* inactive */);
}

PlatformWindowStarboard::~PlatformWindowStarboard() {
  if (PlatformEventSource::GetInstance()) {
    PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
    static_cast<PlatformEventSourceStarboard*>(
        PlatformEventSource::GetInstance())
        ->RemovePlatformEventObserverStarboard(this);
  }
  DestroySbWindowInstance();
}

bool PlatformWindowStarboard::CanDispatchEvent(const PlatformEvent& event) {
  return true;
}

uint32_t PlatformWindowStarboard::DispatchEvent(const PlatformEvent& event) {
  DispatchEventFromNativeUiEvent(
      event, base::BindOnce(&PlatformWindowDelegate::DispatchEvent,
                            base::Unretained(delegate())));

  return ui::POST_DISPATCH_STOP_PROPAGATION;
}

void PlatformWindowStarboard::ProcessWindowSizeChangedEvent(int width,
                                                            int height) {
  gfx::Rect old_bounds = PlatformWindowStarboard::GetBoundsInPixels();
  gfx::Rect new_bounds_px(old_bounds.x(), old_bounds.y(), width, height);
  PlatformWindowStarboard::SetBoundsInPixels(new_bounds_px);
}

void PlatformWindowStarboard::ProcessFocusEvent(bool is_focused) {
  if (is_focused) {
    Activate();
  } else {
    Deactivate();
  }
}

bool PlatformWindowStarboard::ShouldUseNativeFrame() const {
  return use_native_frame_;
}

void PlatformWindowStarboard::SetUseNativeFrame(bool use_native_frame) {
  use_native_frame_ = use_native_frame;
}

void PlatformWindowStarboard::SetBoundsInPixels(const gfx::Rect& bounds) {
  // Even if the pixel bounds didn't change this call to the delegate should
  // still happen. The device scale factor may have changed which effectively
  // changes the bounds.
  bool origin_changed = bounds_.origin() != bounds.origin();
  bounds_ = bounds;
  // Keep the primary display bounds in PlatformScreenStarboard synchronized
  // with any PlatformWindow bounds update.
  if (PlatformScreenStarboard::GetInstance()) {
    PlatformScreenStarboard::GetInstance()->UpdateDisplayBounds(bounds_);
  }
  delegate_->OnBoundsChanged({origin_changed});
}

gfx::Rect PlatformWindowStarboard::GetBoundsInPixels() const {
  return bounds_;
}

void PlatformWindowStarboard::SetBoundsInDIP(const gfx::Rect& bounds) {
  SetBoundsInPixels(delegate_->ConvertRectToPixels(bounds));
}

gfx::Rect PlatformWindowStarboard::GetBoundsInDIP() const {
  return delegate_->ConvertRectToDIP(bounds_);
}

void PlatformWindowStarboard::Show(bool inactive) {
  CreateSbWindow(/*notify_bounds_changed=*/false);
  EnsureAcceleratedWidgetAvailable();
}

void PlatformWindowStarboard::Hide() {
  if (widget_available_) {
    widget_available_ = false;
    delegate_->OnAcceleratedWidgetDestroyed();
  }
  DestroySbWindowInstance();
}

void PlatformWindowStarboard::Close() {
  delegate_->OnClosed();
}

bool PlatformWindowStarboard::IsVisible() const {
  return SbWindowIsValid(sb_window_);
}

void PlatformWindowStarboard::PrepareForShutdown() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void PlatformWindowStarboard::SetTitle(const std::u16string& title) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void PlatformWindowStarboard::SetCapture() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void PlatformWindowStarboard::ReleaseCapture() {
  NOTIMPLEMENTED_LOG_ONCE();
}

bool PlatformWindowStarboard::HasCapture() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void PlatformWindowStarboard::SetFullscreen(bool fullscreen,
                                            int64_t target_display_id) {
  DCHECK_EQ(target_display_id, display::kInvalidDisplayId);
  window_state_ = fullscreen ? ui::PlatformWindowState::kFullScreen
                             : ui::PlatformWindowState::kUnknown;
}

void PlatformWindowStarboard::Maximize() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void PlatformWindowStarboard::Minimize() {
  if (widget_available_) {
    widget_available_ = false;
    delegate_->OnAcceleratedWidgetDestroyed();
  }
  DestroySbWindowInstance();
}

void PlatformWindowStarboard::DestroySbWindowInstance() {
  if (SbWindowIsValid(sb_window_)) {
    (*g_destroyed_callback).Run(sb_window_);
    SbWindowDestroy(sb_window_);
    sb_window_ = kSbWindowInvalid;
  }
}

void PlatformWindowStarboard::Restore() {
  CreateSbWindow(/*notify_bounds_changed=*/true);
  EnsureAcceleratedWidgetAvailable();
}

void PlatformWindowStarboard::CreateSbWindow(bool notify_bounds_changed) {
  if (SbWindowIsValid(sb_window_)) {
    return;
  }

  SbWindowOptions options{};
  SbWindowSetDefaultOptions(&options);
  // Only request an explicit SbWindow size when overridden via --window-size
  // (translated to --content-shell-host-window-size) on the command line.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kContentShellHostWindowSize)) {
    options.size.width = bounds_.width();
    options.size.height = bounds_.height();
  }

  sb_window_ = SbWindowCreate(&options);
  CHECK(SbWindowIsValid(sb_window_));

  // Query the actual native window dimensions from Starboard and update both
  // the window bounds and primary screen display bounds.
  SbWindowSize size{};
  if (SbWindowGetSize(sb_window_, &size)) {
    gfx::Rect new_bounds(bounds_.x(), bounds_.y(), size.width, size.height);
    bool bounds_changed = bounds_ != new_bounds;
    bounds_ = new_bounds;
    if (PlatformScreenStarboard::GetInstance()) {
      PlatformScreenStarboard::GetInstance()->UpdateDisplayBounds(bounds_);
    }
    if (notify_bounds_changed && bounds_changed) {
      delegate_->OnBoundsChanged({/*origin_changed=*/false});
    }
  } else {
    LOG(WARNING)
        << "PlatformWindowStarboard::CreateSbWindow(): SbWindowGetSize failed.";
  }

  (*g_created_callback).Run(sb_window_);
}

void PlatformWindowStarboard::EnsureAcceleratedWidgetAvailable() {
  if (!widget_available_) {
    widget_available_ = true;

    gfx::AcceleratedWidget handle = reinterpret_cast<gfx::AcceleratedWidget>(
        SbWindowGetPlatformHandle(sb_window_));
    delegate_->OnAcceleratedWidgetAvailable(handle);
  }
}

PlatformWindowState PlatformWindowStarboard::GetPlatformWindowState() const {
  return window_state_;
}

void PlatformWindowStarboard::Activate() {
  if (waiting_for_reveal_ack_) {
    return;
  }
  if (activation_state_ != ActivationState::kActive) {
    activation_state_ = ActivationState::kActive;
    delegate_->OnActivationChanged(/*active=*/true);
  }
}

void PlatformWindowStarboard::Deactivate() {
  if (activation_state_ != ActivationState::kInactive) {
    activation_state_ = ActivationState::kInactive;
    delegate_->OnActivationChanged(/*active=*/false);
  }
}

void PlatformWindowStarboard::SetCursor(scoped_refptr<PlatformCursor> cursor) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void PlatformWindowStarboard::MoveCursorTo(const gfx::Point& location) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void PlatformWindowStarboard::ConfineCursorToBounds(const gfx::Rect& bounds) {}

void PlatformWindowStarboard::SetRestoredBoundsInDIP(const gfx::Rect& bounds) {
  NOTIMPLEMENTED_LOG_ONCE();
}

gfx::Rect PlatformWindowStarboard::GetRestoredBoundsInDIP() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::Rect();
}

void PlatformWindowStarboard::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                             const gfx::ImageSkia& app_icon) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void PlatformWindowStarboard::SizeConstraintsChanged() {
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace ui
#endif  // BUILDFLAG(IS_COBALT)
