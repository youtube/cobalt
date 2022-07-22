// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/persistent_storage/persistent_settings.h"

#include <utility>
#include <vector>

#include "base/values.h"
#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"

namespace cobalt {
namespace persistent_storage {

namespace {

// Protected persistent settings key indicating whether or not the persistent
// settings file has been validated.
const char kValidated[] = "validated";

}  // namespace

PersistentSettings::PersistentSettings(
    const std::string& file_name,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  std::vector<char> storage_dir(kSbFileMaxPath + 1, 0);
  SbSystemGetPath(kSbSystemPathStorageDirectory, storage_dir.data(),
                  kSbFileMaxPath);

  persistent_settings_file_ =
      std::string(storage_dir.data()) + kSbFileSepString + file_name;
  SB_LOG(INFO) << "Persistent settings file path: "
               << persistent_settings_file_;

  task_runner_ = task_runner;

  pref_store_ = base::MakeRefCounted<JsonPrefStore>(
      base::FilePath(persistent_settings_file_));
  pref_store_->ReadPrefs();

  validated_initial_settings_ = GetPersistentSettingAsBool(kValidated, false);
  if (!validated_initial_settings_)
    starboard::SbFileDeleteRecursive(persistent_settings_file_.c_str(), true);
}

void PersistentSettings::ValidatePersistentSettings() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PersistentSettings::ValidatePersistentSettingsHelper,
                     base::Unretained(this)));
}

void PersistentSettings::ValidatePersistentSettingsHelper() {
  if (!validated_initial_settings_) {
    pref_store_->SetValue(kValidated, std::make_unique<base::Value>(true),
                          WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    pref_store_->CommitPendingWrite();
    validated_initial_settings_ = true;
  }
}

bool PersistentSettings::GetPersistentSettingAsBool(const std::string& key,
                                                    bool default_setting) {
  auto persistent_settings = pref_store_->GetValues();
  const base::Value* result = persistent_settings->FindKey(key);
  if (result && result->is_bool()) return result->GetBool();
  SB_LOG(INFO) << "Persistent setting does not exist: " << key;
  return default_setting;
}

int PersistentSettings::GetPersistentSettingAsInt(const std::string& key,
                                                  int default_setting) {
  auto persistent_settings = pref_store_->GetValues();
  const base::Value* result = persistent_settings->FindKey(key);
  if (result && result->is_int()) return result->GetInt();
  SB_LOG(INFO) << "Persistent setting does not exist: " << key;
  return default_setting;
}

double PersistentSettings::GetPersistentSettingAsDouble(
    const std::string& key, double default_setting) {
  auto persistent_settings = pref_store_->GetValues();
  const base::Value* result = persistent_settings->FindKey(key);
  if (result && result->is_double()) return result->GetDouble();
  SB_LOG(INFO) << "Persistent setting does not exist: " << key;
  return default_setting;
}

std::string PersistentSettings::GetPersistentSettingAsString(
    const std::string& key, const std::string& default_setting) {
  auto persistent_settings = pref_store_->GetValues();
  const base::Value* result = persistent_settings->FindKey(key);
  if (result && result->is_string()) return result->GetString();
  SB_LOG(INFO) << "Persistent setting does not exist: " << key;
  return default_setting;
}

void PersistentSettings::SetPersistentSetting(
    const std::string& key, std::unique_ptr<base::Value> value) {
  if (key == kValidated) {
    SB_LOG(ERROR) << "Cannot set protected persistent setting: " << key;
    return;
  }
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PersistentSettings::SetPersistentSettingHelper,
                                base::Unretained(this), key, std::move(value)));
}

void PersistentSettings::SetPersistentSettingHelper(
    const std::string& key, std::unique_ptr<base::Value> value) {
  if (validated_initial_settings_) {
    pref_store_->SetValue(kValidated, std::make_unique<base::Value>(false),
                          WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    pref_store_->SetValue(key, std::move(value),
                          WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    pref_store_->CommitPendingWrite();
  } else {
    SB_LOG(ERROR) << "Cannot set persistent setting while unvalidated: " << key;
  }
}

void PersistentSettings::RemovePersistentSetting(const std::string& key) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PersistentSettings::RemovePersistentSettingHelper,
                     base::Unretained(this), key));
}

void PersistentSettings::RemovePersistentSettingHelper(const std::string& key) {
  if (validated_initial_settings_) {
    pref_store_->SetValue(kValidated, std::make_unique<base::Value>(false),
                          WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    pref_store_->RemoveValue(key, WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    pref_store_->CommitPendingWrite();
  } else {
    SB_LOG(ERROR) << "Cannot remove persistent setting while unvalidated: "
                  << key;
  }
}

void PersistentSettings::DeletePersistentSettings() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PersistentSettings::DeletePersistentSettingsHelper,
                     base::Unretained(this)));
}

void PersistentSettings::DeletePersistentSettingsHelper() {
  if (validated_initial_settings_) {
    starboard::SbFileDeleteRecursive(persistent_settings_file_.c_str(), true);
    pref_store_->ReadPrefs();
  } else {
    SB_LOG(ERROR) << "Cannot delete persistent setting while unvalidated.";
  }
}

}  // namespace persistent_storage
}  // namespace cobalt
