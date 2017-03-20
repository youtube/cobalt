// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_THREAD_LOCAL_STORAGE_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_THREAD_LOCAL_STORAGE_INTERNAL_H_

#include <map>
#include <vector>

#include "starboard/mutex.h"
#include "starboard/shared/internal_only.h"
#include "starboard/thread.h"

namespace starboard {
namespace shared {

class TLSKeyManager {
 public:
  static TLSKeyManager* Get();

  TLSKeyManager();

  // Returns a unique ID that acts as a new key for a thread local storage
  // value.
  SbThreadLocalKey CreateKey(SbThreadLocalDestructor destructor);

  // Destroys the specified key, making its ID available for re-use in a
  // subsequent call to AllocateKey().
  void DestroyKey(SbThreadLocalKey key);

  // Sets the thread local value for the given key.
  bool SetLocalValue(SbThreadLocalKey key, void* value);

  // Returns the thread local value for the given key.
  void* GetLocalValue(SbThreadLocalKey key);

  // Called whenever a thread is created.
  void InitializeTLSForThread();

  // Called whenever a thread is destroyed.
  void ShutdownTLSForThread();

 private:
  // We add 1 to SB_MAX_THREADS here to account for the main thread.
  static const int kMaxThreads = SB_MAX_THREADS + 1;
  struct KeyRecord {
    bool valid;
    SbThreadLocalDestructor destructor;
    void* values[kMaxThreads];
  };

  // Sets up the specified key.
  int GetUnusedKeyIndex();

  // Returns true if the given key is a valid, current unique TLS ID.  Active
  // means it has been allocated via AllocateKey() but has not yet been
  // destroyed via DestroyKey().
  bool IsKeyActive(SbThreadLocalKey key);

  // Returns the TLS ID of the current thread, used for indexing into
  // KeyRecord::values.
  int GetCurrentThreadId();

  // Access to the class members below are protected by this mutex so that they
  // can be updated by any thread at any time.
  Mutex mutex_;

  // The key record tracks all key values among all threads, along with their
  // destructors, if specified.
  std::vector<KeyRecord> key_table_;

  // Tracks all thread IDs that are still available.
  std::vector<int> available_thread_ids_;

  // This maps Starboard thread IDs to TLS thread ids.
  std::map<SbThreadId, int> thread_id_map_;
};

}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_THREAD_LOCAL_STORAGE_INTERNAL_H_
