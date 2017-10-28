---
layout: doc
title: "Starboard Module Reference: condition_variable.h"
---

Defines an interface for condition variables.

## Enums

### SbConditionVariableResult

Enumeration of possible results from waiting on a condvar.

**Values**

*   `kSbConditionVariableSignaled` - The wait completed because the condition variable was signaled.
*   `kSbConditionVariableTimedOut` - The wait completed because it timed out, and was not signaled.
*   `kSbConditionVariableFailed` - The wait failed, either because a parameter wasn't valid, or the conditionvariable has already been destroyed, or something similar.

## Functions

### SbConditionVariableBroadcast

**Description**

Broadcasts to all current waiters of `condition` to stop waiting. This
function wakes all of the threads waiting on `condition` while
SbConditionVariableSignal wakes a single thread.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbConditionVariableBroadcast-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbConditionVariableBroadcast-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbConditionVariableBroadcast-declaration">
<pre>
SB_EXPORT bool SbConditionVariableBroadcast(SbConditionVariable* condition);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbConditionVariableBroadcast-stub">

```
#include "starboard/condition_variable.h"

bool SbConditionVariableBroadcast(SbConditionVariable* /*condition*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbConditionVariable*</code><br>        <code>condition</code></td>
    <td>The condition that should no longer be waited for.</td>
  </tr>
</table>

### SbConditionVariableCreate

**Description**

Creates a new condition variable to work with `opt_mutex`, which may be null,
placing the newly created condition variable in `out_condition`.<br>
The return value indicates whether the condition variable could be created.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbConditionVariableCreate-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbConditionVariableCreate-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbConditionVariableCreate-declaration">
<pre>
SB_EXPORT bool SbConditionVariableCreate(SbConditionVariable* out_condition,
                                         SbMutex* opt_mutex);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbConditionVariableCreate-stub">

```
#include "starboard/condition_variable.h"

bool SbConditionVariableCreate(SbConditionVariable* /*out_condition*/,
                               SbMutex* /*opt_mutex*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbConditionVariable*</code><br>
        <code>out_condition</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbMutex*</code><br>
        <code>opt_mutex</code></td>
    <td> </td>
  </tr>
</table>

### SbConditionVariableDestroy

**Description**

Destroys the specified SbConditionVariable. The return value indicates
whether the destruction was successful. The behavior is undefined if other
threads are currently waiting on this condition variable.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbConditionVariableDestroy-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbConditionVariableDestroy-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbConditionVariableDestroy-declaration">
<pre>
SB_EXPORT bool SbConditionVariableDestroy(SbConditionVariable* condition);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbConditionVariableDestroy-stub">

```
#include "starboard/condition_variable.h"

bool SbConditionVariableDestroy(SbConditionVariable* /*condition*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbConditionVariable*</code><br>        <code>condition</code></td>
    <td>The SbConditionVariable to be destroyed. This invalidates the
condition variable.</td>
  </tr>
</table>

### SbConditionVariableIsSignaled

**Description**

Returns whether the given result is a success.

**Declaration**

```
static SB_C_INLINE bool SbConditionVariableIsSignaled(
    SbConditionVariableResult result) {
  return result == kSbConditionVariableSignaled;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbConditionVariableResult</code><br>
        <code>result</code></td>
    <td> </td>
  </tr>
</table>

### SbConditionVariableSignal

**Description**

Signals the next waiter of `condition` to stop waiting. This function wakes
a single thread waiting on `condition` while <code><a href="#sbconditionvariablebroadcast">SbConditionVariableBroadcast</a></code>
wakes all threads waiting on it.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbConditionVariableSignal-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbConditionVariableSignal-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbConditionVariableSignal-declaration">
<pre>
SB_EXPORT bool SbConditionVariableSignal(SbConditionVariable* condition);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbConditionVariableSignal-stub">

```
#include "starboard/condition_variable.h"

bool SbConditionVariableSignal(SbConditionVariable* /*condition*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbConditionVariable*</code><br>        <code>condition</code></td>
    <td>The condition that the waiter should stop waiting for.</td>
  </tr>
</table>

### SbConditionVariableWait

**Description**

Waits for `condition`, releasing the held lock `mutex`, blocking
indefinitely, and returning the result. Behavior is undefined if `mutex` is
not held.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbConditionVariableWait-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbConditionVariableWait-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbConditionVariableWait-declaration">
<pre>
SB_EXPORT SbConditionVariableResult
SbConditionVariableWait(SbConditionVariable* condition, SbMutex* mutex);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbConditionVariableWait-stub">

```
#include "starboard/condition_variable.h"

SbConditionVariableResult SbConditionVariableWait(
    SbConditionVariable* /*condition*/,
    SbMutex* /*mutex*/) {
  return kSbConditionVariableFailed;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbConditionVariable*</code><br>
        <code>condition</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbMutex*</code><br>
        <code>mutex</code></td>
    <td> </td>
  </tr>
</table>

### SbConditionVariableWaitTimed

**Description**

Waits for `condition`, releasing the held lock `mutex`, blocking up to
`timeout_duration`, and returning the acquisition result. Behavior is
undefined if `mutex` is not held.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbConditionVariableWaitTimed-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbConditionVariableWaitTimed-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbConditionVariableWaitTimed-declaration">
<pre>
SB_EXPORT SbConditionVariableResult
SbConditionVariableWaitTimed(SbConditionVariable* condition,
                             SbMutex* mutex,
                             SbTime timeout_duration);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbConditionVariableWaitTimed-stub">

```
#include "starboard/condition_variable.h"

SbConditionVariableResult SbConditionVariableWaitTimed(
    SbConditionVariable* /*condition*/,
    SbMutex* /*mutex*/,
    SbTime /*timeout*/) {
  return kSbConditionVariableFailed;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbConditionVariable*</code><br>        <code>condition</code></td>
    <td> </td>  </tr>
  <tr>
    <td><code>SbMutex*</code><br>        <code>mutex</code></td>
    <td> </td>  </tr>
  <tr>
    <td><code>SbTime</code><br>        <code>timeout_duration</code></td>
    <td>The maximum amount of time that function should wait
for <code>condition</code>. If the <code>timeout_duration</code> value is less than or equal to
zero, the function returns as quickly as possible with a
kSbConditionVariableTimedOut result.</td>
  </tr>
</table>

