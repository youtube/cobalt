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

#include "cobalt/browser/cpu_usage_tracker.h"

#include <atomic>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/singleton.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/base/process/process_metrics_helper.h"

namespace cobalt {
namespace browser {

namespace {

const char kIntervals[] = "cpu_usage_tracker_intervals";
const char kOneTimeTracking[] = "cpu_usage_tracker_one_time_tracking";
const char kIntervalsEnabled[] = "cpu_usage_tracker_intervals_enabled";

enum IntervalType {
  PER_THREAD,
  TOTAL,
};

struct Interval {
  Interval(IntervalType type, int seconds) : type(type), seconds(seconds) {}

  IntervalType type;
  int seconds;
};

struct Config {
  Config() {}
  Config(std::initializer_list<Interval> intervals) : intervals(intervals) {}

  std::vector<Interval> intervals;
};

static std::atomic<uint64_t> next_id{0};

base::Value GetDefaultIntervalsValue() {
  base::DictionaryValue interval_per_thread_2;
  interval_per_thread_2.SetKey("type", base::Value("per_thread"));
  interval_per_thread_2.SetKey("seconds", base::Value(2));
  base::DictionaryValue interval_per_thread_30;
  interval_per_thread_30.SetKey("type", base::Value("per_thread"));
  interval_per_thread_30.SetKey("seconds", base::Value(30));
  base::DictionaryValue interval_total_2;
  interval_total_2.SetKey("type", base::Value("total"));
  interval_total_2.SetKey("seconds", base::Value(2));
  base::DictionaryValue interval_total_30;
  interval_total_30.SetKey("type", base::Value("total"));
  interval_total_30.SetKey("seconds", base::Value(30));
  base::ListValue list_value;
  base::Value::ListStorage& list_storage = list_value.GetList();
  list_storage.push_back(std::move(interval_per_thread_2));
  list_storage.push_back(std::move(interval_per_thread_30));
  list_storage.push_back(std::move(interval_total_2));
  list_storage.push_back(std::move(interval_total_30));
  return std::move(list_value);
}

std::vector<Interval> GetDefaultIntervals() {
  return std::vector<Interval>({
      Interval(IntervalType::PER_THREAD, /*seconds=*/2),
      Interval(IntervalType::PER_THREAD, /*seconds=*/30),
      Interval(IntervalType::TOTAL, /*seconds=*/2),
      Interval(IntervalType::TOTAL, /*seconds=*/30),
  });
}

base::Optional<std::vector<Interval>> ParseIntervals(
    const base::Value& interval_values) {
  std::vector<Interval> intervals;
  for (auto& item_value : interval_values.GetList()) {
    if (!item_value.is_dict()) return base::nullopt;
    const base::Value* raw_type = item_value.FindKey("type");
    if (!raw_type || !raw_type->is_string()) return base::nullopt;
    IntervalType type;
    if (raw_type->GetString().compare("total") == 0) {
      type = IntervalType::TOTAL;
    } else if (raw_type->GetString().compare("per_thread") == 0) {
      type = IntervalType::PER_THREAD;
    } else {
      return base::nullopt;
    }
    const base::Value* seconds = item_value.FindKey("seconds");
    if (!seconds || seconds->GetInt() == 0) return base::nullopt;
    intervals.emplace_back(type, seconds->GetInt());
  }
  return std::move(intervals);
}

}  // namespace

// static
CpuUsageTracker* CpuUsageTracker::GetInstance() {
  return base::Singleton<CpuUsageTracker,
                         base::LeakySingletonTraits<CpuUsageTracker>>::get();
}

CpuUsageTracker::CpuUsageTracker()
    : storage_(nullptr),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      intervals_enabled_(false),
      one_time_tracking_started_(false),
      cval_one_time_tracking_per_thread_(
          std::make_unique<base::CVal<std::string, base::CValPublic>>(
              "CPU.PerThread.Usage.OneTime",
              /*initial_value=*/"", /*description=*/"")),
      cval_one_time_tracking_total_(
          std::make_unique<base::CVal<double, base::CValPublic>>(
              "CPU.Total.Usage.OneTime",
              /*initial_value=*/0.0, /*description=*/"")) {
  base::MessageLoopCurrent::Get()->AddDestructionObserver(this);
}

CpuUsageTracker::~CpuUsageTracker() { ClearIntervalContexts(); }

void CpuUsageTracker::Initialize(
    persistent_storage::PersistentSettings* storage) {
  DCHECK_EQ(storage_, nullptr);
  storage_ = storage;
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&CpuUsageTracker::InitializeAsync,
                                        base::Unretained(this)));
}

void CpuUsageTracker::InitializeAsync() {
  base::ProcessMetricsHelper::PopulateClockTicksPerS();
  UpdateIntervalsDefinition(std::make_unique<base::Value>(
      storage_->GetPersistentSettingAsList(kIntervals)));
  UpdateIntervalsEnabled(
      storage_->GetPersistentSettingAsBool(kIntervalsEnabled, false));
  if (storage_->GetPersistentSettingAsBool(kOneTimeTracking, false)) {
    StartOneTimeTracking();
  }
}

