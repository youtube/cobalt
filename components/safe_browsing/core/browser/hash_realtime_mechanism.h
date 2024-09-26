// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASH_REALTIME_MECHANISM_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASH_REALTIME_MECHANISM_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/hash_database_mechanism.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_service.h"
#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism.h"
#include "url/gurl.h"

namespace safe_browsing {

// This performs the hash-prefix real-time Safe Browsing check.
class HashRealTimeMechanism : public SafeBrowsingLookupMechanism {
 public:
  HashRealTimeMechanism(
      const GURL& url,
      const SBThreatTypeSet& threat_types,
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      bool can_check_db,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      base::WeakPtr<HashRealTimeService> lookup_service_on_ui,
      MechanismExperimentHashDatabaseCache experiment_cache_selection);
  HashRealTimeMechanism(const HashRealTimeMechanism&) = delete;
  HashRealTimeMechanism& operator=(const HashRealTimeMechanism&) = delete;
  ~HashRealTimeMechanism() override;

  // Returns whether the |url| is eligible for hash-prefix real-time checks.
  // It's never eligible if the |request_destination| is not mainframe.
  static bool CanCheckUrl(
      const GURL& url,
      network::mojom::RequestDestination request_destination);

 private:
  // SafeBrowsingLookupMechanism implementation:
  StartCheckResult StartCheckInternal() override;

  // If |did_match_allowlist| is true, this will fall back to the hash-based
  // check instead of performing the URL lookup.
  void OnCheckUrlForHighConfidenceAllowlist(bool did_match_allowlist);

  // This function has to be static because it is called in UI thread. This
  // function starts a hash-prefix real-time lookup if |lookup_service_on_ui| is
  // available and is not in backoff mode. Otherwise, it hops back to the IO
  // thread and performs the hash-based database check.
  static void StartLookupOnUIThread(
      base::WeakPtr<HashRealTimeMechanism> weak_checker_on_io,
      const GURL& url,
      base::WeakPtr<HashRealTimeService> lookup_service_on_ui,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);

  // Called when the |response| from the real-time lookup service is received.
  // |is_lookup_successful| is true if the response code is OK and the
  // response body is successfully parsed.
  // |threat_type| will not be populated if the lookup was unsuccessful, but
  // will otherwise always be populated with the result of the lookup.
  // |locally_cached_results_threat_type| is the threat type based on locally
  // cached results only. This is only used for logging purposes.
  void OnLookupResponse(bool is_lookup_successful,
                        absl::optional<SBThreatType> threat_type,
                        SBThreatType locally_cached_results_threat_type);

  // Perform the hash-based database check for the url.
  // |real_time_request_failed| specifies whether this was triggered due to the
  // real-time request having failed (e.g. due to backoff, network errors, other
  // service unavailability).
  void PerformHashBasedCheck(const GURL& url, bool real_time_request_failed);

  // The hash-prefix real-time check can sometimes default back to the
  // hash-based database check. In these cases, this function is called once the
  // check has completed, which reports back the final results to the caller.
  // |real_time_request_failed| specifies whether the real-time request failed
  // (e.g. due to backoff, network errors, other service unavailability).
  void OnHashDatabaseCompleteCheckResult(
      bool real_time_request_failed,
      std::unique_ptr<SafeBrowsingLookupMechanism::CompleteCheckResult> result);
  void OnHashDatabaseCompleteCheckResultInternal(SBThreatType threat_type,
                                                 const ThreatMetadata& metadata,
                                                 bool real_time_request_failed);

  SEQUENCE_CHECKER(sequence_checker_);

  // The task runner for the UI thread.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  // This object is used to perform the hash-prefix real-time check. Can only be
  // accessed in UI thread.
  base::WeakPtr<HashRealTimeService> lookup_service_on_ui_;

  // This will be created in cases where the hash-prefix real-time check decides
  // to fall back to the hash-based database checks.
  std::unique_ptr<HashDatabaseMechanism> hash_database_mechanism_ = nullptr;

  base::WeakPtrFactory<HashRealTimeMechanism> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASH_REALTIME_MECHANISM_H_
