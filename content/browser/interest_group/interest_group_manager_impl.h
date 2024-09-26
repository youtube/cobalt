// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_MANAGER_IMPL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_MANAGER_IMPL_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/browser/interest_group/interest_group_k_anonymity_manager.h"
#include "content/browser/interest_group/interest_group_permissions_checker.h"
#include "content/browser/interest_group/interest_group_update.h"
#include "content/browser/interest_group/interest_group_update_manager.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "content/common/content_export.h"
#include "content/public/browser/interest_group_manager.h"
#include "content/public/browser/k_anonymity_service_delegate.h"
#include "content/public/browser/storage_partition.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom-forward.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/ad_auction_service.mojom.h"
#include "url/origin.h"

namespace base {

class FilePath;

}  // namespace base

namespace content {

class InterestGroupStorage;

// InterestGroupManager is a per-StoragePartition class that owns shared
// state needed to run FLEDGE auctions. It lives on the UI thread.
//
// It acts as a proxy to access an InterestGroupStorage, which lives off-thread
// as it performs blocking file IO when backed by on-disk storage.
class CONTENT_EXPORT InterestGroupManagerImpl : public InterestGroupManager {
 public:
  // Controls how auction worklets will be run. kDedicated will use
  // fully-isolated utility processes solely for worklet. kInRenderer will
  // re-use regular renderers following the normal site isolation policy.
  enum class ProcessMode { kDedicated, kInRenderer };

  // Types of even-level reports, for use with EnqueueReports(). Currently only
  // used for histograms. Raw values are not logged directly in histograms, so
  // values do not need to be consistent across versions.
  enum class ReportType {
    kSendReportTo,
    kDebugWin,
    kDebugLoss,
  };

  // Creates an interest group manager using the provided directory path for
  // persistent storage. If `in_memory` is true the path is ignored and only
  // in-memory storage is used.
  explicit InterestGroupManagerImpl(
      const base::FilePath& path,
      bool in_memory,
      ProcessMode process_mode,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      KAnonymityServiceDelegate* k_anonymity_service);
  ~InterestGroupManagerImpl() override;
  InterestGroupManagerImpl(const InterestGroupManagerImpl& other) = delete;
  InterestGroupManagerImpl& operator=(const InterestGroupManagerImpl& other) =
      delete;

  class CONTENT_EXPORT InterestGroupObserver : public base::CheckedObserver {
   public:
    enum AccessType { kJoin, kLeave, kUpdate, kLoaded, kBid, kWin };
    virtual void OnInterestGroupAccessed(const base::Time& access_time,
                                         AccessType type,
                                         const url::Origin& owner_origin,
                                         const std::string& name) = 0;
  };

  // InterestGroupManager overrides:
  void GetAllInterestGroupJoiningOrigins(
      base::OnceCallback<void(std::vector<url::Origin>)> callback) override;

  void GetAllInterestGroupDataKeys(
      base::OnceCallback<void(std::vector<InterestGroupDataKey>)> callback)
      override;

  void RemoveInterestGroupsByDataKey(InterestGroupDataKey data_key,
                                     base::OnceClosure callback) override;

  /******** Proxy function calls to InterestGroupsStorage **********/

  // Checks if `frame_origin` can join the specified InterestGroup, performing
  // .well-known fetches if needed. If joining is allowed, then joins the
  // interest group.
  //
  // `network_isolation_key` must be the NetworkIsolationKey associated with
  // `url_loader_factory`.
  //
  // `url_loader_factory` is the factory for renderer frame where
  // navigator.joinInterestGroup() was invoked, and will be used for the
  // .well-known fetch if one is needed.
  //
  // `report_result_only`, if true, results in calling `callback` with the
  // result of the permissions check, but not actually joining the interest
  // group, regardless of success or failure. This is used to avoid leaking
  // fingerprinting information if the join operation is disallowed by the
  // browser configuration (e.g., 3P cookie blocking).
  //
  // See JoinInterestGroup() for more details on how the join operation is
  // performed.
  void CheckPermissionsAndJoinInterestGroup(
      blink::InterestGroup group,
      const GURL& joining_url,
      const url::Origin& frame_origin,
      const net::NetworkIsolationKey& network_isolation_key,
      bool report_result_only,
      network::mojom::URLLoaderFactory& url_loader_factory,
      blink::mojom::AdAuctionService::JoinInterestGroupCallback callback);

  // Same as CheckPermissionsAndJoinInterestGroup(), except for a leave
  // operation.
  void CheckPermissionsAndLeaveInterestGroup(
      const blink::InterestGroupKey& group_key,
      const url::Origin& main_frame,
      const url::Origin& frame_origin,
      const net::NetworkIsolationKey& network_isolation_key,
      bool report_result_only,
      network::mojom::URLLoaderFactory& url_loader_factory,
      blink::mojom::AdAuctionService::LeaveInterestGroupCallback callback);

