// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

// Rollback PoC (option B of "C25->C27->C25 Upgrade and Rollback Storage
// Handling"): C27 keeps a minimal, always-current sign-in record in the C25
// Starboard storage format so that a rollback to C25 keeps the user signed in.
// Throwaway prototype code; DO NOT SUBMIT.

#ifndef COBALT_SHELL_BROWSER_MIGRATE_STORAGE_RECORD_LEGACY_STORAGE_WRITER_H_
#define COBALT_SHELL_BROWSER_MIGRATE_STORAGE_RECORD_LEGACY_STORAGE_WRITER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"
#include "url/origin.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace content {
class StoragePartition;
}  // namespace content

namespace cobalt {
namespace migrate_storage_record {

// localStorage keys (on the origin of the initial URL) that make up the
// minimal YouTube TV sign-in credential: the device's identities with their
// OAuth refresh tokens, and the active identity.
inline constexpr const char* kLegacyCredentialKeys[] = {
    "yt.leanback.default::available-identities",
    "yt.leanback.default::last-identity-used",
};

// Inverse of blink::CachedStorageArea's kLocalStorageDetectFormat encoding
// (and of MigrationManager's FormatStringForLocalStorage()): byte 0 is 1
// (Latin-1, one byte per char) or 0 (UTF-16, host endian). Returns valid UTF-8,
// or nullopt if |bytes| is corrupt.
std::optional<std::string> DecodeLocalStorageString(
    const std::vector<uint8_t>& bytes);

// Returns "SAV1" + serialized cobalt.storage.Storage holding one
// LocalStorage{serialized_origin = origin.Serialize(), entries} and no cookies.
// Empty |entries| -> exactly "SAV1" (a valid, empty store for C25; a non-empty
// file also stops C25 from falling back to ~/.starboard.storage).
std::string SerializeLegacyStorageRecord(
    const url::Origin& origin,
    const std::map<std::string, std::string>& entries);

// Keeps ~/.starboard.<record_name>.storage (C25 format) in sync with the
// allowlisted localStorage keys of |origin|. Lives on the browser UI sequence.
// The record is written once from an initial snapshot, then rewritten (at most
// once per kWriteDelay) whenever an allowlisted key changes.
class LegacyStorageWriter : public blink::mojom::StorageAreaObserver {
 public:
  static constexpr base::TimeDelta kWriteDelay = base::Milliseconds(500);

  // Derives origin + record name from --url (or the default URL), binds the
  // origin's localStorage area on |partition| and starts. Returns nullptr if
  // the initial URL is invalid or has an opaque origin.
  static std::unique_ptr<LegacyStorageWriter> CreateForInitialUrl(
      content::StoragePartition* partition);

  LegacyStorageWriter(const url::Origin& origin, std::string record_name);
  LegacyStorageWriter(const LegacyStorageWriter&) = delete;
  LegacyStorageWriter& operator=(const LegacyStorageWriter&) = delete;
  // Flushes a pending write from the cached entries (BLOCK_SHUTDOWN task).
  ~LegacyStorageWriter() override;

  // Takes the snapshot (GetAll + observer) and writes the initial record.
  void Start(mojo::PendingRemote<blink::mojom::StorageArea> area);

  // blink::mojom::StorageAreaObserver:
  void KeyChanged(const std::vector<uint8_t>& key,
                  const std::vector<uint8_t>& new_value,
                  const std::optional<std::vector<uint8_t>>& old_value,
                  const std::string& source) override;
  void KeyChangeFailed(const std::vector<uint8_t>& key,
                       const std::string& source) override {}
  void KeyDeleted(const std::vector<uint8_t>& key,
                  const std::optional<std::vector<uint8_t>>& old_value,
                  const std::string& source) override;
  void AllDeleted(bool was_nonempty, const std::string& source) override;
  void ShouldSendOldValueOnMutations(bool value) override {}

 private:
  // Returns true if |key| is allowlisted (cache updated; null value = erase).
  bool UpdateEntry(const std::vector<uint8_t>& key,
                   const std::vector<uint8_t>* value);
  void ScheduleWrite();
  void OnWriteTimer();
  void OnSnapshot(std::vector<blink::mojom::KeyValuePtr> data);
  void WriteNow();

  const url::Origin origin_;
  const std::string record_name_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  mojo::Remote<blink::mojom::StorageArea> area_;
  mojo::Receiver<blink::mojom::StorageAreaObserver> observer_receiver_{this};
  // Allowlisted entries, UTF-8. Authoritative after each snapshot; between
  // snapshots it is a best-effort cache used only for the shutdown flush.
  std::map<std::string, std::string> entries_;
  std::optional<std::string> last_written_;
  base::OneShotTimer write_timer_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<LegacyStorageWriter> weak_factory_{this};
};

}  // namespace migrate_storage_record
}  // namespace cobalt

#endif  // COBALT_SHELL_BROWSER_MIGRATE_STORAGE_RECORD_LEGACY_STORAGE_WRITER_H_
