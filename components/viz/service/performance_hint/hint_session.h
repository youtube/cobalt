// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_PERFORMANCE_HINT_HINT_SESSION_H_
#define COMPONENTS_VIZ_SERVICE_PERFORMANCE_HINT_HINT_SESSION_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/threading/platform_thread.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

// This is a wrapper for the Android `android.os.PerformanceHintManager` APIs
// used to provide hints to the OS on rendering performance, so the OS can
// ramp CPU performance up or down on demand.
// This header is platform-agnostic though it's only implemented on Android

class VIZ_SERVICE_EXPORT HintSession {
 public:
  enum class BoostType {
    kDefault,
    kScrollBoost,
    kWakeUpBoost,
  };

  enum class SessionType {
    kAnimation = 0,
    kRendererMain = 1,
  };

  virtual ~HintSession() = default;

  virtual void UpdateTargetDuration(base::TimeDelta target_duration) = 0;

  // `actual_duration` is compared to `target_duration` in `CreateSession` to
  // determine the performance of a frame.
  // 'preferable_boost_type' is a hint which mode to use. There is no guarantee
  // that this mode will be used immediately or will be used at all.
  virtual void ReportCpuCompletionTime(base::TimeDelta actual_duration,
                                       base::TimeTicks draw_start,
                                       BoostType preferable_boost_type) = 0;

  virtual void SetThreads(
      const base::flat_set<base::PlatformThreadId>& thread_ids) = 0;
};

class VIZ_SERVICE_EXPORT HintSessionFactory {
 public:
  // `permanent_thread_ids` are thread IDs that will be present in all
  // HintSessions.
  // Can return nullptr which can be used as feature detection.
  static std::unique_ptr<HintSessionFactory> Create(
      base::flat_set<base::PlatformThreadId> permanent_thread_ids);

  virtual ~HintSessionFactory() = default;

  // For animation (SessionType::kAnimation) sessions, `transient_thread_ids`
  // are added to `permanent_thread_ids` to create this session. Changing
  // `transient_thread_ids` still requires deleting and recreating the session.
  // `target_duration` is compared to `actual_duration` in
  // `ReportCpuCompletionTime` to determine the performance of a frame.
  //
  // For renderer main (SessionType::kRendererMain) sessions, the session
  // includes only `transient_thread_ids`. These sessions never send any timing
  // hints, i.e. `ReportCpuCompletionTime` calls are no-op.
  virtual std::unique_ptr<HintSession> CreateSession(
      base::flat_set<base::PlatformThreadId> transient_thread_ids,
      base::TimeDelta target_duration,
      HintSession::SessionType type) = 0;

  // Issue an early hint to wake up some session.
  virtual void WakeUp() = 0;

  // Returns the full list of threads for the given session type.
  // For animation (SessionType::kAnimation) sessions, this is a union of
  // `transient_thread_ids` and `permanent_thread_ids_`.
  // For renderer main (SessionType::kRendererMain) sessions, this is just
  // `transient_thread_ids`.
  virtual base::flat_set<base::PlatformThreadId> GetSessionThreadIds(
      base::flat_set<base::PlatformThreadId> transient_thread_ids,
      HintSession::SessionType type) = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_PERFORMANCE_HINT_HINT_SESSION_H_
