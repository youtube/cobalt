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

#include "services/device/time_zone_monitor/time_zone_monitor.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/platform/starboard/platform_event_observer_starboard.h"
#include "ui/ozone/platform/starboard/platform_event_source_starboard.h"

namespace device {

class TimeZoneMonitorStarboard : public TimeZoneMonitor,
                                 public ui::PlatformEventObserverStarboard {
 public:
  TimeZoneMonitorStarboard() {
    ui::PlatformEventSource* source = ui::PlatformEventSource::GetInstance();
    if (source) {
      static_cast<ui::PlatformEventSourceStarboard*>(source)
          ->AddPlatformEventObserverStarboard(this);
    }
  }

  TimeZoneMonitorStarboard(const TimeZoneMonitorStarboard&) = delete;
  TimeZoneMonitorStarboard& operator=(const TimeZoneMonitorStarboard&) = delete;

  ~TimeZoneMonitorStarboard() override {
    ui::PlatformEventSource* source = ui::PlatformEventSource::GetInstance();
    if (source) {
      static_cast<ui::PlatformEventSourceStarboard*>(source)
          ->RemovePlatformEventObserverStarboard(this);
    }
  }

  // ui::PlatformEventObserverStarboard implementation:
  void ProcessWindowSizeChangedEvent(int width, int height) override {}
  void ProcessFocusEvent(bool is_focused) override {}
  void ProcessDateTimeConfigurationChangedEvent() override {
    UpdateIcuAndNotifyClients(DetectHostTimeZoneFromIcu());
  }
};

std::unique_ptr<TimeZoneMonitor> TimeZoneMonitor::Create(
    scoped_refptr<base::SequencedTaskRunner> file_task_runner) {
  return std::make_unique<TimeZoneMonitorStarboard>();
}

}  // namespace device
