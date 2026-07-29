// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "services/device/time_zone_monitor/time_zone_monitor_starboard.h"

#include <cstdlib>
#include <cstring>
#include <ctime>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "services/device/time_zone_monitor/time_zone_monitor.h"
#include "starboard/time_zone.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace device {

class TimeZoneMonitorStarboard : public TimeZoneMonitor {
 public:
  TimeZoneMonitorStarboard()
      : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
    base::AutoLock lock(GetInstanceLock());
    DCHECK(!g_instance);
    g_instance = this;
  }

  TimeZoneMonitorStarboard(const TimeZoneMonitorStarboard&) = delete;
  TimeZoneMonitorStarboard& operator=(const TimeZoneMonitorStarboard&) = delete;

  ~TimeZoneMonitorStarboard() override {
    base::AutoLock lock(GetInstanceLock());
    g_instance = nullptr;
  }

  static base::Lock& GetInstanceLock() {
    static base::NoDestructor<base::Lock> lock;
    return *lock;
  }

  static TimeZoneMonitorStarboard* g_instance;

  void Notify() {
    // This may be called from any thread.
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&TimeZoneMonitorStarboard::OnTimeZoneChanged,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

  void OnTimeZoneChanged() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    const char* iana_id = SbTimeZoneGetName();
    if (!iana_id) {
      return;
    }

    const char* old_iana_id = getenv("TZ");
    if (old_iana_id && strcmp(old_iana_id, iana_id) == 0) {
      return;
    }

    // Update TZ environment variable and call tzset(). Overwriting TZ is necessary
    // because it is set at app startup and Cobalt's internal TimeZoneState will
    // ignore system changes if TZ is set to an old value
    setenv("TZ", iana_id, 1 /* overwrite */);
    tzset();

    std::unique_ptr<icu::TimeZone> new_zone(DetectHostTimeZoneFromIcu());

    // We get here multiple times per a single tz change, but want to update
    // the ICU default zone and notify clients only once.
    std::unique_ptr<icu::TimeZone> current_zone(icu::TimeZone::createDefault());
    if (*current_zone == *new_zone) {
      VLOG(1) << "timezone already updated";
      return;
    }

    UpdateIcuAndNotifyClients(std::move(new_zone));
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<TimeZoneMonitorStarboard> weak_ptr_factory_{this};
};

TimeZoneMonitorStarboard* TimeZoneMonitorStarboard::g_instance = nullptr;

void NotifyTimeZoneChangeStarboard() {
  base::AutoLock lock(TimeZoneMonitorStarboard::GetInstanceLock());
  if (TimeZoneMonitorStarboard::g_instance) {
    TimeZoneMonitorStarboard::g_instance->Notify();
  }
}

// static
std::unique_ptr<TimeZoneMonitor> TimeZoneMonitor::Create(
    scoped_refptr<base::SequencedTaskRunner> file_task_runner) {
  // Starboard implementation does not need the file_task_runner because it
  // receives notifications directly from cobalt/app/cobalt.cc.
  return std::make_unique<TimeZoneMonitorStarboard>();
}

}  // namespace device