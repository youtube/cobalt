// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef UI_OZONE_PLATFORM_STARBOARD_PLATFORM_EVENT_OBSERVER_STARBOARD_H_
#define UI_OZONE_PLATFORM_STARBOARD_PLATFORM_EVENT_OBSERVER_STARBOARD_H_

namespace ui {

class EVENTS_EXPORT PlatformEventObserverStarboard {
 public:
  virtual void ProcessWindowSizeChangedEvent(int width, int height) = 0;
  virtual void ProcessFocusEvent(bool is_focused) = 0;
  virtual void ProcessDateTimeConfigurationChangedEvent() = 0;

 protected:
  virtual ~PlatformEventObserverStarboard() = default;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_STARBOARD_PLATFORM_EVENT_OBSERVER_STARBOARD_H_
