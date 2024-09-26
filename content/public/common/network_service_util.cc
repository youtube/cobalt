// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/network_service_util.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#endif

namespace content {
namespace {

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kNetworkServiceOutOfProcessMemoryThreshold,
             "NetworkServiceOutOfProcessMemoryThreshold",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Using 1077 rather than 1024 because it helps ensure that devices with
// exactly 1GB of RAM won't get included because of inaccuracies or off-by-one
// errors.
constexpr base::FeatureParam<int> kNetworkServiceOutOfProcessThresholdMb{
    &kNetworkServiceOutOfProcessMemoryThreshold,
    "network_service_oop_threshold_mb", 1077};
#endif

// Indicates whether the network service is forced to be running in the browser
// process.
bool g_force_in_process_network_service = false;

}  // namespace

bool IsOutOfProcessNetworkService() {
  return !IsInProcessNetworkService();
}

bool IsInProcessNetworkService() {
#if BUILDFLAG(IS_ANDROID)
  // Check RAM size before looking at kNetworkServiceInProcess flag
  // so that we can throttle the finch groups including control.
  if (base::SysInfo::AmountOfPhysicalMemoryMB() <=
      kNetworkServiceOutOfProcessThresholdMb.Get()) {
    return true;
  }
#endif

  if (g_force_in_process_network_service ||
      base::FeatureList::IsEnabled(features::kNetworkServiceInProcess) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess)) {
    return true;
  }
  return false;
}

void ForceInProcessNetworkService(bool is_forced) {
  g_force_in_process_network_service = is_forced;
}

}  // namespace content
