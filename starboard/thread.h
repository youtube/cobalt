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

// Module Overview: Starboard Thread module
//
// Defines functionality related to thread creation and cleanup.

#ifndef STARBOARD_THREAD_H_
#define STARBOARD_THREAD_H_

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/thread_types.h"
#include "starboard/time.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if SB_API_VERSION >= 12

// An opaque handle to a thread type.
typedef void* SbThread;

#define kSbThreadInvalid (SbThread) NULL

#endif  // SB_API_VERSION >= 12

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
  // may be the same as kSbThreadPriorityHighest.
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

// Creates a new thread, which starts immediately.
// - If the function succeeds, the return value is a handle to the newly
//   created thread.
// - If the function fails, the return value is |kSbThreadInvalid|.
//
// |stack_size|: The amount of memory reserved for the thread. Set the value
//   to |0| to indicate that the default stack size should be used.
// |priority|: The thread's priority. This value can be set to
//   |kSbThreadNoPriority| to use the platform's default priority. As examples,
//   it could be set to a fixed, standard priority or to a priority inherited
//   from the thread that is calling SbThreadCreate(), or to something else.
// |affinity|: The thread's affinity. This value can be set to
//   |kSbThreadNoAffinity| to use the platform's default affinity.
// |joinable|: Indicates whether the thread can be joined (|true|) or should
//   start out "detached" (|false|). Note that for joinable threads, when
//   you are done with the thread handle, you must call |SbThreadJoin| to
//   release system resources associated with the thread. This is not necessary
//   for detached threads, but detached threads cannot be joined.
// |name|: A name used to identify the thread. This value is used mainly for
//   debugging, it can be |NULL|, and it might not be used in production builds.
// |entry_point|: A pointer to a function that will be executed on the newly
//   created thread.
// |context|: This value will be passed to the |entry_point| function.
SB_EXPORT SbThread SbThreadCreate(int64_t stack_size,
                                  SbThreadPriority priority,
                                  SbThreadAffinity affinity,
                                  bool joinable,
                                  const char* name,
                                  SbThreadEntryPoint entry_point,
                                  void* context);

// Joins the thread on which this function is called with joinable |thread|.
// This function blocks the caller until the designated thread exits, and then
// cleans up that thread's resources. The cleanup process essentially detaches
// thread.
//
// The return value is |true| if the function is successful and |false| if
// |thread| is invalid or detached.
//
// Each joinable thread can only be joined once and must be joined to be fully
// cleaned up. Once SbThreadJoin is called, the thread behaves as if it were
// detached to all threads other than the joining thread.
//
// |thread|: The thread to which the current thread will be joined. The
//   |thread| must have been created with SbThreadCreate.
// |out_return|: If this is not |NULL|, then the SbThreadJoin function populates
//   it with the return value of the thread's |main| function.
SB_EXPORT bool SbThreadJoin(SbThread thread, void** out_return);

// Detaches |thread|, which prevents it from being joined. This is sort of like
// a non-blocking join. This function is a no-op if the thread is already
// detached or if the thread is already being joined by another thread.
//
// |thread|: The thread to be detached.
SB_EXPORT void SbThreadDetach(SbThread thread);

// Yields the currently executing thread, so another thread has a chance to run.
SB_EXPORT void SbThreadYield();

// Sleeps the currently executing thread.
//
// |duration|: The minimum amount of time, in microseconds, that the currently
//   executing thread should sleep. The function is a no-op if this value is
//   negative or |0|.
SB_EXPORT void SbThreadSleep(SbTime duration);

// Returns the handle of the currently executing thread.
SB_EXPORT SbThread SbThreadGetCurrent();

// Returns the Thread ID of the currently executing thread.
SB_EXPORT SbThreadId SbThreadGetId();

// Indicates whether |thread1| and |thread2| refer to the same thread.
//
// |thread1|: The first thread to compare.
// |thread2|: The second thread to compare.
SB_EXPORT bool SbThreadIsEqual(SbThread thread1, SbThread thread2);

// Returns the debug name of the currently executing thread.
SB_EXPORT void SbThreadGetName(char* buffer, int buffer_size);

// Sets the debug name of the currently executing thread by copying the
// specified name string.
//
// |name|: The name to assign to the thread.
SB_EXPORT void SbThreadSetName(const char* name);

// Creates and returns a new, unique key for thread local data. If the function
// does not succeed, the function returns |kSbThreadLocalKeyInvalid|.
//
// If |destructor| is specified, it will be called in the owning thread, and
// only in the owning thread, when the thread exits. In that case, it is called
// on the local value associated with the key in the current thread as long as
// the local value is not NULL.
//
// |destructor|: A pointer to a function. The value may be NULL if no clean up
//   is needed.
SB_EXPORT SbThreadLocalKey
SbThreadCreateLocalKey(SbThreadLocalDestructor destructor);

