// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_STARBOARD_STARBOARD_PLATFORM_EVENT_SOURCE_H_
#define UI_OZONE_PLATFORM_STARBOARD_STARBOARD_PLATFORM_EVENT_SOURCE_H_

#include "ui/events/platform/platform_event_source.h"

#include "content/public/browser/browser_thread.h"
#include "starboard/event.h"

#include <thread>

namespace starboard {

class StarboardPlatformEventSource : public ui::PlatformEventSource {
 public:
  StarboardPlatformEventSource();

  StarboardPlatformEventSource(const StarboardPlatformEventSource&) = delete;
  StarboardPlatformEventSource& operator=(const StarboardPlatformEventSource&) =
      delete;

  ~StarboardPlatformEventSource() override;

  static void SbEventHandle(const SbEvent* event);
//  static void DeliverEventHandler(ui::PlatformEvent platform_event);


  uint32_t DeliverEvent(std::unique_ptr<ui::Event> ui_event);

private:

  std::unique_ptr<std::thread> sb_main_;
};

}  // namespace headless

#endif  // UI_OZONE_PLATFORM_STARBOARD_STARBOARD_PLATFORM_EVENT_SOURCE_H_