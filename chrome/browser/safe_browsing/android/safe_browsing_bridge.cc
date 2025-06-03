// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/files/file_path.h"
// NOTE: This target is transitively depended on by //chrome/browser and thus
// can't depend on it.
#include "chrome/browser/profiles/profile_manager.h"  // nogncheck
#include "chrome/browser/safe_browsing/android/jni_headers/SafeBrowsingBridge_jni.h"
// NOTE: This target is transitively depended on by //chrome/browser and thus
// can't depend on it.
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"  // nogncheck
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_utils.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

using base::android::JavaParamRef;

namespace {

PrefService* GetPrefService() {
  return ProfileManager::GetActiveUserProfile()
      ->GetOriginalProfile()
      ->GetPrefs();
}

}  // namespace

namespace safe_browsing {

static jint JNI_SafeBrowsingBridge_UmaValueForFile(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& path) {
  base::FilePath file_path(ConvertJavaStringToUTF8(env, path));
  return safe_browsing::FileTypePolicies::GetInstance()->UmaValueForFile(
      file_path);
}

static jboolean JNI_SafeBrowsingBridge_GetSafeBrowsingExtendedReportingEnabled(
    JNIEnv* env) {
  return safe_browsing::IsExtendedReportingEnabled(*GetPrefService());
}

static void JNI_SafeBrowsingBridge_SetSafeBrowsingExtendedReportingEnabled(
    JNIEnv* env,
    jboolean enabled) {
  safe_browsing::SetExtendedReportingPrefAndMetric(
      GetPrefService(), enabled,
      safe_browsing::SBER_OPTIN_SITE_ANDROID_SETTINGS);
}

static jboolean JNI_SafeBrowsingBridge_GetSafeBrowsingExtendedReportingManaged(
    JNIEnv* env) {
  PrefService* pref_service = GetPrefService();
  return pref_service->IsManagedPreference(
      prefs::kSafeBrowsingScoutReportingEnabled);
}

static jint JNI_SafeBrowsingBridge_GetSafeBrowsingState(JNIEnv* env) {
  return static_cast<jint>(
      safe_browsing::GetSafeBrowsingState(*GetPrefService()));
}

static void JNI_SafeBrowsingBridge_SetSafeBrowsingState(JNIEnv* env,
                                                        jint state) {
  return safe_browsing::SetSafeBrowsingState(
      GetPrefService(), static_cast<SafeBrowsingState>(state),
      /*is_esb_enabled_in_sync=*/false);
}

static jboolean JNI_SafeBrowsingBridge_IsSafeBrowsingManaged(JNIEnv* env) {
  return safe_browsing::IsSafeBrowsingPolicyManaged(*GetPrefService());
}

static jboolean JNI_SafeBrowsingBridge_IsUnderAdvancedProtection(JNIEnv* env) {
  Profile* profile =
      ProfileManager::GetActiveUserProfile()->GetOriginalProfile();
  return profile &&
         safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
             profile)
             ->IsUnderAdvancedProtection();
}

static jboolean JNI_SafeBrowsingBridge_IsHashRealTimeLookupEligibleInSession(
    JNIEnv* env) {
  return safe_browsing::hash_realtime_utils::
      IsHashRealTimeLookupEligibleInSession();
}

}  // namespace safe_browsing
