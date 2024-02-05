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
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "components/prefs/persistent_pref_store.h"

namespace cobalt {
namespace persistent_storage {

// PersistentSettings manages Cobalt settings that persist from one application
// lifecycle to another as a JSON file in kSbSystemPathCacheDirectory. The
// persistent settings will persist until the cache is cleared.
// PersistentSettings uses JsonPrefStore for most of its functionality.
// JsonPrefStore maintains thread safety by requiring that all access occurs on
// the Sequence that created it.
class PersistentSettings : public base::MessageLoop::DestructionObserver {
 public:
  explicit PersistentSettings(const std::string& file_name);
  ~PersistentSettings();

  // The message loop this object is running on.
  base::MessageLoop* message_loop() const { return thread_.message_loop(); }

  // From base::MessageLoop::DestructionObserver.
  void WillDestroyCurrentMessageLoop() override;

  // Validates persistent settings by restoring the file on successful start up.
  void ValidatePersistentSettings();

  // Getters and Setters for persistent settings.
  bool GetPersistentSettingAsBool(const std::string& key, bool default_setting);

  int GetPersistentSettingAsInt(const std::string& key, int default_setting);

  double GetPersistentSettingAsDouble(const std::string& key,
                                      double default_setting);

  std::string GetPersistentSettingAsString(const std::string& key,
                                           const std::string& default_setting);

  std::vector<base::Value> GetPersistentSettingAsList(const std::string& key);

  base::flat_map<std::string, std::unique_ptr<base::Value>>
  GetPersistentSettingAsDictionary(const std::string& key);

  void SetPersistentSetting(
      const std::string& key, std::unique_ptr<base::Value> value,
      base::OnceClosure closure = std::move(base::DoNothing()),
      bool blocking = false);

  void RemovePersistentSetting(
      const std::string& key,
      base::OnceClosure closure = std::move(base::DoNothing()),
      bool blocking = false);

  void DeletePersistentSettings(
      base::OnceClosure closure = std::move(base::DoNothing()));

 private:
  // Called by the constructor to initialize pref_store_ from
  // the dedicated thread_ as a JSONPrefStore.
  void InitializePrefStore();

  void ValidatePersistentSettingsHelper();

  void SetPersistentSettingHelper(const std::string& key,
                                  std::unique_ptr<base::Value> value,
                                  base::OnceClosure closure, bool blocking);

  void RemovePersistentSettingHelper(const std::string& key,
                                     base::OnceClosure closure, bool blocking);

  void DeletePersistentSettingsHelper(base::OnceClosure closure);

  void CommitPendingWrite(bool blocking);

  // Persistent settings file path.
  std::string persistent_settings_file_;

  // PrefStore used for persistent settings.
  scoped_refptr<PersistentPrefStore> pref_store_;

  // Flag indicating whether or not initial persistent settings have been
  // validated.
  bool validated_initial_settings_;

  // The thread created and owned by PersistentSettings. All pref_store_
  // methods must be called from this thread.
  base::Thread thread_;

  // This event is used to signal when Initialize has been called and
  // pref_store_ mutations can now occur.
  base::WaitableEvent pref_store_initialized_ = {
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED};

  // This event is used to signal when the destruction observers have been
  // added to the message loop. This is then used in Stop() to ensure that
  // processing doesn't continue until the thread is cleanly shutdown.
  base::WaitableEvent destruction_observer_added_ = {
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED};

  base::Lock pref_store_lock_;
};

}  // namespace persistent_storage
}  // namespace cobalt

#endif  // COBALT_PERSISTENT_STORAGE_PERSISTENT_SETTINGS_H_
