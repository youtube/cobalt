// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/child_call_stack_profile_collector.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"

namespace metrics {

ChildCallStackProfileCollector::ProfileState::ProfileState() = default;
ChildCallStackProfileCollector::ProfileState::ProfileState(ProfileState&&) =
    default;

ChildCallStackProfileCollector::ProfileState::ProfileState(
    base::TimeTicks start_timestamp,
    SampledProfile profile)
    : start_timestamp(start_timestamp), profile(std::move(profile)) {}

ChildCallStackProfileCollector::ProfileState::~ProfileState() = default;

// Some versions of GCC need this for push_back to work with std::move.
ChildCallStackProfileCollector::ProfileState&
ChildCallStackProfileCollector::ProfileState::operator=(ProfileState&&) =
    default;

ChildCallStackProfileCollector::ChildCallStackProfileCollector() {}

ChildCallStackProfileCollector::~ChildCallStackProfileCollector() {}

void ChildCallStackProfileCollector::SetParentProfileCollector(
    metrics::mojom::CallStackProfileCollectorPtr parent_collector) {
  base::AutoLock alock(lock_);
  // This function should only invoked once, during the mode of operation when
  // retaining profiles after construction.
  DCHECK(retain_profiles_);
  retain_profiles_ = false;
  task_runner_ = base::ThreadTaskRunnerHandle::Get();
  // This should only be set one time per child process.
  DCHECK(!parent_collector_);
  parent_collector_ = std::move(parent_collector);
  if (parent_collector_) {
    for (ProfileState& state : profiles_) {
      parent_collector_->Collect(state.start_timestamp,
                                 std::move(state.profile));
    }
  }
  profiles_.clear();
}

void ChildCallStackProfileCollector::Collect(base::TimeTicks start_timestamp,
                                             SampledProfile profile) {
  base::AutoLock alock(lock_);
  if (task_runner_ &&
      // The profiler thread does not have a task runner. Attempting to
      // invoke Get() on it results in a DCHECK.
      (!base::ThreadTaskRunnerHandle::IsSet() ||
       base::ThreadTaskRunnerHandle::Get() != task_runner_)) {
    // Post back to the thread that owns the the parent interface.
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ChildCallStackProfileCollector::Collect,
                                  // This class has lazy instance lifetime.
                                  base::Unretained(this), start_timestamp,
                                  std::move(profile)));
    return;
  }

  if (parent_collector_) {
    parent_collector_->Collect(start_timestamp, std::move(profile));
  } else if (retain_profiles_) {
    profiles_.push_back(ProfileState(start_timestamp, std::move(profile)));
  }
}

}  // namespace metrics
