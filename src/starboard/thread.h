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

// Thread creation and cleanup.

#ifndef STARBOARD_THREAD_H_
#define STARBOARD_THREAD_H_

#include "starboard/export.h"
#include "starboard/thread_types.h"
#include "starboard/time.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// A spectrum of thread priorities. Platforms map them appropriately to their
// own priority system. Note that scheduling is platform-specific, and what
// these priorities mean, if they mean anything at all, is also
// platform-specific.
//
// In particular, several of these priority values can map to the same priority
// on a given platform. The only guarantee is that each lower priority should be
// treated less-than-or-equal-to a higher priority.
typedef enum SbThreadPriority {
  // The lowest thread priority available on the current platform.
  kSbThreadPriorityLowest,

  // A lower-than-normal thread priority, if available on the current platform.
  kSbThreadPriorityLow,

  // Really, what is normal? You should spend time pondering that question more
  // than you consider less-important things, but less than you think about
  // more-important things.
  kSbThreadPriorityNormal,

  // A higher-than-normal thread priority, if available on the current platform.
  kSbThreadPriorityHigh,

  // The highest thread priority available on the current platform that isn't
  // considered "real-time" or "time-critical," if those terms have any meaning
  // on the current platform.
  kSbThreadPriorityHighest,

  // If the platform provides any kind of real-time or time-critical scheduling,
  // this priority will request that treatment. Real-time scheduling generally
  // means that the thread will have more consistency in scheduling than
  // non-real-time scheduled threads, often by being more deterministic in how
  // threads run in relation to each other. But exactly how being real-time
  // affects the thread scheduling is platform-specific.
  //
  // For platforms where that is not offered, or otherwise not meaningful, this
  // will just be the highest priority available in the platform's scheme, which
  // may be the same as kThreadPriority_Highest.
  kSbThreadPriorityRealTime,

  // Well-defined constant value to mean "no priority."  This means to use the
  // default priority assignment method of that platform. This may mean to
  // inherit the priority of the spawning thread, or it may mean a specific
  // default priority, or it may mean something else, depending on the platform.
  kSbThreadNoPriority = kSbInvalidInt,
} SbThreadPriority;

// An ID type that is unique per thread.
typedef int32_t SbThreadId;

// Function pointer type for SbThreadCreate.  |context| is a pointer-sized bit
// of data passed in from the calling thread.
typedef void* (*SbThreadEntryPoint)(void* context);

// Function pointer type for Thread-Local destructors.
typedef void (*SbThreadLocalDestructor)(void* value);

// Type for thread core affinity. This generally will be a single cpu (or core
// or hyperthread) identifier. Some platforms may not support affinity, and some
// may have specific rules about how it must be used.
typedef int32_t SbThreadAffinity;

// Private structure representing a thread-local key.
typedef struct SbThreadLocalKeyPrivate SbThreadLocalKeyPrivate;

// A handle to a thread-local key.
typedef SbThreadLocalKeyPrivate* SbThreadLocalKey;

// Well-defined constant value to mean "no thread ID."
#define kSbThreadInvalidId (SbThreadId)0

// Well-defined constant value to mean "no affinity."
#define kSbThreadNoAffinity (SbThreadAffinity) kSbInvalidInt

// Well-defined constant value to mean "no thread local key."
#define kSbThreadLocalKeyInvalid (SbThreadLocalKey) NULL

// Returns whether the given thread handle is valid.
static SB_C_INLINE bool SbThreadIsValid(SbThread thread) {
  return thread != kSbThreadInvalid;
}

// Returns whether the given thread ID is valid.
static SB_C_INLINE bool SbThreadIsValidId(SbThreadId id) {
  return id != kSbThreadInvalidId;
}

// Returns whether the given thread priority is valid.
static SB_C_INLINE bool SbThreadIsValidPriority(SbThreadPriority priority) {
  return priority != kSbThreadNoPriority;
}

// Returns whether the given thread affinity is valid.
static SB_C_INLINE bool SbThreadIsValidAffinity(SbThreadAffinity affinity) {
  return affinity != kSbThreadNoAffinity;
}

// Returns whether the given thread local variable key is valid.
static SB_C_INLINE bool SbThreadIsValidLocalKey(SbThreadLocalKey key) {
  return key != kSbThreadLocalKeyInvalid;
}

