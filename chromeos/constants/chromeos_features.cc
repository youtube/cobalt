// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/constants/chromeos_features.h"

#include "base/feature_list.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace chromeos::features {

// Enables or disables more filtering out of phones from the Bluetooth UI.
BASE_FEATURE(kBluetoothPhoneFilter,
             "BluetoothPhoneFilter",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables cloud game features. A separate flag "LauncherGameSearch" controls
// launcher-only cloud gaming features, since they can also be enabled on
// non-cloud-gaming devices.
BASE_FEATURE(kCloudGamingDevice,
             "CloudGamingDevice",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Demo Mode System Web App migration
BASE_FEATURE(kDemoModeSWA, "DemoModeSWA", base::FEATURE_ENABLED_BY_DEFAULT);

// Disable idle sockets closing on memory pressure for NetworkContexts that
// belong to Profiles. It only applies to Profiles because the goal is to
// improve perceived performance of web browsing within the ChromeOS user
// session by avoiding re-estabshing TLS connections that require client
// certificates.
BASE_FEATURE(kDisableIdleSocketsCloseOnMemoryPressure,
             "disable_idle_sockets_close_on_memory_pressure",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Disables "Office Editing for Docs, Sheets & Slides" component app so handlers
// won't be registered, making it possible to install another version for
// testing.
BASE_FEATURE(kDisableOfficeEditingComponentApp,
             "DisableOfficeEditingComponentApp",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Disables translation services of the Quick Answers V2.
BASE_FEATURE(kDisableQuickAnswersV2Translation,
             "DisableQuickAnswersV2Translation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable experimental goldfish web app profile isolation.
BASE_FEATURE(kExperimentalWebAppProfileIsolation,
             "ExperimentalWebAppProfileIsolation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable experimental goldfish web app isolation.
BASE_FEATURE(kExperimentalWebAppStoragePartitionIsolation,
             "ExperimentalWebAppStoragePartitionIsolation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Jelly features.
BASE_FEATURE(kJelly, "Jelly", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Jellyroll features. Jellyroll is a feature flag for CrOSNext, which
// controls all system UI updates and new system components.
BASE_FEATURE(kJellyroll, "Jellyroll", base::FEATURE_DISABLED_BY_DEFAULT);

// Enables system authentication on Ash for password manager, which uses
// WebUI instead by default. Cleanup CL: https://crrev.com/c/4055733/2.
BASE_FEATURE(kPasswordManagerSystemAuthentication,
             "PasswordManagerSystemAuthentication",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to enable quick answers V2 settings sub-toggles.
BASE_FEATURE(kQuickAnswersV2SettingsSubToggle,
             "QuickAnswersV2SettingsSubToggle",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable Quick Answers Rich card.
BASE_FEATURE(kQuickAnswersRichCard,
             "QuickAnswersRichCard",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Office files upload workflow to improve Office files support.
BASE_FEATURE(kUploadOfficeToCloud,
             "UploadOfficeToCloud",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsCloudGamingDeviceEnabled() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->IsCloudGamingDevice();
#else
  return base::FeatureList::IsEnabled(kCloudGamingDevice);
#endif
}

bool IsDemoModeSWAEnabled() {
  return base::FeatureList::IsEnabled(kDemoModeSWA);
}

bool IsJellyEnabled() {
  return base::FeatureList::IsEnabled(kJelly);
}

bool IsJellyrollEnabled() {
  // Force Jellyroll features on if Jelly is enabled since they need to be
  // tested together. b/270742469
  return IsJellyEnabled() || base::FeatureList::IsEnabled(kJellyroll);
}

bool IsPasswordManagerSystemAuthenticationEnabled() {
  return base::FeatureList::IsEnabled(kPasswordManagerSystemAuthentication);
}

bool IsQuickAnswersV2TranslationDisabled() {
  return base::FeatureList::IsEnabled(kDisableQuickAnswersV2Translation);
}

bool IsQuickAnswersRichCardEnabled() {
  return base::FeatureList::IsEnabled(kQuickAnswersRichCard);
}

bool IsQuickAnswersV2SettingsSubToggleEnabled() {
  return base::FeatureList::IsEnabled(kQuickAnswersV2SettingsSubToggle);
}

bool IsUploadOfficeToCloudEnabled() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->IsUploadOfficeToCloudEnabled();
#else
  return base::FeatureList::IsEnabled(kUploadOfficeToCloud);
#endif
}

}  // namespace chromeos::features
