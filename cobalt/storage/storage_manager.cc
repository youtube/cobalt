// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/storage/storage_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/storage/savegame_thread.h"
#include "cobalt/storage/upgrade/upgrade_reader.h"

namespace cobalt {
namespace storage {

namespace {

// FlushOnChange() delays for a while to avoid spamming writes to disk; we often
// get several FlushOnChange() calls in a row.
const int kDatabaseFlushOnLastChangeDelayMs = 500;
const int kDatabaseFlushOnChangeMaxDelayMs = 2000;
}  // namespace

StorageManager::StorageManager(std::unique_ptr<UpgradeHandler> upgrade_handler,
                               const Options& options)
    : upgrade_handler_(std::move(upgrade_handler)),
      options_(options),
      storage_thread_(new base::Thread("StorageManager")),
      memory_store_(new MemoryStore()),
      loaded_database_version_(0),
      initialized_(false),
      flush_processing_(false),
      flush_pending_(false),
      no_flushes_pending_(base::WaitableEvent::ResetPolicy::MANUAL,
                          base::WaitableEvent::InitialState::SIGNALED) {
  DCHECK(upgrade_handler_);
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);

  savegame_thread_.reset(new SavegameThread(options_.savegame_options));

  // Start the savegame load immediately.
  storage_thread_->Start();
  storage_task_runner_ = storage_thread_->task_runner();
  DCHECK(storage_task_runner_);

  flush_on_last_change_timer_.reset(new base::OneShotTimer());
  flush_on_change_max_delay_timer_.reset(new base::OneShotTimer());
}

StorageManager::~StorageManager() {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  DCHECK(!storage_task_runner_->BelongsToCurrentThread());

  // Wait for all I/O operations to complete.
  FinishIO();

  // Destroy various objects on the proper thread.
  storage_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&StorageManager::OnDestroy, base::Unretained(this)));

  // Force all tasks to finish. Then we can safely let the rest of our
  // member variables be destroyed.
  storage_thread_.reset();
}

void StorageManager::WithReadOnlyMemoryStore(
    const ReadOnlyMemoryStoreCallback& callback) {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  if (!storage_task_runner_->BelongsToCurrentThread()) {
    storage_task_runner_->PostTask(
        FROM_HERE, base::Bind(&StorageManager::WithReadOnlyMemoryStore,
                              base::Unretained(this), callback));
    return;
  }
  FinishInit();
  callback.Run(*memory_store_.get());
}

void StorageManager::WithMemoryStore(const MemoryStoreCallback& callback) {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  if (!storage_task_runner_->BelongsToCurrentThread()) {
    storage_task_runner_->PostTask(
        FROM_HERE, base::Bind(&StorageManager::WithMemoryStore,
                              base::Unretained(this), callback));
    return;
  }
  FinishInit();
  callback.Run(memory_store_.get());
  FlushOnChange();
}

void StorageManager::FlushOnChange() {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  // Make sure this runs on the correct thread.
  if (!storage_task_runner_->BelongsToCurrentThread()) {
    storage_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&StorageManager::FlushOnChange, base::Unretained(this)));
    return;
  }

  // Only start the timers if there isn't already a flush pending.
  if (!flush_pending_) {
    // The last change timer is always re-started on any change.
    flush_on_last_change_timer_->Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kDatabaseFlushOnLastChangeDelayMs),
        this, &StorageManager::OnFlushOnChangeTimerFired);
    // The max delay timer is never re-started after it starts running.
    if (!flush_on_change_max_delay_timer_->IsRunning()) {
      flush_on_change_max_delay_timer_->Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kDatabaseFlushOnChangeMaxDelayMs),
          this, &StorageManager::OnFlushOnChangeTimerFired);
    }
  }
}

void StorageManager::FlushNow(base::OnceClosure callback) {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  // Make sure this runs on the correct thread.
  if (!storage_task_runner_->BelongsToCurrentThread()) {
    storage_task_runner_->PostTask(
        FROM_HERE, base::Bind(&StorageManager::FlushNow, base::Unretained(this),
                              base::Passed(&callback)));
    return;
  }

  StopFlushOnChangeTimers();
  QueueFlush(std::move(callback));
}

void StorageManager::FinishInit() {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  DCHECK(storage_task_runner_->BelongsToCurrentThread());
  if (initialized_) {
    return;
  }

  initialized_ = true;

  // Savegame has finished loading. Now initialize the database connection.
  // Check if this is upgrade data, if so, handle it, otherwise:
  // Check if the savegame data contains a VFS header.
  // If so, proceed to deserialize it.
  // If not, load the file into the VFS directly.
  std::unique_ptr<Savegame::ByteVector> loaded_raw_bytes =
      savegame_thread_->GetLoadedRawBytes();
  DCHECK(loaded_raw_bytes);
  Savegame::ByteVector& raw_bytes = *loaded_raw_bytes;
  bool has_upgrade_data = false;

  if (raw_bytes.size() > 0) {
    const char* buffer = reinterpret_cast<char*>(&raw_bytes[0]);
    int buffer_size = static_cast<int>(raw_bytes.size());
    // Is this upgrade data?
    if (upgrade::UpgradeReader::IsUpgradeData(buffer, buffer_size)) {
      has_upgrade_data = true;
    } else {
      bool result = memory_store_->Initialize(raw_bytes);
      LOG(INFO) << "Deserialize result=" << result;
    }
  }

  // Very old legacy save data may contain multiple files (e.g. db-journal as
  // well as db), so use the first one that looks like a valid database file.

  if (has_upgrade_data) {
    const char* buffer = reinterpret_cast<char*>(&raw_bytes[0]);
    int buffer_size = static_cast<int>(raw_bytes.size());
    upgrade_handler_->OnUpgrade(this, buffer, buffer_size);
  }
}

