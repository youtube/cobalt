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

#ifndef COBALT_PERSISTENT_STORAGE_PERSISTENT_SETTINGS_H_
#define COBALT_PERSISTENT_STORAGE_PERSISTENT_SETTINGS_H_

#include <memory>
#include <string>

#include "components/prefs/json_pref_store.h"

namespace cobalt {
namespace persistent_storage {

// PersistentSettings manages Cobalt settings that persist from one application
// lifecycle to another as a JSON file in kSbSystemPathStorageDirectory.
// PersistentSettings uses JsonPrefStore for most of its functionality.
// JsonPrefStore maintains thread safety by requiring that all access occurs on
// the Sequence that created it. This is why some functions rely on a PostTask
// to the SingleThreadTaskRunner that calls and is subsequently passed into
// PersistentSettings' constructor.
class PersistentSettings {
 public:
  explicit PersistentSettings(
      const std::string& file_name,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Validates persistent settings by restoring the file on successful start up.
  void ValidatePersistentSettings();

  // Getters and Setters for persistent settings.
  bool GetPersistentSettingAsBool(const std::string& key, bool default_setting);
  int GetPersistentSettingAsInt(const std::string& key, int default_setting);
  double GetPersistentSettingAsDouble(const std::string& key,
                                      double default_setting);
  std::string GetPersistentSettingAsString(const std::string& key,
                                           const std::string& default_setting);
  void SetPersistentSetting(const std::string& key,
                            std::unique_ptr<base::Value> value);
  void RemovePersistentSetting(const std::string& key);

  void DeletePersistentSettings();

 private:
  // Persistent settings file path.
  std::string persistent_settings_file_;
  // TaskRunner of the thread that initialized PersistentSettings and
  // PrefStore.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  // PrefStore used for persistent settings.
  scoped_refptr<JsonPrefStore> pref_store_;
  // Flag indicating whether or not initial persistent settings have been
  // validated.
  bool validated_initial_settings_;

  void ValidatePersistentSettingsHelper();

  void SetPersistentSettingHelper(const std::string& key,
                                  std::unique_ptr<base::Value> value);

  void RemovePersistentSettingHelper(const std::string& key);

  void DeletePersistentSettingsHelper();
};

}  // namespace persistent_storage
}  // namespace cobalt

#endif  // COBALT_PERSISTENT_STORAGE_PERSISTENT_SETTINGS_H_
