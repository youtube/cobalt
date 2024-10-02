// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_utils.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_reuse_manager_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/browser/store_metrics_reporter.h"
#include "components/safe_browsing/buildflags.h"

namespace password_manager {
class PasswordStoreInterface;
class PasswordReuseManager;
}  // namespace password_manager

namespace {

// Used for attaching metrics reporter to a WebContents.
constexpr char kPasswordStoreMetricsReporterKey[] =
    "PasswordStoreMetricsReporterKey";

// Whether the primary account of the current profile is under Advanced
// Protection - a type of Google Account that helps protect our most at-risk
// users.
bool IsUnderAdvancedProtection(Profile* profile) {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  return safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
             profile)
      ->IsUnderAdvancedProtection();
#else
  return false;
#endif
}

// A class used to delay the construction of StoreMetricsReporter by 30 seconds.
class StoreMetricReporterHelper : public base::SupportsUserData::Data {
 public:
  explicit StoreMetricReporterHelper(Profile* profile) : profile_(profile) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&StoreMetricReporterHelper::StartMetricsReporting,
                       weak_ptr_factory_.GetWeakPtr()),
        base::Seconds(30));
  }
  ~StoreMetricReporterHelper() override = default;

 private:
  void StartMetricsReporting() {
    password_manager::PasswordStoreInterface* profile_store =
        PasswordStoreFactory::GetForProfile(profile_,
                                            ServiceAccessType::EXPLICIT_ACCESS)
            .get();
    password_manager::PasswordStoreInterface* account_store =
        AccountPasswordStoreFactory::GetForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS)
            .get();
    syncer::SyncService* sync_service =
        SyncServiceFactory::HasSyncService(profile_)
            ? SyncServiceFactory::GetForProfile(profile_)
            : nullptr;
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_->GetOriginalProfile());
    password_manager::PasswordReuseManager* password_reuse_manager =
        PasswordReuseManagerFactory::GetForProfile(profile_);
    PrefService* pref_service = profile_->GetPrefs();

    metrics_reporter_ =
        std::make_unique<password_manager::StoreMetricsReporter>(
            profile_store, account_store, sync_service, identity_manager,
            pref_service, password_reuse_manager,
            IsUnderAdvancedProtection(profile_),
            base::BindOnce(
                &StoreMetricReporterHelper::RemoveInstanceFromProfileUserData,
                weak_ptr_factory_.GetWeakPtr()));
  }

  void RemoveInstanceFromProfileUserData() {
    profile_->RemoveUserData(kPasswordStoreMetricsReporterKey);
  }

  const raw_ptr<Profile> profile_;
  // StoreMetricReporterHelper is owned by the profile. `metrics_reporter_` life
  // time is now bound to the profile.
  std::unique_ptr<password_manager::StoreMetricsReporter> metrics_reporter_;
  base::WeakPtrFactory<StoreMetricReporterHelper> weak_ptr_factory_{this};
};

}  // namespace

void DelayReportingPasswordStoreMetrics(Profile* profile) {
  profile->SetUserData(kPasswordStoreMetricsReporterKey,
                       std::make_unique<StoreMetricReporterHelper>(profile));
}
