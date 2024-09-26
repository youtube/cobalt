// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/device_info_sync_client_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/sync/sync_invalidations_service_factory.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/invalidations/sync_invalidations_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/webauthn/android/cable_module_android.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/browser_util.h"
#endif

namespace browser_sync {

DeviceInfoSyncClientImpl::DeviceInfoSyncClientImpl(Profile* profile)
    : profile_(profile) {}

DeviceInfoSyncClientImpl::~DeviceInfoSyncClientImpl() = default;

// syncer::DeviceInfoSyncClient:
std::string DeviceInfoSyncClientImpl::GetSigninScopedDeviceId() const {
// Since the local sync backend is currently only supported on Windows, Mac and
// Linux don't even check the pref on other os-es.
// TODO(crbug.com/1052397): Reassess whether the next block needs to be included
// in lacros-chrome once build flag switch of lacros-chrome is
// complete.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  syncer::SyncPrefs prefs(profile_->GetPrefs());
  if (prefs.IsLocalSyncEnabled()) {
    return "local_device";
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS_LACROS))

  return GetSigninScopedDeviceIdForProfile(profile_);
}

// syncer::DeviceInfoSyncClient:
bool DeviceInfoSyncClientImpl::GetSendTabToSelfReceivingEnabled() const {
  // TODO(crbug.com/1286405): Current logic allows to disable receiving tabs
  // in Ash, while sending is still enabled - this seems to be the best solution
  // for Lacros-Primary. Once Lacros-Only is the only available option, this
  // should simply check whether SendTabToSelf datatype is enabled.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return !crosapi::browser_util::IsLacrosPrimaryBrowser();
#else
  return true;
#endif
}

// syncer::DeviceInfoSyncClient:
absl::optional<syncer::DeviceInfo::SharingInfo>
DeviceInfoSyncClientImpl::GetLocalSharingInfo() const {
  return SharingSyncPreference::GetLocalSharingInfoForSync(
      profile_->GetPrefs());
}

// syncer::DeviceInfoSyncClient:
absl::optional<std::string> DeviceInfoSyncClientImpl::GetFCMRegistrationToken()
    const {
  return SyncInvalidationsServiceFactory::GetForProfile(profile_)
      ->GetFCMRegistrationToken();
}

// syncer::DeviceInfoSyncClient:
absl::optional<syncer::ModelTypeSet>
DeviceInfoSyncClientImpl::GetInterestedDataTypes() const {
  return SyncInvalidationsServiceFactory::GetForProfile(profile_)
      ->GetInterestedDataTypes();
}

absl::optional<syncer::DeviceInfo::PhoneAsASecurityKeyInfo>
DeviceInfoSyncClientImpl::GetPhoneAsASecurityKeyInfo() const {
#if BUILDFLAG(IS_ANDROID)
  return webauthn::authenticator::GetSyncDataIfRegistered();
#else
  return absl::nullopt;
#endif
}

bool DeviceInfoSyncClientImpl::IsUmaEnabledOnCrOSDevice() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
#else
  return false;
#endif
}
}  // namespace browser_sync
