// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/progress_delay.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "content/public/common/content_switches.h"

namespace web_app {

ProgressDelay::ProgressDelay(base::TimeDelta delay_time, int total_steps)
    // Set delay to 0 for tests to prevent flakiness and speed up
    // test execution.
    : delay_time_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType)
              ? base::TimeDelta()
              : delay_time),
      total_steps_(total_steps) {}

ProgressDelay::~ProgressDelay() = default;

// Starts filing timers posted tasks handles.
void ProgressDelay::Start(
    base::RepeatingCallback<void(std::optional<double>)> progress_callback) {
  progress_callback_ = std::move(progress_callback);

  if (delay_time_.is_zero()) {
    progress_callback_.Run(1.0);
    progress_callback_.Run(std::nullopt);
    return;
  }

  timer_.Start(FROM_HERE, delay_time_ / total_steps_,
               base::BindRepeating(&ProgressDelay::OnTimerTick,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void ProgressDelay::DelayComplete() {
  progress_callback_.Run(std::nullopt);
}

void ProgressDelay::OnTimerTick() {
  double progress = next_step_ * 1.0 / total_steps_;
  progress_callback_.Run(progress);

  if (next_step_ >= total_steps_) {
    timer_.Stop();
    DelayComplete();
  }
  ++next_step_;
}

}  // namespace web_app
