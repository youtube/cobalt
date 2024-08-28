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

#ifndef COBALT_BROWSER_CPU_USAGE_TRACKER_H_
#define COBALT_BROWSER_CPU_USAGE_TRACKER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/task/current_thread.h"
#include "base/timer/timer.h"
#include "base/uuid.h"
#include "base/values.h"
#include "cobalt/base/c_val.h"
#include "cobalt/persistent_storage/persistent_settings.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace cobalt {
namespace browser {

class CpuUsageTracker : base::CurrentThread::DestructionObserver {
 public:
  static CpuUsageTracker* GetInstance();

  void Initialize(persistent_storage::PersistentSettings*);
  base::Value GetIntervalsDefinition();
  void UpdateIntervalsDefinition(const base::Value&);
  void UpdateIntervalsEnabled(bool);
  void StartOneTimeTracking();
  void StopAndCaptureOneTimeTracking();

 private:
  friend struct base::DefaultSingletonTraits<CpuUsageTracker>;
  CpuUsageTracker();
  ~CpuUsageTracker();

  void InitializeAsync();
  void TotalIntervalTask(const base::Uuid&);
  void PerThreadIntervalTask(const base::Uuid&);
  void ClearIntervalContexts();
  void CreateTotalIntervalContext(int);
  void CreatePerThreadIntervalContext(int);

  // CurrentThread::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override;

  enum IntervalContextType {
    PER_THREAD,
    TOTAL,
  };

  struct IntervalContext {
    IntervalContextType type;
    std::unique_ptr<base::RepeatingTimer> timer;
  };

  using UuidIntervalContextPair =
      std::pair<const base::Uuid, std::unique_ptr<IntervalContext>>;


  struct PerThreadIntervalContext : IntervalContext {
    PerThreadIntervalContext() { type = IntervalContextType::PER_THREAD; }

    std::unique_ptr<base::CVal<std::string, base::CValPublic>> cval;
    std::unique_ptr<base::Value> previous;
  };

  struct TotalIntervalContext : IntervalContext {
    TotalIntervalContext() { type = IntervalContextType::TOTAL; }

    std::unique_ptr<base::CVal<double, base::CValPublic>> cval;
    base::TimeDelta previous;
  };

  bool intervals_enabled_;
  std::map<base::Uuid, std::unique_ptr<IntervalContext>>
      uuid_to_interval_context_;
  base::Lock lock_;

  persistent_storage::PersistentSettings* storage_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  bool one_time_tracking_started_;
  std::unique_ptr<base::CVal<double, base::CValPublic>>
      cval_one_time_tracking_total_;
  base::TimeDelta one_time_tracking_total_at_start_;
  std::unique_ptr<base::CVal<std::string, base::CValPublic>>
      cval_one_time_tracking_per_thread_;
  std::unique_ptr<base::Value> one_time_tracking_per_thread_at_start_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_CPU_USAGE_TRACKER_H_
