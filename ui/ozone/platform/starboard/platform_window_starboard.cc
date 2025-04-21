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
    : StubWindow(delegate, /*use_default_accelerated_widget=*/false, bounds) {
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
    static_cast<starboard::PlatformEventSourceStarboard*>(
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
    static_cast<starboard::PlatformEventSourceStarboard*>(
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

}  // namespace ui
