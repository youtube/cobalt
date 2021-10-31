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

#ifndef COBALT_STORAGE_SAVEGAME_THREAD_H_
#define COBALT_STORAGE_SAVEGAME_THREAD_H_

#include <memory>

#include "base/callback.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "cobalt/storage/savegame.h"

namespace cobalt {
namespace storage {

// The SavegameThread wraps a Savegame object within its own thread in order to
// enable asynchronous writes to the savegame object.
class SavegameThread {
 public:
  explicit SavegameThread(const Savegame::Options& options);
  ~SavegameThread();

  // Returns the savegame data that existed on disk on startup.  This method
  // should only be called once, as the data will be released after it is
  // called.
  std::unique_ptr<Savegame::ByteVector> GetLoadedRawBytes();

  // Flush data to be written to the savegame file.  The write will happen
  // asynchronously on SavegameThread's I/O thread, so this method will return
  // immediately.
  void Flush(std::unique_ptr<Savegame::ByteVector> raw_bytes_ptr,
             const base::Closure& on_flush_complete);

 private:
  // Run on the I/O thread to start loading the savegame.
  void InitializeOnIOThread();

  // Called by the destructor, to ensure we destroy certain objects on the
  // I/O thread.
  void ShutdownOnIOThread();

  // Runs on the I/O thread to write the database to the savegame's persistent
  // storage.
  void FlushOnIOThread(std::unique_ptr<Savegame::ByteVector> raw_bytes_ptr,
                       const base::Closure& on_flush_complete);

  // Savegame options passed in to SavegameThread's constructor.
  Savegame::Options options_;

  // On initialization, we'll read from the existing savegame file to get
  // the latest data.  This holds a reference to that data, but is released
  // as soon as GetLoadedRawBytes() is called for the first time.
  std::unique_ptr<Savegame::ByteVector> loaded_raw_bytes_;

  // The database gets loaded from disk. We block on returning a SQL context
  // until storage_ready_ is signalled.
  base::WaitableEvent initialized_;

  // Storage I/O (savegame reads/writes) runs on a separate thread.
  std::unique_ptr<base::Thread> thread_;

  // Interface to platform-specific savegame data.
  std::unique_ptr<Savegame> savegame_;

  // How many flush failures have occurred since the last successful flush.
  // Flushes (storage writes) may sometimes fail, but we want to make sure
  // they're not consistently failing.
  int num_consecutive_flush_failures_;
};

}  // namespace storage
}  // namespace cobalt

#endif  // COBALT_STORAGE_SAVEGAME_THREAD_H_
