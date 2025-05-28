// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/server_certificate_database/server_certificate_database_service.h"

#include <string>
#include <string_view>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/metrics/histogram_functions.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/server_certificate_database/server_certificate_database_nss_migrator.h"
#endif

namespace net {

#if BUILDFLAG(IS_CHROMEOS)
ServerCertificateDatabaseService::ServerCertificateDatabaseService(
    base::FilePath profile_path,
    PrefService* prefs,
    ServerCertificateDatabaseNSSMigrator::NssSlotGetter nss_slot_getter)
    : profile_path_(std::move(profile_path)),
      prefs_(prefs),
      nss_slot_getter_(std::move(nss_slot_getter))
#else
ServerCertificateDatabaseService::ServerCertificateDatabaseService(
    base::FilePath profile_path)
    : profile_path_(std::move(profile_path))
#endif
{
  server_cert_database_ = base::SequenceBound<net::ServerCertificateDatabase>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
      profile_path_);
}

ServerCertificateDatabaseService::~ServerCertificateDatabaseService() = default;

void ServerCertificateDatabaseService::AddOrUpdateUserCertificates(
    std::vector<net::ServerCertificateDatabase::CertInformation> cert_infos,
    base::OnceCallback<void(bool)> callback) {
  server_cert_database_
      .AsyncCall(&net::ServerCertificateDatabase::InsertOrUpdateCerts)
      .WithArgs(std::move(cert_infos))
      .Then(base::BindOnce(
          &ServerCertificateDatabaseService::HandleModificationResult,
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ServerCertificateDatabaseService::GetAllCertificates(
    base::OnceCallback<
        void(std::vector<net::ServerCertificateDatabase::CertInformation>)>
        callback) {
#if BUILDFLAG(IS_CHROMEOS)
  // Migrate certificates from NSS and then read all certificates from the
  // database. Migration will only be done once per profile. If called multiple
  // times before migration completes, all the callbacks will be queued and
  // processed once the migration is done.
  // TODO(crbug.com/390333881): Remove the migration code once sufficient time
  // has passed after the feature is launched.
  if (prefs_->GetInteger(prefs::kNSSCertsMigratedToServerCertDb) ==
      static_cast<int>(NSSMigrationResultPref::kNotMigrated)) {
    if (!nss_migrator_) {
      DVLOG(1) << "starting migration for profile "
               << profile_path_.AsUTF8Unsafe();
      nss_migrator_ = std::make_unique<ServerCertificateDatabaseNSSMigrator>(
          this, std::move(nss_slot_getter_));
      // Unretained is safe as ServerCertificateDatabaseNSSMigrator will not
      // run the callback after it is deleted.
      nss_migrator_->MigrateCerts(base::BindOnce(
          &ServerCertificateDatabaseService::NSSMigrationComplete,
          base::Unretained(this)));
    }
    DVLOG(1) << "queuing migration request";
    get_certificates_pending_migration_.push_back(std::move(callback));
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  server_cert_database_
      .AsyncCall(&net::ServerCertificateDatabase::RetrieveAllCertificates)
      .Then(std::move(callback));
}

#if BUILDFLAG(IS_CHROMEOS)
void ServerCertificateDatabaseService::NSSMigrationComplete(
    ServerCertificateDatabaseNSSMigrator::MigrationResult result) {
  DVLOG(1) << "Migration for " << profile_path_.AsUTF8Unsafe()
           << " finished: nss cert count=" << result.cert_count
           << " errors=" << result.error_count;
  NSSMigrationResultHistogram result_for_histogram;
  if (result.cert_count == 0) {
    result_for_histogram = NSSMigrationResultHistogram::kNssDbEmpty;
  } else if (result.error_count == 0) {
    result_for_histogram = NSSMigrationResultHistogram::kSuccess;
  } else if (result.error_count < result.cert_count) {
    result_for_histogram = NSSMigrationResultHistogram::kPartialSuccess;
  } else {
    result_for_histogram = NSSMigrationResultHistogram::kFailed;
  }
  base::UmaHistogramEnumeration("Net.CertVerifier.NSSCertMigrationResult",
                                result_for_histogram);
  base::UmaHistogramCounts100(
      "Net.CertVerifier.NSSCertMigrationQueuedRequestsWhenFinished",
      get_certificates_pending_migration_.size());

  prefs_->SetInteger(
      prefs::kNSSCertsMigratedToServerCertDb,
      static_cast<int>((result.error_count == 0)
                           ? NSSMigrationResultPref::kMigratedSuccessfully
                           : NSSMigrationResultPref::kMigrationHadErrors));
  for (GetCertificatesCallback& callback :
       get_certificates_pending_migration_) {
    // TODO(https://crbug.com/40928765): kinda silly to start multiple
    // simultaneous reads here, but dunno if it actually occurs enough to be
    // worth optimizing. Evaluate the histograms to see if this seems worth
    // addressing.
    GetAllCertificates(std::move(callback));
  }
  get_certificates_pending_migration_.clear();
  nss_migrator_.reset();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void ServerCertificateDatabaseService::PostTaskWithDatabase(
    base::OnceCallback<void(net::ServerCertificateDatabase*)> callback) {
  server_cert_database_.PostTaskWithThisObject(std::move(callback));
}

void ServerCertificateDatabaseService::GetCertificatesCount(
    base::OnceCallback<void(uint32_t)> callback) {
  server_cert_database_
      .AsyncCall(&net::ServerCertificateDatabase::RetrieveCertificatesCount)
      .Then(std::move(callback));
}

void ServerCertificateDatabaseService::DeleteCertificate(
    const std::string& sha256hash_hex,
    base::OnceCallback<void(bool)> callback) {
  server_cert_database_
      .AsyncCall(&net::ServerCertificateDatabase::DeleteCertificate)
      .WithArgs(sha256hash_hex)
      .Then(base::BindOnce(
          &ServerCertificateDatabaseService::HandleModificationResult,
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

base::CallbackListSubscription ServerCertificateDatabaseService::AddObserver(
    base::RepeatingClosure callback) {
  return observers_.Add(std::move(callback));
}

void ServerCertificateDatabaseService::HandleModificationResult(
    base::OnceCallback<void(bool)> callback,
    bool success) {
  std::move(callback).Run(success);
  if (success) {
    observers_.Notify();
  }
}

#if BUILDFLAG(IS_CHROMEOS)
void ServerCertificateDatabaseService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kNSSCertsMigratedToServerCertDb,
      static_cast<int>(net::ServerCertificateDatabaseService::
                           NSSMigrationResultPref::kNotMigrated));
}
#endif

}  // namespace net
