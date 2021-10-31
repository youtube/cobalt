// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

// Based on thread_pthread.c.

#include "internal.h"

#if defined(STARBOARD)

#include <openssl/mem.h>

#include "starboard/once.h"
#include "starboard/thread.h"

namespace {

// Each enum of thread_local_data_t corresponds to a ThreadLocalEntry.
struct ThreadLocalEntry {
  thread_local_destructor_t destructor;
  void* value;
};

// One thread local key will point to an array of ThreadLocalEntry.
SbThreadLocalKey g_thread_local_key = kSbThreadLocalKeyInvalid;

// Control the creation of the global thread local key.
SbOnceControl g_thread_local_once_control = SB_ONCE_INITIALIZER;

void ThreadLocalDestructor(void* value) {
  if (value) {
    ThreadLocalEntry* thread_locals = static_cast<ThreadLocalEntry*>(value);
    for (int i = 0; i < NUM_OPENSSL_THREAD_LOCALS; ++i) {
      if (thread_locals[i].destructor != nullptr) {
        thread_locals[i].destructor(thread_locals[i].value);
      }
    }
    OPENSSL_free(value);
  }
}

void ThreadLocalInit() {
  g_thread_local_key = SbThreadCreateLocalKey(&ThreadLocalDestructor);
}

void EnsureInitialized(struct CRYPTO_STATIC_MUTEX* lock) {
  enum {
    kUninitialized = 0,
    kInitializing,
    kInitialized
  };

  if (SbAtomicNoBarrier_Load(&lock->initialized) == kInitialized) {
    return;
  }
  if (SbAtomicNoBarrier_CompareAndSwap(&lock->initialized,
      kUninitialized, kInitializing) == kUninitialized) {
    CRYPTO_MUTEX_init(&lock->mutex);
    SbAtomicNoBarrier_Store(&lock->initialized, kInitialized);
    return;
  }
  while (SbAtomicNoBarrier_Load(&lock->initialized) != kInitialized) {
    SbThreadSleep(kSbTimeMillisecond);
  }
}

}  // namespace

void CRYPTO_MUTEX_init(CRYPTO_MUTEX* lock) {
  if (!SbMutexCreate(&lock->mutex)) {
    OPENSSL_port_abort();
  }
  if (!SbConditionVariableCreate(&lock->condition, &lock->mutex)) {
    OPENSSL_port_abort();
  }
  lock->readers = 0;
  lock->writing = false;
}

// https://en.wikipedia.org/wiki/Readers%E2%80%93writer_lock
void CRYPTO_MUTEX_lock_read(CRYPTO_MUTEX* lock) {
  if (SbMutexAcquire(&lock->mutex) != kSbMutexAcquired) {
    OPENSSL_port_abort();
  }
  while (lock->writing) {
    if (SbConditionVariableWait(&lock->condition, &lock->mutex) ==
        kSbConditionVariableFailed) {
      OPENSSL_port_abort();
    }
  }
  ++(lock->readers);
  if (!SbMutexRelease(&lock->mutex)) {
    OPENSSL_port_abort();
  }
}

// https://en.wikipedia.org/wiki/Readers%E2%80%93writer_lock
void CRYPTO_MUTEX_lock_write(CRYPTO_MUTEX* lock) {
  if (SbMutexAcquire(&lock->mutex) != kSbMutexAcquired) {
    OPENSSL_port_abort();
  }
  while (lock->writing) {
    if (SbConditionVariableWait(&lock->condition, &lock->mutex) ==
        kSbConditionVariableFailed) {
      OPENSSL_port_abort();
    }
  }
  lock->writing = true;
  while (lock->readers > 0) {
    if (SbConditionVariableWait(&lock->condition, &lock->mutex) ==
        kSbConditionVariableFailed) {
      OPENSSL_port_abort();
    }
  }
  if (!SbMutexRelease(&lock->mutex)) {
    OPENSSL_port_abort();
  }
}

