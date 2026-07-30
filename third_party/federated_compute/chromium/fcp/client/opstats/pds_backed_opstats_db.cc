/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "fcp/client/opstats/pds_backed_opstats_db.h"

#include <fcntl.h>
#include <sys/file.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>  // NOLINT(build/c++17)
#include <functional>
#include <memory>
#include <string>
#include <system_error>  // NOLINT(build/c++11)
#include <utility>

#include "google/protobuf/timestamp.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/const_init.h"
#include "absl/container/flat_hash_set.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/histogram_counters.pb.h"
#include "fcp/client/log_manager.h"
#include "fcp/client/opstats/opstats_db.h"
#include "google/protobuf/repeated_ptr_field.h"
#include "google/protobuf/util/time_util.h"
#include "protostore/file-storage.h"
#include "protostore/proto-data-store.h"

namespace fcp {
namespace client {
namespace opstats {
namespace {

using ::google::protobuf::util::TimeUtil;

absl::Mutex& GetFileLockMutex() {
  static absl::Mutex* file_lock_mutex = new absl::Mutex();
  return *file_lock_mutex;
}

absl::flat_hash_set<std::string>* GetFilesInUseSet() {
  // Create the heap allocated static set only once, never call d'tor.
  // See: go/totw/110
  static absl::flat_hash_set<std::string>* files_in_use =
      new absl::flat_hash_set<std::string>();
  return files_in_use;
}

absl::StatusOr<int> AcquireFileLock(const std::string& db_path,
                                    LogManager& log_manager) {
  absl::WriterMutexLock lock(GetFileLockMutex());
  // If the underlying file is already in the hash set, it means another
  // instance of OpStatsDb is using it, and we'll return an error.
  absl::flat_hash_set<std::string>* files_in_use = GetFilesInUseSet();
  if (!files_in_use->insert(db_path).second) {
    log_manager.LogDiag(ProdDiagCode::OPSTATS_MULTIPLE_DB_INSTANCE_DETECTED);
    return absl::InternalError(
        "Another instance is already using the underlying database file.");
  }
  // Create a new file descriptor.
  // Create the file if it doesn't exist, set permission to 0644.
  int fd = open(db_path.c_str(), O_CREAT | O_RDWR,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fd < 0) {
    files_in_use->erase(db_path);
    log_manager.LogDiag(ProdDiagCode::OPSTATS_FAILED_TO_OPEN_FILE);
    return absl::InternalError(absl::StrCat("Failed to open file: ", db_path));
  }
  // Acquire exclusive lock on the file in a non-blocking mode.
  // flock(2) applies lock on the file object in the open file table, so it can
  // apply lock across different processes.  Within a process, flock doesn't
  // necessarily guarantee synchronization across multiple threads.
  // See:https://man7.org/linux/man-pages/man2/flock.2.html
  if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
    files_in_use->erase(db_path);
    close(fd);
    log_manager.LogDiag(ProdDiagCode::OPSTATS_MULTIPLE_DB_INSTANCE_DETECTED);
    return absl::InternalError(
        "Failed to acquire file lock on the underlying database file.");
  }
  return fd;
}

void ReleaseFileLock(const std::string& db_path, int fd) {
  absl::WriterMutexLock lock(GetFileLockMutex());
  GetFilesInUseSet()->erase(db_path);
  FCP_CHECK(fd >= 0);
  // File lock is released when the descriptor is closed.
  close(fd);
}

std::unique_ptr<OpStatsSequence> CreateEmptyData() {
  auto empty_data = std::make_unique<OpStatsSequence>();
  *(empty_data->mutable_earliest_trustworthy_time()) =
      google::protobuf::util::TimeUtil::GetCurrentTime();
  return empty_data;
}

// Returns the data in the db, or an error from the read operation.
absl::StatusOr<OpStatsSequence> ReadInternal(
    protostore::ProtoDataStore<OpStatsSequence>& db, LogManager& log_manager) {
  absl::StatusOr<const OpStatsSequence*> data = db.Read();
  if (data.ok()) {
    return *data.value();
  } else {
    log_manager.LogDiag(ProdDiagCode::OPSTATS_READ_FAILED);
    return absl::InternalError(
        absl::StrCat("Failed to read from database, with error message: ",
                     data.status().message()));
  }
}

// Overwrites the db to contain an empty OpStatsSequence message.
absl::Status ResetInternal(protostore::ProtoDataStore<OpStatsSequence>& db,
                           LogManager& log_manager) {
  absl::Status reset_status = db.Write(CreateEmptyData());
  if (!reset_status.ok()) {
    log_manager.LogDiag(ProdDiagCode::OPSTATS_RESET_FAILED);
    return absl::InternalError(
        absl::StrCat("Failed to reset the database, with error message: ",
                     reset_status.code()));
  }
  return absl::OkStatus();
}

absl::Time GetLastUpdateTime(const OperationalStats& operational_stats) {
  // Depending on the state of the use_phase_stats flag when the opstats entry
  // was logged, the entry may have events either in PhaseStats or in the
  // deprecated events field.
  // In order to support both kinds of entries until use_phase_stats has rolled
  // out and all opstats entries with the legacy schema have TTLed, we will
  // check PhaseStats, then the legacy events field.
  for (auto it = operational_stats.phase_stats().rbegin();
       it != operational_stats.phase_stats().rend(); ++it) {
    if (!it->events().empty()) {
      return absl::FromUnixSeconds(
          TimeUtil::TimestampToSeconds(it->events().rbegin()->timestamp()));
    }
  }
  if (operational_stats.events().empty()) {
    return absl::InfinitePast();
  }
  return absl::FromUnixSeconds(TimeUtil::TimestampToSeconds(
      operational_stats.events().rbegin()->timestamp()));
}

// If there's data, use the timestamp of the first event as the earliest
// trustworthy time; otherwise, the current time will be used.
::google::protobuf::Timestamp GetEarliestTrustWorthyTime(
    const google::protobuf::RepeatedPtrField<OperationalStats>& op_stats) {
  ::google::protobuf::Timestamp timestamp = TimeUtil::GetCurrentTime();
  for (const auto& stat : op_stats) {
    if (!stat.events().empty()) {
      timestamp = stat.events().begin()->timestamp();
      break;
    }
  }
  return timestamp;
}

void RemoveOutdatedData(OpStatsSequence& data, absl::Duration ttl) {
  absl::Time earliest_accepted_time = absl::Now() - ttl;
  auto* op_stats = data.mutable_opstats();
  int64_t original_num_entries = op_stats->size();
  op_stats->erase(
      std::remove_if(op_stats->begin(), op_stats->end(),
                     [earliest_accepted_time](const OperationalStats& data) {
                       return GetLastUpdateTime(data) < earliest_accepted_time;
                     }),
      op_stats->end());
  int64_t num_entries_after_purging = op_stats->size();
  if (num_entries_after_purging < original_num_entries) {
    *(data.mutable_earliest_trustworthy_time()) =
        TimeUtil::MillisecondsToTimestamp(
            absl::ToUnixMillis(earliest_accepted_time));
  }
}

void PruneOldDataUntilBelowSizeLimit(OpStatsSequence& data,
                                     const int64_t max_size_bytes,
                                     LogManager& log_manager) {
  int64_t current_size = data.ByteSizeLong();
  auto& op_stats = *(data.mutable_opstats());
  if (current_size > max_size_bytes) {
    int64_t num_pruned_entries = 0;
    auto it = op_stats.begin();
    absl::Time earliest_event_time = absl::InfinitePast();
    // The OperationalStats are sorted by time from earliest to latest, so we'll
    // remove from the start.
    while (current_size > max_size_bytes && it != op_stats.end()) {
      if (earliest_event_time == absl::InfinitePast()) {
        earliest_event_time = GetLastUpdateTime(*it);
      }
      num_pruned_entries++;
      // Note that the size of an OperationalStats is smaller than the size
      // impact it has on the OpStatsSequence. We are being conservative here.
      current_size -= it->ByteSizeLong();
      it++;
    }
    op_stats.erase(op_stats.begin(), it);
    *data.mutable_earliest_trustworthy_time() =
        GetEarliestTrustWorthyTime(op_stats);
    log_manager.LogToLongHistogram(
        HistogramCounters::OPSTATS_NUM_PRUNED_ENTRIES, num_pruned_entries);
    log_manager.LogToLongHistogram(
        HistogramCounters::OPSTATS_OLDEST_PRUNED_ENTRY_TENURE_HOURS,
        absl::ToInt64Hours(absl::Now() - earliest_event_time));
  }
  log_manager.LogToLongHistogram(HistogramCounters::OPSTATS_DB_SIZE_BYTES,
                                 current_size);
  log_manager.LogToLongHistogram(HistogramCounters::OPSTATS_DB_NUM_ENTRIES,
                                 op_stats.size());
}

}  // anonymous namespace

absl::StatusOr<std::unique_ptr<OpStatsDb>> PdsBackedOpStatsDb::Create(
    const std::string& base_dir, absl::Duration ttl, LogManager& log_manager,
    int64_t max_size_bytes) {
  std::filesystem::path path(base_dir);
  if (!path.is_absolute()) {
    log_manager.LogDiag(ProdDiagCode::OPSTATS_INVALID_FILE_PATH);
    return absl::InvalidArgumentError(
        absl::StrCat("The provided path: ", base_dir,
                     " is invalid. The path must start with \"/\""));
  }
  path /= kParentDir;
  std::error_code error;
  std::filesystem::create_directories(path, error);
  if (error.value() != 0) {
    log_manager.LogDiag(ProdDiagCode::OPSTATS_PARENT_DIR_CREATION_FAILED);
    return absl::InternalError(
        absl::StrCat("Failed to create directory ", path.generic_string()));
  }
  path /= kDbFileName;
  std::function<void()> lock_releaser;
  auto file_storage = std::make_unique<protostore::FileStorage>();
  std::unique_ptr<protostore::ProtoDataStore<OpStatsSequence>> pds;
  std::string db_path = path.generic_string();
  FCP_ASSIGN_OR_RETURN(int fd, AcquireFileLock(db_path, log_manager));
  lock_releaser = [db_path, fd]() { ReleaseFileLock(db_path, fd); };
  pds = std::make_unique<protostore::ProtoDataStore<OpStatsSequence>>(
      *file_storage, db_path);
  absl::StatusOr<int64_t> file_size = file_storage->GetFileSize(db_path);
  if (!file_size.ok()) {
    lock_releaser();
    return file_size.status();
  }
  // If the size of the underlying file is zero, it means this is the first
  // time we create the database.
  bool should_initiate = file_size.value() == 0;

  // If this is the first time we create the OpStatsDb, we want to create an
  // empty database.
  if (should_initiate) {
    absl::Status write_status = pds->Write(CreateEmptyData());
    if (!write_status.ok()) {
      lock_releaser();
      return write_status;
    }
  }
  return absl::WrapUnique(
      new PdsBackedOpStatsDb(std::move(pds), std::move(file_storage), ttl,
                             log_manager, max_size_bytes, lock_releaser));
}

PdsBackedOpStatsDb::~PdsBackedOpStatsDb() { lock_releaser_(); }

absl::StatusOr<OpStatsSequence> PdsBackedOpStatsDb::Read() {
  absl::WriterMutexLock lock(mutex_);
  auto data_or = ReadInternal(*db_, log_manager_);
  if (!data_or.ok()) {
    // Try resetting after a failed read.
    auto reset_status = ResetInternal(*db_, log_manager_);
  }
  return data_or;
}

absl::Status PdsBackedOpStatsDb::Transform(
    std::function<void(OpStatsSequence&)> func) {
  absl::WriterMutexLock lock(mutex_);
  OpStatsSequence data;
  auto data_or = ReadInternal(*db_, log_manager_);
  if (!data_or.ok()) {
    // Try resetting after a failed read.
    FCP_RETURN_IF_ERROR(ResetInternal(*db_, log_manager_));
  } else {
    data = std::move(data_or).value();
    RemoveOutdatedData(data, ttl_);
  }
  func(data);
  PruneOldDataUntilBelowSizeLimit(data, max_size_bytes_, log_manager_);
  if (!data.has_earliest_trustworthy_time()) {
    *data.mutable_earliest_trustworthy_time() =
        GetEarliestTrustWorthyTime(data.opstats());
  }
  absl::Status status =
      db_->Write(std::make_unique<OpStatsSequence>(std::move(data)));
  if (!status.ok()) {
    log_manager_.LogDiag(ProdDiagCode::OPSTATS_WRITE_FAILED);
  }
  return status;
}

}  // namespace opstats
}  // namespace client
}  // namespace fcp