  // Joins an interest group. If the interest group does not exist, a new one
  // is created based on the provided group information. If the interest group
  // exists, the existing interest group is overwritten. In either case a join
  // record for this interest group is created.
  void JoinInterestGroup(blink::InterestGroup group, const GURL& joining_url);
  // Remove the interest group if it exists.
  void LeaveInterestGroup(const blink::InterestGroupKey& group_key,
                          const url::Origin& main_frame);
  // Loads all interest groups owned by `owner`, then updates their definitions
  // by fetching their `dailyUpdateUrl`. Interest group updates that fail to
  // load or validate are skipped, but other updates will proceed.
  void UpdateInterestGroupsOfOwner(
      const url::Origin& owner,
      network::mojom::ClientSecurityStatePtr client_security_state);
  // Like UpdateInterestGroupsOfOwner(), but handles multiple interest group
  // owners.
  //
  // The list is shuffled in-place to ensure fairness.
  void UpdateInterestGroupsOfOwners(
      base::span<url::Origin> owners,
      network::mojom::ClientSecurityStatePtr client_security_state);

  // For testing *only*; changes the maximum amount of time that the update
  // process can run before it gets cancelled for taking too long.
  void set_max_update_round_duration_for_testing(base::TimeDelta delta) {
    update_manager_.set_max_update_round_duration_for_testing(
        delta);  // IN-TEST
  }

  // For testing *only*; changes the maximum number of groups that can be
  // updated at the same time.
  void set_max_parallel_updates_for_testing(int max_parallel_updates) {
    update_manager_.set_max_parallel_updates_for_testing(  // IN-TEST
        max_parallel_updates);
  }

  // Adds an entry to the bidding history for this interest group.
  void RecordInterestGroupBids(const blink::InterestGroupSet& groups);
  // Adds an entry to the win history for this interest group. `ad_json` is a
  // piece of opaque data to identify the winning ad.
  void RecordInterestGroupWin(const blink::InterestGroupKey& group_key,
                              const std::string& ad_json);

  // Reports the ad keys to the k-anonymity service. Should be called when
  // FLEDGE selects an ad.
  void RegisterAdKeysAsJoined(base::flat_set<std::string> keys);

  // Gets a single interest group.
  void GetInterestGroup(
      const blink::InterestGroupKey& group_key,
      base::OnceCallback<void(absl::optional<StorageInterestGroup>)> callback);

  // Gets a single interest group.
  void GetInterestGroup(
      const url::Origin& owner,
      const std::string& name,
      base::OnceCallback<void(absl::optional<StorageInterestGroup>)> callback);

  // Gets a list of all interest group owners. Each owner will only appear
  // once.
  void GetAllInterestGroupOwners(
      base::OnceCallback<void(std::vector<url::Origin>)> callback);

  // Gets a list of all interest groups with their bidding information
  // associated with the provided owner.
  void GetInterestGroupsForOwner(
      const url::Origin& owner,
      base::OnceCallback<void(std::vector<StorageInterestGroup>)> callback);

  // Clear out storage for the matching owning storage key. If the matcher is
  // empty then apply to all storage keys.
  void DeleteInterestGroupData(
      StoragePartition::StorageKeyMatcherFunction storage_key_matcher,
      base::OnceClosure completion_callback);

  // Completely delete all interest group data, including k-anonymity data that
  // is not cleared by DeleteInterestGroupData.
  void DeleteAllInterestGroupData(base::OnceClosure completion_callback);

  // Get the last maintenance time from the underlying InterestGroupStorage.
  void GetLastMaintenanceTimeForTesting(
      base::RepeatingCallback<void(base::Time)> callback) const;

