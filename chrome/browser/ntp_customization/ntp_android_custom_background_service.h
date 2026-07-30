// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_ANDROID_CUSTOM_BACKGROUND_SERVICE_H_
#define CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_ANDROID_CUSTOM_BACKGROUND_SERVICE_H_

#include "components/themes/ntp_custom_background_service_base.h"

class PrefRegistrySimple;
class Profile;

namespace base {
class FilePath;
}  // namespace base

// Android-specific service for managing custom backgrounds on the NTP.
class NtpAndroidCustomBackgroundService
    : public NtpCustomBackgroundServiceBase {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit NtpAndroidCustomBackgroundService(Profile* profile);
  ~NtpAndroidCustomBackgroundService() override;

  // NtpCustomBackgroundServiceBase:
  void SelectLocalBackgroundImage(const base::FilePath& path) override;
  std::optional<int> GetNextRefreshTimestamp() const override;
};

#endif  // CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_ANDROID_CUSTOM_BACKGROUND_SERVICE_H_
