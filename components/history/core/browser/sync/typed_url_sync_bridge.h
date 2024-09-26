// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_SYNC_TYPED_URL_SYNC_BRIDGE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_SYNC_TYPED_URL_SYNC_BRIDGE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_backend_observer.h"
#include "components/history/core/browser/sync/typed_url_sync_metadata_database.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/model/sync_error.h"

namespace history {

class TypedURLSyncBridge : public syncer::ModelTypeSyncBridge,
                           public HistoryBackendObserver {
 public:
  // `sync_metadata_store` is owned by `history_backend`, and must outlive
  // TypedURLSyncBridge.
  TypedURLSyncBridge(
      HistoryBackend* history_backend,
      TypedURLSyncMetadataDatabase* sync_metadata_store,
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor);

  TypedURLSyncBridge(const TypedURLSyncBridge&) = delete;
  TypedURLSyncBridge& operator=(const TypedURLSyncBridge&) = delete;

  ~TypedURLSyncBridge() override;

  // syncer::ModelTypeSyncBridge implementation.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  absl::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  absl::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  bool SupportsGetStorageKey() const override;

  // HistoryBackendObserver:
  void OnURLVisited(HistoryBackend* history_backend,
                    const URLRow& row,
                    const VisitRow& visit_row) override;
  void OnURLsModified(HistoryBackend* history_backend,
                      const std::vector<URLRow>& changed_urls,
                      bool is_from_expiration) override;
  void OnURLsDeleted(HistoryBackend* history_backend,
                     bool all_history,
                     bool expired,
                     const std::vector<URLRow>& deleted_rows,
                     const std::set<GURL>& favicon_urls) override;
  void OnVisitUpdated(const VisitRow& visit) override;
  void OnVisitDeleted(const VisitRow& visit) override;

  // Must be called after creation and before any operations.
  void Init();

  // Called by HistoryBackend when database error is reported through
  // DatabaseErrorCallback.
  void OnDatabaseError();

  // Return true if this function successfully converts the passed URL
  // information to a TypedUrlSpecifics structure for writing to the sync DB.
  [[nodiscard]] static bool WriteToTypedUrlSpecifics(
      const URLRow& url,
      const std::vector<VisitRow>& visits,
      sync_pb::TypedUrlSpecifics* specifics);

 private:
  friend class TypedURLSyncBridgeTest;

  struct URLWithVisits {
    URLWithVisits(const GURL& url, const std::vector<VisitInfo>& visits);
    ~URLWithVisits();

    URLWithVisits(URLWithVisits&&);

    GURL url;
    std::vector<VisitInfo> visits;
  };

  // Bitfield returned from MergeUrls to specify the result of a merge.
  typedef uint32_t MergeResult;
  static const MergeResult DIFF_NONE = 0;
  static const MergeResult DIFF_UPDATE_NODE = 1 << 0;
  static const MergeResult DIFF_LOCAL_ROW_CHANGED = 1 << 1;
  static const MergeResult DIFF_LOCAL_VISITS_ADDED = 1 << 2;

  // Merges the URL information in `typed_url` with the URL information from the
  // history database in `url` and `visits`, and returns a bitmask with the
  // results of the merge:
  // DIFF_UPDATE_NODE - changes have been made to `new_url` and `visits` which
  //   should be persisted to the sync node.
  // DIFF_LOCAL_ROW_CHANGED - The history data in `new_url` should be persisted
  //   to the history DB.
  // DIFF_LOCAL_VISITS_ADDED - `new_visits` contains a list of visits that
  //   should be written to the history DB for this URL. Deletions are not
  //   written to the DB - each client is left to age out visits on their own.
  static MergeResult MergeUrls(const sync_pb::TypedUrlSpecifics& typed_url,
                               const URLRow& url,
                               std::vector<VisitRow>* visits,
                               URLRow* new_url,
                               std::vector<VisitInfo>* new_visits);

  // Diffs the set of visits between the history DB and the sync DB, using the
  // sync DB as the canonical copy. Result is the set of `new_visits` and
  // `removed_visits` that can be applied to the history DB to make it match
  // the sync DB version. `removed_visits` can be null if the caller does not
  // care about which visits to remove.
  static void DiffVisits(const std::vector<VisitRow>& history_visits,
                         const sync_pb::TypedUrlSpecifics& sync_specifics,
                         std::vector<VisitInfo>* new_visits,
                         std::vector<VisitRow>* removed_visits);

  // Fills `new_url` with formatted data from `typed_url`.
  static void UpdateURLRowFromTypedUrlSpecifics(
      const sync_pb::TypedUrlSpecifics& typed_url,
      URLRow* new_url);

  // Synchronously load sync metadata from the TypedURLSyncMetadataDatabase and
  // pass it to the processor so that it can start tracking changes.
  void LoadMetadata();

  // Compares `server_typed_url` from the server against local history to decide
  // how to merge any existing data, and updates appropriate data containers to
  // write to server and backend.
  void MergeURLWithSync(const sync_pb::TypedUrlSpecifics& server_typed_url,
                        std::map<GURL, URLRow>* local_typed_urls,
                        std::map<GURL, std::vector<VisitRow>>* visit_vectors,
                        std::vector<URLRow>* new_synced_urls,
                        std::vector<URLWithVisits>* new_synced_visits,
                        std::vector<URLRow>* updated_synced_urls);

  // Given a typed URL in the sync DB, looks for an existing entry in the
  // local history DB and generates a list of visits to add to the
  // history DB to bring it up to date (avoiding duplicates).
  // Updates the passed `visits_to_add` and `visits_to_remove` vectors with the
  // visits to add to/remove from the history DB, and adds a new entry to either
  // `updated_urls` or `new_urls` depending on whether the URL already existed
  // in the history DB.
  void UpdateFromSync(const sync_pb::TypedUrlSpecifics& typed_url,
                      std::vector<URLWithVisits>* visits_to_add,
                      std::vector<VisitRow>* visits_to_remove,
                      std::vector<URLRow>* updated_urls,
                      std::vector<URLRow>* new_urls);

  // Utility routine that (a) updates an existing sync node or (b) creates a
  // new one for the passed `typed_url` if one does not already exist or (c)
  // removes metadata for `row` if `is_from_expiration` is true and the `row`
  // has no more typed visits.
  void UpdateSyncFromLocal(URLRow row,
                           bool is_from_expiration,
                           syncer::MetadataChangeList* metadata_change_list);

  // Deletes metadata for an expired URL `row` but does not send up the deletion
  // to the server (each client expires them independently). It is an no-op when
  // called on an url with already expired metadata.
  void ExpireMetadataForURL(const URLRow& row);

  // Writes new typed url data from sync server to history backend.
  absl::optional<syncer::ModelError> WriteToHistoryBackend(
      const std::vector<URLRow>* new_urls,
      const std::vector<URLRow>* updated_urls,
      const std::vector<GURL>* deleted_urls,
      const std::vector<URLWithVisits>* new_visits,
      const std::vector<VisitRow>* deleted_visits);

  // Given a TypedUrlSpecifics object, removes all visits that are older than
  // the current expiration time. Note that this can result in having no visits
  // at all.
  sync_pb::TypedUrlSpecifics FilterExpiredVisits(
      const sync_pb::TypedUrlSpecifics& specifics);

  // Helper function that determines if we should ignore a URL for the purposes
  // of sync, because it contains invalid data.
  bool ShouldIgnoreUrl(const GURL& url);

  // Helper function that determines if we should ignore a URL for the purposes
  // of sync, based on the visits the URL had.
  bool ShouldIgnoreVisits(const std::vector<VisitRow>& visits);

  // Returns true if the caller should sync as a result of the passed visit
  // notification. We use this to throttle the number of sync changes we send
  // to the server so we don't hit the server for every
  // single typed URL visit.
  bool ShouldSyncVisit(int typed_count, ui::PageTransition transition);

  // Fetches visits from the history DB corresponding to the passed URL. This
  // function compensates for the fact that the history DB has rather poor data
  // integrity (duplicate visits, visit timestamps that don't match the
  // last_visit timestamp, huge data sets that exhaust memory when fetched,
  // expired visits that are not deleted by `ExpireHistoryBackend`, etc) by
  // modifying the passed `url` object and `visits` vector. The order of
  // `visits` will be from the oldest to the newest order.
  // Returns false if we could not fetch the visits for the passed URL, DB
  // error. If there are no visits for the passed url, or all the visits are
  // expired, it returns true but `visits` is empty.
  bool FixupURLAndGetVisits(URLRow* url, std::vector<VisitRow>* visits);

  // Create an EntityData by URL `row` and its visits `visits`.
  std::unique_ptr<syncer::EntityData> CreateEntityData(
      const URLRow& row,
      const std::vector<VisitRow>& visits);

  // Get all the typed urls and visits from the history db, after filtering
  // them, put them into `url_to_visit` and `url_to_urlrow`.
  // Return false if cannot get urls from HistoryBackend.
  bool GetValidURLsAndVisits(
      std::map<GURL, std::vector<VisitRow>>* url_to_visit,
      std::map<GURL, URLRow>* url_to_urlrow);

  // Get URLID from HistoryBackend, and return URLID as storage key.
  std::string GetStorageKeyInternal(const std::string& url);

  // Send local typed url to processor().
  void SendTypedURLToProcessor(
      const URLRow& row,
      const std::vector<VisitRow>& visits,
      syncer::MetadataChangeList* metadata_change_list);

  // A non-owning pointer to the backend, which we're syncing local changes from
  // and sync changes to.
  const raw_ptr<HistoryBackend, DanglingUntriaged> history_backend_;

  // Whether we're currently processing changes from the syncer. While this is
  // true, we ignore any local url changes, since we triggered them.
  bool processing_syncer_changes_ = false;

  // A non-owning pointer to the database, which is for storing typed urls sync
  // metadata and state.
  raw_ptr<TypedURLSyncMetadataDatabase, DanglingUntriaged>
      sync_metadata_database_;

  // Since HistoryBackend use SequencedTaskRunner, so should use SequenceChecker
  // here.
  SEQUENCE_CHECKER(sequence_checker_);

  // Tracks observed history backend, for receiving updates from history
  // backend.
  base::ScopedObservation<HistoryBackend, HistoryBackendObserver>
      history_backend_observation_{this};
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_SYNC_TYPED_URL_SYNC_BRIDGE_H_
