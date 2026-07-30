// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_record_store.h"

#import <tuple>
#import <utility>

#import "base/files/file_util.h"
#import "ios/chrome/browser/download/model/download_record_database.h"

namespace {

// Determines whether a download record update should be persisted to the
// database by comparing critical fields between the new and cached
// records. Incognito records are never persisted; non-incognito records
// only need a DB write when a non-progress field actually changed.
bool ShouldPersistUpdate(const DownloadRecord& new_record,
                         const DownloadRecord& cached_record) {
  // Incognito records are not persistently stored.
  if (cached_record.is_incognito) {
    return false;
  }

  // Persist only if critical fields have changed.
  // Progress fields (received_bytes, progress_percent) are not persisted to
  // database.
  return !new_record.EqualsExcludingProgress(cached_record);
}

}  // namespace

DownloadRecordStore::DownloadRecordStore(bool pagination_enabled)
    : pagination_enabled_(pagination_enabled) {
  // `base::SequenceBound` constructs the store on the database sequence,
  // but the SEQUENCE_CHECKER is unconditionally detached at construction
  // so it rebinds on the first method call rather than on whichever
  // sequence happened to construct the object.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DownloadRecordStore::~DownloadRecordStore() {
  // `base::SequenceBound` posts the destructor onto the bound database
  // sequence, so it always runs on the same sequence as every other
  // method.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DownloadRecordStore::InitializeDatabase(
    const base::FilePath& profile_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::FilePath downloads_dir =
      profile_path.Append(FILE_PATH_LITERAL("download_record_db"));
  base::FilePath db_path =
      downloads_dir.Append(FILE_PATH_LITERAL("DownloadRecord"));

  if (!base::CreateDirectory(downloads_dir)) {
    return;
  }

  auto database = std::make_unique<DownloadRecordDatabase>(db_path);
  sql::InitStatus init_status = database->Init();

  if (init_status == sql::INIT_OK) {
    database_ = std::move(database);
  }
}

void DownloadRecordStore::LoadHistoricalRecords() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsDatabaseReady()) {
    return;
  }

  record_cache_.clear();

  std::vector<DownloadRecord> all_records = database_->GetAllDownloadRecords();
  for (const auto& record : all_records) {
    record_cache_[record.download_id] = record;
  }

  // Mark any record stuck in kInProgress / kNotStarted as kFailed. This
  // mirrors the original CleanupInconsistentStates legacy helper.
  std::vector<std::string> records_to_fix;
  for (const auto& [id, record] : record_cache_) {
    if (record.state == web::DownloadTask::State::kInProgress ||
        record.state == web::DownloadTask::State::kNotStarted) {
      records_to_fix.push_back(id);
    }
  }

  if (records_to_fix.empty()) {
    return;
  }

  UpdateRecordsState(records_to_fix, web::DownloadTask::State::kFailed);
}

void DownloadRecordStore::MarkUnfinishedDownloadsAsFailed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsDatabaseReady()) {
    return;
  }

  // Single SQL UPDATE — no full-table load. Any record found in
  // kInProgress / kNotStarted at startup is treated as interrupted by
  // app termination and flipped to kFailed.
  //
  // Return value is intentionally discarded: a transient SQL failure
  // here is self-healing (the next OnDownload* update for the row, or
  // the next session's identical UPDATE, will repair the state) and no
  // caller depends on its success.
  std::ignore = database_->MarkUnfinishedDownloadsAsFailed();

  // `record_cache_` is intentionally not populated here. It is only
  // populated by `LoadHistoricalRecords` on the flag-OFF path; on the
  // flag-ON path it stays empty by design. This keeps service
  // construction O(1) instead of O(N) on the persisted-row count.
}

bool DownloadRecordStore::InsertRecord(const DownloadRecord& record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (record.is_incognito) {
    // Incognito records are never persisted; live only in memory.
    if (pagination_enabled_) {
      incognito_records_[record.download_id] = record;
    } else {
      record_cache_[record.download_id] = record;
    }
    return true;
  }

  if (!IsDatabaseReady()) {
    return false;
  }

  if (!database_->InsertDownloadRecord(record)) {
    return false;
  }

  if (pagination_enabled_) {
    WriteThroughToActiveCache(record);
  } else {
    record_cache_[record.download_id] = record;
  }
  return true;
}

