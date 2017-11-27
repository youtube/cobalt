// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "starboard/storage.h"
#include "starboard/user.h"

namespace cobalt {
namespace storage {
namespace {

// An arbitrary max size for the save game file so that, for example, a corrupt
// filesystem cannot cause us to allocate a fatally large memory buffer.
size_t kMaxSaveGameSizeBytes = 4 * 1024 * 1024;

bool ReadRecord(Savegame::ByteVector* bytes_ptr, size_t max_to_read,
                const scoped_ptr<starboard::StorageRecord>& record) {
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
  return bytes_read == size;
}

bool WriteRecord(const scoped_ptr<starboard::StorageRecord>& record,
                 const Savegame::ByteVector& bytes) {
  int64_t byte_count = static_cast<int64_t>(bytes.size());
  return record->Write(reinterpret_cast<const char*>(bytes.data()), byte_count);
}

scoped_ptr<starboard::StorageRecord> CreateRecord(
    const base::optional<std::string>& id) {
#if SB_API_VERSION >= 6
  if (id) {
    return make_scoped_ptr(new starboard::StorageRecord(id->c_str()));
  }
#else  // SB_API_VERSION >= 6
  UNREFERENCED_PARAMETER(id);
#endif  // SB_API_VERSION >= 6
  return make_scoped_ptr(new starboard::StorageRecord());
}

bool EnsureRecord(scoped_ptr<starboard::StorageRecord>* record,
                  const base::optional<std::string>& id) {
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

  scoped_ptr<starboard::StorageRecord> record_;
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
                  << "Failed to ensure record for ID: " << options_.id;
    return false;
  }

  scoped_ptr<starboard::StorageRecord> fallback_record;
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
    return true;
  }

  if (!WriteRecord(record_, buffer)) {
    DLOG(WARNING) << __FUNCTION__ << ": "
                  << "Failed to write record for ID: " << options_.id;
    return false;
  }

  // Flush the migrated record by closing and reopening it.
  record_.reset();
  if (!EnsureRecord(&record_, options_.id)) {
    return false;
  }

  // Now cleanup the fallback record.
  fallback_record->Delete();
  return true;
}

}  // namespace

// static
scoped_ptr<Savegame> Savegame::Create(const Options& options) {
  return make_scoped_ptr(new SavegameStarboard(options)).PassAs<Savegame>();
}

}  // namespace storage
}  // namespace cobalt
