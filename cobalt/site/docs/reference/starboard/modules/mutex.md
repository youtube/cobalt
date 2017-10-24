---
layout: doc
title: "Starboard Module Reference: mutex.h"
---

Defines a mutually exclusive lock that can be used to coordinate with other
threads.

## Enums

### SbMutexResult

Enumeration of possible results from acquiring a mutex.

**Values**

*   `kSbMutexAcquired` - The mutex was acquired successfully.
*   `kSbMutexBusy` - The mutex was not acquired because it was held by someone else.
*   `kSbMutexDestroyed` - The mutex has already been destroyed.

## Functions

### SbMutexAcquire

**Description**

Acquires `mutex`, blocking indefinitely. The return value identifies
the acquisition result. SbMutexes are not reentrant, so a recursive
acquisition blocks forever.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMutexAcquire-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMutexAcquire-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMutexAcquire-declaration">
<pre>
SB_EXPORT SbMutexResult SbMutexAcquire(SbMutex* mutex);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMutexAcquire-stub">

```
#include "starboard/mutex.h"

SbMutexResult SbMutexAcquire(SbMutex* /*mutex*/) {
  return kSbMutexDestroyed;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbMutex*</code><br>        <code>mutex</code></td>
    <td>The mutex to be acquired.</td>
  </tr>
</table>

### SbMutexAcquireTry

**Description**

Acquires `mutex`, without blocking. The return value identifies
the acquisition result. SbMutexes are not reentrant, so a recursive
acquisition always fails.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMutexAcquireTry-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMutexAcquireTry-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMutexAcquireTry-declaration">
<pre>
SB_EXPORT SbMutexResult SbMutexAcquireTry(SbMutex* mutex);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMutexAcquireTry-stub">

```
#include "starboard/mutex.h"

SbMutexResult SbMutexAcquireTry(SbMutex* /*mutex*/) {
  return kSbMutexDestroyed;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbMutex*</code><br>        <code>mutex</code></td>
    <td>The mutex to be acquired.</td>
  </tr>
</table>

### SbMutexCreate

**Description**

Creates a new mutex. The return value indicates whether the function
was able to create a new mutex.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMutexCreate-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMutexCreate-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMutexCreate-declaration">
<pre>
SB_EXPORT bool SbMutexCreate(SbMutex* out_mutex);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMutexCreate-stub">

```
#include "starboard/mutex.h"

bool SbMutexCreate(SbMutex* /*mutex*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbMutex*</code><br>        <code>out_mutex</code></td>
    <td>The handle to the newly created mutex.</td>
  </tr>
</table>

### SbMutexDestroy

**Description**

Destroys a mutex. The return value indicates whether the destruction was
successful.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMutexDestroy-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMutexDestroy-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMutexDestroy-declaration">
<pre>
SB_EXPORT bool SbMutexDestroy(SbMutex* mutex);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMutexDestroy-stub">

```
#include "starboard/mutex.h"

bool SbMutexDestroy(SbMutex* /*mutex*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbMutex*</code><br>        <code>mutex</code></td>
    <td>The mutex to be invalidated.</td>
  </tr>
</table>

### SbMutexIsSuccess

**Description**

Indicates whether the given result is a success. A value of `true` indicates
that the mutex was acquired.

**Declaration**

```
static SB_C_FORCE_INLINE bool SbMutexIsSuccess(SbMutexResult result) {
  return result == kSbMutexAcquired;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbMutexResult</code><br>        <code>result</code></td>
    <td>The result being checked.</td>
  </tr>
</table>

### SbMutexRelease

**Description**

Releases `mutex` held by the current thread. The return value indicates
whether the release was successful. Releases should always be successful
if `mutex` is held by the current thread.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMutexRelease-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMutexRelease-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMutexRelease-declaration">
<pre>
SB_EXPORT bool SbMutexRelease(SbMutex* mutex);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMutexRelease-stub">

```
#include "starboard/mutex.h"

bool SbMutexRelease(SbMutex* /*mutex*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbMutex*</code><br>        <code>mutex</code></td>
    <td>The mutex to be released.</td>
  </tr>
</table>

