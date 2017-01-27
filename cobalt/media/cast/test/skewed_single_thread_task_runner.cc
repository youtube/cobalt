// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/skewed_single_thread_task_runner.h"

#include "base/logging.h"
#include "base/time/tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {
namespace test {

SkewedSingleThreadTaskRunner::SkewedSingleThreadTaskRunner(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) :
    skew_(1.0),
    task_runner_(task_runner) {
}

SkewedSingleThreadTaskRunner::~SkewedSingleThreadTaskRunner() {}

void SkewedSingleThreadTaskRunner::SetSkew(double skew) {
  skew_ = skew;
}

bool SkewedSingleThreadTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return task_runner_->PostDelayedTask(
      from_here,
      task,
      base::TimeDelta::FromMicroseconds(delay.InMicroseconds() * skew_));
}

bool SkewedSingleThreadTaskRunner::RunsTasksOnCurrentThread() const {
  return task_runner_->RunsTasksOnCurrentThread();
}

bool SkewedSingleThreadTaskRunner::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return task_runner_->PostNonNestableDelayedTask(
      from_here,
      task,
      base::TimeDelta::FromMicroseconds(delay.InMicroseconds() * skew_));
}

}  // namespace test
}  // namespace cast
}  // namespace media
