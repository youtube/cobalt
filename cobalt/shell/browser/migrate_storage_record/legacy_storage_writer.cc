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

#include "cobalt/shell/browser/migrate_storage_record/legacy_storage_writer.h"

#include <cstring>
#include <iterator>
#include <string_view>
#include <utility>

#include "base/barrier_closure.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "cobalt/browser/switches.h"
#include "cobalt/shell/browser/migrate_storage_record/migration_manager.h"
#include "cobalt/shell/browser/migrate_storage_record/storage.pb.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "content/public/browser/storage_partition.h"
#include "crypto/sha2.h"
#include "starboard/common/storage.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace cobalt {
namespace migrate_storage_record {
namespace {

// C25 cobalt/storage/storage_constants.cc kStorageHeader.
constexpr char kRecordHeader[] = "SAV1";
// blink::CachedStorageArea StorageFormat values.
constexpr uint8_t kFormatUTF16 = 0;
constexpr uint8_t kFormatLatin1 = 1;

bool IsCredentialKey(const std::string& key) {
  for (const char* credential_key : kLegacyCredentialKeys) {
    if (key == credential_key) {
      return true;
    }
  }
  return false;
}

constexpr char kReconcileSource[] = "rollback-reconcile";

// Runs on the writer's MayBlock sequence. Never logs the payload: it contains
// OAuth refresh tokens.
void WriteRecordBlocking(const std::string& record_name,
                         const base::FilePath& digest_path,
                         const std::string& bytes) {
  starboard::StorageRecord record(record_name.c_str());
  if (!record.IsValid() ||
      !record.Write(bytes.data(), static_cast<int64_t>(bytes.size()))) {
    LOG(ERROR) << "LegacyStorageWriter: failed to write C25 record.";
    return;
  }
  LOG(INFO) << "LegacyStorageWriter: wrote C25 record (" << bytes.size()
            << " bytes).";
  if (digest_path.empty()) {
    return;
  }
  // Written after the record: a crash in between leaves a stale digest, so
  // the next launch re-imports C27's own record (idempotent), never the
  // reverse.
  if (!base::CreateDirectory(digest_path.DirName()) ||
      !base::ImportantFileWriter::WriteFileAtomically(
          digest_path, ComputeLegacyRecordDigest(bytes))) {
    LOG(ERROR) << "LegacyStorageWriter: failed to write record digest.";
  }
}

// Reads the whole payload of the named record; nullopt if absent or empty.
std::optional<std::string> ReadLegacyRecordBytes(
    const std::string& record_name) {
  starboard::StorageRecord record(record_name.c_str());
  if (!record.IsValid()) {
    return std::nullopt;
  }
  const int64_t size = record.GetSize();
  if (size <= 0) {
    return std::nullopt;
  }
  std::string bytes(static_cast<size_t>(size), '\0');
  if (record.Read(bytes.data(), size) != size) {
    return std::nullopt;
  }
  return bytes;
}

// Keeps the StorageArea pipe alive until every Put/Delete has replied, and
// runs |done| exactly once (on completion or timeout).
class ReconcileState : public base::RefCounted<ReconcileState> {
 public:
  explicit ReconcileState(base::OnceClosure done) : done_(std::move(done)) {}
  mojo::Remote<blink::mojom::StorageArea>& area() { return area_; }
  void Finish(bool timed_out) {
    if (done_) {
      LOG(INFO) << "LegacyStorageWriter: credential reconcile "
                << (timed_out ? "timed out" : "complete") << ".";
      std::move(done_).Run();
    }
  }

 private:
  friend class base::RefCounted<ReconcileState>;
  ~ReconcileState() = default;

