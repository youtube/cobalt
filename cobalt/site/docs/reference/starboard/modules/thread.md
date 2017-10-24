---
layout: doc
title: "Starboard Module Reference: thread.h"
---

Defines functionality related to thread creation and cleanup.

## Enums

### SbThreadPriority

A spectrum of thread priorities. Platforms map them appropriately to their
own priority system. Note that scheduling is platform-specific, and what
these priorities mean, if they mean anything at all, is also
platform-specific.<br>
In particular, several of these priority values can map to the same priority
on a given platform. The only guarantee is that each lower priority should be
treated less-than-or-equal-to a higher priority.

**Values**

*   `kSbThreadPriorityLowest` - The lowest thread priority available on the current platform.
*   `kSbThreadPriorityLow` - A lower-than-normal thread priority, if available on the current platform.
*   `kSbThreadPriorityNormal` - Really, what is normal? You should spend time pondering that question morethan you consider less-important things, but less than you think aboutmore-important things.
*   `kSbThreadPriorityHigh` - A higher-than-normal thread priority, if available on the current platform.
*   `kSbThreadPriorityHighest` - The highest thread priority available on the current platform that isn'tconsidered "real-time" or "time-critical," if those terms have any meaningon the current platform.
*   `kSbThreadPriorityRealTime` - If the platform provides any kind of real-time or time-critical scheduling,this priority will request that treatment. Real-time scheduling generallymeans that the thread will have more consistency in scheduling thannon-real-time scheduled threads, often by being more deterministic in howthreads run in relation to each other. But exactly how being real-timeaffects the thread scheduling is platform-specific.<br>For platforms where that is not offered, or otherwise not meaningful, thiswill just be the highest priority available in the platform's scheme, whichmay be the same as kSbThreadPriorityHighest.
*   `kSbThreadNoPriority` - Well-defined constant value to mean "no priority."  This means to use thedefault priority assignment method of that platform. This may mean toinherit the priority of the spawning thread, or it may mean a specificdefault priority, or it may mean something else, depending on the platform.

## Structs

### SbThreadLocalKeyPrivate

Private structure representing a thread-local key.

## Functions

### SbThreadCreate

**Description**

Creates a new thread, which starts immediately.
<ul><li>If the function succeeds, the return value is a handle to the newly
created thread.
</li><li>If the function fails, the return value is `kSbThreadInvalid`.</li></ul>

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbThreadCreate-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbThreadCreate-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbThreadCreate-declaration">
<pre>
SB_EXPORT SbThread SbThreadCreate(int64_t stack_size,
                                  SbThreadPriority priority,
                                  SbThreadAffinity affinity,
                                  bool joinable,
                                  const char* name,
                                  SbThreadEntryPoint entry_point,
                                  void* context);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbThreadCreate-stub">

```
#include "starboard/thread.h"

SbThread SbThreadCreate(int64_t /*stack_size*/,
                        SbThreadPriority /*priority*/,
                        SbThreadAffinity /*affinity*/,
                        bool /*joinable*/,
                        const char* /*name*/,
                        SbThreadEntryPoint /*entry_point*/,
                        void* /*context*/) {
  return kSbThreadInvalid;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>int64_t</code><br>        <code>stack_size</code></td>
    <td>The amount of memory reserved for the thread. Set the value
to <code>0</code> to indicate that the default stack size should be used.</td>
  </tr>
  <tr>
    <td><code>SbThreadPriority</code><br>        <code>priority</code></td>
    <td>The thread's priority. This value can be set to
<code>kSbThreadNoPriority</code> to use the platform's default priority. As examples,
it could be set to a fixed, standard priority or to a priority inherited
from the thread that is calling <code>SbThreadCreate()</code>, or to something else.</td>
  </tr>
  <tr>
    <td><code>SbThreadAffinity</code><br>        <code>affinity</code></td>
    <td>The thread's affinity. This value can be set to
<code>kSbThreadNoAffinity</code> to use the platform's default affinity.</td>
  </tr>
  <tr>
    <td><code>bool</code><br>        <code>joinable</code></td>
    <td>Indicates whether the thread can be joined (<code>true</code>) or should
start out "detached" (<code>false</code>). Note that for joinable threads, when
you are done with the thread handle, you must call <code>SbThreadJoin</code> to
release system resources associated with the thread. This is not necessary
for detached threads, but detached threads cannot be joined.</td>
  </tr>
  <tr>
    <td><code>const char*</code><br>        <code>name</code></td>
    <td>A name used to identify the thread. This value is used mainly for
debugging, it can be <code>NULL</code>, and it might not be used in production builds.</td>
  </tr>
  <tr>
    <td><code>SbThreadEntryPoint</code><br>        <code>entry_point</code></td>
    <td>A pointer to a function that will be executed on the newly
created thread.</td>
  </tr>
  <tr>
    <td><code>void*</code><br>        <code>context</code></td>
    <td>This value will be passed to the <code>entry_point</code> function.</td>
  </tr>
</table>

### SbThreadCreateLocalKey

**Description**

Creates and returns a new, unique key for thread local data. If the function
does not succeed, the function returns `kSbThreadLocalKeyInvalid`.<br>
If `destructor` is specified, it will be called in the owning thread, and
only in the owning thread, when the thread exits. In that case, it is called
on the local value associated with the key in the current thread as long as
the local value is not NULL.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbThreadCreateLocalKey-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbThreadCreateLocalKey-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbThreadCreateLocalKey-declaration">
<pre>
SB_EXPORT SbThreadLocalKey
SbThreadCreateLocalKey(SbThreadLocalDestructor destructor);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbThreadCreateLocalKey-stub">

```
#include "starboard/thread.h"

