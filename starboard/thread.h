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

#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "starboard/configuration.h"
#include "starboard/export.h"

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
  // may be the same as kSbThreadPriorityHighest.
  kSbThreadPriorityRealTime,

  // Well-defined constant value to mean "no priority."  This means to use the
  // default priority assignment method of that platform. This may mean to
  // inherit the priority of the spawning thread, or it may mean a specific
  // default priority, or it may mean something else, depending on the platform.
  kSbThreadNoPriority = INT_MIN,
} SbThreadPriority;

// An ID type that is unique per thread.
typedef int32_t SbThreadId;

// Well-defined constant value to mean "no thread ID."
#define kSbThreadInvalidId (SbThreadId)0

// Returns whether the given thread ID is valid.
static inline bool SbThreadIsValidId(SbThreadId id) {
  return id != kSbThreadInvalidId;
}

// Returns whether the given thread priority is valid.
static inline bool SbThreadIsValidPriority(SbThreadPriority priority) {
  return priority != kSbThreadNoPriority;
}

// Returns the Thread ID of the currently executing thread.
SB_EXPORT SbThreadId SbThreadGetId();

// Set the thread priority of the current thread.
SB_EXPORT bool SbThreadSetPriority(SbThreadPriority priority);

// Get the thread priority of the current thread.
SB_EXPORT bool SbThreadGetPriority(SbThreadPriority* priority);

// Private structure representing the context of a frozen thread.
typedef struct SbThreadContextPrivate SbThreadContextPrivate;

// A handle to the context of a frozen thread.
typedef SbThreadContextPrivate* SbThreadContext;

// Well-defined value for an invalid thread context.
#define kSbThreadContextInvalid ((SbThreadContext)NULL)

// Returns whether the given thread context is valid.
static inline bool SbThreadContextIsValid(SbThreadContext context) {
  return context != kSbThreadContextInvalid;
}

typedef enum SbThreadContextProperty {
  // Pointer to the current instruction (aka program counter).
  kSbThreadContextInstructionPointer,

  // Pointer to the top of the stack.
  kSbThreadContextStackPointer,

  // Pointer to the the current stack frame.
  kSbThreadContextFramePointer,

  // Pointer to where to return to when the current function call completes, or
  // nullptr on platforms without a link register.
  kSbThreadContextLinkRegister,
} SbThreadContextProperty;

// Gets the specified pointer-type |property| from the specified |context|.
// Returns |true| if successful and |out_value| has been modified, otherwise
// returns |false| and |out_value| is not modified.
SB_EXPORT bool SbThreadContextGetPointer(SbThreadContext context,
                                         SbThreadContextProperty property,
                                         void** out_value);

// A handle to a thread sampler.
typedef void* SbThreadSampler;

// Well-defined value for an invalid thread sampler.
#define kSbThreadSamplerInvalid ((SbThreadSampler)NULL)

// Returns whether the given thread sampler is valid.
static inline bool SbThreadSamplerIsValid(SbThreadSampler sampler) {
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
// If unsuccessful, this function returns |kSbThreadSamplerInvalid|
SB_EXPORT SbThreadSampler SbThreadSamplerCreate(pthread_t thread);

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
