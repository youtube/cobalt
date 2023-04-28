// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_TEST_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_TEST_METRICS_PROVIDER_H_

#include "base/macros.h"
#include "components/metrics/metrics_provider.h"

namespace metrics {

// A simple implementation of MetricsProvider that checks that its providing
// functions are called, for use in tests.
class TestMetricsProvider : public MetricsProvider {
 public:
  TestMetricsProvider()
      : init_called_(false),
        on_recording_disabled_called_(false),
        has_initial_stability_metrics_(false),
        has_initial_stability_metrics_called_(false),
        provide_initial_stability_metrics_called_(false),
        provide_stability_metrics_called_(false),
        provide_system_profile_metrics_called_(false) {}

  // MetricsProvider:
  void Init() override;
  void OnRecordingDisabled() override;
  bool HasPreviousSessionData() override;
  void ProvidePreviousSessionData(
      ChromeUserMetricsExtension* uma_proto) override;
  void ProvideCurrentSessionData(
      ChromeUserMetricsExtension* uma_proto) override;
  void ProvideSystemProfileMetrics(
      SystemProfileProto* system_profile_proto) override;

  bool init_called() { return init_called_; }
  bool on_recording_disabled_called() { return on_recording_disabled_called_; }
  bool has_initial_stability_metrics_called() {
    return has_initial_stability_metrics_called_;
  }
  void set_has_initial_stability_metrics(bool has_initial_stability_metrics) {
    has_initial_stability_metrics_ = has_initial_stability_metrics;
  }
  bool provide_initial_stability_metrics_called() const {
    return provide_initial_stability_metrics_called_;
  }
  bool provide_stability_metrics_called() const {
    return provide_stability_metrics_called_;
  }
  bool provide_system_profile_metrics_called() const {
    return provide_system_profile_metrics_called_;
  }

 private:
  bool init_called_;
  bool on_recording_disabled_called_;
  bool has_initial_stability_metrics_;
  bool has_initial_stability_metrics_called_;
  bool provide_initial_stability_metrics_called_;
  bool provide_stability_metrics_called_;
  bool provide_system_profile_metrics_called_;

  DISALLOW_COPY_AND_ASSIGN(TestMetricsProvider);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_TEST_METRICS_PROVIDER_H_
