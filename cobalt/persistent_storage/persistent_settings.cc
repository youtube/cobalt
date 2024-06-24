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
#include "base/files/file_util.h"
#include "base/logging.h"
#include "cobalt/base/task_runner_util.h"
#include "starboard/configuration_constants.h"
#include "starboard/system.h"

namespace cobalt {
namespace persistent_storage {

void PersistentSettings::WillDestroyCurrentMessageLoop() {
  if (!pref_store_) return;
  base::AutoLock auto_lock(pref_store_lock_);
  pref_store_.reset();
}

PersistentSettings::PersistentSettings(const std::string& file_name)
    : thread_("PersistentSettings"), validated_initial_settings_(false) {
  if (!thread_.Start()) return;
  DCHECK(task_runner());

  std::vector<char> storage_dir(kSbFileMaxPath, 0);
  SbSystemGetPath(kSbSystemPathCacheDirectory, storage_dir.data(),
                  kSbFileMaxPath);
  file_path_ =
      base::FilePath(std::string(storage_dir.data())).Append(file_name);
  DLOG(INFO) << "Persistent settings file path: " << file_path_;

  Run(FROM_HERE,
      base::Bind(&PersistentSettings::InitializePrefStore,
                 base::Unretained(this)),
      /*blocking=*/true);
}

PersistentSettings::~PersistentSettings() {
  DCHECK(task_runner());
  DCHECK(thread_.IsRunning());

  // Wait for all previously posted tasks to finish.
  Fence(FROM_HERE);
  thread_.Stop();
}

void PersistentSettings::InitializePrefStore() {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());
  // Register as a destruction observer to shut down the thread once all
  // pending tasks have been executed and the task runner is about to be
  // destroyed. This allows us to safely stop the thread, drain the task queue,
  // then destroy the internal components before the task runner is reset.
  // No posted tasks will be executed once the thread is stopped.
  base::CurrentThread::Get()->AddDestructionObserver(this);

  // Read preferences into memory.
  pref_store_ = base::MakeRefCounted<JsonPrefStore>(file_path_);
  pref_store_->ReadPrefs();

  // Remove settings file and do not allow writes to the file until |Validate()|
  // is called.
  base::DeleteFile(file_path_);
}

void PersistentSettings::Validate() {
  if (validated_initial_settings_) return;
  if (!task_runner()->RunsTasksInCurrentSequence()) {
    Run(FROM_HERE,
        base::BindOnce(&PersistentSettings::Validate, base::Unretained(this)));
    return;
  }
  validated_initial_settings_ = true;
  // Report a dummy value as "dirty", so that commit actually writes
  pref_store_->ReportValueChanged("", 0);
  pref_store_->CommitPendingWrite();
}

void PersistentSettings::Get(const std::string& key, base::Value* value) const {
  if (!pref_store_) return;
  if (!task_runner()->RunsTasksInCurrentSequence()) {
    Run(FROM_HERE,
        base::BindOnce(&PersistentSettings::Get, base::Unretained(this), key,
                       value),
        /*blocking=*/true);
    return;
  }
  base::AutoLock auto_lock(pref_store_lock_);
  const base::Value* pref_value = nullptr;
  pref_store_->GetValue(key, &pref_value);
  *value = std::move(pref_value ? pref_value->Clone() : base::Value());
}

WriteablePrefStore::PrefWriteFlags PersistentSettings::GetPrefWriteFlags()
    const {
  return validated_initial_settings_
             ? WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS
             : WriteablePrefStore::LOSSY_PREF_WRITE_FLAG;
}


void PersistentSettings::Set(const std::string& key, base::Value value) {
  if (!pref_store_) return;
  if (!task_runner()->RunsTasksInCurrentSequence()) {
    Run(FROM_HERE,
        base::BindOnce(&PersistentSettings::Set, base::Unretained(this), key,
                       std::move(value)));
    return;
  }
  base::AutoLock auto_lock(pref_store_lock_);
  pref_store_->SetValue(key, std::move(value), GetPrefWriteFlags());
  if (validated_initial_settings_) pref_store_->CommitPendingWrite();
}

void PersistentSettings::Remove(const std::string& key) {
  if (!pref_store_) return;
  if (!task_runner()->RunsTasksInCurrentSequence()) {
    Run(FROM_HERE, base::BindOnce(&PersistentSettings::Remove,
                                  base::Unretained(this), key));
    return;
  }
  base::AutoLock auto_lock(pref_store_lock_);
  pref_store_->RemoveValue(key, GetPrefWriteFlags());
  if (validated_initial_settings_) pref_store_->CommitPendingWrite();
}

void PersistentSettings::RemoveAll() {
  if (!pref_store_) return;
  if (!task_runner()->RunsTasksInCurrentSequence()) {
    Run(FROM_HERE,
        base::BindOnce(&PersistentSettings::RemoveAll, base::Unretained(this)));
    return;
  }
  base::AutoLock auto_lock(pref_store_lock_);
  base::DeleteFile(file_path_);
  pref_store_->ReadPrefs();
}

void PersistentSettings::WaitForPendingFileWrite() {
  if (!pref_store_) return;
  base::WaitableEvent done;
  Run(FROM_HERE, base::BindOnce(&JsonPrefStore::CommitPendingWrite,
                                pref_store_->AsWeakPtr(), base::OnceClosure(),
                                base::BindOnce(&base::WaitableEvent::Signal,
                                               base::Unretained(&done))));
  done.Wait();
}

void PersistentSettings::Fence(const base::Location& location) {
  Run(location, base::OnceClosure(), /*blocking=*/true);
}

void PersistentSettings::Run(const base::Location& location,
                             base::OnceClosure task, bool blocking) const {
  DCHECK(!task_runner()->RunsTasksInCurrentSequence());
  std::unique_ptr<base::WaitableEvent> done;
  if (blocking) done.reset(new base::WaitableEvent());
  task_runner()->PostTask(
      location, base::BindOnce(
                    [](base::OnceClosure task, base::WaitableEvent* done) {
                      if (task) std::move(task).Run();
                      if (done) done->Signal();
                    },
                    std::move(task), done.get()));
  if (done) done->Wait();
}

}  // namespace persistent_storage
}  // namespace cobalt
