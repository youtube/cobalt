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

#include "cobalt/browser/metrics/cobalt_enabled_state_provider.h"

#include "components/metrics/enabled_state_provider.h"

namespace cobalt {
namespace browser {
namespace metrics {

bool CobaltEnabledStateProvider::IsConsentGiven() const {
  return is_consent_given_;
}

bool CobaltEnabledStateProvider::IsReportingEnabled() const {
  return is_reporting_enabled_;
}

void CobaltEnabledStateProvider::SetConsentGiven(bool is_consent_given) {
  is_consent_given_ = is_consent_given;
}
void CobaltEnabledStateProvider::SetReportingEnabled(
    bool is_reporting_enabled) {
  is_reporting_enabled_ = is_reporting_enabled;
}

}  // namespace metrics
}  // namespace browser
}  // namespace cobalt
