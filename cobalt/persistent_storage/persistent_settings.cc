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

#include "base/bind.h"
#include "base/logging.h"
#include "cobalt/base/task_runner_util.h"
#include "components/prefs/json_pref_store.h"
#include "starboard/common/file.h"
#include "starboard/configuration_constants.h"
#include "starboard/system.h"

namespace cobalt {
namespace persistent_storage {

void PersistentSettings::WillDestroyCurrentMessageLoop() {
  // Clear all member variables allocated from the thread.
  pref_store_.reset();
}

PersistentSettings::PersistentSettings(const std::string& file_name)
    : thread_("PersistentSettings"), validated_initial_settings_(false) {
  if (!thread_.Start()) return;
  DCHECK(task_runner());

  std::vector<char> storage_dir(kSbFileMaxPath, 0);
  SbSystemGetPath(kSbSystemPathCacheDirectory, storage_dir.data(),
                  kSbFileMaxPath);
  persistent_settings_file_ =
      std::string(storage_dir.data()) + kSbFileSepString + file_name;
  LOG(INFO) << "Persistent settings file path: " << persistent_settings_file_;

  task_runner()->PostTask(FROM_HERE,
                          base::Bind(&PersistentSettings::InitializePrefStore,
                                     base::Unretained(this)));
  pref_store_initialized_.Wait();
  destruction_observer_added_.Wait();
}

PersistentSettings::~PersistentSettings() {
  DCHECK(task_runner());
  DCHECK(thread_.IsRunning());

  // Wait for all previously posted tasks to finish.
  base::task_runner_util::WaitForFence(thread_.task_runner(), FROM_HERE);
  thread_.Stop();
}

void PersistentSettings::InitializePrefStore() {
  DCHECK_EQ(base::SequencedTaskRunner::GetCurrentDefault(), task_runner());
  // Register as a destruction observer to shut down the thread once all
  // pending tasks have been executed and the task runner is about to be
  // destroyed. This allows us to safely stop the thread, drain the task queue,
  // then destroy the internal components before the task runner is reset.
  // No posted tasks will be executed once the thread is stopped.
  base::CurrentThread::Get()->AddDestructionObserver(this);
  // Read preferences into memory.
  {
    base::AutoLock auto_lock(pref_store_lock_);
    pref_store_ = base::MakeRefCounted<JsonPrefStore>(
        base::FilePath(persistent_settings_file_));
    pref_store_->ReadPrefs();
    persistent_settings_ = pref_store_->GetValues();
    pref_store_initialized_.Signal();
  }
  // Remove settings file and do not allow writes to the file until the
  // |ValidatePersistentSettings()| is called.
  starboard::SbFileDeleteRecursive(persistent_settings_file_.c_str(), true);
  destruction_observer_added_.Signal();
}

void PersistentSettings::ValidatePersistentSettings(bool blocking) {
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&PersistentSettings::ValidatePersistentSettingsHelper,
                     base::Unretained(this), blocking));
}

void PersistentSettings::ValidatePersistentSettingsHelper(bool blocking) {
  DCHECK_EQ(base::SequencedTaskRunner::GetCurrentDefault(), task_runner());
  if (!validated_initial_settings_) {
    base::AutoLock auto_lock(pref_store_lock_);
    validated_initial_settings_ = true;
    CommitPendingWrite(blocking);
  }
}

bool PersistentSettings::GetPersistentSettingAsBool(const std::string& key,
                                                    bool default_setting) {
  // Wait for all previously posted tasks to finish.
  base::task_runner_util::WaitForFence(thread_.task_runner(), FROM_HERE);
  base::AutoLock auto_lock(pref_store_lock_);
  absl::optional<bool> result = persistent_settings_.FindBool(key);
  return result.value_or(default_setting);
}

int PersistentSettings::GetPersistentSettingAsInt(const std::string& key,
                                                  int default_setting) {
  // Wait for all previously posted tasks to finish.
  base::task_runner_util::WaitForFence(thread_.task_runner(), FROM_HERE);
  base::AutoLock auto_lock(pref_store_lock_);
  absl::optional<int> result = persistent_settings_.FindInt(key);
  return result.value_or(default_setting);
}

double PersistentSettings::GetPersistentSettingAsDouble(
    const std::string& key, double default_setting) {
  // Wait for all previously posted tasks to finish.
  base::task_runner_util::WaitForFence(thread_.task_runner(), FROM_HERE);
  base::AutoLock auto_lock(pref_store_lock_);
  absl::optional<double> result = persistent_settings_.FindDouble(key);
  return result.value_or(default_setting);
}

