// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_UMA_SESSION_H_
#define CHROME_BROWSER_ASH_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_UMA_SESSION_H_

#include <string>

#include "base/time/time.h"
#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing_session.h"

namespace arc {

class ArcAppPerformanceTracing;

// Handles tracing of an app of known category. Tracing is done periodically
// during the all time of app is active. Tracing results are published in UMA.
class ArcAppPerformanceTracingUmaSession
    : public ArcAppPerformanceTracingSession {
 public:
  ArcAppPerformanceTracingUmaSession(ArcAppPerformanceTracing* owner,
                                     const std::string& category);

  ArcAppPerformanceTracingUmaSession(
      const ArcAppPerformanceTracingUmaSession&) = delete;
  ArcAppPerformanceTracingUmaSession& operator=(
      const ArcAppPerformanceTracingUmaSession&) = delete;

  ~ArcAppPerformanceTracingUmaSession() override;

  static void SetTracingPeriodForTesting(const base::TimeDelta& period);

  // ArcAppPerformanceTracingSession:
  void Schedule() override;

 protected:
  // ArcAppPerformanceTracingSession:
  void OnTracingDone(double fps,
                     double commit_deviation,
                     double render_quality) override;
  void OnTracingFailed() override;

 private:
  // Determines the tracing start delay. If we already reported this category,
  // start delay will be increased.
  base::TimeDelta GetStartDelay() const;

  // Tracing category.
  const std::string category_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_UMA_SESSION_H_
