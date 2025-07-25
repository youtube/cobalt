// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNALS_SIGNALS_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNALS_SIGNALS_UTILS_H_

#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

#include "third_party/abseil-cpp/absl/types/optional.h"

class PolicyBlocklistService;
class PrefService;

namespace enterprise_signals {
namespace utils {

absl::optional<bool> GetThirdPartyBlockingEnabled(PrefService* local_state);

bool GetBuiltInDnsClientEnabled(PrefService* local_state);

absl::optional<safe_browsing::PasswordProtectionTrigger>
GetPasswordProtectionWarningTrigger(PrefService* profile_prefs);

safe_browsing::SafeBrowsingState GetSafeBrowsingProtectionLevel(
    PrefService* profile_prefs);

bool GetChromeRemoteDesktopAppBlocked(PolicyBlocklistService* service);

}  // namespace utils
}  // namespace enterprise_signals

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNALS_SIGNALS_UTILS_H_