void CpuUsageTracker::ClearIntervalContexts() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  lock_.AssertAcquired();
  for (IdIntervalContextPair& pair : id_to_interval_context_) {
    std::unique_ptr<IntervalContext> interval_context = std::move(pair.second);
    if (interval_context->timer) interval_context->timer->Stop();
    interval_context->timer.reset();
    if (interval_context->type == IntervalContextType::PER_THREAD) {
      auto* per_thread_context =
          static_cast<PerThreadIntervalContext*>(interval_context.get());
      per_thread_context->cval.reset();
      per_thread_context->previous.reset();
    } else {
      auto* total_context =
          static_cast<TotalIntervalContext*>(interval_context.get());
      total_context->cval.reset();
    }
  }
  id_to_interval_context_.clear();
}

void CpuUsageTracker::CreatePerThreadIntervalContext(int interval_seconds) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  lock_.AssertAcquired();

  uint64_t id = next_id.fetch_add(1);
  auto context = std::make_unique<PerThreadIntervalContext>();
  auto name = base::StringPrintf("CPU.PerThread.Usage.IntervalSeconds.%d",
                                 interval_seconds);
  context->cval = std::make_unique<base::CVal<std::string, base::CValPublic>>(
      name,
      /*initial_value=*/"", /*description=*/"");
  context->timer = std::make_unique<base::RepeatingTimer>(
      FROM_HERE, base::TimeDelta::FromSecondsD(interval_seconds),
      base::BindRepeating(&CpuUsageTracker::PerThreadIntervalTask,
                          base::Unretained(this), id));
  context->previous = base::Value::ToUniquePtrValue(
      base::ProcessMetricsHelper::GetCumulativeCPUUsagePerThread());
  id_to_interval_context_.emplace(id, std::move(context));
  if (intervals_enabled_) {
    // Start repeating timer with existing interval duration and task.
    id_to_interval_context_[id]->timer->Reset();
  }
}

void CpuUsageTracker::CreateTotalIntervalContext(int interval_seconds) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  lock_.AssertAcquired();
  uint64_t id = next_id.fetch_add(1);
  auto context = std::make_unique<TotalIntervalContext>();
  auto name = base::StringPrintf("CPU.Total.Usage.IntervalSeconds.%d",
                                 interval_seconds);
  context->cval = std::make_unique<base::CVal<double, base::CValPublic>>(
      name,
      /*initial_value=*/0.0, /*description=*/"");
  context->timer = std::make_unique<base::RepeatingTimer>(
      FROM_HERE, base::TimeDelta::FromSecondsD(interval_seconds),
      base::BindRepeating(&CpuUsageTracker::TotalIntervalTask,
                          base::Unretained(this), id));
  context->previous = base::ProcessMetricsHelper::GetCumulativeCPUUsage();
  id_to_interval_context_.emplace(id, std::move(context));
  if (intervals_enabled_) {
    // Start repeating timer with existing interval duration and task.
    id_to_interval_context_[id]->timer->Reset();
  }
}

void CpuUsageTracker::StartOneTimeTracking() {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CpuUsageTracker::StartOneTimeTracking,
                                  base::Unretained(this)));
    return;
  }
  base::AutoLock auto_lock(lock_);
  one_time_tracking_started_ = true;
  storage_->SetPersistentSetting(kOneTimeTracking,
                                 std::make_unique<base::Value>(true));
  one_time_tracking_per_thread_at_start_ = base::Value::ToUniquePtrValue(
      base::ProcessMetricsHelper::GetCumulativeCPUUsagePerThread());
  one_time_tracking_total_at_start_ =
      base::ProcessMetricsHelper::GetCumulativeCPUUsage();
}

void CpuUsageTracker::StopAndCaptureOneTimeTracking() {
  if (!one_time_tracking_started_) return;
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CpuUsageTracker::StopAndCaptureOneTimeTracking,
                       base::Unretained(this)));
    return;
  }
  base::AutoLock auto_lock(lock_);
  one_time_tracking_started_ = false;
  storage_->SetPersistentSetting(kOneTimeTracking,
                                 std::make_unique<base::Value>(false));
  {
    base::Value current =
        std::move(base::ProcessMetricsHelper::GetCumulativeCPUUsagePerThread());
    std::string serialized;
    base::DictionaryValue value;
    value.SetKey("previous", one_time_tracking_per_thread_at_start_->Clone());
    value.SetKey("current", current.Clone());
    bool serialized_success = base::JSONWriter::Write(value, &serialized);
    DCHECK(serialized_success);
    *cval_one_time_tracking_per_thread_ = serialized;
  }
  {
    base::TimeDelta current =
        base::ProcessMetricsHelper::GetCumulativeCPUUsage();
    base::TimeDelta usage_during_interval =
        current - one_time_tracking_total_at_start_;
    *cval_one_time_tracking_total_ = usage_during_interval.InSecondsF();
  }
}

