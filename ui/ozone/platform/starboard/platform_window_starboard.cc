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
#include "ui/events/event.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/native_widget_types.h"

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
  }
}

PlatformWindowStarboard::~PlatformWindowStarboard() {
  if (sb_window_) {
    SbWindowDestroy(sb_window_);
  }

  if (PlatformEventSource::GetInstance()) {
    PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
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

}  // namespace ui
