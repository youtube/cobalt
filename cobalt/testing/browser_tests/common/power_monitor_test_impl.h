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

#ifndef COBALT_TESTING_BROWSER_TESTS_COMMON_POWER_MONITOR_TEST_IMPL_H_
#define COBALT_TESTING_BROWSER_TESTS_COMMON_POWER_MONITOR_TEST_IMPL_H_

#include "base/power_monitor/power_monitor.h"
#include "cobalt/shell/common/power_monitor_test.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

class PowerMonitorTestImpl : public base::PowerStateObserver,
                             public mojom::PowerMonitorTest {
 public:
  static void MakeSelfOwnedReceiver(
      mojo::PendingReceiver<mojom::PowerMonitorTest> receiver);

  PowerMonitorTestImpl();

  PowerMonitorTestImpl(const PowerMonitorTestImpl&) = delete;
  PowerMonitorTestImpl& operator=(const PowerMonitorTestImpl&) = delete;

  ~PowerMonitorTestImpl() override;

 private:
  // mojom::PowerMonitorTest:
  void QueryNextState(QueryNextStateCallback callback) override;

  // base::PowerStateObserver:
  void OnBatteryPowerStatusChange(base::PowerStateObserver::BatteryPowerStatus
                                      battery_power_status) override;

  void ReportState();

  QueryNextStateCallback callback_;
  PowerStateObserver::BatteryPowerStatus battery_power_status_ =
      PowerStateObserver::BatteryPowerStatus::kUnknown;
  bool need_to_report_ = false;
};

}  // namespace content

#endif  // COBALT_TESTING_BROWSER_TESTS_COMMON_POWER_MONITOR_TEST_IMPL_H_
