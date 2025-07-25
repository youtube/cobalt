// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/database_manager_mechanism.h"

#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism.h"

namespace safe_browsing {

DatabaseManagerMechanism::DatabaseManagerMechanism(
    const GURL& url,
    const SBThreatTypeSet& threat_types,
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    MechanismExperimentHashDatabaseCache experiment_cache_selection,
    CheckBrowseUrlType check_type)
    : SafeBrowsingLookupMechanism(url,
                                  threat_types,
                                  database_manager,
                                  experiment_cache_selection),
      check_type_(check_type) {}

DatabaseManagerMechanism::~DatabaseManagerMechanism() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_async_database_manager_check_in_progress_) {
    database_manager_->CancelCheck(this);
  }
}

SafeBrowsingLookupMechanism::StartCheckResult
DatabaseManagerMechanism::StartCheckInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool is_safe_synchronously = database_manager_->CheckBrowseUrl(
      url_, threat_types_, this, experiment_cache_selection_, check_type_);
  if (!is_safe_synchronously) {
    is_async_database_manager_check_in_progress_ = true;
  }
  return StartCheckResult(is_safe_synchronously,
                          /*did_check_url_real_time_allowlist=*/false);
}

void DatabaseManagerMechanism::OnCheckBrowseUrlResult(
    const GURL& url,
    SBThreatType threat_type,
    const ThreatMetadata& metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_async_database_manager_check_in_progress_ = false;
  CompleteCheck(std::make_unique<CompleteCheckResult>(
      url, threat_type, metadata,
      /*threat_source=*/
      database_manager_->GetBrowseUrlThreatSource(check_type_),
      /*url_real_time_lookup_response=*/nullptr,
      /*matched_high_confidence_allowlist=*/absl::nullopt,
      /*locally_cached_results_threat_type=*/absl::nullopt,
      /*real_time_request_failed=*/false));
}

}  // namespace safe_browsing