  mojo::Remote<blink::mojom::StorageArea> area_;
  base::OnceClosure done_;
};

void OnReconcileOp(base::RepeatingClosure barrier,
                   const std::string& key,
                   bool success) {
  if (!success) {
    LOG(ERROR) << "LegacyStorageWriter: reconcile failed for " << key;
  }
  barrier.Run();
}

}  // namespace

std::optional<std::string> DecodeLocalStorageString(
    const std::vector<uint8_t>& bytes) {
  if (bytes.empty()) {
    return std::string();  // Blink decodes an empty vector as "".
  }
  std::u16string utf16;
  const size_t payload_size = bytes.size() - 1;
  switch (bytes[0]) {
    case kFormatLatin1:
      // Widen each Latin-1 byte to a UTF-16 code unit (U+0000..U+00FF). Raw
      // bytes >= 0x80 must not be copied into the UTF-8 output.
      utf16.reserve(payload_size);
      for (size_t i = 1; i < bytes.size(); ++i) {
        utf16.push_back(static_cast<char16_t>(bytes[i]));
      }
      break;
    case kFormatUTF16:
      if (payload_size % sizeof(char16_t) != 0) {
        return std::nullopt;
      }
      utf16.resize(payload_size / sizeof(char16_t));
      std::memcpy(utf16.data(), bytes.data() + 1, payload_size);
      break;
    default:
      return std::nullopt;
  }
  // UTF16ToUTF8 replaces unpaired surrogates, so the result is always valid
  // UTF-8. C25's proto3 parser rejects (and C25 then wipes) a record with an
  // invalid UTF-8 string field.
  return base::UTF16ToUTF8(utf16);
}

std::string SerializeLegacyStorageRecord(
    const url::Origin& origin,
    const std::map<std::string, std::string>& entries) {
  cobalt::storage::Storage storage;
  if (!entries.empty()) {
    cobalt::storage::LocalStorage* local_storage = storage.add_local_storages();
    // E.g. "https://www.youtube.com"; matches C25's
    // loader::Origin::SerializedOrigin() after it re-normalizes the string.
    local_storage->set_serialized_origin(origin.Serialize());
    for (const auto& [key, value] : entries) {
      cobalt::storage::LocalStorageEntry* entry =
          local_storage->add_local_storage_entries();
      entry->set_key(key);
      entry->set_value(value);
    }
  }
  std::string out(kRecordHeader);
  storage.AppendToString(&out);
  return out;
}

std::string ComputeLegacyRecordDigest(std::string_view bytes) {
  return base::HexEncode(crypto::SHA256HashString(bytes));
}

base::FilePath GetLegacyRecordDigestPath() {
  base::FilePath cache_path;
  if (!base::PathService::Get(base::DIR_CACHE, &cache_path)) {
    return base::FilePath();
  }
  return cache_path.Append(kLegacyRecordDigestFileName);
}

std::map<std::string, std::string> ExtractLegacyCredentialEntries(
    const url::Origin& origin,
    const cobalt::storage::Storage& storage) {
  std::map<std::string, std::string> out;
  for (const auto& local_storage : storage.local_storages()) {
    // Same filter as MigrationManager::ToLocalStorageItems.
    GURL storage_origin(local_storage.serialized_origin());
    if (!storage_origin.SchemeIs("https") ||
        !origin.IsSameOriginWith(storage_origin)) {
      continue;
    }
    for (const auto& entry : local_storage.local_storage_entries()) {
      if (IsCredentialKey(entry.key())) {
        out[entry.key()] = entry.value();
      }
    }
  }
  return out;
}

std::vector<uint8_t> EncodeLocalStorageString(const std::string& utf8) {
  const std::u16string utf16 = base::UTF8ToUTF16(utf8);
  bool latin1 = true;
  for (char16_t c : utf16) {
    if (c > 0xFF) {
      latin1 = false;
      break;
    }
  }
  std::vector<uint8_t> out;
  if (latin1) {
    out.reserve(utf16.size() + 1);
    out.push_back(kFormatLatin1);
    for (char16_t c : utf16) {
      out.push_back(static_cast<uint8_t>(c));
    }
  } else {
    out.resize(1 + utf16.size() * sizeof(char16_t));
    out[0] = kFormatUTF16;
    std::memcpy(out.data() + 1, utf16.data(), utf16.size() * sizeof(char16_t));
  }
  return out;
}

void ReconcileCredentialsFromLegacyRecord(content::StoragePartition* partition,
                                          const url::Origin& origin,
                                          const std::string& record_name,
                                          const base::FilePath& digest_path,
                                          base::OnceClosure done) {
  std::optional<std::string> bytes = ReadLegacyRecordBytes(record_name);
  if (!bytes) {
    LOG(INFO) << "LegacyStorageWriter: no C25 record; nothing to reconcile.";
    std::move(done).Run();
    return;
  }
  const std::string digest = ComputeLegacyRecordDigest(*bytes);
  std::string stored_digest;
  if (!digest_path.empty() &&
      base::ReadFileToString(digest_path, &stored_digest) &&
      base::TrimWhitespaceASCII(stored_digest, base::TRIM_ALL) == digest) {
    LOG(INFO) << "LegacyStorageWriter: C25 record unchanged since C27 wrote "
                 "it; nothing to reconcile.";
    std::move(done).Run();
    return;
  }

  cobalt::storage::Storage storage;
  if (bytes->size() < 4 || bytes->compare(0, 4, kRecordHeader) != 0 ||
      !storage.ParseFromArray(bytes->data() + 4,
                              static_cast<int>(bytes->size() - 4))) {
    // Never sign the user out because of a corrupt file.
    LOG(WARNING) << "LegacyStorageWriter: C25 record changed but does not "
                    "parse; keeping C27 credentials.";
    std::move(done).Run();
    return;
  }
  const std::map<std::string, std::string> entries =
      ExtractLegacyCredentialEntries(origin, storage);
  LOG(INFO) << "LegacyStorageWriter: C25 record changed since C27 last wrote "
               "it; reconciling credential keys (present in record="
            << entries.size() << ").";

  auto state = base::MakeRefCounted<ReconcileState>(std::move(done));
  partition->GetLocalStorageControl()->BindStorageArea(
      blink::StorageKey::CreateFirstParty(origin),
      state->area().BindNewPipeAndPassReceiver());
  base::RepeatingClosure barrier = base::BarrierClosure(
      std::size(kLegacyCredentialKeys) + 1,
      base::BindOnce(&ReconcileState::Finish, state, /*timed_out=*/false));
  for (const char* key : kLegacyCredentialKeys) {
    auto it = entries.find(key);
    if (it != entries.end()) {
      state->area()->Put(
          EncodeLocalStorageString(key), EncodeLocalStorageString(it->second),
          std::nullopt, kReconcileSource,
          base::BindOnce(&OnReconcileOp, barrier, std::string(key)));
    } else {
      state->area()->Delete(
          EncodeLocalStorageString(key), std::nullopt, kReconcileSource,
          base::BindOnce(&OnReconcileOp, barrier, std::string(key)));
    }
  }
  state->area()->Delete(EncodeLocalStorageString(kCachedAccessTokensKey),
                        std::nullopt, kReconcileSource,
                        base::BindOnce(&OnReconcileOp, barrier,
                                       std::string(kCachedAccessTokensKey)));
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ReconcileState::Finish, state, /*timed_out=*/true),
      kReconcileTimeout);
}

