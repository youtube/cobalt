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

// Allows a thread to wait on many sockets at once. The standard usage pattern
// would be for a single I/O thread to:
//   1. Create its own SbSocketWaiter.
//   2. Wait on the SbSocketWaiter, indefinitely if no scheduled tasks, or timed
//      if there are scheduled future tasks.
//   3. While waiting, the SbSocketWaiter will call back to service ready
//      SbSockets.
//   4. Wake up, if signaled to do so.
//   5. If ready to exit, go to 7.
//   6. Add and remove SbSockets to and from the SbSocketWaiter, and go to 2.
//   7. Destroy its SbSocketWaiter and exit.
//
// If another thread wants to queue immediate or schedule future work on the I/O
// thread, it needs to call SbSocketWaiterWakeUp() on the SbSocketWaiter after
// queuing the work item, or the SbSocketWaiter is not otherwise guaranteed to
// wake up.

#ifndef STARBOARD_SOCKET_WAITER_H_
#define STARBOARD_SOCKET_WAITER_H_

#include "starboard/export.h"
#include "starboard/socket.h"
#include "starboard/time.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Private structure representing a waiter which can wait for many sockets at
// once on a single thread.
typedef struct SbSocketWaiterPrivate SbSocketWaiterPrivate;

// A handle to a socket waiter.
typedef SbSocketWaiterPrivate* SbSocketWaiter;

// All the interests that a socket may register for on a waiter.
typedef enum SbSocketWaiterInterest {
  // No interests whatsoever.
  kSbSocketWaiterInterestNone = 0,

  // An interest in or readiness to read from a socket without blocking.
  kSbSocketWaiterInterestRead = 1 << 0,

  // An interest in or readiness to write to a socket without blocking.
  kSbSocketWaiterInterestWrite = 1 << 1,
} SbSocketWaiterInterest;

// Possible reasons why a call to SbSocketWaiterWaitTimed returned.
typedef enum SbSocketWaiterResult {
  // The wait didn't block because the waiter was invalid.
  kSbSocketWaiterResultInvalid,

  // The wait stopped because the timeout expired.
  kSbSocketWaiterResultTimedOut,

  // The wait stopped because a call to SbSocketWaiterWakeUp was consumed.
  kSbSocketWaiterResultWokenUp,
} SbSocketWaiterResult;

// Function pointer for socket waiter callbacks.
typedef void (*SbSocketWaiterCallback)(SbSocketWaiter waiter,
                                       SbSocket socket,
                                       void* context,
                                       int ready_interests);

// Well-defined value for an invalid socket watcher handle.
#define kSbSocketWaiterInvalid ((SbSocketWaiter)NULL)

// Returns whether the given socket handle is valid.
static SB_C_INLINE bool SbSocketWaiterIsValid(SbSocketWaiter watcher) {
  return watcher != kSbSocketWaiterInvalid;
}

// Creates a new socket waiter for the current thread. A socket waiter should
// have a 1-1 relationship to a thread that wants to wait for socket
// activity. There is no reason to create multiple waiters for a single thread,
// because the thread can only block on one waiter at a time. The results of two
// threads waiting on the same waiter is undefined (and definitely won't work,
// so don't do it). SbSocketWaiters are not thread-safe, except for the
// SbSocketWaiterWakeUp() function, and don't expect to be modified
// concurrently.
SB_EXPORT SbSocketWaiter SbSocketWaiterCreate();

// Destroys a socket waiter. This will also remove all sockets still registered
// by way of SbSocketWaiterAdd. This function may be called on any thread, but
// only if there is no worry of concurrent access to the waiter.
SB_EXPORT bool SbSocketWaiterDestroy(SbSocketWaiter waiter);

// Adds a new socket to be waited on by the |waiter| with a bitfield of
// |interests|. When the event fires, |callback| will be called with |waiter|,
// |socket|, and |context|, as well as a bitfield of which interests the socket
// is actually ready for. This should only be called on the thread that
// waits on this waiter.
//
// If |socket| is already registered with this or another waiter, the function
// will do nothing and return false. The client must remove the socket and then
// add it back with the new |interests|.
//
// If |socket| is already ready for one or more operations set in the
// |interests| mask, then the callback will be called on the next call to
// SbSocketWaiterWait() or SbSocketWaiterWaitTimed().
//
// If |persistent| is true, then |socket| will stay registered with |waiter|
// until SbSocketWaiterRemove() is called with |waiter| and |socket|. Otherwise,
// |socket| will be removed after the next call to |callback|, even if not all
// registered |interests| became ready.
SB_EXPORT bool SbSocketWaiterAdd(SbSocketWaiter waiter,
                                 SbSocket socket,
                                 void* context,
                                 SbSocketWaiterCallback callback,
                                 int interests,
                                 bool persistent);

// Removes |socket| from |waiter|, previously added with SbSocketWaiterAdd. If
// |socket| wasn't registered with |waiter|, then this does nothing. This should
// only be called on the thread that waits on this waiter.
SB_EXPORT bool SbSocketWaiterRemove(SbSocketWaiter waiter, SbSocket socket);

// Waits on all registered sockets, calling the registered callbacks if and when
// the corresponding sockets become ready for an interested operation. This
// version exits only after SbSocketWaiterWakeUp() is called. This should only
// be called on the thread that waits on this waiter.
SB_EXPORT void SbSocketWaiterWait(SbSocketWaiter waiter);

// Behaves the same as SbSocketWaiterWait(), waking up when
// SbSocketWaiterWakeUp(), but also exits on its own after at least |duration|
// has passed, whichever comes first, returning an indicator of which one
// happened. Note that, as with SbThreadSleep(), it may wait longer than
// |duration|. For example if the timeout expires while a callback is being
// fired. This should only be called on the thread that waits on this waiter.
SB_EXPORT SbSocketWaiterResult SbSocketWaiterWaitTimed(SbSocketWaiter waiter,
                                                       SbTime duration);

// Wakes up |waiter| once. Can be called from a SbSocketWaiterCallback to wake
// up its own waiter. Can also be called from another thread at any time, it's
// the only thread-safe waiter function. In either case, the waiter will exit
// the next wait gracefully, first completing any in-progress callback.
//
// For every time this function is called, it will cause the waiter to wake up
// one time. This is true whether the waiter is currently waiting or not. If not
// waiting, it will just take affect immediately the next time the waiter
// waits. The number of wake-ups accumulate and are only consumed by waiting and
// getting woken up. e.g. If you call this function 7 times, then
// SbSocketWaiterWait() and WaitTimed() will not block the next 7 times they are
// called.
SB_EXPORT void SbSocketWaiterWakeUp(SbSocketWaiter waiter);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SOCKET_WAITER_H_
