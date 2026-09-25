// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "base/tracing_buildflags.h"
#include "build/buildflag.h"
#include "components/memory_pressure/system_memory_pressure_evaluator.h"

#if BUILDFLAG(IS_COBALT)
#include "base/feature_list.h"
#include "base/features.h"
#include "base/logging.h"
#endif

#if BUILDFLAG(ENABLE_BASE_TRACING)
#include "base/trace_event/memory_pressure_level_proto.h"  // no-presubmit-check
#endif

namespace memory_pressure {

MultiSourceMemoryPressureMonitor::MultiSourceMemoryPressureMonitor()
    : current_pressure_level_(
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE),
      dispatch_callback_(base::BindRepeating(
          &base::MemoryPressureListener::NotifyMemoryPressure)),
      aggregator_(this),
      level_reporter_(current_pressure_level_) {
#if BUILDFLAG(IS_COBALT)
  LOG(INFO) << "[CobaltMemoryPressure] MultiSourceMemoryPressureMonitor "
               "initialized (cooldown="
            << GetCooldownPeriod().InSeconds() << "s)";
#endif
}

MultiSourceMemoryPressureMonitor::~MultiSourceMemoryPressureMonitor() {
  // Destroy system evaluator early while the remaining members of this class
  // still exist. MultiSourceMemoryPressureMonitor implements
  // MemoryPressureVoteAggregator::Delegate, and
  // delegate_->OnMemoryPressureLevelChanged() gets indirectly called during
  // ~SystemMemoryPressureEvaluator().
  system_evaluator_.reset();
}

void MultiSourceMemoryPressureMonitor::MaybeStartPlatformVoter() {
  system_evaluator_ =
      SystemMemoryPressureEvaluator::CreateDefaultSystemEvaluator(this);
}

base::MemoryPressureListener::MemoryPressureLevel
MultiSourceMemoryPressureMonitor::GetCurrentPressureLevel() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return current_pressure_level_;
}

std::unique_ptr<MemoryPressureVoter>
MultiSourceMemoryPressureMonitor::CreateVoter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return aggregator_.CreateVoter();
}

#if BUILDFLAG(IS_COBALT)
base::TimeDelta MultiSourceMemoryPressureMonitor::GetCooldownPeriod() const {
  if (base::FeatureList::IsEnabled(
          base::features::kCobaltMemoryPressureCooldown)) {
    int cooldown_sec =
        base::features::kCobaltMemoryPressureCooldownSeconds.Get();
    return base::Seconds(std::max(1, cooldown_sec));
  }
  return base::Seconds(30);
}
#endif

void MultiSourceMemoryPressureMonitor::OnMemoryPressureLevelChanged(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(current_pressure_level_, level);

  level_reporter_.OnMemoryPressureLevelChanged(level);

  TRACE_EVENT_INSTANT(
      "base", "MultiSourceMemoryPressureMonitor::OnMemoryPressureLevelChanged",
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_chrome_memory_pressure_notification();
        data->set_level(
            base::trace_event::MemoryPressureLevelToTraceEnum(level));
      });

  current_pressure_level_ = level;
}

void MultiSourceMemoryPressureMonitor::OnNotifyListenersRequested() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(IS_COBALT)
  if (current_pressure_level_ ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
    return;
  }

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta cooldown_period = GetCooldownPeriod();
  bool is_in_cooldown = !last_dispatch_time_.is_null() &&
                        (now - last_dispatch_time_) < cooldown_period;

  // If memory pressure escalates to a higher severity than the last dispatched
  // level, bypass the cooldown window immediately.
  bool is_escalation = current_pressure_level_ > last_dispatched_level_;

  if (is_in_cooldown && !is_escalation) {
    LOG(INFO) << "[CobaltMemoryPressure] Monitor SUPPRESSED pulse (level="
              << current_pressure_level_ << ", elapsed="
              << (now - last_dispatch_time_).InMilliseconds() << "ms < "
              << cooldown_period.InMilliseconds() << "ms cooldown)";
    return;
  }

  if (is_in_cooldown && is_escalation) {
    LOG(INFO) << "[CobaltMemoryPressure] Monitor ESCALATION OVERRIDE ("
              << last_dispatched_level_ << " -> " << current_pressure_level_
              << "), bypassing cooldown!";
  }

  LOG(INFO) << "[CobaltMemoryPressure] Monitor DISPATCHING level="
            << current_pressure_level_
            << " to base::MemoryPressureListener";

  last_dispatch_time_ = base::TimeTicks::Now();
  last_dispatched_level_ = current_pressure_level_;
#endif
  dispatch_callback_.Run(current_pressure_level_);
}

void MultiSourceMemoryPressureMonitor::SetSystemEvaluator(
    std::unique_ptr<SystemMemoryPressureEvaluator> evaluator) {
  DCHECK(!system_evaluator_);
  system_evaluator_ = std::move(evaluator);
}

void MultiSourceMemoryPressureMonitor::SetDispatchCallbackForTesting(
    const DispatchCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Must be called before `Start()`.
  DCHECK(!system_evaluator_);
  dispatch_callback_ = callback;
}

}  // namespace memory_pressure