std::string PersistentSettings::GetPersistentSettingAsString(
    const std::string& key, const std::string& default_setting) {
  // Wait for all previously posted tasks to finish.
  base::task_runner_util::WaitForFence(thread_.task_runner(), FROM_HERE);
  base::AutoLock auto_lock(pref_store_lock_);
  const std::string* result = persistent_settings_.FindString(key);
  if (result) return *result;
  return default_setting;
}

std::vector<base::Value> PersistentSettings::GetPersistentSettingAsList(
    const std::string& key) {
  // Wait for all previously posted tasks to finish.
  base::task_runner_util::WaitForFence(thread_.task_runner(), FROM_HERE);
  base::AutoLock auto_lock(pref_store_lock_);
  const base::Value::List* result = persistent_settings_.FindList(key);
  std::vector<base::Value> values;
  if (result) {
    for (const auto& value : *result) {
      values.emplace_back(value.Clone());
    }
  }
  return values;
}

base::flat_map<std::string, std::unique_ptr<base::Value>>
PersistentSettings::GetPersistentSettingAsDictionary(const std::string& key) {
  // Wait for all previously posted tasks to finish.
  base::task_runner_util::WaitForFence(thread_.task_runner(), FROM_HERE);
  base::AutoLock auto_lock(pref_store_lock_);
  base::Value::Dict* result = persistent_settings_.FindDict(key);
  base::flat_map<std::string, std::unique_ptr<base::Value>> dict;
  if (result) {
    for (base::Value::Dict::iterator it = result->begin(); it != result->end();
         ++it) {
      dict.insert(std::make_pair(
          it->first, base::Value::ToUniquePtrValue(std::move(it->second))));
    }
    return dict;
  }
  return dict;
}

void PersistentSettings::SetPersistentSetting(
    const std::string& key, std::unique_ptr<base::Value> value, bool blocking) {
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&PersistentSettings::SetPersistentSettingHelper,
                     base::Unretained(this), key, std::move(value), blocking));
}

void PersistentSettings::SetPersistentSettingHelper(
    const std::string& key, std::unique_ptr<base::Value> value, bool blocking) {
  DCHECK_EQ(base::SequencedTaskRunner::GetCurrentDefault(), task_runner());
  base::AutoLock auto_lock(pref_store_lock_);
  pref_store_->SetValue(key, base::Value::FromUniquePtrValue(std::move(value)),
                        WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  persistent_settings_ = pref_store_->GetValues();
  if (validated_initial_settings_) {
    CommitPendingWrite(blocking);
  }
}

void PersistentSettings::RemovePersistentSetting(const std::string& key,
                                                 bool blocking) {
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&PersistentSettings::RemovePersistentSettingHelper,
                     base::Unretained(this), key, blocking));
}

void PersistentSettings::RemovePersistentSettingHelper(const std::string& key,
                                                       bool blocking) {
  DCHECK_EQ(base::SequencedTaskRunner::GetCurrentDefault(), task_runner());
  base::AutoLock auto_lock(pref_store_lock_);
  pref_store_->RemoveValue(key, WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  persistent_settings_ = pref_store_->GetValues();
  if (validated_initial_settings_) {
    CommitPendingWrite(blocking);
  }
}

void PersistentSettings::DeletePersistentSettings() {
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&PersistentSettings::DeletePersistentSettingsHelper,
                     base::Unretained(this)));
}

void PersistentSettings::DeletePersistentSettingsHelper() {
  DCHECK_EQ(base::SequencedTaskRunner::GetCurrentDefault(), task_runner());
  base::AutoLock auto_lock(pref_store_lock_);
  starboard::SbFileDeleteRecursive(persistent_settings_file_.c_str(), true);
  pref_store_->ReadPrefs();
  persistent_settings_ = pref_store_->GetValues();
}

void PersistentSettings::CommitPendingWrite(bool blocking) {
  if (blocking) {
    base::WaitableEvent written;
    pref_store_->CommitPendingWrite(
        base::OnceClosure(),
        base::BindOnce(&base::WaitableEvent::Signal, Unretained(&written)));
    written.Wait();
  } else {
    pref_store_->CommitPendingWrite();
  }
}

}  // namespace persistent_storage
}  // namespace cobalt
