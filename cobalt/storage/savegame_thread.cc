// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include <memory>

#include "cobalt/storage/savegame_thread.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/trace_event/trace_event.h"

// An arbitrary max size for the save game file so that, eg, a
// corrupt filesystem cannot cause us to allocate a fatally large
// memory buffer.
static size_t kMaxSaveGameSizeBytes = 4 * 1024 * 1024;

namespace cobalt {
namespace storage {

SavegameThread::SavegameThread(const Savegame::Options& options)
    : options_(options),
      initialized_(base::WaitableEvent::ResetPolicy::MANUAL,
                   base::WaitableEvent::InitialState::NOT_SIGNALED),
      thread_(new base::Thread("Savegame I/O")),
      num_consecutive_flush_failures_(0) {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  thread_->Start();
  thread_->message_loop()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&SavegameThread::InitializeOnIOThread,
                            base::Unretained(this)));
}

SavegameThread::~SavegameThread() {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  // Wait for all tasks to finish.
  thread_->message_loop()->task_runner()->PostBlockingTask(
      FROM_HERE,
      base::Bind(&SavegameThread::ShutdownOnIOThread, base::Unretained(this)));

  thread_.reset();
}

std::unique_ptr<Savegame::ByteVector> SavegameThread::GetLoadedRawBytes() {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  initialized_.Wait();
  return std::move(loaded_raw_bytes_);
}

void SavegameThread::Flush(std::unique_ptr<Savegame::ByteVector> raw_bytes_ptr,
                           const base::Closure& on_flush_complete) {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  DCHECK(raw_bytes_ptr->size() < kMaxSaveGameSizeBytes);
  if (raw_bytes_ptr->size() > kMaxSaveGameSizeBytes) {
    // We must not save a file larger than kMaxSaveGameSizeBytes
    // because we won't be able to reload it. Something must have
    // gone wrong, and it's far better to leave any existing readable
    // data that's persisted than it is to risk making it unreadable.
    if (!on_flush_complete.is_null()) {
      on_flush_complete.Run();
    }
    return;
  }
  thread_->message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&SavegameThread::FlushOnIOThread, base::Unretained(this),
                 base::Passed(&raw_bytes_ptr), on_flush_complete));
}

void SavegameThread::InitializeOnIOThread() {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  DCHECK_EQ(thread_->message_loop(), base::MessageLoop::current());

  // Create a savegame object on the storage I/O thread.
  savegame_ = options_.CreateSavegame();

  // Load the save data into our VFS, if it exists.
  loaded_raw_bytes_.reset(new Savegame::ByteVector());
  savegame_->Read(loaded_raw_bytes_.get(), kMaxSaveGameSizeBytes);

  // Signal storage is ready. Anyone waiting for the load will now be unblocked.
  initialized_.Signal();
}

void SavegameThread::ShutdownOnIOThread() {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  DCHECK_EQ(thread_->message_loop(), base::MessageLoop::current());
  // Ensure these objects are destroyed on the proper thread.
  savegame_.reset(NULL);
}

void SavegameThread::FlushOnIOThread(
    std::unique_ptr<Savegame::ByteVector> raw_bytes_ptr,
    const base::Closure& on_flush_complete) {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  DCHECK_EQ(thread_->message_loop(), base::MessageLoop::current());
  if (raw_bytes_ptr->size() > 0) {
    bool ret = savegame_->Write(*raw_bytes_ptr);
    if (ret) {
      num_consecutive_flush_failures_ = 0;
    } else {
      DLOG(ERROR) << "Save failed.";
      const int kMaxConsecutiveFlushFailures = 2;
      DCHECK_LT(++num_consecutive_flush_failures_,
                kMaxConsecutiveFlushFailures);
    }
  }

  if (!on_flush_complete.is_null()) {
    on_flush_complete.Run();
  }
}

}  // namespace storage
}  // namespace cobalt
