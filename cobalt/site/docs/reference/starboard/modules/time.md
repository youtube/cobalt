---
layout: doc
title: "Starboard Module Reference: time.h"
---

Provides access to system time and timers.

## Functions

### SbTimeFromPosix

**Description**

Converts microseconds from the POSIX epoch into an `SbTime`.

**Declaration**

```
static SB_C_FORCE_INLINE SbTime SbTimeFromPosix(int64_t time) {
  return time - kSbTimeToPosixDelta;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>int64_t</code><br>        <code>time</code></td>
    <td>A time that measures the number of microseconds since
January 1, 1970, 00:00:00, UTC.</td>
  </tr>
</table>

### SbTimeGetMonotonicNow

**Description**

Gets a monotonically increasing time representing right now.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbTimeGetMonotonicNow-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbTimeGetMonotonicNow-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbTimeGetMonotonicNow-declaration">
<pre>
SB_EXPORT SbTimeMonotonic SbTimeGetMonotonicNow();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbTimeGetMonotonicNow-stub">

```
#include "starboard/time.h"

SbTimeMonotonic SbTimeGetMonotonicNow() {
  return SbTimeMonotonic();
}
```

  </div>
</div>

### SbTimeGetMonotonicThreadNow

**Description**

Gets a monotonically increasing time representing how long the current
thread has been in the executing state (i.e. not pre-empted nor waiting
on an event). This is not necessarily total time and is intended to allow
measuring thread execution time between two timestamps. If this is not
available then <code><a href="#sbtimegetmonotonicnow">SbTimeGetMonotonicNow()</a></code> should be used.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbTimeGetMonotonicThreadNow-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbTimeGetMonotonicThreadNow-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbTimeGetMonotonicThreadNow-declaration">
<pre>
SB_EXPORT SbTimeMonotonic SbTimeGetMonotonicThreadNow();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbTimeGetMonotonicThreadNow-stub">

```
#include "starboard/time.h"

SbTimeMonotonic SbTimeGetMonotonicThreadNow() {
  return SbTimeMonotonic(0);
}
```

  </div>
</div>

### SbTimeGetNow

**Description**

Gets the current system time as an `SbTime`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbTimeGetNow-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbTimeGetNow-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbTimeGetNow-declaration">
<pre>
SB_EXPORT SbTime SbTimeGetNow();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbTimeGetNow-stub">

```
#include "starboard/time.h"

SbTime SbTimeGetNow() {
  return SbTime();
}
```

  </div>
</div>

### SbTimeNarrow

**Description**

Safely narrows a number from a more precise unit to a less precise one. This
function rounds negative values toward negative infinity.

**Declaration**

```
static SB_C_FORCE_INLINE int64_t SbTimeNarrow(int64_t time, int64_t divisor) {
  return time >= 0 ? time / divisor : (time - divisor + 1) / divisor;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>int64_t</code><br>
        <code>time</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>int64_t</code><br>
        <code>divisor</code></td>
    <td> </td>
  </tr>
</table>

### SbTimeToPosix

**Description**

Converts `SbTime` into microseconds from the POSIX epoch.

**Declaration**

```
static SB_C_FORCE_INLINE int64_t SbTimeToPosix(SbTime time) {
  return time + kSbTimeToPosixDelta;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbTime</code><br>        <code>time</code></td>
    <td>A time that is either measured in microseconds since the epoch of
January 1, 1601, UTC, or that measures the number of microseconds
between two times.</td>
  </tr>
</table>

