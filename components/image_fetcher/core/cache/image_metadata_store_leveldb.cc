// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/cache/image_metadata_store_leveldb.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/image_fetcher/core/cache/proto/cached_image_metadata.pb.h"
#include "components/image_fetcher/core/image_fetcher_metrics_reporter.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

using image_fetcher::CachedImageMetadataProto;

// TODO(wylieb): Emit a histogram for various failure cases. Find a way to
// share it with other implementations.
namespace image_fetcher {

namespace {

leveldb::ReadOptions CreateReadOptions() {
  leveldb::ReadOptions opts;
  opts.fill_cache = false;
  return opts;
}

int64_t ToDatabaseTime(base::Time time) {
  return time.since_origin().InMicroseconds();
}

CacheOption ToCacheOption(CacheStrategy cache_strategy) {
  switch (cache_strategy) {
    case CacheStrategy::BEST_EFFORT:
      return CacheOption::kBestEffort;
    case CacheStrategy::HOLD_UNTIL_EXPIRED:
      return CacheOption::kHoldUntilExpired;
  }
}

// The folder where the data will be stored on disk.
const char kImageDatabaseFolder[] = "cached_image_fetcher_images";

// Most writes are very small, <100 bytes. This should buffer a handful of them
// together, but not much more. This should allow a handful of them to be
// buffered together without causing a significant impact to memory.
const size_t kDatabaseWriteBufferSizeBytes = 1 * 1024;  // 1KB

bool KeyMatcherFilter(std::string key, const std::string& other_key) {
  return key.compare(other_key) == 0;
}

bool SortByLastUsedTime(const CachedImageMetadataProto* a,
                        const CachedImageMetadataProto* b) {
  return a->last_used_time() < b->last_used_time();
}

}  // namespace

using MetadataKeyEntryVector =
    leveldb_proto::ProtoDatabase<CachedImageMetadataProto>::KeyEntryVector;

ImageMetadataStoreLevelDB::ImageMetadataStoreLevelDB(
    leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
    const base::FilePath& database_dir,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::Clock* clock)
    : ImageMetadataStoreLevelDB(
          proto_database_provider->GetDB<CachedImageMetadataProto>(
              leveldb_proto::ProtoDbType::CACHED_IMAGE_METADATA_STORE,
              database_dir.AppendASCII(kImageDatabaseFolder),
              task_runner),
          clock) {}

ImageMetadataStoreLevelDB::ImageMetadataStoreLevelDB(
    std::unique_ptr<leveldb_proto::ProtoDatabase<CachedImageMetadataProto>>
        database,
    base::Clock* clock)
    : initialization_status_(InitializationStatus::UNINITIALIZED),
      database_(std::move(database)),
      clock_(clock) {}

ImageMetadataStoreLevelDB::~ImageMetadataStoreLevelDB() = default;

void ImageMetadataStoreLevelDB::Initialize(base::OnceClosure callback) {
  leveldb_env::Options options = leveldb_proto::CreateSimpleOptions();
  options.write_buffer_size = kDatabaseWriteBufferSizeBytes;

  database_->Init(
      options,
      base::BindOnce(&ImageMetadataStoreLevelDB::OnDatabaseInitialized,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

bool ImageMetadataStoreLevelDB::IsInitialized() {
  return initialization_status_ == InitializationStatus::INITIALIZED ||
         initialization_status_ == InitializationStatus::INIT_FAILURE;
}

void ImageMetadataStoreLevelDB::LoadImageMetadata(
    const std::string& key,
    ImageMetadataCallback callback) {
  DCHECK(IsInitialized());

  database_->LoadEntriesWithFilter(
      base::BindRepeating(&KeyMatcherFilter, key), CreateReadOptions(),
      /* target_prefix */ "",
      base::BindOnce(&ImageMetadataStoreLevelDB::LoadImageMetadataImpl,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ImageMetadataStoreLevelDB::SaveImageMetadata(
    const std::string& key,
    const size_t data_size,
    bool needs_transcoding,
    ExpirationInterval expiration_interval) {
  // If the database is not initialized yet, ignore the request.
  if (!IsInitialized()) {
    return;
  }

  int64_t current_time = ToDatabaseTime(clock_->Now());
  CachedImageMetadataProto metadata_proto;
  metadata_proto.set_key(key);
  metadata_proto.set_data_size(data_size);
  metadata_proto.set_creation_time(current_time);
  metadata_proto.set_last_used_time(current_time);
  metadata_proto.set_needs_transcoding(needs_transcoding);
  if (expiration_interval.has_value()) {
    metadata_proto.set_cache_strategy(CacheStrategy::HOLD_UNTIL_EXPIRED);
    metadata_proto.set_expiration_interval(
        expiration_interval->InMicroseconds());
    estimated_size_[CacheOption::kHoldUntilExpired] += data_size;
  } else {
    estimated_size_[CacheOption::kBestEffort] += data_size;
  }

  auto entries_to_save = std::make_unique<MetadataKeyEntryVector>();
  entries_to_save->emplace_back(key, metadata_proto);
  database_->UpdateEntries(
      std::move(entries_to_save), std::make_unique<std::vector<std::string>>(),
      base::BindOnce(&ImageMetadataStoreLevelDB::OnImageUpdated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ImageMetadataStoreLevelDB::DeleteImageMetadata(const std::string& key) {
  // If the database is not initialized yet, ignore the request.
  if (!IsInitialized()) {
    return;
  }

  database_->UpdateEntries(
      std::make_unique<MetadataKeyEntryVector>(),
      std::make_unique<std::vector<std::string>>(
          std::initializer_list<std::string>({key})),
      base::BindOnce(&ImageMetadataStoreLevelDB::OnImageUpdated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ImageMetadataStoreLevelDB::UpdateImageMetadata(const std::string& key) {
  // If the database is not initialized yet, ignore the request.
  if (!IsInitialized()) {
    return;
  }

  database_->LoadEntriesWithFilter(
      base::BindRepeating(&KeyMatcherFilter, key), CreateReadOptions(),
      /* target_prefix */ "",
      base::BindOnce(&ImageMetadataStoreLevelDB::UpdateImageMetadataImpl,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ImageMetadataStoreLevelDB::GetAllKeys(KeysCallback callback) {
  // If the database is not initialized yet, ignore the request.
  if (!IsInitialized()) {
    std::move(callback).Run({});
    return;
  }

  database_->LoadKeys(base::BindOnce(&ImageMetadataStoreLevelDB::GetAllKeysImpl,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     std::move(callback)));
}

int64_t ImageMetadataStoreLevelDB::GetEstimatedSize(CacheOption cache_option) {
  return estimated_size_[cache_option];
}

void ImageMetadataStoreLevelDB::EvictImageMetadata(base::Time expiration_time,
                                                   const size_t bytes_left,
                                                   KeysCallback callback) {
  // If the database is not initialized yet, ignore the request.
  if (!IsInitialized()) {
    std::move(callback).Run({});
    return;
  }

  database_->LoadEntries(
      base::BindOnce(&ImageMetadataStoreLevelDB::EvictImageMetadataImpl,
                     weak_ptr_factory_.GetWeakPtr(), expiration_time,
                     bytes_left, std::move(callback)));
}

void ImageMetadataStoreLevelDB::OnDatabaseInitialized(
    base::OnceClosure callback,
    leveldb_proto::Enums::InitStatus status) {
  initialization_status_ = status == leveldb_proto::Enums::InitStatus::kOK
                               ? InitializationStatus::INITIALIZED
                               : InitializationStatus::INIT_FAILURE;
  std::move(callback).Run();
}

void ImageMetadataStoreLevelDB::LoadImageMetadataImpl(
    ImageMetadataCallback callback,
    bool success,
    std::unique_ptr<std::vector<CachedImageMetadataProto>> entries) {
  if (!success || entries->size() == 0) {
    std::move(callback).Run(CachedImageMetadataProto());
    return;
  }

  DCHECK(entries->size() == 1);
  std::move(callback).Run(std::move(entries->at(0)));
}

void ImageMetadataStoreLevelDB::OnImageUpdated(bool success) {
  DVLOG_IF(1, !success) << "ImageMetadataStoreLevelDB update failed.";
}

void ImageMetadataStoreLevelDB::UpdateImageMetadataImpl(
    bool success,
    std::unique_ptr<std::vector<CachedImageMetadataProto>> entries) {
  // If there was a failure, or an entry wasn't found then there's nothing left
  // to do.
  if (!success || entries->size() != 1) {
    return;
  }

  CachedImageMetadataProto metadata_proto = (*entries)[0];
  metadata_proto.set_last_used_time(ToDatabaseTime(clock_->Now()));

  auto entries_to_save = std::make_unique<MetadataKeyEntryVector>();
  entries_to_save->emplace_back(metadata_proto.key(), metadata_proto);
  database_->UpdateEntries(
      std::move(entries_to_save), std::make_unique<std::vector<std::string>>(),
      base::BindOnce(&ImageMetadataStoreLevelDB::OnImageUpdated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ImageMetadataStoreLevelDB::GetAllKeysImpl(
    KeysCallback callback,
    bool success,
    std::unique_ptr<std::vector<std::string>> keys) {
  if (!success) {
    std::move(callback).Run({});
    return;
  }

  std::move(callback).Run(*keys.get());
}

void ImageMetadataStoreLevelDB::EvictImageMetadataImpl(
    base::Time expiration_time,
    const size_t bytes_left,
    KeysCallback callback,
    bool success,
    std::unique_ptr<std::vector<CachedImageMetadataProto>> entries) {
  // In the case where the entries failed to load (LevelDB error), then do
  // nothing.
  if (!success) {
    std::move(callback).Run({});
    return;
  }

  std::vector<std::string> keys_to_remove;
  std::map<CacheOption, std::vector<const CachedImageMetadataProto*>>
      entries_map;
  for (const CachedImageMetadataProto& entry : *entries) {
    entries_map[ToCacheOption(entry.cache_strategy())].push_back(&entry);
  }

  for (auto& cache_strategy : entries_map) {
    GetMetadataToRemove(cache_strategy.first, std::move(cache_strategy.second),
                        expiration_time, bytes_left, &keys_to_remove);
  }

  if (keys_to_remove.empty()) {
    std::move(callback).Run({});
    return;
  }

  database_->UpdateEntries(
      std::make_unique<MetadataKeyEntryVector>(),
      std::make_unique<std::vector<std::string>>(keys_to_remove),
      base::BindOnce(&ImageMetadataStoreLevelDB::OnEvictImageMetadataDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     keys_to_remove));
}

void ImageMetadataStoreLevelDB::GetMetadataToRemove(
    CacheOption cache_option,
    std::vector<const CachedImageMetadataProto*> entries,
    base::Time expiration_time,
    const size_t bytes_left,
    std::vector<std::string>* keys_to_remove) {
  DCHECK(keys_to_remove);
  size_t total_bytes_stored = 0;
  int64_t expiration_database_time = ToDatabaseTime(expiration_time);
  int total_entries_count = 0;

  switch (cache_option) {
    case CacheOption::kBestEffort:
      // Removes expired entries.
      for (const CachedImageMetadataProto* entry : entries) {
        DCHECK_EQ(entry->cache_strategy(), CacheStrategy::BEST_EFFORT);
        if (entry->creation_time() <= expiration_database_time) {
          keys_to_remove->emplace_back(entry->key());
        } else {
          total_bytes_stored += entry->data_size();
          total_entries_count++;
        }
      }

      // Only sort and remove more if the byte limit isn't satisfied.
      if (total_bytes_stored > bytes_left) {
        std::sort(entries.begin(), entries.end(), SortByLastUsedTime);
        for (const CachedImageMetadataProto* entry : entries) {
          if (total_bytes_stored <= bytes_left) {
            break;
          }

          keys_to_remove->emplace_back(entry->key());
          total_bytes_stored -= entry->data_size();
          total_entries_count--;
        }
      }
      break;
    case CacheOption::kHoldUntilExpired:
      int64_t now = ToDatabaseTime(clock_->Now());
      for (const auto* entry : entries) {
        DCHECK_EQ(entry->cache_strategy(), CacheStrategy::HOLD_UNTIL_EXPIRED);
        DCHECK(entry->has_expiration_interval());
        if (entry->last_used_time() + entry->expiration_interval() < now) {
          keys_to_remove->push_back(entry->key());
        } else {
          total_bytes_stored += entry->data_size();
          total_entries_count++;
        }
      }
      break;
  }

  estimated_size_[cache_option] = total_bytes_stored;
  ImageFetcherMetricsReporter::ReportCacheStatus(
      cache_option, total_bytes_stored, total_entries_count);
}

void ImageMetadataStoreLevelDB::OnEvictImageMetadataDone(
    KeysCallback callback,
    std::vector<std::string> deleted_keys,
    bool success) {
  // If the entries have failed to update due to some LevelDB error, return
  // an empty list.
  if (!success) {
    std::move(callback).Run({});
    return;
  }

  std::move(callback).Run(deleted_keys);
}

}  // namespace image_fetcher
