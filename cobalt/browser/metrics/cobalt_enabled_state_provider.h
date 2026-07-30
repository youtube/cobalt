// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BROWSER_METRICS_COBALT_ENABLED_STATE_PROVIDER_H_
#define COBALT_BROWSER_METRICS_COBALT_ENABLED_STATE_PROVIDER_H_

#include "components/metrics/enabled_state_provider.h"
#include "components/prefs/pref_service.h"

namespace cobalt {

// A Cobalt implementation of EnabledStateProvider. This class is the primary
// entry point into enabling/disabling metrics collection and uploading.
class CobaltEnabledStateProvider : public ::metrics::EnabledStateProvider {
 public:
  explicit CobaltEnabledStateProvider(PrefService* local_state)
      : local_state_(local_state) {}

  CobaltEnabledStateProvider(const CobaltEnabledStateProvider&) = delete;
  CobaltEnabledStateProvider& operator=(const CobaltEnabledStateProvider&) =
      delete;

  ~CobaltEnabledStateProvider() override {}

  // Indicates user consent to collect and report metrics. In Cobalt, consent
  // is inherited through the web application, so this is usually true.
  bool IsConsentGiven() const override;

  // Whether metric collection is enabled. This is what controls
  // recording/aggregation of metrics.
  bool IsReportingEnabled() const override;

  // Setters for consent and reporting controls.
  void SetConsentGiven(bool is_consent_given);
  void SetReportingEnabled(bool is_reporting_enabled);

 private:
  const raw_ptr<PrefService> local_state_;
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_ENABLED_STATE_PROVIDER_H_
