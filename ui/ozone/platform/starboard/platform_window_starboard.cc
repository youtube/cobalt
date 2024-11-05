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
#include "base/logging.h"
#include "ui/events/event.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

PlatformWindowStarboard::PlatformWindowStarboard(PlatformWindowDelegate* delegate,
                                       const gfx::Rect& bounds)
    : StubWindow(delegate, false, bounds) {
  gfx::AcceleratedWidget widget = (bounds.width() << 16) + bounds.height();
  delegate->OnAcceleratedWidgetAvailable(widget);

  if (PlatformEventSource::GetInstance())
    PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
}

PlatformWindowStarboard::~PlatformWindowStarboard() {
  if (PlatformEventSource::GetInstance())
    PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
}

bool PlatformWindowStarboard::CanDispatchEvent(const PlatformEvent& ne) {
  return true;
}

uint32_t PlatformWindowStarboard::DispatchEvent(const PlatformEvent& native_event) {
  DispatchEventFromNativeUiEvent(
      native_event, base::BindOnce(&PlatformWindowDelegate::DispatchEvent,
                                   base::Unretained(delegate())));

  return ui::POST_DISPATCH_STOP_PROPAGATION;
}

}  // namespace ui