// Creates a new thread, which starts immediately. The |stack_size| parameter
// can be 0 to indicate that the default stack size should be used. |priority|
// can be set to kSbThreadNoPriority to use the default priority for the
// platform (e.g. possibly a fixed, standard priority, or possibly a priority
// inherited from the thread that is calling SbThreadCreate()). |affinity| can
// be set to kSbThreadNoAffinity to use the default affinity for the platform.
// |joinable| sets whether the thread can be joined, or should start out
// "detached." |name| is mainly used for debugging, may be NULL, and may not
// even be used in production builds. Returns a handle to the newly created
// thread upon success, and returns kSbThreadInvalid if not. |entry_point| will
// be executed on the newly created thread, and passed |context|.
//
// NOTE: For joinable threads, when you are done with the thread handle, you
// must call SbThreadJoin to release system resources associated with the
// thread. This is not necessary for detached threads, but a detached thread
// cannot be joined.
SB_EXPORT SbThread SbThreadCreate(int64_t stack_size,
                                  SbThreadPriority priority,
                                  SbThreadAffinity affinity,
                                  bool joinable,
                                  const char* name,
                                  SbThreadEntryPoint entry_point,
                                  void* context);

// Joins with joinable |thread|, created with SbThreadCreate.  This function
// blocks the caller until the designated thread exits, and then cleans up that
// thread's resources.  Returns true on success, false if |thread| is invalid or
// detached. This will essentially detach |thread|. Each joinable thread must be
// joined to be fully cleaned up. If |out_return| is not NULL, the return value
// of the thread's main function is placed there.
//
// Each joinable thread can only be joined once. Once SbThreadJoin is called,
// the thread behaves as if it was detached to all threads other than the
// joining thread.
SB_EXPORT bool SbThreadJoin(SbThread thread, void** out_return);

// Detaches |thread|, which prevents it from being joined. This is sort of like
// a non-blocking join. Does nothing if the thread is already detached, or is
// already being joined by another thread.
SB_EXPORT void SbThreadDetach(SbThread thread);

// Yields the currently executing thread, so another thread has a chance to run.
SB_EXPORT void SbThreadYield();

// Sleeps the currently executing thread for at least the given |duration|. A
// negative duration does nothing.
SB_EXPORT void SbThreadSleep(SbTime duration);

// Gets the handle of the currently executing thread.
SB_EXPORT SbThread SbThreadGetCurrent();

// Gets the Thread ID of the currently executing thread.
SB_EXPORT SbThreadId SbThreadGetId();

// Returns whether |thread1| and |thread2| refer to the same thread.
SB_EXPORT bool SbThreadIsEqual(SbThread thread1, SbThread thread2);

// Gets the debug name of the currently executing thread.
SB_EXPORT void SbThreadGetName(char* buffer, int buffer_size);

// Sets the debug name of the currently executing thread. Will copy the
// specified name string.
SB_EXPORT void SbThreadSetName(const char* name);

// Creates a new, unique key for thread local data with given optional
// |destructor|. |destructor| may be NULL if there is no clean up
// needed. Returns kSbThreadLocalKeyInvalid if creation was unsuccessful.
//
// When does |destructor| get called? It can only be called in the owning
// thread, and let's just say thread interruption isn't viable. The destructor,
// if specified, is called on every thread's local values when the thread exits.
SB_EXPORT SbThreadLocalKey
SbThreadCreateLocalKey(SbThreadLocalDestructor destructor);

// Destroys thread local data |key|. Does nothing if key is
// kSbThreadLocalKeyInvalid or has already been destroyed. This does NOT call
// the destructor on any stored values.
SB_EXPORT void SbThreadDestroyLocalKey(SbThreadLocalKey key);

// Gets the pointer-sized value for |key| in the currently executing thread's
// local storage. Returns NULL if key is kSbThreadLocalKeyInvalid or has already
// been destroyed.
SB_EXPORT void* SbThreadGetLocalValue(SbThreadLocalKey key);

// Sets the pointer-sized value for |key| in the currently executing thread's
// local storage. Returns whether |key| is valid and has not already been
// destroyed.
SB_EXPORT bool SbThreadSetLocalValue(SbThreadLocalKey key, void* value);

// Returns whether |thread| is the current thread.
SB_C_INLINE bool SbThreadIsCurrent(SbThread thread) {
  return SbThreadGetCurrent() == thread;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_THREAD_H_