void ReconcileCredentialsFromLegacyRecordForInitialUrl(
    content::StoragePartition* partition,
    base::OnceClosure done) {
  GURL initial_url(
      switches::GetInitialURL(*base::CommandLine::ForCurrentProcess()));
  if (!partition || !initial_url.is_valid() ||
      url::Origin::Create(initial_url).opaque()) {
    std::move(done).Run();
    return;
  }
  ReconcileCredentialsFromLegacyRecord(
      partition, url::Origin::Create(initial_url),
      GetApplicationKey(initial_url), GetLegacyRecordDigestPath(),
      std::move(done));
}

// static
std::unique_ptr<LegacyStorageWriter> LegacyStorageWriter::CreateForInitialUrl(
    content::StoragePartition* partition) {
  GURL initial_url(
      switches::GetInitialURL(*base::CommandLine::ForCurrentProcess()));
  if (!partition || !initial_url.is_valid()) {
    return nullptr;
  }
  url::Origin origin = url::Origin::Create(initial_url);
  if (origin.opaque()) {
    return nullptr;
  }

  auto writer = std::make_unique<LegacyStorageWriter>(
      origin, GetApplicationKey(initial_url), GetLegacyRecordDigestPath());
  mojo::PendingRemote<blink::mojom::StorageArea> area;
  partition->GetLocalStorageControl()->BindStorageArea(
      blink::StorageKey::CreateFirstParty(origin),
      area.InitWithNewPipeAndPassReceiver());
  writer->Start(std::move(area));
  return writer;
}

