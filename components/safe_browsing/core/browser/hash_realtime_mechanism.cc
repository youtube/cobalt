// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/hash_realtime_mechanism.h"

#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism.h"
#include "components/safe_browsing/core/common/utils.h"

namespace safe_browsing {

HashRealTimeMechanism::HashRealTimeMechanism(
    const GURL& url,
    const SBThreatTypeSet& threat_types,
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    bool can_check_db,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    base::WeakPtr<HashRealTimeService> lookup_service_on_ui,
    MechanismExperimentHashDatabaseCache experiment_cache_selection)
    : SafeBrowsingLookupMechanism(url,
                                  threat_types,
                                  database_manager,
                                  can_check_db,
                                  experiment_cache_selection),
      ui_task_runner_(ui_task_runner),
      lookup_service_on_ui_(lookup_service_on_ui) {}

HashRealTimeMechanism::~HashRealTimeMechanism() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
bool HashRealTimeMechanism::CanCheckUrl(
    const GURL& url,
    network::mojom::RequestDestination request_destination) {
  return request_destination == network::mojom::RequestDestination::kDocument &&
         CanGetReputationOfUrl(url);
}

SafeBrowsingLookupMechanism::StartCheckResult
HashRealTimeMechanism::StartCheckInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!can_check_db_) {
    return StartCheckResult(
        /*is_safe_synchronously=*/true,
        /*did_check_url_real_time_allowlist=*/false,
        /*matched_high_confidence_allowlist=*/false);
  }

  bool has_allowlist_match =
      database_manager_->CheckUrlForHighConfidenceAllowlist(url_, "HPRT");
  base::UmaHistogramEnumeration(
      "SafeBrowsing.HPRT.LocalMatch.Result",
      has_allowlist_match ? AsyncMatch::MATCH : AsyncMatch::NO_MATCH);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &HashRealTimeMechanism::OnCheckUrlForHighConfidenceAllowlist,
          weak_factory_.GetWeakPtr(),
          /*did_match_allowlist=*/has_allowlist_match));

  return StartCheckResult(
      /*is_safe_synchronously=*/false,
      /*did_check_url_real_time_allowlist=*/false,
      /*matched_high_confidence_allowlist=*/has_allowlist_match);
}

void HashRealTimeMechanism::OnCheckUrlForHighConfidenceAllowlist(
    bool did_match_allowlist) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (did_match_allowlist) {
    // If the URL matches the high-confidence allowlist, still do the hash based
    // checks.
    PerformHashBasedCheck(url_, /*real_time_request_failed=*/false);
  } else {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&HashRealTimeMechanism::StartLookupOnUIThread,
                       weak_factory_.GetWeakPtr(), url_, lookup_service_on_ui_,
                       base::SequencedTaskRunner::GetCurrentDefault()));
  }
}

// static
void HashRealTimeMechanism::StartLookupOnUIThread(
    base::WeakPtr<HashRealTimeMechanism> weak_ptr_on_io,
    const GURL& url,
    base::WeakPtr<HashRealTimeService> lookup_service_on_ui,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
  auto is_lookup_service_found = !!lookup_service_on_ui;
  base::UmaHistogramBoolean("SafeBrowsing.HPRT.IsLookupServiceFound",
                            is_lookup_service_found);
  if (!is_lookup_service_found) {
    io_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&HashRealTimeMechanism::PerformHashBasedCheck,
                       weak_ptr_on_io, url, /*real_time_request_failed=*/true));
    return;
  }

  HPRTLookupResponseCallback response_callback =
      base::BindOnce(&HashRealTimeMechanism::OnLookupResponse, weak_ptr_on_io);

  lookup_service_on_ui->StartLookup(url, std::move(response_callback),
                                    std::move(io_task_runner));
}

void HashRealTimeMechanism::OnLookupResponse(
    bool is_lookup_successful,
    absl::optional<SBThreatType> threat_type,
    SBThreatType locally_cached_results_threat_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_lookup_successful) {
    PerformHashBasedCheck(url_, /*real_time_request_failed=*/true);
    return;
  }
  DCHECK(threat_type.has_value());
  CompleteCheck(std::make_unique<CompleteCheckResult>(
      url_, threat_type.value(), ThreatMetadata(),
      /*is_from_url_real_time_check=*/false,
      /*url_real_time_lookup_response=*/nullptr,
      /*locally_cached_results_threat_type=*/locally_cached_results_threat_type,
      /*real_time_request_failed=*/!is_lookup_successful));
}

void HashRealTimeMechanism::PerformHashBasedCheck(
    const GURL& url,
    bool real_time_request_failed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  hash_database_mechanism_ = std::make_unique<HashDatabaseMechanism>(
      url, threat_types_, database_manager_, can_check_db_,
      experiment_cache_selection_);
  auto result = hash_database_mechanism_->StartCheck(
      base::BindOnce(&HashRealTimeMechanism::OnHashDatabaseCompleteCheckResult,
                     weak_factory_.GetWeakPtr(), real_time_request_failed));
  if (result.is_safe_synchronously) {
    // No match found in the database, so conclude this is safe.
    OnHashDatabaseCompleteCheckResultInternal(
        SB_THREAT_TYPE_SAFE, ThreatMetadata(), real_time_request_failed);
  }
}

void HashRealTimeMechanism::OnHashDatabaseCompleteCheckResult(
    bool real_time_request_failed,
    std::unique_ptr<SafeBrowsingLookupMechanism::CompleteCheckResult> result) {
  DCHECK(!result->real_time_request_failed);
  OnHashDatabaseCompleteCheckResultInternal(
      result->threat_type, result->metadata, real_time_request_failed);
}

void HashRealTimeMechanism::OnHashDatabaseCompleteCheckResultInternal(
    SBThreatType threat_type,
    const ThreatMetadata& metadata,
    bool real_time_request_failed) {
  CompleteCheck(std::make_unique<CompleteCheckResult>(
      url_, threat_type, metadata,
      /*is_from_url_real_time_check=*/false,
      /*url_real_time_lookup_response=*/nullptr,
      /*locally_cached_results_threat_type=*/absl::nullopt,
      /*real_time_request_failed=*/real_time_request_failed));
}

}  // namespace safe_browsing
