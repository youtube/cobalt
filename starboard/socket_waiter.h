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

// Module Overview: Starboard Socket Waiter module
//
// Allows a thread to wait on many sockets at once. The standard
// usage pattern would be for a single I/O thread to:
// 1. Create its own SbSocketWaiter.
// 2. Wait on the SbSocketWaiter, indefinitely if no scheduled tasks, or timed
//    if there are scheduled future tasks.
// 3. While waiting, the SbSocketWaiter will call back to service ready
//    SbSockets.
// 4. Wake up, if signaled to do so.
// 5. If ready to exit, go to 7.
// 6. Add and remove SbSockets to and from the SbSocketWaiter, and go to 2.
// 7. Destroy its SbSocketWaiter and exit.
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

// Private structure representing a waiter that can wait for many sockets at
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

// Creates and returns a new SbSocketWaiter for the current thread. A socket
// waiter should have a 1:1 relationship to a thread that wants to wait for
// socket activity. There is no reason to create multiple waiters for a single
// thread because the thread can only block on one waiter at a time.

// The results of two threads waiting on the same waiter is undefined and will
// not work. Except for the SbSocketWaiterWakeUp() function, SbSocketWaiters
// are not thread-safe and don't expect to be modified concurrently.
SB_EXPORT SbSocketWaiter SbSocketWaiterCreate();

// Destroys |waiter| and removes all sockets still registered by way of
// SbSocketWaiterAdd. This function may be called on any thread as long as
// there is no risk of concurrent access to the waiter.
//
// |waiter|: The SbSocketWaiter to be destroyed.
SB_EXPORT bool SbSocketWaiterDestroy(SbSocketWaiter waiter);

// Adds a new socket to be waited on by the |waiter| with a bitfield of
// |interests|. This function should only be called on the thread that
// waits on this waiter.
//
// If |socket| is already registered with this or another waiter, the function
// does nothing and returns |false|. The client must remove the socket and then
// add it back with the new |interests|.
//
// If |socket| is already ready for one or more of the operations set in the
// |interests| mask, then the callback will be called on the next call to
// either SbSocketWaiterWait() or SbSocketWaiterWaitTimed().
//
// |waiter|: An SbSocketWaiter that waits on the socket for the specified set
//   of operations (|interests|).
// |socket|: The SbSocket on which the waiter waits.
// |context|:
// |callback|: The function that is called when the event fires. The |waiter|,
//   |socket|, |context| are all passed to the callback, along with a bitfield
//   of |interests| that the socket is actually ready for.
// |interests|: A bitfield that identifies operations for which the socket is
//   waiting.
// |persistent|: Identifies the procedure that will be followed for removing
//   the socket:
// - If |persistent| is |true|, then |socket| stays registered with |waiter|
//   until SbSocketWaiterRemove() is called with |waiter| and |socket|.
// - If |persistent| is |false|, then |socket| is removed before the next call
//   to |callback|, even if not all registered |interests| became ready,
//   which allows for adding it again in the |callback|.
SB_EXPORT bool SbSocketWaiterAdd(SbSocketWaiter waiter,
                                 SbSocket socket,
                                 void* context,
                                 SbSocketWaiterCallback callback,
                                 int interests,
                                 bool persistent);

// Removes a socket, previously added with SbSocketWaiterAdd(), from a waiter.
// This function should only be called on the thread that waits on this waiter.
//
// The return value indicates whether the waiter still waits on the socket.
// If |socket| wasn't registered with |waiter|, then the function is a no-op
// and returns |true|.
//
// |waiter|: The waiter from which the socket is removed.
// |socket|: The socket to remove from the waiter.
SB_EXPORT bool SbSocketWaiterRemove(SbSocketWaiter waiter, SbSocket socket);

// Waits on all registered sockets, calling the registered callbacks if and when
// the corresponding sockets become ready for an interested operation. This
// version exits only after SbSocketWaiterWakeUp() is called. This function
// should only be called on the thread that waits on this waiter.
SB_EXPORT void SbSocketWaiterWait(SbSocketWaiter waiter);

// Behaves similarly to SbSocketWaiterWait(), but this function also causes
// |waiter| to exit on its own after at least |duration| has passed if
// SbSocketWaiterWakeUp() it not called by that time.
//
// The return value indicates the reason that the socket waiter exited.
// This function should only be called on the thread that waits on this waiter.
//
// |duration|: The minimum amount of time after which the socket waiter should
//   exit if it is not woken up before then. As with SbThreadSleep() (see
//   thread.h), this function may wait longer than |duration|, such as if the
//   timeout expires while a callback is being fired.
SB_EXPORT SbSocketWaiterResult SbSocketWaiterWaitTimed(SbSocketWaiter waiter,
                                                       SbTime duration);

// Wakes up |waiter| once. This is the only thread-safe waiter function.
// It can can be called from a SbSocketWaiterCallback to wake up its own waiter,
// and it can also be called from another thread at any time. In either case,
// the waiter will exit the next wait gracefully, first completing any
// in-progress callback.
//
// Each time this function is called, it causes the waiter to wake up once,
// regardless of whether the waiter is currently waiting. If the waiter is not
// waiting, the function takes effect immediately the next time the waiter
// waits. The number of wake-ups accumulates, and the queue is only consumed
// as the waiter waits and then is subsequently woken up again. For example,
// if you call this function 7 times, then SbSocketWaiterWait() and WaitTimed()
// will not block the next 7 times they are called.
//
// |waiter|: The socket waiter to be woken up.
SB_EXPORT void SbSocketWaiterWakeUp(SbSocketWaiter waiter);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SOCKET_WAITER_H_