  // Enqueues reports for the specified URLs. Virtual for testing.
  virtual void EnqueueReports(
      ReportType report_type,
      std::vector<GURL> report_urls,
      const url::Origin& frame_origin,
      const network::mojom::ClientSecurityState& client_security_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Update the interest group priority.
  void SetInterestGroupPriority(const blink::InterestGroupKey& group,
                                double priority);

  // Merges `update_priority_signals_overrides` with the previous priority
  // signals of `group`.
  void UpdateInterestGroupPriorityOverrides(
      const blink::InterestGroupKey& group_key,
      base::flat_map<std::string,
                     auction_worklet::mojom::PrioritySignalsDoublePtr>
          update_priority_signals_overrides);

  // Clears the InterestGroupPermissionsChecker's cache of the results of
  // .well-known fetches.
  void ClearPermissionsCache();

  AuctionProcessManager& auction_process_manager() {
    return *auction_process_manager_;
  }

  void AddInterestGroupObserver(InterestGroupObserver* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveInterestGroupObserver(InterestGroupObserver* observer) {
    observers_.RemoveObserver(observer);
  }

  // Allows the AuctionProcessManager to be overridden in unit tests, both to
  // allow not creating a new process, and mocking out the Mojo service
  // interface.
  void set_auction_process_manager_for_testing(
      std::unique_ptr<AuctionProcessManager> auction_process_manager) {
    auction_process_manager_ = std::move(auction_process_manager);
  }

  // For testing *only*; changes the maximum number of active report requests
  // at a time.
  void set_max_active_report_requests_for_testing(
      int max_active_report_requests) {
    max_active_report_requests_ = max_active_report_requests;
  }

  // For testing *only*; changes the maximum number of report requests that can
  // be stored in `report_requests_` queue.
  void set_max_report_queue_length_for_testing(int max_queue_length) {
    max_report_queue_length_ = max_queue_length;
  }

  // For testing *only*; changes `max_reporting_round_duration_`.
  void set_max_reporting_round_duration_for_testing(
      base::TimeDelta max_reporting_round_duration) {
    max_reporting_round_duration_ = max_reporting_round_duration;
  }

  // For testing *only*; changes the time interval to wait before sending the
  // next report after sending one.
  void set_reporting_interval_for_testing(base::TimeDelta interval) {
    reporting_interval_ = interval;
  }

  size_t report_queue_length_for_testing() const {
    return report_requests_.size();
  }

  // Handles daily k-anonymity updates for the interest group. Triggers an
  // update request for the k-anonymity of all parts of the interest group
  // (including ads). Also reports membership in the interest group to the
  // k-anonymity of interest-group service.
  void QueueKAnonymityUpdateForInterestGroup(const StorageInterestGroup& group);
  // Records K-anonymity to the database.
  void UpdateKAnonymity(const StorageInterestGroup::KAnonymityData& data);
  // Gets the last time that the key was reported to the k-anonymity server.
  void GetLastKAnonymityReported(
      const std::string& key,
      base::OnceCallback<void(absl::optional<base::Time>)> callback);
  // Updates the last time that the key was reported to the k-anonymity server.
  void UpdateLastKAnonymityReported(const std::string& key);

  InterestGroupPermissionsChecker& permissions_checker_for_testing() {
    return permissions_checker_;
  }

  void set_k_anonymity_manager_for_testing(
      std::unique_ptr<InterestGroupKAnonymityManager> k_anonymity_manager) {
    k_anonymity_manager_ = std::move(k_anonymity_manager);
  }

 private:
  // InterestGroupUpdateManager calls private members to write updates to the
  // database.
  friend class InterestGroupUpdateManager;

  struct ReportRequest {
    ReportRequest();
    ~ReportRequest();

    // Used to fetch the report URL.
    std::unique_ptr<network::SimpleURLLoader> simple_url_loader;
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;

    // Used for Uma histograms. These are build-time constants contained within
    // the binary, so no need for anything to own them.
    const char* name;
    int request_url_size_bytes;
  };

  // Callbacks for CheckPermissionsAndJoinInterestGroup() and
  // CheckPermissionsAndLeaveInterestGroup(), respectively. Call
  // JoinInterestGroup() and LeaveInterestGroup() if the results of the
  // permissions check allows it.
  void OnJoinInterestGroupPermissionsChecked(
      blink::InterestGroup group,
      const GURL& joining_url,
      bool report_result_only,
      blink::mojom::AdAuctionService::JoinInterestGroupCallback callback,
      bool can_join);
  void OnLeaveInterestGroupPermissionsChecked(
      const blink::InterestGroupKey& group_key,
      const url::Origin& main_frame,
      bool report_result_only,
      blink::mojom::AdAuctionService::LeaveInterestGroupCallback callback,
      bool can_leave);

  // Like GetInterestGroupsForOwner(), but doesn't return any interest groups
  // that are currently rate-limited for updates. Additionally, this will update
  // the `next_update_after` field such that a subsequent
  // GetInterestGroupsForUpdate() call with the same `owner` won't return
  // anything until after the success rate limit period passes.
  //
  // `groups_limit` sets a limit on the maximum number of interest groups that
  // may be returned.
  //
  // To be called only by `update_manager_`.
  void GetInterestGroupsForUpdate(
      const url::Origin& owner,
      int groups_limit,
      base::OnceCallback<void(std::vector<StorageInterestGroup>)> callback);

  // Updates the interest group of the same name based on the information in
  // the provided group. This does not update the interest group expiration
  // time or user bidding signals.
  //
  // `callback` is invoked asynchronously on completion, with a bool indicating
  // success or failure.
  //
  // To be called only by `update_manager_`.
  void UpdateInterestGroup(const blink::InterestGroupKey& group_key,
                           InterestGroupUpdate update,
                           base::OnceCallback<void(bool)> callback);

  // Modifies the update rate limits stored in the database, with a longer delay
  // for parse failure.
  //
  // To be called only by `update_manager_`.
  void ReportUpdateFailed(const blink::InterestGroupKey& group_key,
                          bool parse_failure);
  void NotifyInterestGroupAccessed(InterestGroupObserver::AccessType type,
                                   const url::Origin& owner_origin,
                                   const std::string& name);

  void OnGetInterestGroupsComplete(
      base::OnceCallback<void(std::vector<StorageInterestGroup>)> callback,
      std::vector<StorageInterestGroup> groups);

  // Dequeues and sends the first report request in `report_requests_` queue,
  // if the queue is not empty.
  void TrySendingOneReport();

  // Invoked when a report request completed.
  void OnOneReportSent(
      std::unique_ptr<network::SimpleURLLoader> simple_url_loader,
      scoped_refptr<net::HttpResponseHeaders> response_headers);

  // Clears `report_requests_`.  Does not abort currently pending requests.
  void TimeoutReports();

  // A version of QueueKAnonymityUpdateForInterestGroup() called from
  // JoinInterestGroup which passes the group in an optional (the group must
  // always exist). Called from JoinInterestGroup.
  void QueueKAnonymityUpdateForInterestGroupFromJoinInterestGroup(
      absl::optional<StorageInterestGroup> maybe_group);

  // Owns and manages access to the InterestGroupStorage living on a different
  // thread.
  base::SequenceBound<InterestGroupStorage> impl_;

  // Stored as pointer so that tests can override it.
  std::unique_ptr<AuctionProcessManager> auction_process_manager_;

  base::ObserverList<InterestGroupObserver> observers_;

  // Manages the logic required to support UpdateInterestGroupsOfOwner().
  //
  // InterestGroupUpdateManager keeps a pointer to this InterestGroupManagerImpl
  // to make database writes via calls to GetInterestGroupsForUpdate(),
  // UpdateInterestGroup(), and ReportUpdateFailed().
  //
  // Therefore, `update_manager_` *must* be declared after fields used by those
  // methods so that updates are cancelled before those fields are destroyed.
  InterestGroupUpdateManager update_manager_;

  // Manages the logic required to support k-anonymity updates.
  //
  // InterestGroupKAnonymityManager keeps a pointer to this
  // InterestGroupManagerImpl to make database reads and writes....
  //
  // Therefore, `k_anonymity_manager_` *must* be declared after fields used by
  // those methods so that k-anonymity operations are cancelled before those
  // fields are destroyed.
  // Stored as pointer so that tests can override it.
  std::unique_ptr<InterestGroupKAnonymityManager> k_anonymity_manager_;

  // Checks if a frame can join or leave an interest group. Global so that
  // pending operations can continue after a page has been navigate away from.
  InterestGroupPermissionsChecker permissions_checker_;

  // The queue of report requests. Empty the queue if it's size is larger than
  // `max_report_queue_length` at the time of adding new entries.
  base::circular_deque<std::unique_ptr<ReportRequest>> report_requests_;

  // Current number of active report requests. Includes requests that completed
  // within the last `kReportingInterval`, each of which should have a pending
  // delayed task to invoke TrySendingOneReport().
  int num_active_ = 0;

  // The maximum number of active report requests at a time.
  //
  // Should *only* be changed by tests.
  int max_active_report_requests_;

  // The maximum number of report requests that can be stored in queue
  // `report_requests_`.
  //
  // Should *only* be changed by tests.
  int max_report_queue_length_;

  // The time interval to wait before sending the next report request after
  // sending one.
  //
  // Should *only* be changed by tests.
  base::TimeDelta reporting_interval_;

  // The maximum amount of time that the reporting process can run before the
  // report queue is cleared due to taking too long.
  //
  // Should *only* be changed by tests.
  base::TimeDelta max_reporting_round_duration_;

  // Used to clear all pending reports in the queue if reporting takes too long.
  // Started when sending reports starts. Stopped once all reports are sent.
  // When the timer triggers, all reports are aborted.
  //
  // The resulting behavior is that if reports are continuously being sent for
  // too long, possibly from multiple auctions, all reports are timed out.
  base::OneShotTimer timeout_timer_;

  base::WeakPtrFactory<InterestGroupManagerImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_MANAGER_IMPL_H_