LegacyStorageWriter::LegacyStorageWriter(const url::Origin& origin,
                                         std::string record_name,
                                         base::FilePath digest_path)
    : origin_(origin),
      record_name_(std::move(record_name)),
      digest_path_(std::move(digest_path)),
      io_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {}

LegacyStorageWriter::~LegacyStorageWriter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (write_timer_.IsRunning()) {
    write_timer_.Stop();
    // Best effort: the cache reflects every observed event, but no round trip
    // to the storage service is possible during shutdown.
    WriteNow();
  }
}

void LegacyStorageWriter::Start(
    mojo::PendingRemote<blink::mojom::StorageArea> area) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  area_.Bind(std::move(area));
  // Snapshot + observer registration in one call, as recommended by
  // storage_area.mojom, so that no change is missed in between.
  area_->GetAll(observer_receiver_.BindNewPipeAndPassRemote(),
                base::BindOnce(&LegacyStorageWriter::OnSnapshot,
                               weak_factory_.GetWeakPtr()));
}

void LegacyStorageWriter::KeyChanged(
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& new_value,
    const std::optional<std::vector<uint8_t>>& old_value,
    const std::string& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (UpdateEntry(key, &new_value)) {
    ScheduleWrite();
  }
}

void LegacyStorageWriter::KeyDeleted(
    const std::vector<uint8_t>& key,
    const std::optional<std::vector<uint8_t>>& old_value,
    const std::string& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (UpdateEntry(key, nullptr)) {
    ScheduleWrite();
  }
}

void LegacyStorageWriter::AllDeleted(bool was_nonempty,
                                     const std::string& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  entries_.clear();
  ScheduleWrite();
}

bool LegacyStorageWriter::UpdateEntry(const std::vector<uint8_t>& key,
                                      const std::vector<uint8_t>* value) {
  std::optional<std::string> decoded_key = DecodeLocalStorageString(key);
  if (!decoded_key || !IsCredentialKey(*decoded_key)) {
    return false;
  }
  if (!value) {
    entries_.erase(*decoded_key);
    return true;
  }
  std::optional<std::string> decoded_value = DecodeLocalStorageString(*value);
  if (!decoded_value) {
    LOG(WARNING) << "LegacyStorageWriter: undecodable value for "
                 << *decoded_key << "; keeping previous value.";
    return false;
  }
  entries_[*decoded_key] = std::move(*decoded_value);
  return true;
}

void LegacyStorageWriter::ScheduleWrite() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Fixed delay from the first change of a burst (not restarted on every
  // change), so a stream of writes cannot postpone the record indefinitely.
  if (!write_timer_.IsRunning()) {
    write_timer_.Start(FROM_HERE, kWriteDelay,
                       base::BindOnce(&LegacyStorageWriter::OnWriteTimer,
                                      base::Unretained(this)));
  }
}

void LegacyStorageWriter::OnWriteTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!area_.is_connected()) {
    WriteNow();
    return;
  }
  // Observer events and GetAll replies travel on different pipes, so their
  // relative order is not guaranteed. Re-snapshot so that the record is
  // authoritative: this GetAll is processed after the Put/Delete that fired the
  // event.
  area_->GetAll(mojo::NullRemote(),
                base::BindOnce(&LegacyStorageWriter::OnSnapshot,
                               weak_factory_.GetWeakPtr()));
}

void LegacyStorageWriter::OnSnapshot(
    std::vector<blink::mojom::KeyValuePtr> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  entries_.clear();
  for (const auto& key_value : data) {
    UpdateEntry(key_value->key, &key_value->value);
  }
  WriteNow();
}

void LegacyStorageWriter::WriteNow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string bytes = SerializeLegacyStorageRecord(origin_, entries_);
  if (last_written_ == bytes) {
    return;
  }
  last_written_ = bytes;
  LOG(INFO) << "LegacyStorageWriter: credential keys present="
            << entries_.size();
  io_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(&WriteRecordBlocking, record_name_,
                                           digest_path_, std::move(bytes)));
}

}  // namespace migrate_storage_record
}  // namespace cobalt
