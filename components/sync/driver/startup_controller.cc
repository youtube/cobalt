// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/startup_controller.h"

#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/features.h"

namespace syncer {

namespace {

// The amount of time we'll wait to initialize sync if no data type requests
// immediately initialization.
constexpr base::TimeDelta kDefaultDeferredInitDelay = base::Seconds(10);

base::TimeDelta GetDeferredInitDelay() {
  if (base::FeatureList::IsEnabled(kDeferredSyncStartupCustomDelay)) {
    return base::Seconds(kDeferredSyncStartupCustomDelayInSeconds.Get());
  }

  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  if (cmdline->HasSwitch(kSyncDeferredStartupTimeoutSeconds)) {
    int timeout = 0;
    if (base::StringToInt(
            cmdline->GetSwitchValueASCII(kSyncDeferredStartupTimeoutSeconds),
            &timeout)) {
      DCHECK_GE(timeout, 0);
      DVLOG(2) << "Sync StartupController overriding startup timeout to "
               << timeout << " seconds.";
      return base::Seconds(timeout);
    }
  }
  return kDefaultDeferredInitDelay;
}

}  // namespace

StartupController::StartupController(
    base::RepeatingCallback<ModelTypeSet()> get_preferred_data_types,
    base::RepeatingCallback<bool()> should_start,
    base::OnceClosure start_engine)
    : get_preferred_data_types_callback_(std::move(get_preferred_data_types)),
      should_start_callback_(std::move(should_start)),
      start_engine_callback_(std::move(start_engine)) {}

StartupController::~StartupController() = default;

void StartupController::TryStart(bool force_immediate) {
  // Post a task instead of running the startup checks directly, to guarantee
  // that |start_engine_callback_| is never called synchronously from
  // TryStart().
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&StartupController::TryStartImpl,
                                weak_factory_.GetWeakPtr(), force_immediate));
}

void StartupController::TryStartImpl(bool force_immediate) {
  if (!should_start_callback_.Run()) {
    return;
  }

  const bool first_start = start_up_time_.is_null();
  if (first_start) {
    start_up_time_ = base::Time::Now();
  }

  // For performance reasons, defer the heavy lifting for sync init unless:
  //
  // - a datatype has requested an immediate start of sync, or
  // - sync needs to start up the engine immediately to provide control state
  //   and encryption information to the UI.
  if (!force_immediate && !bypass_deferred_startup_ &&
      get_preferred_data_types_callback_.Run().Has(SESSIONS)) {
    if (first_start) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&StartupController::OnFallbackStartupTimerExpired,
                         weak_factory_.GetWeakPtr()),
          GetDeferredInitDelay());
    }
    return;
  }

  if (start_engine_callback_) {
    std::move(start_engine_callback_).Run();
  }
}

void StartupController::RecordTimeDeferred(DeferredInitTrigger trigger) {
  DCHECK(!start_up_time_.is_null());
  base::TimeDelta time_deferred = base::Time::Now() - start_up_time_;
  base::UmaHistogramCustomTimes("Sync.Startup.TimeDeferred2", time_deferred,
                                base::Seconds(0), base::Minutes(2), 60);
  base::UmaHistogramEnumeration("Sync.Startup.DeferredInitTrigger", trigger);
}

void StartupController::OnFallbackStartupTimerExpired() {
  if (!start_engine_callback_) {
    return;
  }

  DVLOG(2) << "Sync deferred init fallback timer expired, starting engine.";
  RecordTimeDeferred(DeferredInitTrigger::kFallbackTimer);
  // Once the deferred init timer has expired, don't defer startup again (until
  // Reset() or browser restart), even if this startup attempt doesn't succeed.
  bypass_deferred_startup_ = true;
  TryStart(/*force_immediate=*/false);
}

StartupController::State StartupController::GetState() const {
  if (!start_engine_callback_) {
    return State::STARTED;
  }
  if (!start_up_time_.is_null()) {
    return State::STARTING_DEFERRED;
  }
  return State::NOT_STARTED;
}

void StartupController::OnDataTypeRequestsSyncStartup(ModelType type) {
  if (!start_engine_callback_) {
    return;
  }

  DVLOG(2) << "Data type requesting sync startup: "
           << ModelTypeToDebugString(type);
  if (!start_up_time_.is_null()) {
    RecordTimeDeferred(DeferredInitTrigger::kDataTypeRequest);
  }
  bypass_deferred_startup_ = true;
  TryStart(/*force_immediate=*/false);
}

}  // namespace syncer
