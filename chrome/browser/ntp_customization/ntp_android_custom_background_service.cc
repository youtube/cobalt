// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_customization/ntp_android_custom_background_service.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_background_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

// static
void NtpAndroidCustomBackgroundService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(
      prefs::kNtpAndroidCustomBackgroundDict,
      NtpCustomBackgroundServiceBase::NtpCustomBackgroundDefaults());
  registry->RegisterBooleanPref(prefs::kNtpAndroidCustomBackgroundLocalToDevice,
                                false);
}

NtpAndroidCustomBackgroundService::NtpAndroidCustomBackgroundService(
    Profile* profile)
    : NtpCustomBackgroundServiceBase(
          profile->GetPrefs(),
          NtpBackgroundServiceFactory::GetForProfile(profile),
          prefs::kNtpAndroidCustomBackgroundDict,
          prefs::kNtpAndroidCustomBackgroundLocalToDevice) {}

NtpAndroidCustomBackgroundService::~NtpAndroidCustomBackgroundService() =
    default;

void NtpAndroidCustomBackgroundService::SelectLocalBackgroundImage(
    const base::FilePath& path) {
  // Not implemented on Android.
}
