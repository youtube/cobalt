// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_WIN32_THREAD_PRIVATE_H_
#define STARBOARD_SHARED_WIN32_THREAD_PRIVATE_H_

#include <windows.h>

#include <map>
#include <string>

#include "starboard/condition_variable.h"
#include "starboard/mutex.h"
#include "starboard/once.h"
#include "starboard/shared/internal_only.h"
#include "starboard/thread.h"

struct SbThreadLocalKeyPrivate {
  DWORD tls_index;
  SbThreadLocalDestructor destructor;
};

namespace starboard {
namespace shared {
namespace win32 {

class ThreadSubsystemSingleton;

// Creates a SbThreadLocalKey given a ThreadSubsystemSingleton. Used
// to create the first SbThreadLocalKey.
SbThreadLocalKey SbThreadCreateLocalKeyInternal(
    SbThreadLocalDestructor destructor,
    ThreadSubsystemSingleton* singleton);

// Singleton state for the thread subsystem.
class ThreadSubsystemSingleton {
 public:
  ThreadSubsystemSingleton()
      : mutex_(SB_MUTEX_INITIALIZER),
        thread_private_key_(SbThreadCreateLocalKeyInternal(NULL, this)) {}
  // This mutex protects all class members
  SbMutex mutex_;
  // Allocated thread_local_keys. Note that std::map is used
  // so that elements can be deleted without triggering an allocation.
  std::map<DWORD, SbThreadLocalKeyPrivate*> thread_local_keys_;
  // Thread-local key for the thread's SbThreadPrivate
  SbThreadLocalKey thread_private_key_;
};

// Obtains the ThreadsSubsystemSingleton();
ThreadSubsystemSingleton* GetThreadSubsystemSingleton();

// Registers the main thread. setting it's SbThreadPrivate*
void RegisterMainThread();

// Private thread state, stored in thread local storage and
// cleaned up when thread exits.
class SbThreadPrivate {
 public:
  SbThreadPrivate()
      : mutex_(SB_MUTEX_INITIALIZER),
        condition_(SB_CONDITION_VARIABLE_INITIALIZER),
        handle_(NULL),
        result_(NULL),
        wait_for_join_(false),
        result_is_valid_(false) {}

  ~SbThreadPrivate() {
    if (handle_) {
      CloseHandle(handle_);
    }

    SbMutexDestroy(&mutex_);
    SbConditionVariableDestroy(&condition_);
  }

  // This mutex protects all class members
  SbMutex mutex_;
  SbConditionVariable condition_;
  std::string name_;
  HANDLE handle_;
  // The result of the thread. The return value of SbThreadEntryPoint
  // to return to SbThreadJoin.
  void* result_;
  // True if a thread must wait to be joined before completely exiting.
  // Changes to this must signal |condition_|.
  bool wait_for_join_;
  // True if |result_| is valid (the thread has completed and is waiting
  // to exit). Changes to this must signal |condition_|.
  bool result_is_valid_;
};

// Obtains the current thread's SbThreadPrivate* from thread-local storage.
SbThreadPrivate* GetCurrentSbThreadPrivate();

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_THREAD_PRIVATE_H_
