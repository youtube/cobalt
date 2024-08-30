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

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/singleton.h"
#include "base/strings/stringprintf.h"
#include "base/uuid.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/base/process/process_metrics_helper.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

base::Value GetDefaultIntervalsValue() {
  return base::Value(
      base::Value::List()
          .Append(
              base::Value::Dict().Set("type", "per_thread").Set("seconds", 2))
          .Append(
              base::Value::Dict().Set("type", "per_thread").Set("seconds", 30))
          .Append(base::Value::Dict().Set("type", "total").Set("seconds", 2))
          .Append(base::Value::Dict().Set("type", "total").Set("seconds", 30)));
}

std::vector<Interval> GetDefaultIntervals() {
  return std::vector<Interval>({
      Interval(IntervalType::PER_THREAD, /*seconds=*/2),
      Interval(IntervalType::PER_THREAD, /*seconds=*/30),
      Interval(IntervalType::TOTAL, /*seconds=*/2),
      Interval(IntervalType::TOTAL, /*seconds=*/30),
  });
}

absl::optional<std::vector<Interval>> ParseIntervals(
    const base::Value& intervals_value) {
  const base::Value::List* list = intervals_value.GetIfList();
  if (!list) return absl::nullopt;
  std::vector<Interval> intervals;
  for (auto& item_value : *list) {
    const base::Value::Dict* item = item_value.GetIfDict();
    if (!item) return absl::nullopt;
    const std::string* raw_type = item->FindString("type");
    if (!raw_type) return absl::nullopt;
    IntervalType type;
    if (raw_type->compare("total") == 0) {
      type = IntervalType::TOTAL;
    } else if (raw_type->compare("per_thread") == 0) {
      type = IntervalType::PER_THREAD;
    } else {
      return absl::nullopt;
    }
    int seconds = item->FindInt("seconds").value_or(0);
    if (seconds == 0) return absl::nullopt;
    intervals.emplace_back(type, seconds);
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
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
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
  base::CurrentThread::Get()->AddDestructionObserver(this);
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
  base::Value intervals;
  storage_->Get(kIntervals, &intervals);
  UpdateIntervalsDefinition(intervals);
  base::Value intervals_enabled;
  storage_->Get(kIntervalsEnabled, &intervals_enabled);
  UpdateIntervalsEnabled(intervals_enabled.GetIfBool().value_or(false));
  base::Value one_time_tracking;
  storage_->Get(kOneTimeTracking, &one_time_tracking);
  if (one_time_tracking.GetIfBool().value_or(false)) {
    StartOneTimeTracking();
  }
}

void CpuUsageTracker::ClearIntervalContexts() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  lock_.AssertAcquired();
  for (UuidIntervalContextPair& pair : uuid_to_interval_context_) {
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
  uuid_to_interval_context_.clear();
}

void CpuUsageTracker::CreatePerThreadIntervalContext(int interval_seconds) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  lock_.AssertAcquired();
  auto uuid = base::Uuid::GenerateRandomV4();
  auto context = std::make_unique<PerThreadIntervalContext>();
  auto name = base::StringPrintf("CPU.PerThread.Usage.IntervalSeconds.%d",
                                 interval_seconds);
  context->cval = std::make_unique<base::CVal<std::string, base::CValPublic>>(
      name,
      /*initial_value=*/"", /*description=*/"");
  context->timer = std::make_unique<base::RepeatingTimer>(
      FROM_HERE, base::TimeDelta::FromSecondsD(interval_seconds),
      base::BindRepeating(&CpuUsageTracker::PerThreadIntervalTask,
                          base::Unretained(this), uuid));
  context->previous = base::Value::ToUniquePtrValue(
      base::ProcessMetricsHelper::GetCumulativeCPUUsagePerThread());
  uuid_to_interval_context_.emplace(uuid, std::move(context));
  if (intervals_enabled_) {
    // Start repeating timer with existing interval duration and task.
    uuid_to_interval_context_[uuid]->timer->Reset();
  }
}

