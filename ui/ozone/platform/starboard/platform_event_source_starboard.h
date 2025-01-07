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

#ifndef UI_OZONE_PLATFORM_STARBOARD_STARBOARD_PLATFORM_EVENT_SOURCE_H_
#define UI_OZONE_PLATFORM_STARBOARD_STARBOARD_PLATFORM_EVENT_SOURCE_H_

#include "ui/events/platform/platform_event_source.h"

#include "base/task/single_thread_task_runner.h"
#include "starboard/event.h"

#include <thread>

namespace starboard {

class PlatformEventSourceStarboard : public ui::PlatformEventSource {
 public:
  PlatformEventSourceStarboard();

  PlatformEventSourceStarboard(const PlatformEventSourceStarboard&) = delete;
  PlatformEventSourceStarboard& operator=(const PlatformEventSourceStarboard&) =
      delete;

  ~PlatformEventSourceStarboard() override;

  static void SbEventHandle(const SbEvent* event);

  uint32_t DeliverEvent(std::unique_ptr<ui::Event> ui_event);

 private:
  std::unique_ptr<std::thread> sb_main_;
};

}  // namespace starboard

#endif  // UI_OZONE_PLATFORM_STARBOARD_STARBOARD_PLATFORM_EVENT_SOURCE_H_
