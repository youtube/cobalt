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
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "cobalt/browser/switches.h"
#include "cobalt/shell/browser/migrate_storage_record/migration_manager.h"
#include "cobalt/shell/browser/migrate_storage_record/storage.pb.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "content/public/browser/storage_partition.h"
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

// Runs on the writer's MayBlock sequence. Never logs the payload: it contains
// OAuth refresh tokens.
void WriteRecordBlocking(const std::string& record_name,
                         const std::string& bytes) {
  starboard::StorageRecord record(record_name.c_str());
  if (!record.IsValid() ||
      !record.Write(bytes.data(), static_cast<int64_t>(bytes.size()))) {
    LOG(ERROR) << "LegacyStorageWriter: failed to write C25 record.";
    return;
  }
  LOG(INFO) << "LegacyStorageWriter: wrote C25 record (" << bytes.size()
            << " bytes).";
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
      origin, GetApplicationKey(initial_url));
  mojo::PendingRemote<blink::mojom::StorageArea> area;
  partition->GetLocalStorageControl()->BindStorageArea(
      blink::StorageKey::CreateFirstParty(origin),
      area.InitWithNewPipeAndPassReceiver());
  writer->Start(std::move(area));
  return writer;
}

LegacyStorageWriter::LegacyStorageWriter(const url::Origin& origin,
                                         std::string record_name)
    : origin_(origin),
      record_name_(std::move(record_name)),
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
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WriteRecordBlocking, record_name_, std::move(bytes)));
}

}  // namespace migrate_storage_record
}  // namespace cobalt