void CpuUsageTracker::CreateTotalIntervalContext(int interval_seconds) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  lock_.AssertAcquired();
  auto uuid = base::Uuid::GenerateRandomV4();
  auto context = std::make_unique<TotalIntervalContext>();
  auto name = base::StringPrintf("CPU.Total.Usage.IntervalSeconds.%d",
                                 interval_seconds);
  context->cval = std::make_unique<base::CVal<double, base::CValPublic>>(
      name,
      /*initial_value=*/0.0, /*description=*/"");
  context->timer = std::make_unique<base::RepeatingTimer>(
      FROM_HERE, base::TimeDelta::FromSecondsD(interval_seconds),
      base::BindRepeating(&CpuUsageTracker::TotalIntervalTask,
                          base::Unretained(this), uuid));
  context->previous = base::ProcessMetricsHelper::GetCumulativeCPUUsage();
  uuid_to_interval_context_.emplace(uuid, std::move(context));
  if (intervals_enabled_) {
    // Start repeating timer with existing interval duration and task.
    uuid_to_interval_context_[uuid]->timer->Reset();
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
  storage_->Set(kOneTimeTracking, base::Value(true));
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
  storage_->Set(kOneTimeTracking, base::Value(false));
  {
    base::Value current =
        std::move(base::ProcessMetricsHelper::GetCumulativeCPUUsagePerThread());
    absl::optional<std::string> serialized = base::WriteJson(base::Value(
        base::Value::Dict()
            .Set("previous", one_time_tracking_per_thread_at_start_->Clone())
            .Set("current", current.Clone())));
    DCHECK(serialized.has_value());
    cval_one_time_tracking_per_thread_->set_value(*serialized);
  }
  {
    base::TimeDelta current =
        base::ProcessMetricsHelper::GetCumulativeCPUUsage();
    base::TimeDelta usage_during_interval =
        current - one_time_tracking_total_at_start_;
    cval_one_time_tracking_total_->set_value(
        usage_during_interval.InSecondsF());
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
  storage_->Set(kIntervalsEnabled, base::Value(intervals_enabled_));
  for (UuidIntervalContextPair& pair : uuid_to_interval_context_) {
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

base::Value CpuUsageTracker::GetIntervalsDefinition() {
  base::Value intervals;
  storage_->Get(kIntervals, &intervals);
  return std::move(intervals);
}

void CpuUsageTracker::UpdateIntervalsDefinition(
    const base::Value& intervals_value) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CpuUsageTracker::UpdateIntervalsDefinition,
                       base::Unretained(this), intervals_value.Clone()));
    return;
  }
  base::AutoLock auto_lock(lock_);
  absl::optional<std::vector<Interval>> intervals =
      ParseIntervals(intervals_value);
  if (!intervals.has_value()) {
    storage_->Set(kIntervals, GetDefaultIntervalsValue());
    intervals = GetDefaultIntervals();
  } else {
    storage_->Set(kIntervals, intervals_value.Clone());
  }
  ClearIntervalContexts();
  std::set<int> total_intervals_processed;
  std::set<int> per_thread_intervals_processed;
  for (const Interval& interval : *intervals) {
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

void CpuUsageTracker::PerThreadIntervalTask(const base::Uuid& uuid) {
  base::AutoLock auto_lock(lock_);
  if (uuid_to_interval_context_.count(uuid) == 0) {
    NOTREACHED();
    return;
  }
  IntervalContext* interval_context = uuid_to_interval_context_[uuid].get();
  DCHECK_EQ(interval_context->type, IntervalContextType::PER_THREAD);
  auto* per_thread_context =
      static_cast<PerThreadIntervalContext*>(interval_context);
  if (!intervals_enabled_) {
    per_thread_context->timer->Stop();
    return;
  }
  base::Value current =
      std::move(base::ProcessMetricsHelper::GetCumulativeCPUUsagePerThread());
  absl::optional<std::string> serialized = base::WriteJson(
      base::Value(base::Value::Dict()
                      .Set("previous", per_thread_context->previous->Clone())
                      .Set("current", current.Clone())));
  DCHECK(serialized.has_value());
  per_thread_context->cval->set_value(*serialized);
  per_thread_context->previous =
      base::Value::ToUniquePtrValue(std::move(current));
}

void CpuUsageTracker::TotalIntervalTask(const base::Uuid& uuid) {
  base::AutoLock auto_lock(lock_);
  if (uuid_to_interval_context_.count(uuid) == 0) {
    NOTREACHED();
    return;
  }
  IntervalContext* interval_context = uuid_to_interval_context_[uuid].get();
  DCHECK_EQ(interval_context->type, IntervalContextType::TOTAL);
  auto* total_context = static_cast<TotalIntervalContext*>(interval_context);
  if (!intervals_enabled_) {
    total_context->timer->Stop();
    return;
  }
  base::TimeDelta current = base::ProcessMetricsHelper::GetCumulativeCPUUsage();
  base::TimeDelta usage_during_interval = current - total_context->previous;
  total_context->cval->set_value(usage_during_interval.InSecondsF());
  total_context->previous = current;
}

void CpuUsageTracker::WillDestroyCurrentMessageLoop() {
  base::AutoLock auto_lock(lock_);
  ClearIntervalContexts();
}

}  // namespace browser
}  // namespace cobalt
