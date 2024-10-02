// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/script_cached_metadata_handler.h"

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/platform/crypto.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"

namespace blink {

namespace {

void RecordState(StateOnGet state) {
  UMA_HISTOGRAM_ENUMERATION("Memory.Renderer.BlinkCachedMetadataGetResult",
                            state);
}

}  // namespace

ScriptCachedMetadataHandler::ScriptCachedMetadataHandler(
    const WTF::TextEncoding& encoding,
    std::unique_ptr<CachedMetadataSender> sender)
    : sender_(std::move(sender)), encoding_(encoding) {}

ScriptCachedMetadataHandler::~ScriptCachedMetadataHandler() = default;

void ScriptCachedMetadataHandler::Trace(Visitor* visitor) const {
  CachedMetadataHandler::Trace(visitor);
}

void ScriptCachedMetadataHandler::SetCachedMetadata(
    CodeCacheHost* code_cache_host,
    uint32_t data_type_id,
    const uint8_t* data,
    size_t size) {
  DCHECK(!cached_metadata_);
  // Having been discarded once, the further attempts to overwrite the
  // CachedMetadata are ignored. This behavior is necessary to avoid clearing
  // the disk-based cache every time GetCachedMetadata() returns nullptr. The
  // JSModuleScript behaves similarly by preventing the creation of the code
  // cache.
  if (cached_metadata_discarded_)
    return;
  cached_metadata_ = CachedMetadata::Create(data_type_id, data, size);
  if (!disable_send_to_platform_for_testing_)
    CommitToPersistentStorage(code_cache_host);
}

void ScriptCachedMetadataHandler::ClearCachedMetadata(
    CodeCacheHost* code_cache_host,
    ClearCacheType cache_type) {
  cached_metadata_ = nullptr;
  switch (cache_type) {
    case kClearLocally:
      break;
    case kDiscardLocally:
      cached_metadata_discarded_ = true;
      break;
    case kClearPersistentStorage:
      CommitToPersistentStorage(code_cache_host);
      break;
  }
}

scoped_refptr<CachedMetadata> ScriptCachedMetadataHandler::GetCachedMetadata(
    uint32_t data_type_id,
    GetCachedMetadataBehavior behavior) const {
  if (!cached_metadata_) {
    RecordState(cached_metadata_discarded_ ? StateOnGet::kWasDiscarded
                                           : StateOnGet::kWasNeverPresent);
    return nullptr;
  }
  if (cached_metadata_->DataTypeID() != data_type_id) {
    RecordState(StateOnGet::kDataTypeMismatch);
    return nullptr;
  }
  RecordState(StateOnGet::kPresent);
  return cached_metadata_;
}

void ScriptCachedMetadataHandler::SetSerializedCachedMetadata(
    mojo_base::BigBuffer data) {
  // We only expect to receive cached metadata from the platform once. If this
  // triggers, it indicates an efficiency problem which is most likely
  // unexpected in code designed to improve performance.
  DCHECK(!cached_metadata_);
  cached_metadata_ = CachedMetadata::CreateFromSerializedData(std::move(data));
}

String ScriptCachedMetadataHandler::Encoding() const {
  return String(encoding_.GetName());
}

bool ScriptCachedMetadataHandler::IsServedFromCacheStorage() const {
  return sender_->IsServedFromCacheStorage();
}

void ScriptCachedMetadataHandler::OnMemoryDump(
    WebProcessMemoryDump* pmd,
    const String& dump_prefix) const {
  if (!cached_metadata_)
    return;
  const String dump_name = dump_prefix + "/script";
  auto* dump = pmd->CreateMemoryAllocatorDump(dump_name);
  dump->AddScalar("size", "bytes", GetCodeCacheSize());
  pmd->AddSuballocation(dump->Guid(),
                        String(WTF::Partitions::kAllocatedObjectPoolName));
}

size_t ScriptCachedMetadataHandler::GetCodeCacheSize() const {
  return (cached_metadata_) ? cached_metadata_->SerializedData().size() : 0;
}

void ScriptCachedMetadataHandler::CommitToPersistentStorage(
    CodeCacheHost* code_cache_host) {
  if (cached_metadata_) {
    base::span<const uint8_t> serialized_data =
        cached_metadata_->SerializedData();
    sender_->Send(code_cache_host, serialized_data.data(),
                  serialized_data.size());
  } else {
    sender_->Send(code_cache_host, nullptr, 0);
  }
}

void ScriptCachedMetadataHandlerWithHashing::Check(
    CodeCacheHost* code_cache_host,
    const ParkableString& source_text) {
  std::unique_ptr<ParkableStringImpl::SecureDigest> digest_holder;
  const ParkableStringImpl::SecureDigest* digest;
  // ParkableStrings have usually already computed the digest unless they're
  // quite short (see ParkableStringManager::ShouldPark), so usually we can just
  // use the pre-existing digest.
  ParkableStringImpl* impl = source_text.Impl();
  if (impl && impl->may_be_parked()) {
    digest = impl->digest();
  } else {
    const String& unparked = source_text.ToString();
    digest_holder = ParkableStringImpl::HashString(unparked.Impl());
    digest = digest_holder.get();
  }

  CHECK_EQ(digest->size(), kSha256Bytes);

  if (hash_state_ != kUninitialized) {
    // Compare the hash of the new source text with the one previously loaded.
    if (memcmp(digest->data(), hash_, kSha256Bytes) != 0) {
      // If this handler was previously checked and is now being checked again
      // with a different hash value, then something bad happened. We expect the
      // handler to only be used with one script source text.
      CHECK_NE(hash_state_, kChecked);

      // The cached metadata is invalid because the source file has changed.
      ClearCachedMetadata(code_cache_host, kClearPersistentStorage);
    }
  }

  // Remember the computed hash so that it can be used when saving data to
  // persistent storage.
  memcpy(hash_, digest->data(), kSha256Bytes);
  hash_state_ = kChecked;
}

void ScriptCachedMetadataHandlerWithHashing::SetSerializedCachedMetadata(
    mojo_base::BigBuffer data) {
  // We only expect to receive cached metadata from the platform once. If this
  // triggers, it indicates an efficiency problem which is most likely
  // unexpected in code designed to improve performance.
  DCHECK(!cached_metadata_);
  DCHECK_EQ(hash_state_, kUninitialized);

  // The kChecked state guarantees that hash_ will never be updated again.
  CHECK(hash_state_ != kChecked);

  const uint32_t kMetadataTypeSize = sizeof(uint32_t);
  const uint32_t kHashingHeaderSize = kMetadataTypeSize + kSha256Bytes;

  // Ensure the data is big enough, otherwise discard the data.
  if (data.size() < kHashingHeaderSize)
    return;
  // Ensure the marker matches, otherwise discard the data.
  if (*reinterpret_cast<const uint32_t*>(data.data()) !=
      CachedMetadataHandler::kSingleEntryWithHash) {
    return;
  }

  // Split out the data into the hash and the CachedMetadata that follows.
  memcpy(hash_, data.data() + kMetadataTypeSize, kSha256Bytes);
  hash_state_ = kDeserialized;
  cached_metadata_ = CachedMetadata::CreateFromSerializedData(
      data.data() + kHashingHeaderSize, data.size() - kHashingHeaderSize);
}

scoped_refptr<CachedMetadata>
ScriptCachedMetadataHandlerWithHashing::GetCachedMetadata(
    uint32_t data_type_id,
    GetCachedMetadataBehavior behavior) const {
  // The caller should have called Check before attempting to read the cached
  // metadata. If you just want to know whether cached metadata exists, and it's
  // okay for that metadata to possibly mismatch with the loaded script content,
  // then you can pass kAllowUnchecked as the second parameter.
  if (behavior == kCrashIfUnchecked) {
    CHECK(hash_state_ == kChecked);
  }

  scoped_refptr<CachedMetadata> result =
      ScriptCachedMetadataHandler::GetCachedMetadata(data_type_id, behavior);

  return result;
}

void ScriptCachedMetadataHandlerWithHashing::CommitToPersistentStorage(
    CodeCacheHost* code_cache_host) {
  Vector<uint8_t> serialized_data = GetSerializedCachedMetadata();
  Sender()->Send(code_cache_host, serialized_data.data(),
                 serialized_data.size());
}

Vector<uint8_t>
ScriptCachedMetadataHandlerWithHashing::GetSerializedCachedMetadata() const {
  Vector<uint8_t> serialized_data;
  if (cached_metadata_ && hash_state_ == kChecked) {
    uint32_t marker = CachedMetadataHandler::kSingleEntryWithHash;
    serialized_data.Append(reinterpret_cast<uint8_t*>(&marker), sizeof(marker));
    serialized_data.Append(hash_, kSha256Bytes);
    base::span<const uint8_t> data = cached_metadata_->SerializedData();
    serialized_data.Append(data.data(),
                           base::checked_cast<wtf_size_t>(data.size()));
  }
  return serialized_data;
}

void ScriptCachedMetadataHandlerWithHashing::ResetForTesting() {
  if (hash_state_ == kChecked)
    hash_state_ = kDeserialized;
}

}  // namespace blink