std::optional<DownloadRecord> DownloadRecordStore::UpdateRecord(
    const DownloadRecord& updated_record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto existing_record_opt = GetById(updated_record.download_id);
  if (!existing_record_opt.has_value()) {
    return std::nullopt;
  }

  // Preserve created_time and is_incognito from the existing record so a
  // stale-in-memory updated_record cannot drop those invariants.
  DownloadRecord merged_record = updated_record;
  merged_record.created_time = existing_record_opt->created_time;
  merged_record.is_incognito = existing_record_opt->is_incognito;

  // Determine if we need to persist this update to database.
  const bool should_persist =
      ShouldPersistUpdate(merged_record, *existing_record_opt);

  if (!should_persist) {
    // Volatile-only change (e.g. received_bytes / progress_percent without
    // a state transition): refresh the in-memory hot copy so subsequent
    // reads see the freshest value, but skip the DB write.
    if (pagination_enabled_) {
      WriteThroughToActiveCache(merged_record);
    } else {
      record_cache_[merged_record.download_id] = merged_record;
    }
    return merged_record;
  }

  if (!IsDatabaseReady()) {
    return std::nullopt;
  }

  if (!database_->UpdateDownloadRecord(merged_record)) {
    return std::nullopt;
  }

  if (pagination_enabled_) {
    WriteThroughToActiveCache(merged_record);
  } else {
    record_cache_[merged_record.download_id] = merged_record;
  }
  return merged_record;
}

bool DownloadRecordStore::UpdateRecordsState(
    const std::vector<std::string>& download_ids,
    web::DownloadTask::State new_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Legacy flag-OFF helper. Under the flag-ON path the active records
  // cache is the source of truth for in-flight state; any future caller
  // must explicitly decide whether to write-through it, so force such a
  // caller to think about that here rather than silently miss the cache.
  // TODO(crbug.com/524790428): Remove once the pagination flag has
  // shipped to stable and is retired.
  DCHECK(!pagination_enabled_);

  if (download_ids.empty()) {
    // Empty list is considered successful.
    return true;
  }

  if (!IsDatabaseReady()) {
    return false;
  }

  if (database_->UpdateDownloadRecordsState(download_ids, new_state)) {
    // Updates cache for all successfully updated records.
    for (const std::string& download_id : download_ids) {
      auto it = record_cache_.find(download_id);
      if (it != record_cache_.end()) {
        it->second.state = new_state;
      }
    }
    return true;
  }

  return false;
}

std::optional<DownloadRecord> DownloadRecordStore::UpdateFilePathInRecord(
    const std::string& download_id,
    const base::FilePath& file_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto existing_record_opt = GetById(download_id);
  if (!existing_record_opt.has_value()) {
    return std::nullopt;
  }

  // Create updated record with new file path.
  DownloadRecord updated_record = existing_record_opt.value();
  updated_record.file_path = file_path;

  return UpdateRecord(updated_record);
}

bool DownloadRecordStore::DeleteRecord(std::string_view id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!pagination_enabled_) {
    // Legacy flag-OFF path: record_cache_ is the single source of truth
    // for both the existence probe and the incognito test. Heterogeneous
    // lookup via `std::less<>` avoids a string copy on cache miss.
    auto it = record_cache_.find(id);
    if (it == record_cache_.end()) {
      // Consider this a success since the record doesn't exist anyway.
      return true;
    }

    const DownloadRecord& record = it->second;
    bool should_persist = !record.is_incognito;

    if (!should_persist) {
      record_cache_.erase(it);
      return true;
    }

    if (!IsDatabaseReady()) {
      return false;
    }

    if (database_->DeleteDownloadRecord(std::string(id))) {
      record_cache_.erase(it);
      return true;
    }

    return false;
  }

  // Flag-ON path: avoid the synchronous DB SELECT that `GetById` would
  // otherwise perform on a cache miss. Incognito rows never touch the
  // DB and live only in `incognito_records_`, so probing that hot map
  // first is enough to short-circuit the incognito branch. We rely on
  // `base::flat_map::erase` accepting a heterogeneous `std::string_view`
  // key and returning the number of removed elements: a return of 1
  // means the row was an incognito-only entry and we're done. The two
  // maps have disjoint keys by construction (incognito records go only
  // to `incognito_records_`, persisted records go only to
  // `active_records_cache_`), so a successful incognito erase rules
  // out a DB row for the same id.
  if (incognito_records_.erase(id) == 1) {
    return true;
  }

  if (!IsDatabaseReady()) {
    return false;
  }

  // Persisted path. The DB DELETE is idempotent — it returns true with
  // zero rows affected if `id` was never persisted — so an "already
  // gone" non-incognito id (e.g. the row was already deleted in a prior
  // call) is naturally handled here without a separate existence probe.
  if (!database_->DeleteDownloadRecord(std::string(id))) {
    // DB delete failed: do NOT evict the active cache entry. The DB row
    // is still present, so the cache must stay in sync with it.
    return false;
  }

  // DB delete succeeded. The erase is idempotent: a concurrent
  // `EvictOnDestroy` posted by `OnDownloadDestroyed` may have already
  // evicted this id.
  active_records_cache_.erase(id);
  return true;
}

