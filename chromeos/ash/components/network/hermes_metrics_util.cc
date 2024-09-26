// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hermes_metrics_util.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/tick_clock.h"

namespace ash::hermes_metrics {

void LogInstallViaQrCodeResult(HermesResponseStatus status) {
  base::UmaHistogramEnumeration("Network.Cellular.ESim.InstallViaQrCode.Result",
                                status);

  if (status == HermesResponseStatus::kSuccess ||
      !base::Contains(kHermesUserErrorCodes, status)) {
    base::UmaHistogramEnumeration(
        "Network.Cellular.ESim.Installation.NonUserErrorSuccessRate", status);
  }
}

void LogInstallPendingProfileResult(HermesResponseStatus status) {
  base::UmaHistogramEnumeration(
      "Network.Cellular.ESim.InstallPendingProfile.Result", status);
}

void LogEnableProfileResult(HermesResponseStatus status) {
  base::UmaHistogramEnumeration("Network.Cellular.ESim.EnableProfile.Result",
                                status);
}

void LogDisableProfileResult(HermesResponseStatus status) {
  base::UmaHistogramEnumeration("Network.Cellular.ESim.DisableProfile.Result",
                                status);
}

void LogUninstallProfileResult(HermesResponseStatus status) {
  base::UmaHistogramEnumeration("Network.Cellular.ESim.UninstallProfile.Result",
                                status);
}

void LogRequestPendingProfilesResult(HermesResponseStatus status) {
  base::UmaHistogramEnumeration(
      "Network.Cellular.ESim.RequestPendingProfiles.Result", status);
}

void LogRequestPendingProfilesLatency(base::TimeDelta call_latency) {
  UMA_HISTOGRAM_LONG_TIMES(
      "Network.Cellular.ESim.RequestPendingProfiles.Latency", call_latency);
}

}  // namespace ash::hermes_metrics
