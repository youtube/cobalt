// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/store_update_data.h"

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_store.h"
#include "components/optimization_guide/proto/hint_cache.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace optimization_guide {

// static
std::unique_ptr<StoreUpdateData>
StoreUpdateData::CreateComponentStoreUpdateData(
    const base::Version& component_version) {
  return base::WrapUnique<StoreUpdateData>(new StoreUpdateData(
      absl::optional<base::Version>(component_version),
      absl::optional<base::Time>(), absl::optional<base::Time>()));
}

// static
std::unique_ptr<StoreUpdateData> StoreUpdateData::CreateFetchedStoreUpdateData(
    base::Time fetch_update_time) {
  return base::WrapUnique<StoreUpdateData>(
      new StoreUpdateData(absl::optional<base::Version>(),
                          absl::optional<base::Time>(fetch_update_time),
                          absl::optional<base::Time>()));
}

// static
std::unique_ptr<StoreUpdateData>
StoreUpdateData::CreatePredictionModelStoreUpdateData(base::Time expiry_time) {
  return base::WrapUnique<StoreUpdateData>(new StoreUpdateData(expiry_time));
}

StoreUpdateData::StoreUpdateData(base::Time expiry_time)
    : expiry_time_(expiry_time),
      entries_to_save_(std::make_unique<EntryVector>()) {
  entry_key_prefix_ =
      OptimizationGuideStore::GetPredictionModelEntryKeyPrefix();

  // |this| may be modified on another thread after construction but all
  // future modifications, from that call forward, must be made on the same
  // thread.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

StoreUpdateData::StoreUpdateData(
    absl::optional<base::Version> component_version,
    absl::optional<base::Time> fetch_update_time,
    absl::optional<base::Time> expiry_time)
    : component_version_(component_version),
      update_time_(fetch_update_time),
      expiry_time_(expiry_time),
      entries_to_save_(std::make_unique<EntryVector>()) {
  DCHECK_NE(!component_version_, !update_time_);

  if (component_version_.has_value()) {
    entry_key_prefix_ = OptimizationGuideStore::GetComponentHintEntryKeyPrefix(
        *component_version_);

    // Add a component metadata entry for the component's version.
    proto::StoreEntry metadata_component_entry;

    metadata_component_entry.set_entry_type(static_cast<proto::StoreEntryType>(
        OptimizationGuideStore::StoreEntryType::kMetadata));
    metadata_component_entry.set_version(component_version_->GetString());
    entries_to_save_->emplace_back(
        OptimizationGuideStore::GetMetadataTypeEntryKey(
            OptimizationGuideStore::MetadataType::kComponent),
        std::move(metadata_component_entry));
  } else if (update_time_.has_value()) {
    entry_key_prefix_ = OptimizationGuideStore::GetFetchedHintEntryKeyPrefix();
    proto::StoreEntry metadata_fetched_entry;
    metadata_fetched_entry.set_entry_type(static_cast<proto::StoreEntryType>(
        OptimizationGuideStore::StoreEntryType::kMetadata));
    metadata_fetched_entry.set_update_time_secs(
        update_time_->ToDeltaSinceWindowsEpoch().InSeconds());
    entries_to_save_->emplace_back(
        OptimizationGuideStore::GetMetadataTypeEntryKey(
            OptimizationGuideStore::MetadataType::kFetched),
        std::move(metadata_fetched_entry));
  } else {
    NOTREACHED();
  }
  // |this| may be modified on another thread after construction but all
  // future modifications, from that call forward, must be made on the same
  // thread.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

StoreUpdateData::~StoreUpdateData() = default;

void StoreUpdateData::MoveHintIntoUpdateData(proto::Hint&& hint) {
  // All future modifications must be made by the same thread. Note, |this| may
  // have been constructed on another thread.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!entry_key_prefix_.empty());

  // To avoid any unnecessary copying, the hint is moved into proto::StoreEntry.
  OptimizationGuideStore::EntryKey hint_entry_key =
      entry_key_prefix_ + hint.key();
  proto::StoreEntry entry_proto;
  if (component_version()) {
    entry_proto.set_entry_type(static_cast<proto::StoreEntryType>(
        OptimizationGuideStore::StoreEntryType::kComponentHint));
  } else if (update_time()) {
    base::TimeDelta expiry_duration;
    if (hint.has_max_cache_duration()) {
      expiry_duration = base::Seconds(hint.max_cache_duration().seconds());
    } else {
      expiry_duration = features::StoredFetchedHintsFreshnessDuration();
    }
    entry_proto.set_expiry_time_secs((base::Time::Now() + expiry_duration)
                                         .ToDeltaSinceWindowsEpoch()
                                         .InSeconds());
    entry_proto.set_entry_type(static_cast<proto::StoreEntryType>(
        OptimizationGuideStore::StoreEntryType::kFetchedHint));
  }
  entry_proto.set_allocated_hint(new proto::Hint(std::move(hint)));
  entries_to_save_->emplace_back(std::move(hint_entry_key),
                                 std::move(entry_proto));
}

void StoreUpdateData::CopyPredictionModelIntoUpdateData(
    const proto::PredictionModel& prediction_model) {
  // All future modifications must be made by the same thread. Note, |this| may
  // have been constructed on another thread.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!entry_key_prefix_.empty());
  DCHECK(expiry_time_);

  // To avoid any unnecessary copying, the prediction model is moved into
  // proto::StoreEntry.
  OptimizationGuideStore::EntryKey prediction_model_entry_key =
      entry_key_prefix_ +
      base::NumberToString(static_cast<int>(
          prediction_model.model_info().optimization_target()));
  proto::StoreEntry entry_proto;
  entry_proto.set_entry_type(static_cast<proto::StoreEntryType>(
      OptimizationGuideStore::StoreEntryType::kPredictionModel));

  base::TimeDelta expiry_duration;
  if (prediction_model.model_info().has_valid_duration()) {
    expiry_duration =
        base::Seconds(prediction_model.model_info().valid_duration().seconds());
  } else {
    expiry_duration = features::StoredModelsValidDuration();
  }
  expiry_time_ = base::Time::Now() + expiry_duration;
  entry_proto.set_expiry_time_secs(
      expiry_time_.value().ToDeltaSinceWindowsEpoch().InSeconds());
  entry_proto.set_keep_beyond_valid_duration(
      prediction_model.model_info().keep_beyond_valid_duration());
  entry_proto.mutable_prediction_model()->CopyFrom(prediction_model);
  entries_to_save_->emplace_back(std::move(prediction_model_entry_key),
                                 std::move(entry_proto));
}

std::unique_ptr<EntryVector> StoreUpdateData::TakeUpdateEntries() {
  // TakeUpdateEntries is not be sequence checked as it only gives up ownership
  // of the entries_to_save_ and does not modify any state.
  DCHECK(entries_to_save_);

  return std::move(entries_to_save_);
}

}  // namespace optimization_guide