// Destroys thread local data for the specified key. The function is a no-op
// if the key is invalid (kSbThreadLocalKeyInvalid|) or has already been
// destroyed. This function does NOT call the destructor on any stored values.
//
// |key|: The key for which to destroy thread local data.
SB_EXPORT void SbThreadDestroyLocalKey(SbThreadLocalKey key);

// Returns the pointer-sized value for |key| in the currently executing thread's
// local storage. Returns |NULL| if key is |kSbThreadLocalKeyInvalid| or if the
// key has already been destroyed.
//
// |key|: The key for which to return the value.
SB_EXPORT void* SbThreadGetLocalValue(SbThreadLocalKey key);

// Sets the pointer-sized value for |key| in the currently executing thread's
// local storage. The return value indicates whether |key| is valid and has
// not already been destroyed.
//
// |key|: The key for which to set the key value.
// |value|: The new pointer-sized key value.
SB_EXPORT bool SbThreadSetLocalValue(SbThreadLocalKey key, void* value);

// Returns whether |thread| is the current thread.
//
// |thread|: The thread to check.
static SB_C_INLINE bool SbThreadIsCurrent(SbThread thread) {
  return SbThreadGetCurrent() == thread;
}

// Private structure representing the context of a frozen thread.
typedef struct SbThreadContextPrivate SbThreadContextPrivate;

// A handle to the context of a frozen thread.
typedef SbThreadContextPrivate* SbThreadContext;

// Well-defined value for an invalid thread context.
#define kSbThreadContextInvalid ((SbThreadContext)NULL)

// Returns whether the given thread context is valid.
static SB_C_INLINE bool SbThreadContextIsValid(SbThreadContext context) {
  return context != kSbThreadContextInvalid;
}

typedef enum SbThreadContextProperty {
  // Pointer to the current instruction (aka program counter).
  kSbThreadContextInstructionPointer,

  // Pointer to the top of the stack.
  kSbThreadContextStackPointer,

  // Pointer to the the current stack frame.
  kSbThreadContextFramePointer,

#if SB_API_VERSION >= 12
  // Pointer to where to return to when the current function call completes, or
  // nullptr on platforms without a link register.
  kSbThreadContextLinkRegister,
#endif  // SB_API_VERSION >= 12
} SbThreadContextProperty;

// Gets the specified pointer-type |property| from the specified |context|.
// Returns |true| if successful and |out_value| has been modified, otherwise
// returns |false| and |out_value| is not modified.
SB_EXPORT bool SbThreadContextGetPointer(SbThreadContext context,
                                         SbThreadContextProperty property,
                                         void** out_value);

// Private structure representing a thread sampler.
typedef struct SbThreadSamplerPrivate SbThreadSamplerPrivate;

// A handle to a thread sampler.
typedef SbThreadSamplerPrivate* SbThreadSampler;

// Well-defined value for an invalid thread sampler.
#define kSbThreadSamplerInvalid ((SbThreadSampler)NULL)

// Returns whether the given thread sampler is valid.
static SB_C_INLINE bool SbThreadSamplerIsValid(SbThreadSampler sampler) {
  return sampler != kSbThreadSamplerInvalid;
}

// Whether the current platform supports thread sampling. The result of this
// function must not change over the course of the program, which means that the
// results of this function may be cached indefinitely. If this returns false,
// |SbThreadSamplerCreate| will return an invalid sampler.
SB_EXPORT bool SbThreadSamplerIsSupported();

// Creates a new thread sampler for the specified |thread|.
//
// If successful, this function returns the newly created handle.
// If unsuccessful, this function returns |kSbThreadSamplerInvalid|.
SB_EXPORT SbThreadSampler SbThreadSamplerCreate(SbThread thread);

// Destroys the |sampler| and frees whatever resources it was using.
SB_EXPORT void SbThreadSamplerDestroy(SbThreadSampler sampler);

// Suspends execution of the thread that |sampler| was created for.
//
// If successful, this function returns a |SbThreadContext| for the frozen
// thread, from which properties may be read while the thread remains frozen.
// If unsuccessful, this function returns |kSbThreadContextInvalid|.
SB_EXPORT
SbThreadContext SbThreadSamplerFreeze(SbThreadSampler sampler);

// Resumes execution of the thread that |sampler| was created for. This
// invalidates the context returned from |SbThreadSamplerFreeze|.
SB_EXPORT bool SbThreadSamplerThaw(SbThreadSampler sampler);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_THREAD_H_
