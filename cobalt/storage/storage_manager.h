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

#ifndef COBALT_STORAGE_STORAGE_MANAGER_H_
#define COBALT_STORAGE_STORAGE_MANAGER_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "cobalt/storage/savegame_thread.h"
#include "cobalt/storage/store/memory_store.h"

namespace cobalt {
namespace storage {

// StorageManager manages a store containing cookies and local
// storage data. On most platforms, this is written to disk as a savegame
// using platform APIs. On Linux/Windows, it's a regular file.
// Internally this runs two threads: one thread to perform blocking I/O,
// and one where store operations occur.  Users are expected to access the
// store via an MemoryStore, which can be obtained with GetReadOnlyMemoryStore()
// or WithMemoryStore(). The callback to will run on the store thread.
// Operations on MemoryStore will block the store thread until the savegame
// is loaded.
class StorageManager : public base::MessageLoop::DestructionObserver {
 public:
  struct Options {
    Savegame::Options savegame_options;
  };

  typedef base::Callback<void(const MemoryStore&)> ReadOnlyMemoryStoreCallback;
  typedef base::Callback<void(MemoryStore*)> MemoryStoreCallback;

  explicit StorageManager(const Options& options);
  virtual ~StorageManager();

  // Ensures the StorageManager thread is started.
  void EnsureStarted();

  void WithReadOnlyMemoryStore(const ReadOnlyMemoryStoreCallback& callback);
  void WithMemoryStore(const MemoryStoreCallback& callback);

  // Schedule a write of our memory store to disk to happen at some point in the
  // future after a change occurs. Multiple calls to Flush() do not necessarily
  // result in multiple writes to disk. This call returns immediately.
  void FlushOnChange();

  // Triggers a write to disk to happen immediately.  Each call to FlushNow()
  // will result in a write to disk.
  // |callback|, if provided, will be called when the I/O has completed,
  // and will be run on the storage manager's IO thread.
  // This call returns immediately.
  void FlushNow(base::OnceClosure callback = base::Closure());

  // Triggers a write to disk to happen immediately and doesn't return until the
  // I/O has completed.
  void FlushSynchronous();

  const Options& options() const { return options_; }

  void set_network_task_runner(
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner) {
    DCHECK(network_task_runner);
    network_task_runner_ = network_task_runner;
  }

 protected:
  // Queues a flush to be executed as soon as possible.  As soon as possible
  // will be as soon as any existing flush completes, or right away if no
  // existing flush is happening.  Note that it is protected and virtual for
  // white box testing purposes.
  virtual void QueueFlush(base::OnceClosure callback);

 private:
  // Called by the constructor to create the private implementation object and
  // perform any other initialization required on the dedicated thread.
  void InitializeTaskInThread();

  // Give StorageManagerTest access, so we can more easily test some internals.
  friend class StorageManagerTest;

  // Flushes all queued flushes to the savegame thread.
  void FlushInternal();

  // Initialize the store. This blocks until the savegame load is
  // complete.
  void FinishInit();

  // Stops any timers that are currently running.
  void StopFlushOnChangeTimers();

  // Callback when flush timer has elapsed.
  void OnFlushOnChangeTimerFired();

  // Logic to be executed on the store thread when a flush completes.  Will
  // dispatch |flush_processing_callbacks_| callbacks and execute a new flush
  // if |flush_requested_| is true.
  void OnFlushIOCompletedCallback();

  // This function will not return until all queued I/O is completed.  Since
  // it will require the store message loop to process, it must be called from
  // outside the store message loop (such as from StorageManager's destructor).
  void FinishIO();

  // This function will immediately the on change timers if they are running.
  void FireRunningOnChangeTimers();

  // Ensure that we destroy certain objects on the store thread.
  // From base::MessageLoop::DestructionObserver.
  void WillDestroyCurrentMessageLoop() override;

  // Configuration options for the Storage Manager.
  Options options_;

  // Storage manager runs on its own thread. This is where store
  // operations are done.
  std::unique_ptr<base::Thread> storage_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> storage_task_runner_;

  // Cookie operations must be posted to network thread.
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  std::unique_ptr<MemoryStore> memory_store_;

  // When the savegame is loaded at startup, we keep the raw data around
  // until we can initialize the store on the correct thread.
  std::unique_ptr<Savegame::ByteVector> loaded_raw_bytes_;

  // Timers that start running when FlushOnChange() is called. When the time
  // elapses, we actually perform the write. This is a simple form of rate
  // limiting I/O writes.
  // |flush_on_last_change_timer_| is re-started on each change, enabling
  // changes to collect if several happen within a short period of time.
  // |flush_on_change_max_delay_timer_| starts on the first change and is never
  // re-started, ensuring that the flush always occurs within its delay and
  // cannot be pushed back indefinitely.
  std::unique_ptr<base::OneShotTimer> flush_on_last_change_timer_;
  std::unique_ptr<base::OneShotTimer> flush_on_change_max_delay_timer_;

  // See comments for for kDatabaseUserVersion.
  int loaded_database_version_;
  // false until the store is fully configured.
  bool initialized_;

  // True if a flush is currently being processed on the storage message loop.
  // In this case, we should not issue more flushes, but instead set
  // |flush_requested_| to true to ensure that a new flush is submitted as
  // soon as we are done processing the current one.
  bool flush_processing_;

  // The queue of callbacks that are should be called when the current flush
  // completes.  If this is not empty, then |flush_processing_| must be true.
  std::vector<base::OnceClosure> flush_processing_callbacks_;

  // True if |flush_processing_| is true, but we would like to perform a new
  // flush as soon as it completes.
  bool flush_pending_;

  // The queue of callbacks that will be called when the flush that follows
  // the current flush completes.  If this is non-empty, then |flush_pending_|
  // must be true.
  std::vector<base::OnceClosure> flush_pending_callbacks_;

  base::WaitableEvent no_flushes_pending_;

  // An object that wraps Savegame inside of an I/O thread so that we can
  // flush data asynchronously.
  std::unique_ptr<SavegameThread> savegame_thread_;

  DISALLOW_COPY_AND_ASSIGN(StorageManager);
};

}  // namespace storage
}  // namespace cobalt

#endif  // COBALT_STORAGE_STORAGE_MANAGER_H_
