// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_store_metadata_entry.h"

#include "base/files/file_path.h"
#include "base/json/values_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/prefs/pref_service.h"

namespace optimization_guide {

namespace {

// Key names for the metadata entries.
const char kKeyModelBaseDir[] = "mbd";
const char kKeyVersion[] = "v";
const char kKeyExpiryTime[] = "et";
const char kKeyKeepBeyondValidDuration[] = "kbvd";

// Returns the hash of server returned ModelCacheKey for
// |client_model_cache_key| from |local_state|.
std::string GetServerModelCacheKeyHash(
    PrefService* local_state,
    proto::OptimizationTarget optimization_target,
    const proto::ModelCacheKey& client_model_cache_key) {
  std::string client_model_cache_key_hash =
      GetModelCacheKeyHash(client_model_cache_key);
  auto* server_model_cache_key_hash =
      local_state->GetDict(prefs::localstate::kModelCacheKeyMapping)
          .FindString(
              base::NumberToString(static_cast<int>(optimization_target)) +
              client_model_cache_key_hash);
  if (server_model_cache_key_hash) {
    return *server_model_cache_key_hash;
  }

  // Fallback to using the client ModelCacheKey if server mapping did not
  // exist.
  return client_model_cache_key_hash;
}

}  // namespace

// static
absl::optional<ModelStoreMetadataEntry>
ModelStoreMetadataEntry::GetModelMetadataEntryIfExists(
    PrefService* local_state,
    proto::OptimizationTarget optimization_target,
    const proto::ModelCacheKey& model_cache_key) {
  auto* metadata_target =
      local_state->GetDict(prefs::localstate::kModelStoreMetadata)
          .FindDict(
              base::NumberToString(static_cast<int>(optimization_target)));
  if (!metadata_target) {
    return absl::nullopt;
  }

  auto* metadata_entry = metadata_target->FindDict(GetServerModelCacheKeyHash(
      local_state, optimization_target, model_cache_key));
  if (!metadata_entry) {
    return absl::nullopt;
  }

  return ModelStoreMetadataEntry(metadata_entry);
}

ModelStoreMetadataEntry::ModelStoreMetadataEntry(
    const base::Value::Dict* metadata_entry)
    : metadata_entry_(metadata_entry) {}

ModelStoreMetadataEntry::~ModelStoreMetadataEntry() = default;

absl::optional<base::FilePath> ModelStoreMetadataEntry::GetModelBaseDir()
    const {
  return base::ValueToFilePath(metadata_entry_->Find(kKeyModelBaseDir));
}

absl::optional<int64_t> ModelStoreMetadataEntry::GetVersion() const {
  auto* version_str = metadata_entry_->FindString(kKeyVersion);
  if (!version_str) {
    return absl::nullopt;
  }
  int64_t version;
  if (!base::StringToInt64(*version_str, &version)) {
    return absl::nullopt;
  }
  return version;
}

base::Time ModelStoreMetadataEntry::GetExpiryTime() const {
  return base::ValueToTime(metadata_entry_->Find(kKeyExpiryTime))
      .value_or(base::Time::Now() + features::StoredModelsValidDuration());
}

bool ModelStoreMetadataEntry::GetKeepBeyondValidDuration() const {
  return metadata_entry_->FindBool(kKeyKeepBeyondValidDuration).value_or(false);
}

void ModelStoreMetadataEntry::SetMetadataEntry(
    const base::Value::Dict* metadata_entry) {
  metadata_entry_ = metadata_entry;
}

// static
void ModelStoreMetadataEntryUpdater::UpdateModelCacheKeyMapping(
    PrefService* local_state,
    proto::OptimizationTarget optimization_target,
    const proto::ModelCacheKey& client_model_cache_key,
    const proto::ModelCacheKey& server_model_cache_key) {
  ScopedDictPrefUpdate pref_updater(local_state,
                                    prefs::localstate::kModelCacheKeyMapping);
  pref_updater->Set(
      base::NumberToString(static_cast<int>(optimization_target)) +
          GetModelCacheKeyHash(client_model_cache_key),
      GetModelCacheKeyHash(server_model_cache_key));
}

ModelStoreMetadataEntryUpdater::ModelStoreMetadataEntryUpdater(
    PrefService* local_state,
    proto::OptimizationTarget optimization_target,
    const proto::ModelCacheKey& model_cache_key)
    : ModelStoreMetadataEntry(/*metadata_entry=*/nullptr),
      pref_updater_(local_state, prefs::localstate::kModelStoreMetadata) {
  auto* metadata_target = pref_updater_->EnsureDict(
      base::NumberToString(static_cast<int>(optimization_target)));
  metadata_entry_updater_ =
      metadata_target->EnsureDict(GetServerModelCacheKeyHash(
          local_state, optimization_target, model_cache_key));
  SetMetadataEntry(metadata_entry_updater_);
}

void ModelStoreMetadataEntryUpdater::SetModelBaseDir(
    base::FilePath model_base_dir) {
  metadata_entry_updater_->Set(kKeyModelBaseDir,
                               base::FilePathToValue(model_base_dir));
}

void ModelStoreMetadataEntryUpdater::SetVersion(int64_t version) {
  metadata_entry_updater_->Set(kKeyVersion, base::NumberToString(version));
}

void ModelStoreMetadataEntryUpdater::SetExpiryTime(base::Time expiry_time) {
  metadata_entry_updater_->Set(kKeyExpiryTime, base::TimeToValue(expiry_time));
}

void ModelStoreMetadataEntryUpdater::SetKeepBeyondValidDuration(
    bool keep_beyond_valid_duration) {
  metadata_entry_updater_->Set(kKeyKeepBeyondValidDuration,
                               keep_beyond_valid_duration);
}

void ModelStoreMetadataEntryUpdater::ClearMetadata() {
  metadata_entry_updater_->clear();
}

std::vector<base::FilePath>
ModelStoreMetadataEntryUpdater::PurgeAllInactiveMetadata(
    PrefService* local_state) {
  ScopedDictPrefUpdate updater(local_state,
                               prefs::localstate::kModelStoreMetadata);
  std::vector<base::FilePath> inactive_model_dirs;
  for (auto optimization_target_entry : *updater) {
    if (!optimization_target_entry.second.is_dict()) {
      continue;
    }
    for (auto model_cache_key_hash :
         optimization_target_entry.second.GetDict()) {
      if (!model_cache_key_hash.second.is_dict()) {
        continue;
      }
      bool should_remove_model =
          switches::ShouldPurgeModelAndFeaturesStoreOnStartup();

      // Check if the model expired.
      auto metadata =
          ModelStoreMetadataEntry(&model_cache_key_hash.second.GetDict());
      if (!should_remove_model && !metadata.GetKeepBeyondValidDuration() &&
          metadata.GetExpiryTime() <= base::Time::Now()) {
        should_remove_model = true;
        RecordPredictionModelStoreModelRemovalVersionHistogram(
            PredictionModelStoreModelRemovalReason::kModelExpired);
      }

      if (should_remove_model) {
        auto base_model_dir = metadata.GetModelBaseDir();
        if (base_model_dir) {
          inactive_model_dirs.emplace_back(*base_model_dir);
        }
        optimization_target_entry.second.GetDict().Remove(
            model_cache_key_hash.first);
      }
    }
  }
  return inactive_model_dirs;
}

}  // namespace optimization_guide