void StorageManager::StopFlushOnChangeTimers() {
  if (flush_on_last_change_timer_->IsRunning()) {
    flush_on_last_change_timer_->Stop();
  }
  if (flush_on_change_max_delay_timer_->IsRunning()) {
    flush_on_change_max_delay_timer_->Stop();
  }
}

void StorageManager::OnFlushOnChangeTimerFired() {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  DCHECK(storage_task_runner_->BelongsToCurrentThread());

  StopFlushOnChangeTimers();
  QueueFlush(base::Closure());
}

void StorageManager::OnFlushIOCompletedCallback() {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  // Make sure this runs on the SQL message loop.
  if (!storage_task_runner_->BelongsToCurrentThread()) {
    storage_task_runner_->PostTask(
        FROM_HERE, base::Bind(&StorageManager::OnFlushIOCompletedCallback,
                              base::Unretained(this)));
    return;
  }

  flush_processing_ = false;

  // Fire all the callbacks waiting on the current flush to complete.
  for (size_t i = 0; i < flush_processing_callbacks_.size(); ++i) {
    DCHECK(!flush_processing_callbacks_[i].is_null());
    std::move(flush_processing_callbacks_[i]).Run();
  }
  flush_processing_callbacks_.clear();

  if (flush_pending_) {
    // If another flush has been requested while we were processing the one that
    // just completed, start that next flush now.
    flush_processing_callbacks_.swap(flush_pending_callbacks_);
    flush_pending_ = false;
    FlushInternal();
    DCHECK(flush_processing_);
  } else {
    no_flushes_pending_.Signal();
  }
}

void StorageManager::QueueFlush(base::OnceClosure callback) {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  DCHECK(storage_task_runner_->BelongsToCurrentThread());

  if (!flush_processing_) {
    // If no flush is currently in progress, flush immediately.
    if (!callback.is_null()) {
      flush_processing_callbacks_.push_back(std::move(callback));
    }
    FlushInternal();
    DCHECK(flush_processing_);
  } else {
    // Otherwise, indicate that we would like to re-flush as soon as the
    // current one completes.
    flush_pending_ = true;
    if (!callback.is_null()) {
      flush_pending_callbacks_.push_back(std::move(callback));
    }
  }
}

void StorageManager::FlushInternal() {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  DCHECK(storage_task_runner_->BelongsToCurrentThread());
  FinishInit();

  flush_processing_ = true;
  no_flushes_pending_.Reset();

  // Serialize the database into a buffer. Then send the bytes
  // to OnFlushIO for a blocking write to the savegame.
  std::unique_ptr<Savegame::ByteVector> raw_bytes_ptr(
      new Savegame::ByteVector());
  memory_store_->Serialize(raw_bytes_ptr.get());

  // Send the savegame bytes off to the SavegameThread object to be
  // asynchronously written to the savegame file.
  savegame_thread_->Flush(
      std::move(raw_bytes_ptr),
      base::Bind(&StorageManager::OnFlushIOCompletedCallback,
                 base::Unretained(this)));
}

void StorageManager::FinishIO() {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  DCHECK(!storage_task_runner_->BelongsToCurrentThread());

  // Make sure that the on change timers fire if they're running.
  storage_task_runner_->PostTask(
      FROM_HERE, base::Bind(&StorageManager::FireRunningOnChangeTimers,
                            base::Unretained(this)));

  // The SQL thread may be communicating with the savegame I/O thread still,
  // flushing all pending updates.  This process can require back and forth
  // communication.  This method exists to wait for that communication to
  // finish and for all pending flushes to complete.

  // Start by finishing all commands currently in the sql message loop queue.
  // This method is called by the destructor, so the only new tasks posted
  // after this one will be generated internally.  We need to do this because
  // it is possible that there are no flushes pending at this instant, but there
  // are tasks queued on |storage_task_runner_| that will begin a flush, and so
  // we make sure that these are executed first.
  storage_task_runner_->WaitForFence();

  // Now wait for all pending flushes to wrap themselves up.  This may involve
  // the savegame I/O thread and the SQL thread posting tasks to each other.
  no_flushes_pending_.Wait();
}

void StorageManager::FireRunningOnChangeTimers() {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  DCHECK(storage_task_runner_->BelongsToCurrentThread());

  if (flush_on_last_change_timer_->IsRunning() ||
      flush_on_change_max_delay_timer_->IsRunning()) {
    OnFlushOnChangeTimerFired();
  }
}

void StorageManager::OnDestroy() {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  DCHECK(storage_task_runner_->BelongsToCurrentThread());

  // Stop the savegame thread and have it wrap up any pending I/O operations.
  savegame_thread_.reset();

  // Ensure these objects are destroyed on the proper thread.
  flush_on_last_change_timer_.reset(NULL);
  flush_on_change_max_delay_timer_.reset(NULL);
}

}  // namespace storage
}  // namespace cobalt