std::vector<DownloadRecord> DownloadRecordStore::GetAllFromCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (pagination_enabled_) {
    // Flag-ON in-memory layer is hot-only. A snapshot covers
    // `active_records_cache_` + `incognito_records_`; full history is served
    // by `GetDownloadsPage`.
    std::vector<DownloadRecord> records;
    records.reserve(active_records_cache_.size() + incognito_records_.size());
    for (const auto& [id, record] : active_records_cache_) {
      records.push_back(record);
    }
    for (const auto& [id, record] : incognito_records_) {
      records.push_back(record);
    }
    return records;
  }

  std::vector<DownloadRecord> records;
  records.reserve(record_cache_.size());

  for (const auto& [id, record] : record_cache_) {
    records.push_back(record);
  }

  return records;
}

std::optional<DownloadRecord> DownloadRecordStore::GetById(
    std::string_view download_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (pagination_enabled_) {
    // Probe the hot caches first (active records, then incognito), then
    // fall back to a single-row DB lookup so a persisted-but-not-cached
    // row (e.g. cold DB row from a previous session) is still reachable.
    // Heterogeneous lookup keeps cache hits allocation-free.
    if (auto it = active_records_cache_.find(download_id);
        it != active_records_cache_.end()) {
      return it->second;
    }
    if (auto it = incognito_records_.find(download_id);
        it != incognito_records_.end()) {
      return it->second;
    }
    if (!IsDatabaseReady()) {
      return std::nullopt;
    }
    return database_->GetDownloadRecord(std::string(download_id));
  }

  auto it = record_cache_.find(download_id);
  if (it != record_cache_.end()) {
    return it->second;
  }

  return std::nullopt;
}

std::vector<DownloadRecord> DownloadRecordStore::GetDownloadsPage(
    const DownloadRecordQuery& query) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsDatabaseReady()) {
    return {};
  }
  return database_->GetDownloadRecordsPage(query);
}

size_t DownloadRecordStore::GetDownloadsCount(
    const DownloadRecordQuery& query) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsDatabaseReady()) {
    return 0;
  }
  return static_cast<size_t>(database_->GetDownloadRecordsCount(query));
}

bool DownloadRecordStore::IsDatabaseReady() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return database_ && database_->IsInitialized();
}

void DownloadRecordStore::WriteThroughToActiveCache(
    const DownloadRecord& record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Routes the merged record into the appropriate hot cache. Incognito
  // records never go to `active_records_cache_` (which is reserved for
  // persisted rows) so callers can pass any record without pre-checking.
  if (record.is_incognito) {
    incognito_records_[record.download_id] = record;
  } else {
    active_records_cache_[record.download_id] = record;
  }
}

void DownloadRecordStore::EvictOnDestroy(const std::string& download_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pagination_enabled_) {
    // Legacy flag-OFF path keeps `record_cache_` populated for the full
    // session; task destruction must not evict it.
    return;
  }
  // Both erases are idempotent; either map (or neither) may hold the id.
  // Posting on the database sequence makes this FIFO-ordered with all
  // other CRUD work, so a still-pending UpdateRecord finishes first and
  // can never resurrect the entry after this evict runs.
  active_records_cache_.erase(download_id);
  incognito_records_.erase(download_id);
}
