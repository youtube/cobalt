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

#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>

#include <openssl/mem.h>

#include "starboard/system.h"
#include "starboard/thread.h"


// Each enum of thread_local_data_t corresponds to a ThreadLocalEntry.
typedef struct ThreadLocalEntry {
  thread_local_destructor_t destructor;
  void* value;
} ThreadLocalEntry;

// One thread local key will point to an array of ThreadLocalEntry.
static pthread_key_t g_thread_local_key = 0;
static int g_thread_local_key_created = 0;

// Control the creation of the global thread local key.
static pthread_once_t g_thread_local_once_control = PTHREAD_ONCE_INIT;

static void ThreadLocalDestructor(void* value) {
  if (value) {
    ThreadLocalEntry* thread_locals = (ThreadLocalEntry*)(value);
    for (int i = 0; i < NUM_OPENSSL_THREAD_LOCALS; ++i) {
      if (thread_locals[i].destructor != NULL) {
        thread_locals[i].destructor(thread_locals[i].value);
      }
    }
    OPENSSL_free(value);
  }
}

static void ThreadLocalInit() {
  g_thread_local_key_created = pthread_key_create(&g_thread_local_key, &ThreadLocalDestructor) == 0;
}

static void EnsureInitialized(struct CRYPTO_STATIC_MUTEX* lock) {
  enum {
    kUninitialized = 0,
    kInitializing,
    kInitialized
  };

  if (atomic_load((_Atomic uint32_t *)&lock->initialized) == kInitialized) {
    return;
  }

  uint32_t expected = kUninitialized;

  if (atomic_compare_exchange_weak((_Atomic uint32_t *)&lock->initialized,
                                   &expected, kInitializing)) {
    CRYPTO_MUTEX_init(&lock->mutex);
    atomic_store((_Atomic uint32_t *)&lock->initialized, kInitialized);
    return;
  }
  while (atomic_load((_Atomic uint32_t *)&lock->initialized) != kInitialized) {
    usleep(1000);  // 1ms
  }
}

void CRYPTO_MUTEX_init(CRYPTO_MUTEX* lock) {
  if (pthread_mutex_init(&lock->mutex, NULL) != 0) {
    SbSystemBreakIntoDebugger();
  }
  if (pthread_cond_init(&lock->condition, NULL) != 0) {
    SbSystemBreakIntoDebugger();
  }
  lock->readers = 0;
  lock->writing = false;
}

// https://en.wikipedia.org/wiki/Readers%E2%80%93writer_lock
void CRYPTO_MUTEX_lock_read(CRYPTO_MUTEX* lock) {
  if (pthread_mutex_lock(&lock->mutex) != 0) {
    SbSystemBreakIntoDebugger();
  }
  while (lock->writing) {
    if (pthread_cond_wait(&lock->condition, &lock->mutex) != 0) {
      SbSystemBreakIntoDebugger();
    }
  }
  ++(lock->readers);
  if (pthread_mutex_unlock(&lock->mutex) != 0) {
    SbSystemBreakIntoDebugger();
  }
}

// https://en.wikipedia.org/wiki/Readers%E2%80%93writer_lock
void CRYPTO_MUTEX_lock_write(CRYPTO_MUTEX* lock) {
  if (pthread_mutex_lock(&lock->mutex) != 0) {
    SbSystemBreakIntoDebugger();
  }
  while (lock->writing) {
    if (pthread_cond_wait(&lock->condition, &lock->mutex) != 0) {
      SbSystemBreakIntoDebugger();
    }
  }
  lock->writing = true;
  while (lock->readers > 0) {
    if (pthread_cond_wait(&lock->condition, &lock->mutex) != 0) {
      SbSystemBreakIntoDebugger();
    }
  }
  if (pthread_mutex_unlock(&lock->mutex) != 0) {
    SbSystemBreakIntoDebugger();
  }
}

// https://en.wikipedia.org/wiki/Readers%E2%80%93writer_lock
void CRYPTO_MUTEX_unlock_read(CRYPTO_MUTEX* lock) {
  if (pthread_mutex_lock(&lock->mutex) != 0) {
    SbSystemBreakIntoDebugger();
  }
  if (--(lock->readers) == 0) {
    pthread_cond_broadcast(&lock->condition);
  }
  if (pthread_mutex_unlock(&lock->mutex) != 0) {
    SbSystemBreakIntoDebugger();
  }
}

// https://en.wikipedia.org/wiki/Readers%E2%80%93writer_lock
void CRYPTO_MUTEX_unlock_write(CRYPTO_MUTEX* lock) {
  if (pthread_mutex_lock(&lock->mutex) != 0) {
    SbSystemBreakIntoDebugger();
  }
  lock->writing = false;
  pthread_cond_broadcast(&lock->condition);
  if (pthread_mutex_unlock(&lock->mutex) != 0) {
    SbSystemBreakIntoDebugger();
  }
}

void CRYPTO_MUTEX_cleanup(CRYPTO_MUTEX* lock) {
  if (pthread_cond_destroy(&lock->condition) != 0) {
    SbSystemBreakIntoDebugger();
  }
  if (pthread_mutex_destroy(&lock->mutex) != 0) {
    SbSystemBreakIntoDebugger();
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
  pthread_once(&g_thread_local_once_control, &ThreadLocalInit);
  if (!g_thread_local_key_created) {
    return NULL;
  }

  ThreadLocalEntry* thread_locals = (ThreadLocalEntry*)(
      pthread_getspecific(g_thread_local_key));
  if (thread_locals == NULL) {
    return NULL;
  }

  return thread_locals[index].value;
}

int CRYPTO_set_thread_local(thread_local_data_t index, void *value,
                            thread_local_destructor_t destructor) {
  pthread_once(&g_thread_local_once_control, &ThreadLocalInit);
  if (!g_thread_local_key_created) {
    destructor(value);
    return 0;
  }

  ThreadLocalEntry* thread_locals = (ThreadLocalEntry*)(
      pthread_getspecific(g_thread_local_key));
  if (thread_locals == NULL) {
    size_t thread_locals_size =
        sizeof(ThreadLocalEntry) * NUM_OPENSSL_THREAD_LOCALS;
    thread_locals = (ThreadLocalEntry*)(
        OPENSSL_malloc(thread_locals_size));
    if (thread_locals == NULL) {
      destructor(value);
      return 0;
    }
    OPENSSL_memset(thread_locals, 0, thread_locals_size);
    if (pthread_setspecific(g_thread_local_key, thread_locals) !=0 ) {
      OPENSSL_free(thread_locals);
      destructor(value);
      return 0;
    }
  }

  thread_locals[index].destructor = destructor;
  thread_locals[index].value = value;
  return 1;
}
