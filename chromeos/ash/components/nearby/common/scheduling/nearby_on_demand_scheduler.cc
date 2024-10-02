// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/common/scheduling/nearby_on_demand_scheduler.h"

#include <utility>

namespace ash::nearby {

NearbyOnDemandScheduler::NearbyOnDemandScheduler(bool retry_failures,
                                                 bool require_connectivity,
                                                 const std::string& pref_name,
                                                 PrefService* pref_service,
                                                 OnRequestCallback callback,
                                                 const base::Clock* clock)
    : NearbySchedulerBase(retry_failures,
                          require_connectivity,
                          pref_name,
                          pref_service,
                          std::move(callback),
                          clock) {}

NearbyOnDemandScheduler::~NearbyOnDemandScheduler() = default;

absl::optional<base::TimeDelta>
NearbyOnDemandScheduler::TimeUntilRecurringRequest(base::Time now) const {
  return absl::nullopt;
}

}  // namespace ash::nearby
