---
layout: doc
title: "Starboard Module Reference: socket_waiter.h"
---

Allows a thread to wait on many sockets at once. The standard
usage pattern would be for a single I/O thread to:
<ol><li>Create its own SbSocketWaiter.
</li><li>Wait on the SbSocketWaiter, indefinitely if no scheduled tasks, or timed
if there are scheduled future tasks.
</li><li>While waiting, the SbSocketWaiter will call back to service ready
SbSockets.
</li><li>Wake up, if signaled to do so.
</li><li>If ready to exit, go to 7.
</li><li>Add and remove SbSockets to and from the SbSocketWaiter, and go to 2.
</li><li>Destroy its SbSocketWaiter and exit.</li></ol>
If another thread wants to queue immediate or schedule future work on the I/O
thread, it needs to call SbSocketWaiterWakeUp() on the SbSocketWaiter after
queuing the work item, or the SbSocketWaiter is not otherwise guaranteed to
wake up.

## Enums

### SbSocketWaiterInterest

All the interests that a socket may register for on a waiter.

**Values**

*   `kSbSocketWaiterInterestNone` - No interests whatsoever.
*   `kSbSocketWaiterInterestRead` - An interest in or readiness to read from a socket without blocking.
*   `kSbSocketWaiterInterestWrite` - An interest in or readiness to write to a socket without blocking.

### SbSocketWaiterResult

Possible reasons why a call to SbSocketWaiterWaitTimed returned.

**Values**

*   `kSbSocketWaiterResultInvalid` - The wait didn't block because the waiter was invalid.
*   `kSbSocketWaiterResultTimedOut` - The wait stopped because the timeout expired.
*   `kSbSocketWaiterResultWokenUp` - The wait stopped because a call to SbSocketWaiterWakeUp was consumed.

## Structs

### SbSocketWaiterPrivate

Private structure representing a waiter that can wait for many sockets at
once on a single thread.

## Functions

### SbSocketWaiterAdd

**Description**

Adds a new socket to be waited on by the `waiter` with a bitfield of
`interests`. This function should only be called on the thread that
waits on this waiter.<br>
If `socket` is already registered with this or another waiter, the function
does nothing and returns `false`. The client must remove the socket and then
add it back with the new `interests`.<br>
If `socket` is already ready for one or more of the operations set in the
`interests` mask, then the callback will be called on the next call to
either <code><a href="#sbsocketwaiterwait">SbSocketWaiterWait()</a></code> or <code><a href="#sbsocketwaiterwait">SbSocketWaiterWait</a></code>Timed().<br></li></ul>

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketWaiterAdd-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketWaiterAdd-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketWaiterAdd-declaration">
<pre>
SB_EXPORT bool SbSocketWaiterAdd(SbSocketWaiter waiter,
                                 SbSocket socket,
                                 void* context,
                                 SbSocketWaiterCallback callback,
                                 int interests,
                                 bool persistent);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketWaiterAdd-stub">

```
#include "starboard/socket_waiter.h"

bool SbSocketWaiterAdd(SbSocketWaiter /*waiter*/,
                       SbSocket /*socket*/,
                       void* /*context*/,
                       SbSocketWaiterCallback /*callback*/,
                       int /*interests*/,
                       bool /*persistent*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocketWaiter</code><br>        <code>waiter</code></td>
    <td>An SbSocketWaiter that waits on the socket for the specified set
of operations (<code>interests</code>).</td>
  </tr>
  <tr>
    <td><code>SbSocket</code><br>        <code>socket</code></td>
    <td>The SbSocket on which the waiter waits.</td>
  </tr>
  <tr>
    <td><code>void*</code><br>        <code>context</code></td>
    <td></td>
  </tr>
  <tr>
    <td><code>SbSocketWaiterCallback</code><br>        <code>callback</code></td>
    <td>The function that is called when the event fires. The <code>waiter</code>,
<code>socket</code>, <code>context</code> are all passed to the callback, along with a bitfield
of <code>interests</code> that the socket is actually ready for.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>interests</code></td>
    <td>A bitfield that identifies operations for which the socket is
waiting.</td>
  </tr>
  <tr>
    <td><code>bool</code><br>        <code>persistent</code></td>
    <td>Identifies the procedure that will be followed for removing
the socket:
<ul><li>If <code>persistent</code> is <code>true</code>, then <code>socket</code> stays registered with <code>waiter</code>
until <code><a href="#sbsocketwaiterremove">SbSocketWaiterRemove()</a></code> is called with <code>waiter</code> and <code>socket</code>.
</li><li>If <code>persistent</code> is <code>false</code>, then <code>socket</code> is removed after the next call
to <code>callback</code>, even if not all registered <code>interests</code> became ready.</td>
  </tr>
</table>

### SbSocketWaiterCreate

**Description**

The results of two threads waiting on the same waiter is undefined and will
not work. Except for the <code><a href="#sbsocketwaiterwakeup">SbSocketWaiterWakeUp()</a></code> function, SbSocketWaiters
are not thread-safe and don't expect to be modified concurrently.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketWaiterCreate-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketWaiterCreate-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketWaiterCreate-declaration">
<pre>
SB_EXPORT SbSocketWaiter SbSocketWaiterCreate();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketWaiterCreate-stub">

```
#include "starboard/socket_waiter.h"

