// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/signals_features.h"

namespace enterprise_signals::features {

BASE_FEATURE(kAllowClientCertificateReportingForUsers,
             "AllowClientCertificateReportingForUsers",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_CHROMEOS)
// Enables the triggering of device signals consent dialog when conditions met
// This feature also requires UnmanagedDeviceSignalsConsentFlowEnabled policy to
// be enabled
BASE_FEATURE(kDeviceSignalsConsentDialog,
             "DeviceSignalsConsentDialog",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsConsentDialogEnabled() {
  return base::FeatureList::IsEnabled(kDeviceSignalsConsentDialog);
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_CHROMEOS)

BASE_FEATURE(kNewEvSignalsUnaffiliatedEnabled,
             "NewEvSignalsUnaffiliatedEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace enterprise_signals::features
