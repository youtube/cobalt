// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_scheduler.h"

#include "build/build_config.h"
#if defined(STARBOARD)
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "components/metrics/metrics_switches.h"
#endif

namespace metrics {

namespace {

// The delay, in seconds, after startup before sending the first log message.
#if defined(OS_ANDROID) || defined(OS_IOS)
// Sessions are more likely to be short on a mobile device, so handle the
// initial log quickly.
const int kInitialIntervalSeconds = 15;
#else
const int kInitialIntervalSeconds = 60;
#endif

}  // namespace

// In Cobalt, we need a command line argument to override the initial upload
// interval for testing.
#if defined(STARBOARD)
MetricsScheduler::MetricsScheduler(const base::Closure& task_callback)
    : task_callback_(task_callback), running_(false), callback_pending_(false) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kInitialMetricsUploadIntervalSec)) {
    int custom_initial_upload_interval = -1;
    if (base::StringToInt(command_line->GetSwitchValueASCII(
                              switches::kInitialMetricsUploadIntervalSec),
                          &custom_initial_upload_interval)) {
      interval_ = base::TimeDelta::FromSeconds(custom_initial_upload_interval);
      LOG(INFO) << "Initial upload interval overriden on the command line to: "
                << custom_initial_upload_interval;
    } else {
      interval_ = base::TimeDelta::FromSeconds(kInitialIntervalSeconds);
      LOG(WARNING)
          << "Initial upload interval was set on the command line, but "
             "converting it to an int failed. Falling back to the default: "
          << kInitialIntervalSeconds;
    }
  } else {
    interval_ = base::TimeDelta::FromSeconds(kInitialIntervalSeconds);
  }
}
#else
MetricsScheduler::MetricsScheduler(const base::Closure& task_callback)
    : task_callback_(task_callback),
      interval_(base::TimeDelta::FromSeconds(kInitialIntervalSeconds)),
      running_(false),
      callback_pending_(false) {}
#endif

MetricsScheduler::~MetricsScheduler() {}

void MetricsScheduler::Start() {
  running_ = true;
  ScheduleNextTask();
}

void MetricsScheduler::Stop() {
  running_ = false;
  if (timer_.IsRunning())
    timer_.Stop();
}

void MetricsScheduler::TaskDone(base::TimeDelta next_interval) {
  DCHECK(callback_pending_);
  callback_pending_ = false;
  interval_ = next_interval;
  if (running_)
    ScheduleNextTask();
}

void MetricsScheduler::TriggerTask() {
  callback_pending_ = true;
  task_callback_.Run();
}

void MetricsScheduler::ScheduleNextTask() {
  DCHECK(running_);
  if (timer_.IsRunning() || callback_pending_)
    return;

  timer_.Start(FROM_HERE, interval_, this, &MetricsScheduler::TriggerTask);
}

}  // namespace metrics
