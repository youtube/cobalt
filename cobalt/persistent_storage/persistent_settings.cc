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
#include "components/prefs/json_pref_store.h"
#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"
#include "starboard/system.h"

namespace cobalt {
namespace persistent_storage {

namespace {
// Protected persistent settings key indicating whether or not the persistent
// settings file has been validated.
const char kValidated[] = "validated";
}  // namespace

void PersistentSettings::WillDestroyCurrentMessageLoop() {
  // Clear all member variables allocated from the thread.
  pref_store_.reset();
}

PersistentSettings::PersistentSettings(const std::string& file_name)
    : thread_("PersistentSettings") {
  if (!thread_.Start()) return;
  DCHECK(message_loop());

  std::vector<char> storage_dir(kSbFileMaxPath, 0);
  SbSystemGetPath(kSbSystemPathCacheDirectory, storage_dir.data(),
                  kSbFileMaxPath);
  persistent_settings_file_ =
      std::string(storage_dir.data()) + kSbFileSepString + file_name;
  LOG(INFO) << "Persistent settings file path: " << persistent_settings_file_;

  message_loop()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&PersistentSettings::InitializePrefStore,
                            base::Unretained(this)));
  pref_store_initialized_.Wait();
  destruction_observer_added_.Wait();
}

PersistentSettings::~PersistentSettings() {
  DCHECK(message_loop());
  DCHECK(thread_.IsRunning());

  // Wait for all previously posted tasks to finish.
  thread_.message_loop()->task_runner()->WaitForFence();
  thread_.Stop();
}

void PersistentSettings::InitializePrefStore() {
  DCHECK_EQ(base::MessageLoop::current(), message_loop());
  // Register as a destruction observer to shut down the thread once all
  // pending tasks have been executed and the message loop is about to be
  // destroyed. This allows us to safely stop the thread, drain the task queue,
  // then destroy the internal components before the message loop is reset.
  // No posted tasks will be executed once the thread is stopped.
  base::MessageLoopCurrent::Get()->AddDestructionObserver(this);
  {
    base::AutoLock auto_lock(pref_store_lock_);
    pref_store_ = base::MakeRefCounted<JsonPrefStore>(
        base::FilePath(persistent_settings_file_));
    pref_store_->ReadPrefs();
    pref_store_initialized_.Signal();
  }
  validated_initial_settings_ = GetPersistentSettingAsBool(kValidated, false);
  if (!validated_initial_settings_) {
    starboard::SbFileDeleteRecursive(persistent_settings_file_.c_str(), true);
  }
  destruction_observer_added_.Signal();
}

void PersistentSettings::ValidatePersistentSettings() {
  message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&PersistentSettings::ValidatePersistentSettingsHelper,
                     base::Unretained(this)));
}

