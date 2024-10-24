// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  LOG(INFO) << "PlatformWindowStarboard::PlatformWindowStarboard";
  gfx::AcceleratedWidget widget = (bounds.width() << 16) + bounds.height();
  delegate->OnAcceleratedWidgetAvailable(widget);

  if (PlatformEventSource::GetInstance())
    PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
}

PlatformWindowStarboard::~PlatformWindowStarboard() {
  LOG(INFO) << "PlatformWindowStarboard::~PlatformWindowStarboard";
  if (PlatformEventSource::GetInstance())
    PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
}

bool PlatformWindowStarboard::CanDispatchEvent(const PlatformEvent& ne) {
  LOG(INFO) << "PlatformWindowStarboard::CanDispatchEvent";
  return true;
}

uint32_t PlatformWindowStarboard::DispatchEvent(const PlatformEvent& native_event) {
  LOG(INFO) << "PlatformWindowStarboard::DispatchEvent delegate=" << delegate();
  DispatchEventFromNativeUiEvent(
      native_event, base::BindOnce(&PlatformWindowDelegate::DispatchEvent,
                                   base::Unretained(delegate())));

  return ui::POST_DISPATCH_STOP_PROPAGATION;
}

}  // namespace ui
