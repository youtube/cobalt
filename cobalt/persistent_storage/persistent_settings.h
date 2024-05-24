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
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/current_thread.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "components/prefs/json_pref_store.h"

namespace cobalt {

namespace watchdog {
class WatchdogTest;
}

namespace persistent_storage {

// PersistentSettings manages Cobalt settings that persist from one application
// lifecycle to another as a JSON file in kSbSystemPathCacheDirectory. The
// persistent settings will persist until the cache is cleared.
// PersistentSettings uses JsonPrefStore for most of its functionality.
// JsonPrefStore maintains thread safety by requiring that all access occurs on
// the Sequence that created it.
class PersistentSettings : public base::CurrentThread::DestructionObserver,
                           public base::SupportsWeakPtr<PersistentSettings> {
 public:
  explicit PersistentSettings(const std::string& file_name);
  ~PersistentSettings();

  // base::CurrentThread::DestructionObserver
  void WillDestroyCurrentMessageLoop() override;

  // Validates persistent settings by restoring the file on successful start up.
  void Validate();

  void Get(const std::string& key, base::Value* value) const;

  void Set(const std::string& key, base::Value value);

  void Remove(const std::string& key);

  void RemoveAll();

 private:
  friend class PersistentSettingTest;
  friend class cobalt::watchdog::WatchdogTest;

  void InitializePrefStore();

  WriteablePrefStore::PrefWriteFlags GetPrefWriteFlags() const;

  void CommitPendingWrite();

  void Run(const base::Location& location, base::OnceClosure closure,
           bool blocking = false) const;

  void Fence(const base::Location& location);

  // The task runner this object is running on.
  base::SequencedTaskRunner* task_runner() const {
    return thread_.task_runner();
  }

  void WaitForPendingFileWrite();

  // Persistent settings file path.
  base::FilePath file_path_;

  // PrefStore used for persistent settings.
  scoped_refptr<JsonPrefStore> pref_store_;
  mutable base::Lock pref_store_lock_;

  // The thread created and owned by PersistentSettings. All pref_store_
  // methods must be called from this thread.
  base::Thread thread_;

  // Flag indicating whether or not initial persistent settings have been
  // validated.
  bool validated_initial_settings_;
};

}  // namespace persistent_storage
}  // namespace cobalt

#endif  // COBALT_PERSISTENT_STORAGE_PERSISTENT_SETTINGS_H_
