// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/metrics/cobalt_enabled_state_provider.h"

#include "base/logging.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_switches.h"

namespace cobalt {

bool CobaltEnabledStateProvider::IsConsentGiven() const {
  // If override on the command line, always report.
  if (metrics::IsMetricsReportingForceEnabled()) {
    return true;
  }
  // Else read currently stored pref.
  return local_state_->GetBoolean(metrics::prefs::kMetricsReportingEnabled);
}

bool CobaltEnabledStateProvider::IsReportingEnabled() const {
  // Consent and Reporting Enabled are the same thing in Cobalt.
  return IsConsentGiven();
}

void CobaltEnabledStateProvider::SetConsentGiven(bool is_consent_given) {
  local_state_->SetBoolean(metrics::prefs::kMetricsReportingEnabled,
                           is_consent_given);
}
void CobaltEnabledStateProvider::SetReportingEnabled(
    bool is_reporting_enabled) {
  SetConsentGiven(is_reporting_enabled);
}

}  // namespace cobalt
