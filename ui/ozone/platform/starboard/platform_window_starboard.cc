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

#include "base/functional/bind.h"
#include "starboard/event.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/starboard/platform_event_source_starboard.h"

namespace ui {

PlatformWindowStarboard::PlatformWindowStarboard(
    PlatformWindowDelegate* delegate,
    const gfx::Rect& bounds)
    : bounds_(bounds), delegate_(delegate) {
  DCHECK(delegate);
  SbWindowOptions options{};
  SbWindowSetDefaultOptions(&options);
  options.size.width = bounds.width();
  options.size.height = bounds.height();
  sb_window_ = SbWindowCreate(&options);
  CHECK(SbWindowIsValid(sb_window_));

  delegate->OnAcceleratedWidgetAvailable(
      reinterpret_cast<intptr_t>(SbWindowGetPlatformHandle(sb_window_)));

  if (PlatformEventSource::GetInstance()) {
    PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
    static_cast<PlatformEventSourceStarboard*>(
        PlatformEventSource::GetInstance())
        ->AddPlatformEventObserverStarboard(this);
  }
}

PlatformWindowStarboard::~PlatformWindowStarboard() {
  if (sb_window_) {
    SbWindowDestroy(sb_window_);
  }

  if (PlatformEventSource::GetInstance()) {
    PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
    static_cast<PlatformEventSourceStarboard*>(
        PlatformEventSource::GetInstance())
        ->RemovePlatformEventObserverStarboard(this);
  }
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
  NOTIMPLEMENTED_LOG_ONCE();
}

void PlatformWindowStarboard::Hide() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void PlatformWindowStarboard::Close() {
  delegate_->OnClosed();
}

bool PlatformWindowStarboard::IsVisible() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return true;
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
  NOTIMPLEMENTED_LOG_ONCE();
}

void PlatformWindowStarboard::Restore() {
  NOTIMPLEMENTED_LOG_ONCE();
}

PlatformWindowState PlatformWindowStarboard::GetPlatformWindowState() const {
  return window_state_;
}

void PlatformWindowStarboard::Activate() {
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
