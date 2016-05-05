/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/storage/savegame.h"

#include <algorithm>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "starboard/storage.h"
#include "starboard/user.h"

namespace cobalt {
namespace storage {

// Savegame implementation that writes to the Starboard Storage API.
class SavegameStarboard : public Savegame {
 public:
  explicit SavegameStarboard(const Options& options);
  ~SavegameStarboard() OVERRIDE;
  bool PlatformRead(ByteVector* bytes) OVERRIDE;
  bool PlatformWrite(const ByteVector& bytes) OVERRIDE;
  bool PlatformDelete() OVERRIDE;

 private:
  scoped_ptr<starboard::StorageRecord> record_;
};

SavegameStarboard::SavegameStarboard(const Options& options)
    : Savegame(options) {
  record_.reset(new starboard::StorageRecord());
}

SavegameStarboard::~SavegameStarboard() {
  if (options_.delete_on_destruction) {
    Delete();
  }
}

bool SavegameStarboard::PlatformRead(ByteVector* bytes_ptr) {
  if (!record_->IsValid()) {
    DLOG(WARNING) << __FUNCTION__ << ": Invalid StorageRecord";
    return false;
  }

  int64_t size = record_->GetSize();
  if (size < 0) {
    DLOG(WARNING) << "StorageRecord::GetSize failed";
    return false;
  }

  ByteVector& bytes = *bytes_ptr;
  bytes.resize(static_cast<size_t>(size));
  if (bytes.empty()) {
    return true;
  }

  int64_t bytes_read = record_->Read(reinterpret_cast<char*>(&bytes[0]), size);
  bytes.resize(
      static_cast<size_t>(std::max(static_cast<int64_t>(0), bytes_read)));
  return bytes_read == size;
}

bool SavegameStarboard::PlatformWrite(const ByteVector& bytes) {
  if (!record_->IsValid()) {
    // Might have been deleted, so we'll create a new one.
    record_.reset(new starboard::StorageRecord());
  }

  if (!record_->IsValid()) {
    DLOG(WARNING) << __FUNCTION__ << ": Invalid StorageRecord: Signed in?";
    return false;
  }

  int64_t byte_count = static_cast<int64_t>(bytes.size());
  return record_->Write(reinterpret_cast<const char*>(&bytes[0]), byte_count);
}

bool SavegameStarboard::PlatformDelete() {
  if (!record_->IsValid()) {
    DLOG(WARNING) << __FUNCTION__ << ": Invalid StorageRecord";
    return false;
  }

  return record_->Delete();
}

// static
scoped_ptr<Savegame> Savegame::Create(const Options& options) {
  scoped_ptr<Savegame> savegame(new SavegameStarboard(options));
  return savegame.Pass();
}

}  // namespace storage
}  // namespace cobalt
