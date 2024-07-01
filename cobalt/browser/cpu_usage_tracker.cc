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

const char kSettingKey[] = "cpu_usage_tracker_intervals";

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

base::Value GetDefaultConfigValue() {
  return base::Value(
      base::Value::List()
          .Append(
              base::Value::Dict().Set("type", "per_thread").Set("seconds", 2))
          .Append(
              base::Value::Dict().Set("type", "per_thread").Set("seconds", 30))
          .Append(base::Value::Dict().Set("type", "total").Set("seconds", 2))
          .Append(base::Value::Dict().Set("type", "total").Set("seconds", 30)));
}

Config GetDefaultConfig() {
  return Config({
      Interval(IntervalType::PER_THREAD, /*seconds=*/2),
      Interval(IntervalType::PER_THREAD, /*seconds=*/30),
      Interval(IntervalType::TOTAL, /*seconds=*/2),
      Interval(IntervalType::TOTAL, /*seconds=*/30),
  });
}

absl::optional<Config> ParseConfig(const base::Value& config_value) {
  const base::Value::List* list = config_value.GetIfList();
  if (!list) return absl::nullopt;
  Config config;
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
    config.intervals.emplace_back(type, seconds);
  }
  return std::move(config);
}

}  // namespace

// static
CpuUsageTracker* CpuUsageTracker::GetInstance() {
  return base::Singleton<CpuUsageTracker,
                         base::LeakySingletonTraits<CpuUsageTracker>>::get();
}

CpuUsageTracker::CpuUsageTracker()
    : storage_(nullptr),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
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
  base::Value config;
  storage_->Get(kSettingKey, &config);
  UpdateConfig(config);
}

void CpuUsageTracker::ClearIntervalContexts() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  lock_.AssertAcquired();
  for (auto& pair : interval_contexts_) {
    if (pair.second->timer) pair.second->timer->Stop();
    pair.second->timer.reset();
    if (pair.second->type == IntervalContextType::PER_THREAD) {
      auto* context = static_cast<PerThreadIntervalContext*>(pair.second.get());
      context->cval.reset();
      context->previous.reset();
    } else {
      auto* context = static_cast<TotalIntervalContext*>(pair.second.get());
      context->cval.reset();
    }
    pair.second.reset();
  }
  interval_contexts_.clear();
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
      base::BindRepeating(&CpuUsageTracker::UpdatePerThread,
                          base::Unretained(this), uuid));
  context->previous = base::Value::ToUniquePtrValue(
      base::ProcessMetricsHelper::GetCumulativeCPUUsagePerThread());
  interval_contexts_.emplace(uuid, std::move(context));
  interval_contexts_[uuid]->timer->Reset();
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
      base::BindRepeating(&CpuUsageTracker::UpdateTotal, base::Unretained(this),
                          uuid));
  context->previous = base::ProcessMetricsHelper::GetCumulativeCPUUsage();
  interval_contexts_.emplace(uuid, std::move(context));
  interval_contexts_[uuid]->timer->Reset();
}

void CpuUsageTracker::UpdateConfig(const base::Value& config_value) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CpuUsageTracker::UpdateConfig, base::Unretained(this),
                       config_value.Clone()));
    return;
  }
  base::AutoLock auto_lock(lock_);
  absl::optional<Config> config = ParseConfig(config_value);
  if (!config.has_value()) {
    base::Value default_config = GetDefaultConfigValue();
    config = GetDefaultConfig();
    storage_->Set(kSettingKey, std::move(default_config));
  } else {
    storage_->Set(kSettingKey, config_value.Clone());
  }
  ClearIntervalContexts();
  std::set<int> total_intervals_processed;
  std::set<int> per_thread_intervals_processed;
  for (const Interval& interval : config->intervals) {
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

void CpuUsageTracker::UpdatePerThread(const base::Uuid& uuid) {
  base::AutoLock auto_lock(lock_);
  if (interval_contexts_.count(uuid) == 0) {
    NOTREACHED();
    return;
  }
  auto* context =
      static_cast<PerThreadIntervalContext*>(interval_contexts_[uuid].get());
  base::Value current =
      std::move(base::ProcessMetricsHelper::GetCumulativeCPUUsagePerThread());
  absl::optional<std::string> serialized = base::WriteJson(
      base::Value(base::Value::Dict()
                      .Set("previous", context->previous->Clone())
                      .Set("current", current.Clone())));
  DCHECK(serialized.has_value());
  *(context->cval) = *serialized;
  context->previous = base::Value::ToUniquePtrValue(std::move(current));
}

void CpuUsageTracker::UpdateTotal(const base::Uuid& uuid) {
  base::AutoLock auto_lock(lock_);
  if (interval_contexts_.count(uuid) == 0) {
    NOTREACHED();
    return;
  }
  auto* context =
      static_cast<TotalIntervalContext*>(interval_contexts_[uuid].get());
  base::TimeDelta current = base::ProcessMetricsHelper::GetCumulativeCPUUsage();
  *(context->cval) = (current - context->previous).InSecondsF();
  context->previous = current;
}

void CpuUsageTracker::WillDestroyCurrentMessageLoop() {
  base::AutoLock auto_lock(lock_);
  ClearIntervalContexts();
}

}  // namespace browser
}  // namespace cobalt
