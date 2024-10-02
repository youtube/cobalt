// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/signal_database_impl.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/trace_event/typed_macros.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/database/signal_key.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/proto/signal.pb.h"
#include "components/segmentation_platform/internal/stats.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform {
namespace {

BASE_FEATURE(kSegmentationCompactionFix,
             "SegmentationCompactionFix",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(shaktisahu): May be make this a class member for ease of testing.
bool FilterKeyBasedOnRange(proto::SignalType signal_type,
                           uint64_t name_hash,
                           base::Time end_time,
                           base::Time start_time,
                           const std::string& signal_key) {
  DCHECK(start_time <= end_time);
  SignalKey key;
  if (!SignalKey::FromBinary(signal_key, &key))
    return false;
  DCHECK(key.IsValid());
  if (key.kind() != metadata_utils::SignalTypeToSignalKind(signal_type) ||
      key.name_hash() != name_hash) {
    return false;
  }

  // Check if the key range is contained within the given range.
  return key.range_end() <= end_time && start_time <= key.range_start();
}

leveldb_proto::Enums::KeyIteratorAction GetSamplesIteratorController(
    const SignalKey& start_key,
    base::Time end_time,
    const std::string& key_bytes) {
  SignalKey key;
  if (!SignalKey::FromBinary(key_bytes, &key))
    return leveldb_proto::Enums::kSkipAndStop;
  DCHECK(key.IsValid());
  if (start_key.kind() != key.kind() ||
      start_key.name_hash() != key.name_hash()) {
    // This key is for a different signal.
    return leveldb_proto::Enums::kSkipAndStop;
  }
  if (key.range_start() > end_time) {
    // All samples under this key are too fresh.
    return leveldb_proto::Enums::kSkipAndStop;
  }
  if (key.range_end() > end_time) {
    // This is the last key with relevant samples.
    return leveldb_proto::Enums::kLoadAndStop;
  }
  return leveldb_proto::Enums::kLoadAndContinue;
}

}  // namespace

SignalDatabaseImpl::SignalDatabaseImpl(std::unique_ptr<SignalProtoDb> database,
                                       base::Clock* clock)
    : database_(std::move(database)),
      clock_(clock),
      should_fix_compaction_(
          base::FeatureList::IsEnabled(kSegmentationCompactionFix)) {}

SignalDatabaseImpl::~SignalDatabaseImpl() = default;

void SignalDatabaseImpl::Initialize(SuccessCallback callback) {
  database_->Init(
      leveldb_proto::CreateSimpleOptions(),
      base::BindOnce(&SignalDatabaseImpl::OnDatabaseInitialized,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SignalDatabaseImpl::WriteSample(proto::SignalType signal_type,
                                     uint64_t name_hash,
                                     absl::optional<int32_t> value,
                                     SuccessCallback callback) {
  DCHECK(initialized_);
  base::Time timestamp = clock_->Now();
  SignalKey key(metadata_utils::SignalTypeToSignalKind(signal_type), name_hash,
                timestamp, timestamp);

  proto::SignalData signal_data;
  // If there is another sample with the same signal key, collate both into a
  // single DB entry.
  if (recently_added_signals_.find(key) != recently_added_signals_.end())
    signal_data = recently_added_signals_[key];

  proto::Sample* sample = signal_data.add_samples();
  if (value.has_value())
    sample->set_value(value.value());

  // Convert to delta from UTC midnight. This results in smaller values thereby
  // requiring less storage space in the DB.
  base::TimeDelta midnight_delta = timestamp - timestamp.UTCMidnight();
  sample->set_time_sec_delta(midnight_delta.InSeconds());

  recently_added_signals_[key] = signal_data;

  // Write as a new db entry.
  auto entries_to_save = std::make_unique<
      std::vector<std::pair<std::string, proto::SignalData>>>();
  auto keys_to_delete = std::make_unique<std::vector<std::string>>();
  entries_to_save->emplace_back(
      std::make_pair(key.ToBinary(), std::move(signal_data)));
  database_->UpdateEntries(std::move(entries_to_save),
                           std::move(keys_to_delete), std::move(callback));
  CleanupStaleCachedEntries(timestamp);
}

void SignalDatabaseImpl::GetSamples(proto::SignalType signal_type,
                                    uint64_t name_hash,
                                    base::Time start_time,
                                    base::Time end_time,
                                    SamplesCallback callback) {
  TRACE_EVENT("segmentation_platform", "SignalDatabaseImpl::GetSamples");
  DCHECK(initialized_);
  DCHECK_LE(start_time, end_time);
  const SignalKey start_key(metadata_utils::SignalTypeToSignalKind(signal_type),
                            name_hash, start_time, base::Time());
  database_->LoadKeysAndEntriesWhile(
      start_key.GetPrefixInBinary(),
      base::BindRepeating(&GetSamplesIteratorController, start_key, end_time),
      base::BindOnce(&SignalDatabaseImpl::OnGetSamples,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     start_time, end_time));
}

void SignalDatabaseImpl::OnGetSamples(
    SamplesCallback callback,
    base::Time start_time,
    base::Time end_time,
    bool success,
    std::unique_ptr<std::map<std::string, proto::SignalData>> entries) {
  TRACE_EVENT("segmentation_platform", "SignalDatabaseImpl::OnGetSamples");
  std::vector<Sample> out;
  if (!success || !entries) {
    stats::RecordSignalDatabaseGetSamplesResult(/* success = */ false);
    std::move(callback).Run(out);
    return;
  }
  stats::RecordSignalDatabaseGetSamplesResult(/* success = */ true);

  stats::RecordSignalDatabaseGetSamplesDatabaseEntryCount(
      entries.get()->size());
  for (const auto& pair : *entries.get()) {
    SignalKey key;
    if (!SignalKey::FromBinary(pair.first, &key))
      continue;
    DCHECK(key.IsValid());
    // TODO(shaktisahu): Remove DCHECK and collect UMA.
    const auto& signal_data = pair.second;
    base::Time midnight = key.range_start().UTCMidnight();
    for (int i = 0; i < signal_data.samples_size(); ++i) {
      const auto& sample = signal_data.samples(i);
      base::Time timestamp = midnight + base::Seconds(sample.time_sec_delta());
      if (timestamp < start_time || timestamp > end_time)
        continue;

      out.emplace_back(std::make_pair(timestamp, sample.value()));
    }
  }

  stats::RecordSignalDatabaseGetSamplesSampleCount(out.size());
  std::move(callback).Run(out);
}

void SignalDatabaseImpl::DeleteSamples(proto::SignalType signal_type,
                                       uint64_t name_hash,
                                       base::Time end_time,
                                       SuccessCallback callback) {
  TRACE_EVENT("segmentation_platform", "SignalDatabaseImpl::DeleteSamples");
  DCHECK(initialized_);
  SignalKey dummy_key(metadata_utils::SignalTypeToSignalKind(signal_type),
                      name_hash, base::Time(), base::Time());
  std::string key_prefix = dummy_key.GetPrefixInBinary();
  database_->LoadKeysAndEntriesWithFilter(
      base::BindRepeating(&FilterKeyBasedOnRange, signal_type, name_hash,
                          end_time, base::Time()),
      leveldb::ReadOptions(), key_prefix,
      base::BindOnce(&SignalDatabaseImpl::OnGetSamplesForDeletion,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SignalDatabaseImpl::OnGetSamplesForDeletion(
    SuccessCallback callback,
    bool success,
    std::unique_ptr<std::map<std::string, proto::SignalData>> entries) {
  TRACE_EVENT("segmentation_platform",
              "SignalDatabaseImpl::OnGetSamplesForDeletion");
  if (!success || !entries) {
    std::move(callback).Run(success);
    return;
  }

  auto entries_to_save = std::make_unique<
      std::vector<std::pair<std::string, proto::SignalData>>>();
  auto keys_to_delete = std::make_unique<std::vector<std::string>>();

  // Collect the keys to be deleted.
  for (const auto& pair : *entries.get()) {
    keys_to_delete->emplace_back(pair.first);
  }

  // Write to DB.
  database_->UpdateEntries(std::move(entries_to_save),
                           std::move(keys_to_delete), std::move(callback));
}

void SignalDatabaseImpl::CompactSamplesForDay(proto::SignalType signal_type,
                                              uint64_t name_hash,
                                              base::Time day_end_time,
                                              SuccessCallback callback) {
  TRACE_EVENT("segmentation_platform",
              "SignalDatabaseImpl::CompactSamplesForDay");
  DCHECK(initialized_);
  // Compact the signals between 00:00:00AM to 23:59:59PM.
  base::Time day_start_time = day_end_time.UTCMidnight();
  SignalKey compact_key(metadata_utils::SignalTypeToSignalKind(signal_type),
                        name_hash, day_end_time, day_start_time);
  database_->LoadKeysAndEntriesWithFilter(
      base::BindRepeating(&FilterKeyBasedOnRange, signal_type, name_hash,
                          day_end_time, day_start_time),
      base::BindOnce(&SignalDatabaseImpl::OnGetSamplesForCompaction,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     compact_key.ToBinary()));
}

void SignalDatabaseImpl::OnGetSamplesForCompaction(
    SuccessCallback callback,
    std::string compact_key,
    bool success,
    std::unique_ptr<std::map<std::string, proto::SignalData>> entries) {
  TRACE_EVENT("segmentation_platform",
              "SignalDatabaseImpl::OnGetSamplesForCompaction");
  if (!success || !entries || entries->empty() ||
      (should_fix_compaction_ && entries->size() == 1)) {
    std::move(callback).Run(success);
    return;
  }

  // We found one or more entries for the day. Let's compact them.
  auto keys_to_delete = std::make_unique<std::vector<std::string>>();

  // Aggregate samples under a new proto. Delete the old entries.
  proto::SignalData compact;
  for (const auto& pair : *entries.get()) {
    const auto& signal_data = pair.second;
    for (int i = 0; i < signal_data.samples_size(); i++) {
      auto* new_sample = compact.add_samples();
      new_sample->CopyFrom(signal_data.samples(i));
    }

    // If the database was already compacted, and some entry was added with
    // older timestamp, then append signals, and do not delete the key.
    if (!(should_fix_compaction_ && pair.first == compact_key)) {
      keys_to_delete->emplace_back(pair.first);
    }
  }

  // Write to DB.
  auto entries_to_save = std::make_unique<
      std::vector<std::pair<std::string, proto::SignalData>>>();
  entries_to_save->emplace_back(
      std::make_pair(compact_key, std::move(compact)));
  database_->UpdateEntries(std::move(entries_to_save),
                           std::move(keys_to_delete), std::move(callback));
}

void SignalDatabaseImpl::OnDatabaseInitialized(
    SuccessCallback callback,
    leveldb_proto::Enums::InitStatus status) {
  initialized_ = status == leveldb_proto::Enums::InitStatus::kOK;
  std::move(callback).Run(status == leveldb_proto::Enums::InitStatus::kOK);
}

void SignalDatabaseImpl::CleanupStaleCachedEntries(
    base::Time current_timestamp) {
  base::Time prev_second = current_timestamp - base::Seconds(1);
  std::vector<SignalKey> keys_to_delete;
  for (const auto& entry : recently_added_signals_) {
    if (entry.first.range_end() < prev_second)
      keys_to_delete.emplace_back(entry.first);
  }
  for (const auto& cache_key : keys_to_delete)
    recently_added_signals_.erase(cache_key);
}

}  // namespace segmentation_platform