SbThreadLocalKey SbThreadCreateLocalKey(
    SbThreadLocalDestructor /*destructor*/) {
  return kSbThreadLocalKeyInvalid;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbThreadLocalDestructor</code><br>        <code>destructor</code></td>
    <td>A pointer to a function. The value may be NULL if no clean up
is needed.</td>
  </tr>
</table>

### SbThreadDestroyLocalKey

**Description**

Destroys thread local data for the specified key. The function is a no-op
if the key is invalid (kSbThreadLocalKeyInvalid`) or has already been
destroyed. This function does NOT call the destructor on any stored values.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbThreadDestroyLocalKey-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbThreadDestroyLocalKey-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbThreadDestroyLocalKey-declaration">
<pre>
SB_EXPORT void SbThreadDestroyLocalKey(SbThreadLocalKey key);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbThreadDestroyLocalKey-stub">

```
#include "starboard/thread.h"

void SbThreadDestroyLocalKey(SbThreadLocalKey /*key*/) {
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbThreadLocalKey</code><br>        <code>key</code></td>
    <td>The key for which to destroy thread local data.</td>
  </tr>
</table>

### SbThreadDetach

**Description**

Detaches `thread`, which prevents it from being joined. This is sort of like
a non-blocking join. This function is a no-op if the thread is already
detached or if the thread is already being joined by another thread.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbThreadDetach-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbThreadDetach-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbThreadDetach-declaration">
<pre>
SB_EXPORT void SbThreadDetach(SbThread thread);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbThreadDetach-stub">

```
#include "starboard/thread.h"

void SbThreadDetach(SbThread /*thread*/) {
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbThread</code><br>        <code>thread</code></td>
    <td>The thread to be detached.</td>
  </tr>
</table>

### SbThreadGetCurrent

**Description**

Returns the handle of the currently executing thread.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbThreadGetCurrent-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbThreadGetCurrent-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbThreadGetCurrent-declaration">
<pre>
SB_EXPORT SbThread SbThreadGetCurrent();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbThreadGetCurrent-stub">

```
#include "starboard/thread.h"

SbThread SbThreadGetCurrent() {
  return kSbThreadInvalid;
}
```

  </div>
</div>

### SbThreadGetId

**Description**

Returns the Thread ID of the currently executing thread.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbThreadGetId-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbThreadGetId-stub" class="mdl-tabs__tab">stub</a>
    <a href="#SbThreadGetId-linux" class="mdl-tabs__tab">linux</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbThreadGetId-declaration">
<pre>
SB_EXPORT SbThreadId SbThreadGetId();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbThreadGetId-stub">

```
#include "starboard/thread.h"

SbThreadId SbThreadGetId() {
  return 0;
}
```

  </div>
  <div class="mdl-tabs__panel" id="SbThreadGetId-linux">

```
#include "starboard/thread.h"

#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

SbThreadId SbThreadGetId() {
  // This is not portable outside of Linux.
  return static_cast<SbThreadId>(syscall(SYS_gettid));
}
```

  </div>
</div>

### SbThreadGetLocalValue

**Description**

Returns the pointer-sized value for `key` in the currently executing thread's
local storage. Returns `NULL` if key is `kSbThreadLocalKeyInvalid` or if the
key has already been destroyed.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbThreadGetLocalValue-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbThreadGetLocalValue-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbThreadGetLocalValue-declaration">
<pre>
SB_EXPORT void* SbThreadGetLocalValue(SbThreadLocalKey key);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbThreadGetLocalValue-stub">

```
#include "starboard/thread.h"

void* SbThreadGetLocalValue(SbThreadLocalKey /*key*/) {
  return NULL;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbThreadLocalKey</code><br>        <code>key</code></td>
    <td>The key for which to return the value.</td>
  </tr>
</table>

### SbThreadGetName

**Description**

Returns the debug name of the currently executing thread.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbThreadGetName-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbThreadGetName-stub" class="mdl-tabs__tab">stub</a>
    <a href="#SbThreadGetName-linux" class="mdl-tabs__tab">linux</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbThreadGetName-declaration">
<pre>
SB_EXPORT void SbThreadGetName(char* buffer, int buffer_size);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbThreadGetName-stub">

```
#include "starboard/thread.h"

void SbThreadGetName(char* /*buffer*/, int /*buffer_size*/) {
}
```

  </div>
  <div class="mdl-tabs__panel" id="SbThreadGetName-linux">

```
#include "starboard/thread.h"

#include <pthread.h>

void SbThreadGetName(char* buffer, int buffer_size) {
  pthread_getname_np(pthread_self(), buffer, static_cast<size_t>(buffer_size));
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>char*</code><br>
        <code>buffer</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>int</code><br>
        <code>buffer_size</code></td>
    <td> </td>
  </tr>
</table>

### SbThreadIsCurrent

**Description**

Returns whether `thread` is the current thread.

**Declaration**

```
static SB_C_INLINE bool SbThreadIsCurrent(SbThread thread) {
  return SbThreadGetCurrent() == thread;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbThread</code><br>        <code>thread</code></td>
    <td>The thread to check.</td>
  </tr>
</table>

### SbThreadIsEqual

**Description**

Indicates whether `thread1` and `thread2` refer to the same thread.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbThreadIsEqual-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbThreadIsEqual-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbThreadIsEqual-declaration">
<pre>
SB_EXPORT bool SbThreadIsEqual(SbThread thread1, SbThread thread2);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbThreadIsEqual-stub">

```
#include "starboard/thread.h"

bool SbThreadIsEqual(SbThread /*thread1*/, SbThread /*thread2*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbThread</code><br>        <code>thread1</code></td>
    <td>The first thread to compare.</td>
  </tr>
  <tr>
    <td><code>SbThread</code><br>        <code>thread2</code></td>
    <td>The second thread to compare.</td>
  </tr>
</table>

### SbThreadIsValid

**Description**

Returns whether the given thread handle is valid.

**Declaration**

```
static SB_C_INLINE bool SbThreadIsValid(SbThread thread) {
  return thread != kSbThreadInvalid;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbThread</code><br>
        <code>thread</code></td>
    <td> </td>
  </tr>
</table>

### SbThreadIsValidAffinity

**Description**

Returns whether the given thread affinity is valid.

**Declaration**

```
static SB_C_INLINE bool SbThreadIsValidAffinity(SbThreadAffinity affinity) {
  return affinity != kSbThreadNoAffinity;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbThreadAffinity</code><br>
        <code>affinity</code></td>
    <td> </td>
  </tr>
</table>

### SbThreadIsValidId

**Description**

Returns whether the given thread ID is valid.

**Declaration**

```
static SB_C_INLINE bool SbThreadIsValidId(SbThreadId id) {
  return id != kSbThreadInvalidId;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbThreadId</code><br>
        <code>id</code></td>
    <td> </td>
  </tr>
</table>

### SbThreadIsValidLocalKey

**Description**

Returns whether the given thread local variable key is valid.

**Declaration**

```
static SB_C_INLINE bool SbThreadIsValidLocalKey(SbThreadLocalKey key) {
  return key != kSbThreadLocalKeyInvalid;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbThreadLocalKey</code><br>
        <code>key</code></td>
    <td> </td>
  </tr>
</table>

### SbThreadIsValidPriority

**Description**

Returns whether the given thread priority is valid.

**Declaration**

```
static SB_C_INLINE bool SbThreadIsValidPriority(SbThreadPriority priority) {
  return priority != kSbThreadNoPriority;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbThreadPriority</code><br>
        <code>priority</code></td>
    <td> </td>
  </tr>
</table>

### SbThreadJoin

**Description**

Joins the thread on which this function is called with joinable `thread`.
This function blocks the caller until the designated thread exits, and then
cleans up that thread's resources. The cleanup process essentially detaches
thread.<br>
The return value is `true` if the function is successful and `false` if
`thread` is invalid or detached.<br>
Each joinable thread can only be joined once and must be joined to be fully
cleaned up. Once <code>SbThreadJoin</code> is called, the thread behaves as if it were
detached to all threads other than the joining thread.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbThreadJoin-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbThreadJoin-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbThreadJoin-declaration">
<pre>
SB_EXPORT bool SbThreadJoin(SbThread thread, void** out_return);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbThreadJoin-stub">

```
#include "starboard/thread.h"

bool SbThreadJoin(SbThread /*thread*/, void** /*out_return*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbThread</code><br>        <code>thread</code></td>
    <td>The thread to which the current thread will be joined. The
<code>thread</code> must have been created with <code><a href="#sbthreadcreate">SbThreadCreate</a></code>.</td>
  </tr>
  <tr>
    <td><code>void**</code><br>        <code>out_return</code></td>
    <td>If this is not <code>NULL</code>, then the <code>SbThreadJoin</code> function populates
it with the return value of the thread's <code>main</code> function.</td>
  </tr>
</table>

### SbThreadSetLocalValue

**Description**

Sets the pointer-sized value for `key` in the currently executing thread's
local storage. The return value indicates whether `key` is valid and has
not already been destroyed.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbThreadSetLocalValue-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbThreadSetLocalValue-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbThreadSetLocalValue-declaration">
<pre>
SB_EXPORT bool SbThreadSetLocalValue(SbThreadLocalKey key, void* value);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbThreadSetLocalValue-stub">

```
#include "starboard/thread.h"

bool SbThreadSetLocalValue(SbThreadLocalKey /*key*/, void* /*value*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbThreadLocalKey</code><br>        <code>key</code></td>
    <td>The key for which to set the key value.</td>
  </tr>
  <tr>
    <td><code>void*</code><br>        <code>value</code></td>
    <td>The new pointer-sized key value.</td>
  </tr>
</table>

### SbThreadSetName

**Description**

Sets the debug name of the currently executing thread by copying the
specified name string.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbThreadSetName-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbThreadSetName-stub" class="mdl-tabs__tab">stub</a>
    <a href="#SbThreadSetName-linux" class="mdl-tabs__tab">linux</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbThreadSetName-declaration">
<pre>
SB_EXPORT void SbThreadSetName(const char* name);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbThreadSetName-stub">

```
#include "starboard/thread.h"

void SbThreadSetName(const char* /*name*/) {
}
```

  </div>
  <div class="mdl-tabs__panel" id="SbThreadSetName-linux">

```
#include "starboard/thread.h"

#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>

#include "starboard/log.h"
#include "starboard/string.h"

void SbThreadSetName(const char* name) {
  // We don't want to rename the main thread.
  if (SbThreadGetId() == getpid()) {
    return;
  }

  const int kMaxThreadNameLength = 16;
  char buffer[kMaxThreadNameLength];

  if (SbStringGetLength(name) >= SB_ARRAY_SIZE_INT(buffer)) {
    SbStringCopy(buffer, name, SB_ARRAY_SIZE_INT(buffer));
    SB_DLOG(WARNING) << "Thread name \"" << name << "\" was truncated to \""
                     << buffer << "\"";
    name = buffer;
  }

  if (pthread_setname_np(pthread_self(), name) != 0) {
    SB_DLOG(ERROR) << "Failed to set thread name to " << name;
  }
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>        <code>name</code></td>
    <td>The name to assign to the thread.</td>
  </tr>
</table>

### SbThreadSleep

**Description**

Sleeps the currently executing thread.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbThreadSleep-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbThreadSleep-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbThreadSleep-declaration">
<pre>
SB_EXPORT void SbThreadSleep(SbTime duration);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbThreadSleep-stub">

```
#include "starboard/thread.h"

void SbThreadSleep(SbTime /*duration*/) {
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbTime</code><br>        <code>duration</code></td>
    <td>The minimum amount of time, in microseconds, that the currently
executing thread should sleep. The function is a no-op if this value is
negative or <code>0</code>.</td>
  </tr>
</table>

### SbThreadYield

**Description**

Yields the currently executing thread, so another thread has a chance to run.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbThreadYield-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbThreadYield-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbThreadYield-declaration">
<pre>
SB_EXPORT void SbThreadYield();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbThreadYield-stub">

```
#include "starboard/thread.h"

void SbThreadYield() {
}
```

  </div>
</div>

