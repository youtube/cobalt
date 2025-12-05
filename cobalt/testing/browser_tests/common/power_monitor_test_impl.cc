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

#include "cobalt/testing/browser_tests/common/power_monitor_test_impl.h"

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

// static
void PowerMonitorTestImpl::MakeSelfOwnedReceiver(
    mojo::PendingReceiver<mojom::PowerMonitorTest> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<PowerMonitorTestImpl>(),
                              std::move(receiver));
}

PowerMonitorTestImpl::PowerMonitorTestImpl() {
  base::PowerMonitor::GetInstance()->AddPowerStateObserver(this);
}

PowerMonitorTestImpl::~PowerMonitorTestImpl() {
  base::PowerMonitor::GetInstance()->RemovePowerStateObserver(this);
}

void PowerMonitorTestImpl::QueryNextState(QueryNextStateCallback callback) {
  // Do not allow overlapping call.
  DCHECK(callback_.is_null());
  callback_ = std::move(callback);

  if (need_to_report_) {
    ReportState();
  }
}

void PowerMonitorTestImpl::OnBatteryPowerStatusChange(
    base::PowerStateObserver::BatteryPowerStatus battery_power_status) {
  battery_power_status_ = battery_power_status;
  need_to_report_ = true;

  if (!callback_.is_null()) {
    ReportState();
  }
}

void PowerMonitorTestImpl::ReportState() {
  std::move(callback_).Run(battery_power_status_);
  need_to_report_ = false;
}

}  // namespace content
