// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/power_monitor_device_source.h"

#if BUILDFLAG(IS_ANDROID)
#include <android/api-level.h>
#endif

#include "base/logging.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

using DeviceThermalState = base::PowerThermalObserver::DeviceThermalState;

namespace base {

class PowerMonitorDeviceSourceTest : public testing::Test {
 public:
  PowerMonitorDeviceSourceTest() = default;
  ~PowerMonitorDeviceSourceTest() override = default;

  DeviceThermalState GetCurrentThermalState() {
    return power_monitor_device_source_.GetCurrentThermalState();
  }

  PowerMonitorDeviceSource power_monitor_device_source_;
};

TEST_F(PowerMonitorDeviceSourceTest, GetCurrentThermalState) {
  const DeviceThermalState current_state = GetCurrentThermalState();
#if BUILDFLAG(IS_MAC)
  // We cannot make assumptions on |current_state|. Print it out to use the var.
  DVLOG(1) << PowerMonitorSource::DeviceThermalStateToString(current_state);
#elif BUILDFLAG(IS_ANDROID)
  if (android_get_device_api_level() >= __ANDROID_API_Q__) {
    EXPECT_NE(current_state, DeviceThermalState::kUnknown);
  } else {
    EXPECT_EQ(current_state, DeviceThermalState::kUnknown);
  }
#else
  EXPECT_EQ(current_state, DeviceThermalState::kUnknown);
#endif
}

}  // namespace base