// https://en.wikipedia.org/wiki/Readers%E2%80%93writer_lock
void CRYPTO_MUTEX_unlock_read(CRYPTO_MUTEX* lock) {
  if (SbMutexAcquire(&lock->mutex) != kSbMutexAcquired) {
    OPENSSL_port_abort();
  }
  if (--(lock->readers) == 0) {
    SbConditionVariableBroadcast(&lock->condition);
  }
  if (!SbMutexRelease(&lock->mutex)) {
    OPENSSL_port_abort();
  }
}

// https://en.wikipedia.org/wiki/Readers%E2%80%93writer_lock
void CRYPTO_MUTEX_unlock_write(CRYPTO_MUTEX* lock) {
  if (SbMutexAcquire(&lock->mutex) != kSbMutexAcquired) {
    OPENSSL_port_abort();
  }
  lock->writing = false;
  SbConditionVariableBroadcast(&lock->condition);
  if (!SbMutexRelease(&lock->mutex)) {
    OPENSSL_port_abort();
  }
}

void CRYPTO_MUTEX_cleanup(CRYPTO_MUTEX* lock) {
  if (!SbConditionVariableDestroy(&lock->condition)) {
    OPENSSL_port_abort();
  }
  if (!SbMutexDestroy(&lock->mutex)) {
    OPENSSL_port_abort();
  }
}

void CRYPTO_STATIC_MUTEX_lock_read(struct CRYPTO_STATIC_MUTEX* lock) {
  EnsureInitialized(lock);
  CRYPTO_MUTEX_lock_read(&lock->mutex);
}

void CRYPTO_STATIC_MUTEX_lock_write(struct CRYPTO_STATIC_MUTEX* lock) {
  EnsureInitialized(lock);
  CRYPTO_MUTEX_lock_write(&lock->mutex);
}

void CRYPTO_STATIC_MUTEX_unlock_read(struct CRYPTO_STATIC_MUTEX* lock) {
  EnsureInitialized(lock);
  CRYPTO_MUTEX_unlock_read(&lock->mutex);
}

void CRYPTO_STATIC_MUTEX_unlock_write(struct CRYPTO_STATIC_MUTEX* lock) {
  EnsureInitialized(lock);
  CRYPTO_MUTEX_unlock_write(&lock->mutex);
}

void* CRYPTO_get_thread_local(thread_local_data_t index) {
  SbOnce(&g_thread_local_once_control, &ThreadLocalInit);
  if (!SbThreadIsValidLocalKey(g_thread_local_key)) {
    return nullptr;
  }

  ThreadLocalEntry* thread_locals = static_cast<ThreadLocalEntry*>(
      SbThreadGetLocalValue(g_thread_local_key));
  if (thread_locals == nullptr) {
    return nullptr;
  }

  return thread_locals[index].value;
}

int CRYPTO_set_thread_local(thread_local_data_t index, void *value,
                            thread_local_destructor_t destructor) {
  SbOnce(&g_thread_local_once_control, &ThreadLocalInit);
  if (!SbThreadIsValidLocalKey(g_thread_local_key)) {
    destructor(value);
    return 0;
  }

  ThreadLocalEntry* thread_locals = static_cast<ThreadLocalEntry*>(
      SbThreadGetLocalValue(g_thread_local_key));
  if (thread_locals == nullptr) {
    size_t thread_locals_size =
        sizeof(ThreadLocalEntry) * NUM_OPENSSL_THREAD_LOCALS;
    thread_locals = static_cast<ThreadLocalEntry*>(
        OPENSSL_malloc(thread_locals_size));
    if (thread_locals == nullptr) {
      destructor(value);
      return 0;
    }
    OPENSSL_memset(thread_locals, 0, thread_locals_size);
    if (!SbThreadSetLocalValue(g_thread_local_key, thread_locals)) {
      OPENSSL_free(thread_locals);
      destructor(value);
      return 0;
    }
  }

  thread_locals[index].destructor = destructor;
  thread_locals[index].value = value;
  return 1;
}

#endif  // defined(STARBOARD)
