// Copyright 2016 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/storage/savegame.h"

#include <algorithm>
#include <memory>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "cobalt/storage/store_upgrade/upgrade.h"
#include "starboard/common/storage.h"
#include "starboard/user.h"

namespace cobalt {
namespace storage {
namespace {

using cobalt::storage::store_upgrade::IsUpgradeRequired;
using cobalt::storage::store_upgrade::UpgradeStore;

// An arbitrary max size for the save game file so that, for example, a corrupt
// filesystem cannot cause us to allocate a fatally large memory buffer.
size_t kMaxSaveGameSizeBytes = 4 * 1024 * 1024;

bool WriteRecord(const std::unique_ptr<starboard::StorageRecord>& record,
                 const Savegame::ByteVector& bytes);

bool Upgrade(Savegame::ByteVector* bytes_ptr,
             const std::unique_ptr<starboard::StorageRecord>& record) {
  if (IsUpgradeRequired(*bytes_ptr)) {
    DLOG(INFO) << "Upgrading Record with size=" << bytes_ptr->size();
    if (!UpgradeStore(bytes_ptr)) {
      DLOG(ERROR) << "Upgrade failed";
      return false;
    }
    DLOG(INFO) << "Upgraded successfully bytes_ptr.size=" << bytes_ptr->size();
  }

  WriteRecord(record, *bytes_ptr);
  return true;
}

bool ReadRecord(Savegame::ByteVector* bytes_ptr, size_t max_to_read,
                const std::unique_ptr<starboard::StorageRecord>& record) {
  if (!record->IsValid()) {
    DLOG(WARNING) << __FUNCTION__ << ": Invalid StorageRecord";
    return false;
  }

  int64_t size = record->GetSize();
  if (size < 0) {
    DLOG(WARNING) << "StorageRecord::GetSize failed";
    return false;
  }

  if (static_cast<size_t>(size) > max_to_read) {
    DLOG(WARNING) << "Savegame larger than max allowed size";
    return false;
  }

  Savegame::ByteVector& bytes = *bytes_ptr;
  bytes.resize(static_cast<size_t>(size));
  if (bytes.empty()) {
    return true;
  }

  int64_t bytes_read =
      record->Read(reinterpret_cast<char*>(bytes.data()), size);
  bytes.resize(
      static_cast<size_t>(std::max(static_cast<int64_t>(0), bytes_read)));

  bool success = (bytes_read == size);
  if (success) {
    DLOG(INFO) << "Successfully read storage record.";
  }
  if (!Upgrade(bytes_ptr, record)) {
    DLOG(WARNING) << __FUNCTION__ << ": Upgrade Failed";
    return false;
  }
  return success;
}

bool WriteRecord(const std::unique_ptr<starboard::StorageRecord>& record,
                 const Savegame::ByteVector& bytes) {
  int64_t byte_count = static_cast<int64_t>(bytes.size());
  bool success =
      record->Write(reinterpret_cast<const char*>(bytes.data()), byte_count);
  if (success) {
    DLOG(INFO) << "Successfully wrote storage record.";
  }
  return success;
}

std::unique_ptr<starboard::StorageRecord> CreateRecord(
    const base::Optional<std::string>& id) {
  if (id) {
    return base::WrapUnique(new starboard::StorageRecord(id->c_str()));
  }
  return base::WrapUnique(new starboard::StorageRecord());
}

bool EnsureRecord(std::unique_ptr<starboard::StorageRecord>* record,
                  const base::Optional<std::string>& id) {
  if (!(*record) || !(*record)->IsValid()) {
    // Might have been deleted, so we'll create a new one.
    (*record) = CreateRecord(id);
  }

  if (!(*record)->IsValid()) {
    DLOG(WARNING) << __FUNCTION__ << ": Invalid StorageRecord: Signed in?";
    return false;
  }

  return true;
}

// Savegame implementation that writes to the Starboard Storage API.
class SavegameStarboard : public Savegame {
 public:
  explicit SavegameStarboard(const Options& options);
  ~SavegameStarboard() override;
  bool PlatformRead(ByteVector* bytes, size_t max_to_read) override;
  bool PlatformWrite(const ByteVector& bytes) override;
  bool PlatformDelete() override;

 private:
  bool MigrateFromFallback();
  std::unique_ptr<starboard::StorageRecord> record_;
};

SavegameStarboard::SavegameStarboard(const Options& options)
    : Savegame(options) {
  EnsureRecord(&record_, options_.id);
}

SavegameStarboard::~SavegameStarboard() {
  if (options_.delete_on_destruction) {
    Delete();
  }
}

bool SavegameStarboard::PlatformRead(ByteVector* bytes_ptr,
                                     size_t max_to_read) {
  bool success = ReadRecord(bytes_ptr, max_to_read, record_);
  if (success && !bytes_ptr->empty()) {
    return true;
  }

  if (!options_.fallback_to_default_id) {
    return false;
  }

  if (!MigrateFromFallback()) {
    DLOG(WARNING) << __FUNCTION__ << ": Migration Failed";
    return false;
  }

  // Now read the migrated data.
  return ReadRecord(bytes_ptr, max_to_read, record_);
}

bool SavegameStarboard::PlatformWrite(const ByteVector& bytes) {
  if (!EnsureRecord(&record_, options_.id)) {
    return false;
  }

  return WriteRecord(record_, bytes);
}

bool SavegameStarboard::PlatformDelete() {
  if (!record_->IsValid()) {
    DLOG(WARNING) << __FUNCTION__ << ": Invalid StorageRecord";
    return false;
  }

  return record_->Delete();
}

bool SavegameStarboard::MigrateFromFallback() {
  ByteVector buffer;
  if (!EnsureRecord(&record_, options_.id)) {
    DLOG(WARNING) << __FUNCTION__ << ": "
                  << "Failed to ensure record for ID: " << options_.id.value();
    return false;
  }

  std::unique_ptr<starboard::StorageRecord> fallback_record;
  if (!EnsureRecord(&fallback_record, base::nullopt)) {
    DLOG(WARNING) << __FUNCTION__ << ": "
                  << "Failed to open default record.";
    return false;
  }

  if (!ReadRecord(&buffer, kMaxSaveGameSizeBytes, fallback_record)) {
    DLOG(WARNING) << __FUNCTION__ << ": "
                  << "Failed to read default record.";
    return false;
  }

  if (buffer.size() == 0) {
    // We migrated nothing successfully.
    DLOG(INFO) << "Migrated storage record data successfully (but trivially, "
               << "there was no data).";
    return true;
  }

  if (!WriteRecord(record_, buffer)) {
    DLOG(WARNING) << __FUNCTION__ << ": "
                  << "Failed to write record for ID: " << options_.id.value();
    return false;
  }

  // Flush the migrated record by closing and reopening it.
  record_.reset();
  if (!EnsureRecord(&record_, options_.id)) {
    return false;
  }

  // Now cleanup the fallback record.
  fallback_record->Delete();
  DLOG(INFO) << "Migrated storage record data successfully for user id: "
             << options_.id.value();
  return true;
}

}  // namespace

// static
std::unique_ptr<Savegame> Savegame::Create(const Options& options) {
  return base::WrapUnique(new SavegameStarboard(options));
}

}  // namespace storage
}  // namespace cobalt
