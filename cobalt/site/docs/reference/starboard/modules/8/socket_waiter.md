---
layout: doc
title: "Starboard Module Reference: socket_waiter.h"
---

Allows a thread to wait on many sockets at once. The standard usage pattern
would be for a single I/O thread to:

1.  Create its own SbSocketWaiter.

1.  Wait on the SbSocketWaiter, indefinitely if no scheduled tasks, or timed if
    there are scheduled future tasks.

1.  While waiting, the SbSocketWaiter will call back to service ready SbSockets.

1.  Wake up, if signaled to do so.

1.  If ready to exit, go to 7.

1.  Add and remove SbSockets to and from the SbSocketWaiter, and go to 2.

1.  Destroy its SbSocketWaiter and exit.

If another thread wants to queue immediate or schedule future work on the I/O
thread, it needs to call SbSocketWaiterWakeUp() on the SbSocketWaiter after
queuing the work item, or the SbSocketWaiter is not otherwise guaranteed to wake
up.

## Macros ##

### kSbSocketWaiterInvalid ###

Well-defined value for an invalid socket watcher handle.

## Enums ##

### SbSocketWaiterInterest ###

All the interests that a socket may register for on a waiter.

#### Values ####

*   `kSbSocketWaiterInterestNone`

    No interests whatsoever.
*   `kSbSocketWaiterInterestRead`

    An interest in or readiness to read from a socket without blocking.
*   `kSbSocketWaiterInterestWrite`

    An interest in or readiness to write to a socket without blocking.

### SbSocketWaiterResult ###

Possible reasons why a call to SbSocketWaiterWaitTimed returned.

#### Values ####

*   `kSbSocketWaiterResultInvalid`

    The wait didn't block because the waiter was invalid.
*   `kSbSocketWaiterResultTimedOut`

    The wait stopped because the timeout expired.
*   `kSbSocketWaiterResultWokenUp`

    The wait stopped because a call to SbSocketWaiterWakeUp was consumed.

## Typedefs ##

### SbSocketWaiter ###

A handle to a socket waiter.

#### Definition ####

```
typedef SbSocketWaiterPrivate* SbSocketWaiter
```

### SbSocketWaiterCallback ###

Function pointer for socket waiter callbacks.

#### Definition ####

```
typedef void(* SbSocketWaiterCallback)(SbSocketWaiter waiter, SbSocket socket, void *context, int ready_interests)
```

## Functions ##

### SbSocketWaiterAdd ###

Adds a new socket to be waited on by the `waiter` with a bitfield of
`interests`. This function should only be called on the thread that waits on
this waiter.

If `socket` is already registered with this or another waiter, the function does
nothing and returns `false`. The client must remove the socket and then add it
back with the new `interests`.

If `socket` is already ready for one or more of the operations set in the
`interests` mask, then the callback will be called on the next call to either
SbSocketWaiterWait() or SbSocketWaiterWaitTimed().

`waiter`: An SbSocketWaiter that waits on the socket for the specified set of
operations (`interests`). `socket`: The SbSocket on which the waiter waits.
`context`: `callback`: The function that is called when the event fires. The
`waiter`, `socket`, `context` are all passed to the callback, along with a
bitfield of `interests` that the socket is actually ready for. `interests`: A
bitfield that identifies operations for which the socket is waiting.
`persistent`: Identifies the procedure that will be followed for removing the
socket:

*   If `persistent` is `true`, then `socket` stays registered with `waiter`
    until SbSocketWaiterRemove() is called with `waiter` and `socket`.

*   If `persistent` is `false`, then `socket` is removed after the next call to
    `callback`, even if not all registered `interests` became ready.

#### Declaration ####

```
bool SbSocketWaiterAdd(SbSocketWaiter waiter, SbSocket socket, void *context, SbSocketWaiterCallback callback, int interests, bool persistent)
```

### SbSocketWaiterCreate ###

The results of two threads waiting on the same waiter is undefined and will not
work. Except for the SbSocketWaiterWakeUp() function, SbSocketWaiters are not
thread-safe and don't expect to be modified concurrently.

#### Declaration ####

```
SbSocketWaiter SbSocketWaiterCreate()
```

### SbSocketWaiterDestroy ###

Destroys `waiter` and removes all sockets still registered by way of
SbSocketWaiterAdd. This function may be called on any thread as long as there is
no risk of concurrent access to the waiter.

`waiter`: The SbSocketWaiter to be destroyed.

#### Declaration ####

```
bool SbSocketWaiterDestroy(SbSocketWaiter waiter)
```

### SbSocketWaiterIsValid ###

Returns whether the given socket handle is valid.

#### Declaration ####

```
static bool SbSocketWaiterIsValid(SbSocketWaiter watcher)
```

### SbSocketWaiterRemove ###

Removes a socket, previously added with SbSocketWaiterAdd(), from a waiter. This
function should only be called on the thread that waits on this waiter.

The return value indicates whether the waiter still waits on the socket. If
`socket` wasn't registered with `waiter`, then the function is a no-op and
returns `true`.

`waiter`: The waiter from which the socket is removed. `socket`: The socket to
remove from the waiter.

#### Declaration ####

```
bool SbSocketWaiterRemove(SbSocketWaiter waiter, SbSocket socket)
```

### SbSocketWaiterWait ###

Waits on all registered sockets, calling the registered callbacks if and when
the corresponding sockets become ready for an interested operation. This version
exits only after SbSocketWaiterWakeUp() is called. This function should only be
called on the thread that waits on this waiter.

#### Declaration ####

```
void SbSocketWaiterWait(SbSocketWaiter waiter)
```

### SbSocketWaiterWaitTimed ###

Behaves similarly to SbSocketWaiterWait(), but this function also causes
`waiter` to exit on its own after at least `duration` has passed if
SbSocketWaiterWakeUp() it not called by that time.

The return value indicates the reason that the socket waiter exited. This
function should only be called on the thread that waits on this waiter.

`duration`: The minimum amount of time after which the socket waiter should exit
if it is not woken up before then. As with SbThreadSleep() (see thread.h), this
function may wait longer than `duration`, such as if the timeout expires while a
callback is being fired.

#### Declaration ####

```
SbSocketWaiterResult SbSocketWaiterWaitTimed(SbSocketWaiter waiter, SbTime duration)
```

### SbSocketWaiterWakeUp ###

Wakes up `waiter` once. This is the only thread-safe waiter function. It can can
be called from a SbSocketWaiterCallback to wake up its own waiter, and it can
also be called from another thread at any time. In either case, the waiter will
exit the next wait gracefully, first completing any in-progress callback.

Each time this function is called, it causes the waiter to wake up once,
regardless of whether the waiter is currently waiting. If the waiter is not
waiting, the function takes effect immediately the next time the waiter waits.
The number of wake-ups accumulates, and the queue is only consumed as the waiter
waits and then is subsequently woken up again. For example, if you call this
function 7 times, then SbSocketWaiterWait() and WaitTimed() will not block the
next 7 times they are called.

`waiter`: The socket waiter to be woken up.

#### Declaration ####

```
void SbSocketWaiterWakeUp(SbSocketWaiter waiter)
```