SbSocketWaiter SbSocketWaiterCreate() {
  return kSbSocketWaiterInvalid;
}
```

  </div>
</div>

### SbSocketWaiterDestroy

**Description**

Destroys `waiter` and removes all sockets still registered by way of
SbSocketWaiterAdd. This function may be called on any thread as long as
there is no risk of concurrent access to the waiter.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketWaiterDestroy-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketWaiterDestroy-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketWaiterDestroy-declaration">
<pre>
SB_EXPORT bool SbSocketWaiterDestroy(SbSocketWaiter waiter);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketWaiterDestroy-stub">

```
#include "starboard/socket_waiter.h"

bool SbSocketWaiterDestroy(SbSocketWaiter /*waiter*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocketWaiter</code><br>        <code>waiter</code></td>
    <td>The SbSocketWaiter to be destroyed.</td>
  </tr>
</table>

### SbSocketWaiterIsValid

**Description**

Returns whether the given socket handle is valid.

**Declaration**

```
static SB_C_INLINE bool SbSocketWaiterIsValid(SbSocketWaiter watcher) {
  return watcher != kSbSocketWaiterInvalid;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocketWaiter</code><br>
        <code>watcher</code></td>
    <td> </td>
  </tr>
</table>

### SbSocketWaiterRemove

**Description**

Removes a socket, previously added with <code><a href="#sbsocketwaiteradd">SbSocketWaiterAdd()</a></code>, from a waiter.
This function should only be called on the thread that waits on this waiter.<br>
The return value indicates whether the waiter still waits on the socket.
If `socket` wasn't registered with `waiter`, then the function is a no-op
and returns `true`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketWaiterRemove-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketWaiterRemove-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketWaiterRemove-declaration">
<pre>
SB_EXPORT bool SbSocketWaiterRemove(SbSocketWaiter waiter, SbSocket socket);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketWaiterRemove-stub">

```
#include "starboard/socket_waiter.h"

bool SbSocketWaiterRemove(SbSocketWaiter /*waiter*/, SbSocket /*socket*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocketWaiter</code><br>        <code>waiter</code></td>
    <td>The waiter from which the socket is removed.</td>
  </tr>
  <tr>
    <td><code>SbSocket</code><br>        <code>socket</code></td>
    <td>The socket to remove from the waiter.</td>
  </tr>
</table>

### SbSocketWaiterWait

**Description**

Waits on all registered sockets, calling the registered callbacks if and when
the corresponding sockets become ready for an interested operation. This
version exits only after <code><a href="#sbsocketwaiterwakeup">SbSocketWaiterWakeUp()</a></code> is called. This function
should only be called on the thread that waits on this waiter.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketWaiterWait-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketWaiterWait-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketWaiterWait-declaration">
<pre>
SB_EXPORT void SbSocketWaiterWait(SbSocketWaiter waiter);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketWaiterWait-stub">

```
#include "starboard/socket_waiter.h"

void SbSocketWaiterWait(SbSocketWaiter /*waiter*/) {
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocketWaiter</code><br>
        <code>waiter</code></td>
    <td> </td>
  </tr>
</table>

### SbSocketWaiterWaitTimed

**Description**

Behaves similarly to <code><a href="#sbsocketwaiterwait">SbSocketWaiterWait()</a></code>, but this function also causes
`waiter` to exit on its own after at least `duration` has passed if
SbSocketWaiterWakeUp() it not called by that time.<br>
The return value indicates the reason that the socket waiter exited.
This function should only be called on the thread that waits on this waiter.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketWaiterWaitTimed-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketWaiterWaitTimed-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketWaiterWaitTimed-declaration">
<pre>
SB_EXPORT SbSocketWaiterResult SbSocketWaiterWaitTimed(SbSocketWaiter waiter,
                                                       SbTime duration);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketWaiterWaitTimed-stub">

```
#include "starboard/socket_waiter.h"

SbSocketWaiterResult SbSocketWaiterWaitTimed(SbSocketWaiter /*waiter*/,
                                             SbTime /*duration*/) {
  return kSbSocketWaiterResultInvalid;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocketWaiter</code><br>        <code>waiter</code></td>
    <td> </td>  </tr>
  <tr>
    <td><code>SbTime</code><br>        <code>duration</code></td>
    <td>The minimum amount of time after which the socket waiter should
exit if it is not woken up before then. As with SbThreadSleep() (see
thread.h), this function may wait longer than <code>duration</code>, such as if the
timeout expires while a callback is being fired.</td>
  </tr>
</table>

### SbSocketWaiterWakeUp

**Description**

Wakes up `waiter` once. This is the only thread-safe waiter function.
It can can be called from a SbSocketWaiterCallback to wake up its own waiter,
and it can also be called from another thread at any time. In either case,
the waiter will exit the next wait gracefully, first completing any
in-progress callback.<br>
Each time this function is called, it causes the waiter to wake up once,
regardless of whether the waiter is currently waiting. If the waiter is not
waiting, the function takes effect immediately the next time the waiter
waits. The number of wake-ups accumulates, and the queue is only consumed
as the waiter waits and then is subsequently woken up again. For example,
if you call this function 7 times, then <code><a href="#sbsocketwaiterwait">SbSocketWaiterWait()</a></code> and WaitTimed()
will not block the next 7 times they are called.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketWaiterWakeUp-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketWaiterWakeUp-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketWaiterWakeUp-declaration">
<pre>
SB_EXPORT void SbSocketWaiterWakeUp(SbSocketWaiter waiter);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketWaiterWakeUp-stub">

```
#include "starboard/socket_waiter.h"

void SbSocketWaiterWakeUp(SbSocketWaiter /*waiter*/) {
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocketWaiter</code><br>        <code>waiter</code></td>
    <td>The socket waiter to be woken up.</td>
  </tr>
</table>