void CpuUsageTracker::UpdateIntervalsEnabled(bool enabled) {
  if (intervals_enabled_ == enabled) return;
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CpuUsageTracker::UpdateIntervalsEnabled,
                                  base::Unretained(this), enabled));
    return;
  }
  base::AutoLock auto_lock(lock_);
  intervals_enabled_ = enabled;
  storage_->SetPersistentSetting(
      kIntervalsEnabled, std::make_unique<base::Value>(intervals_enabled_));
  for (IdIntervalContextPair& pair : id_to_interval_context_) {
    IntervalContext* interval_context = pair.second.get();
    if (!interval_context->timer) continue;
    if (intervals_enabled_) {
      if (interval_context->type == IntervalContextType::PER_THREAD) {
        auto* per_thread_context =
            static_cast<PerThreadIntervalContext*>(interval_context);
        per_thread_context->previous = base::Value::ToUniquePtrValue(
            base::ProcessMetricsHelper::GetCumulativeCPUUsagePerThread());
      } else {
        auto* total_context =
            static_cast<TotalIntervalContext*>(interval_context);
        total_context->previous =
            base::ProcessMetricsHelper::GetCumulativeCPUUsage();
      }
      // Start repeating timer with existing interval duration and task.
      interval_context->timer->Reset();
    } else {
      interval_context->timer->Stop();
    }
  }
}

void CpuUsageTracker::UpdateIntervalsDefinition(
    std::unique_ptr<base::Value> interval_values) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CpuUsageTracker::UpdateIntervalsDefinition,
                       base::Unretained(this), std::move(interval_values)));
    return;
  }
  base::AutoLock auto_lock(lock_);
  base::Optional<std::vector<Interval>> intervals;
  if (interval_values) {
    intervals = ParseIntervals(*interval_values);
  }
  if (!intervals) {
    storage_->SetPersistentSetting(
        kIntervals, base::Value::ToUniquePtrValue(GetDefaultIntervalsValue()));
    intervals = GetDefaultIntervals();
  } else {
    storage_->SetPersistentSetting(kIntervals, std::move(interval_values));
  }
  ClearIntervalContexts();
  std::set<int> total_intervals_processed;
  std::set<int> per_thread_intervals_processed;
  for (const Interval& interval : intervals.value()) {
    if (interval.type == IntervalType::PER_THREAD) {
      if (per_thread_intervals_processed.count(interval.seconds) == 1) {
        DLOG(WARNING) << "Duplicate CPU per-thread usage interval "
                      << interval.seconds << ".";
        continue;
      }
      CreatePerThreadIntervalContext(interval.seconds);
    } else if (interval.type == IntervalType::TOTAL) {
      if (total_intervals_processed.count(interval.seconds) == 1) {
        DLOG(WARNING) << "Duplicate CPU total usage interval "
                      << interval.seconds << ".";
        continue;
      }
      CreateTotalIntervalContext(interval.seconds);
      total_intervals_processed.insert(interval.seconds);
    } else {
      NOTREACHED();
    }
  }
}

void CpuUsageTracker::PerThreadIntervalTask(uint64_t id) {
  base::AutoLock auto_lock(lock_);
  if (id_to_interval_context_.count(id) == 0) {
    NOTREACHED();
    return;
  }
  IntervalContext* interval_context = id_to_interval_context_[id].get();
  DCHECK_EQ(interval_context->type, IntervalContextType::PER_THREAD);
  auto* per_thread_context =
      static_cast<PerThreadIntervalContext*>(interval_context);
  if (!intervals_enabled_) {
    per_thread_context->timer->Stop();
    return;
  }
  base::Value current =
      std::move(base::ProcessMetricsHelper::GetCumulativeCPUUsagePerThread());
  base::DictionaryValue value;
  value.SetKey("previous", per_thread_context->previous->Clone());
  value.SetKey("current", current.Clone());
  std::string serialized;
  bool success = base::JSONWriter::Write(value, &serialized);
  DCHECK(success);
  *(per_thread_context->cval) = serialized;
  per_thread_context->previous =
      base::Value::ToUniquePtrValue(std::move(current));
}

void CpuUsageTracker::TotalIntervalTask(uint64_t id) {
  base::AutoLock auto_lock(lock_);
  if (id_to_interval_context_.count(id) == 0) {
    NOTREACHED();
    return;
  }
  IntervalContext* interval_context = id_to_interval_context_[id].get();
  DCHECK_EQ(interval_context->type, IntervalContextType::TOTAL);
  auto* total_context = static_cast<TotalIntervalContext*>(interval_context);
  if (!intervals_enabled_) {
    total_context->timer->Stop();
    return;
  }
  base::TimeDelta current = base::ProcessMetricsHelper::GetCumulativeCPUUsage();
  base::TimeDelta usage_during_interval = current - total_context->previous;
  *(total_context->cval) = usage_during_interval.InSecondsF();
  total_context->previous = current;
}

void CpuUsageTracker::WillDestroyCurrentMessageLoop() {
  base::AutoLock auto_lock(lock_);
  ClearIntervalContexts();
}

}  // namespace browser
}  // namespace cobalt