void PersistentSettings::ValidatePersistentSettingsHelper() {
  DCHECK_EQ(base::MessageLoop::current(), message_loop());
  if (!validated_initial_settings_) {
    base::AutoLock auto_lock(pref_store_lock_);
#ifndef USE_HACKY_COBALT_CHANGES
    pref_store_->SetValue(kValidated, std::make_unique<base::Value>(true),
                          WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
#endif
    CommitPendingWrite(false);
    validated_initial_settings_ = true;
  }
}

bool PersistentSettings::GetPersistentSettingAsBool(const std::string& key,
                                                    bool default_setting) {
  base::AutoLock auto_lock(pref_store_lock_);
  auto persistent_settings = pref_store_->GetValues();
  const base::Value* result = persistent_settings->FindKey(key);
  if (result && result->is_bool()) return result->GetBool();
  return default_setting;
}

int PersistentSettings::GetPersistentSettingAsInt(const std::string& key,
                                                  int default_setting) {
  base::AutoLock auto_lock(pref_store_lock_);
  auto persistent_settings = pref_store_->GetValues();
  const base::Value* result = persistent_settings->FindKey(key);
  if (result && result->is_int()) return result->GetInt();
  return default_setting;
}

double PersistentSettings::GetPersistentSettingAsDouble(
    const std::string& key, double default_setting) {
  base::AutoLock auto_lock(pref_store_lock_);
  auto persistent_settings = pref_store_->GetValues();
  const base::Value* result = persistent_settings->FindKey(key);
  if (result && result->is_double()) return result->GetDouble();
  return default_setting;
}

std::string PersistentSettings::GetPersistentSettingAsString(
    const std::string& key, const std::string& default_setting) {
  base::AutoLock auto_lock(pref_store_lock_);
  auto persistent_settings = pref_store_->GetValues();
  const base::Value* result = persistent_settings->FindKey(key);
  if (result && result->is_string()) return result->GetString();
  return default_setting;
}

std::vector<base::Value> PersistentSettings::GetPersistentSettingAsList(
    const std::string& key) {
  base::AutoLock auto_lock(pref_store_lock_);
  auto persistent_settings = pref_store_->GetValues();
  base::Value* result = persistent_settings.Find(key);
  if (result && result->is_list()) {
#ifndef USE_HACKY_COBALT_CHANGES
    return std::move(result).TakeList();
#endif
  }
  return std::vector<base::Value>();
}

base::flat_map<std::string, std::unique_ptr<base::Value>>
PersistentSettings::GetPersistentSettingAsDictionary(const std::string& key) {
  base::AutoLock auto_lock(pref_store_lock_);
  auto persistent_settings = pref_store_->GetValues();
  base::Value* result = persistent_settings->FindKey(key);
  base::flat_map<std::string, std::unique_ptr<base::Value>> dict;
#ifndef USE_HACKY_COBALT_CHANGES
  if (result && result->is_dict()) {
    for (const auto& key_value : result->DictItems()) {
      dict.insert(std::make_pair(
          key_value.first,
          std::make_unique<base::Value>(std::move(key_value.second))));
    }
    return dict;
  }
#endif
  return dict;
}

void PersistentSettings::SetPersistentSetting(
    const std::string& key, std::unique_ptr<base::Value> value,
    base::OnceClosure closure, bool blocking) {
  if (key == kValidated) {
    LOG(ERROR) << "Cannot set protected persistent setting: " << key;
    return;
  }
  message_loop()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&PersistentSettings::SetPersistentSettingHelper,
                                base::Unretained(this), key, std::move(value),
                                std::move(closure), blocking));
}

void PersistentSettings::SetPersistentSettingHelper(
    const std::string& key, std::unique_ptr<base::Value> value,
    base::OnceClosure closure, bool blocking) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop());
  if (validated_initial_settings_) {
    base::AutoLock auto_lock(pref_store_lock_);
#ifndef USE_HACKY_COBALT_CHANGES
    pref_store_->SetValue(kValidated, std::make_unique<base::Value>(false),
                          WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    pref_store_->SetValue(key, std::move(value),
                          WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
#endif
    CommitPendingWrite(blocking);
  } else {
    LOG(ERROR) << "Cannot set persistent setting while unvalidated: " << key;
  }
  std::move(closure).Run();
}

void PersistentSettings::RemovePersistentSetting(const std::string& key,
                                                 base::OnceClosure closure,
                                                 bool blocking) {
  message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&PersistentSettings::RemovePersistentSettingHelper,
                     base::Unretained(this), key, std::move(closure),
                     blocking));
}

void PersistentSettings::RemovePersistentSettingHelper(
    const std::string& key, base::OnceClosure closure, bool blocking) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop());
  if (validated_initial_settings_) {
    base::AutoLock auto_lock(pref_store_lock_);
#ifndef USE_HACKY_COBALT_CHANGES
    pref_store_->SetValue(kValidated, std::make_unique<base::Value>(false),
                          WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    pref_store_->RemoveValue(key, WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
#endif
    CommitPendingWrite(blocking);
  } else {
    LOG(ERROR) << "Cannot remove persistent setting while unvalidated: " << key;
  }
  std::move(closure).Run();
}

void PersistentSettings::DeletePersistentSettings(base::OnceClosure closure) {
  message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&PersistentSettings::DeletePersistentSettingsHelper,
                     base::Unretained(this), std::move(closure)));
}

void PersistentSettings::DeletePersistentSettingsHelper(
    base::OnceClosure closure) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop());
  if (validated_initial_settings_) {
    starboard::SbFileDeleteRecursive(persistent_settings_file_.c_str(), true);
    base::AutoLock auto_lock(pref_store_lock_);
    pref_store_->ReadPrefs();
  } else {
    LOG(ERROR) << "Cannot delete persistent setting while unvalidated.";
  }
  std::move(closure).Run();
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
