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

#ifndef UI_OZONE_PLATFORM_STARBOARD_PLATFORM_EVENT_SOURCE_H_
#define UI_OZONE_PLATFORM_STARBOARD_PLATFORM_EVENT_SOURCE_H_

#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/platform/starboard/platform_event_observer_starboard.h"

#include "starboard/event.h"

namespace ui {

class PlatformEventSourceStarboard : public PlatformEventSource {
 public:
  PlatformEventSourceStarboard();

  PlatformEventSourceStarboard(const PlatformEventSourceStarboard&) = delete;
  PlatformEventSourceStarboard& operator=(const PlatformEventSourceStarboard&) =
      delete;

  void HandleEvent(const SbEvent* event);

  ~PlatformEventSourceStarboard() override;

  uint32_t DeliverEvent(std::unique_ptr<ui::Event> ui_event);

  void AddPlatformEventObserverStarboard(
      PlatformEventObserverStarboard* observer);
  void RemovePlatformEventObserverStarboard(
      PlatformEventObserverStarboard* observer);
  void HandleWindowSizeChangedEvent(const SbEvent* event);
  void DispatchWindowSizeChanged(int width, int height);

 private:
  base::ObserverList<PlatformEventObserverStarboard>::Unchecked sb_observers_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_STARBOARD_PLATFORM_EVENT_SOURCE_H_
