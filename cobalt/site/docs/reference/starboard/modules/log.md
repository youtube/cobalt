---
layout: doc
title: "Starboard Module Reference: log.h"
---

Defines debug logging functions.

## Enums

### SbLogPriority

The priority at which a message should be logged. The platform may be
configured to filter logs by priority, or render them differently.

**Values**

*   `kSbLogPriorityUnknown`
*   `kSbLogPriorityInfo`
*   `kSbLogPriorityWarning`
*   `kSbLogPriorityError`
*   `kSbLogPriorityFatal`

## Macros

<div id="macro-documentation-section">

<h3 id="sb_check" class="small-h3">SB_CHECK</h3>

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_dcheck" class="small-h3">SB_DCHECK</h3>

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_dlog" class="small-h3">SB_DLOG</h3>

<h3 id="sb_dlog_if" class="small-h3">SB_DLOG_IF</h3>

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_dlog_is_on" class="small-h3">SB_DLOG_IS_ON</h3>

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_dstack" class="small-h3">SB_DSTACK</h3>

<h3 id="sb_eat_stream_parameters" class="small-h3">SB_EAT_STREAM_PARAMETERS</h3>

<h3 id="sb_lazy_stream" class="small-h3">SB_LAZY_STREAM</h3>

<h3 id="sb_log" class="small-h3">SB_LOG</h3>

<h3 id="sb_logging_is_official_build" class="small-h3">SB_LOGGING_IS_OFFICIAL_BUILD</h3>

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_log_if" class="small-h3">SB_LOG_IF</h3>

<h3 id="sb_log_is_on" class="small-h3">SB_LOG_IS_ON</h3>

<h3 id="sb_log_message_0" class="small-h3">SB_LOG_MESSAGE_0</h3>

Compatibility with base/logging.h which defines ERROR to be 0 to workaround
some system header defines that do the same thing.

<h3 id="sb_log_message_error" class="small-h3">SB_LOG_MESSAGE_ERROR</h3>

<h3 id="sb_log_message_fatal" class="small-h3">SB_LOG_MESSAGE_FATAL</h3>

<h3 id="sb_log_message_info" class="small-h3">SB_LOG_MESSAGE_INFO</h3>

<h3 id="sb_log_message_warning" class="small-h3">SB_LOG_MESSAGE_WARNING</h3>

<h3 id="sb_log_stream" class="small-h3">SB_LOG_STREAM</h3>

<h3 id="sb_notimplemented" class="small-h3">SB_NOTIMPLEMENTED</h3>

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_notimplemented_in" class="small-h3">SB_NOTIMPLEMENTED_IN</h3>

<h3 id="sb_notimplemented_msg" class="small-h3">SB_NOTIMPLEMENTED_MSG</h3>

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_notimplemented_policy" class="small-h3">SB_NOTIMPLEMENTED_POLICY</h3>

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_notreached" class="small-h3">SB_NOTREACHED</h3>

Note that this macro's definition varies depending on the values of one or more other variables.

<h3 id="sb_stack" class="small-h3">SB_STACK</h3>

<h3 id="sb_stack_if" class="small-h3">SB_STACK_IF</h3>

</div>

## Functions

### Break

**Declaration**

```
SB_EXPORT void Break();
```

### GetMinLogLevel

**Declaration**

```
SB_EXPORT SbLogPriority GetMinLogLevel();
```

### SbLog

**Description**

Writes `message` to the platform's debug output log.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbLog-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbLog-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbLog-declaration">
<pre>
SB_EXPORT void SbLog(SbLogPriority priority, const char* message);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbLog-stub">

```
#include "starboard/log.h"

void SbLog(SbLogPriority /*priority*/, const char* /*message*/) {
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbLogPriority</code><br>        <code>priority</code></td>
    <td>The <code>SbLog</code>Priority at which the message should be logged. Note
that passing <code>kSbLogPriorityFatal</code> does not terminate the program. Such a
policy must be enforced at the application level. In fact, <code>priority</code> may
be completely ignored on many platforms.</td>
  </tr>
  <tr>
    <td><code>const char*</code><br>        <code>message</code></td>
    <td>The message to be logged. No formatting is required to be done
on the value, including newline termination. That said, platforms can
adjust the message to be more suitable for their output method by
wrapping the text, stripping unprintable characters, etc.</td>
  </tr>
</table>

### SbLogFlush

**Description**

Flushes the log buffer on some platforms.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbLogFlush-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbLogFlush-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbLogFlush-declaration">
<pre>
SB_EXPORT void SbLogFlush();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbLogFlush-stub">

```
#include "starboard/log.h"

void SbLogFlush() {
}
```

  </div>
</div>

### SbLogFormat

**Description**

A log output method that additionally performs a string format on the
data being logged.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbLogFormat-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbLogFormat-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbLogFormat-declaration">
<pre>
SB_EXPORT void SbLogFormat(const char* format, va_list args)
    SB_PRINTF_FORMAT(1, 0);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbLogFormat-stub">

```
#include "starboard/log.h"

void SbLogFormat(const char* /*format*/, va_list /*arguments*/) {
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>
        <code>format</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>va_list</code><br>
        <code>args</code></td>
    <td> </td>
  </tr>
</table>

### SbLogFormatF

**Description**

Inline wrapper of <code><a href="#sblog">SbLog</a></code>Format that converts from ellipsis to va_args.

**Declaration**

```
static SB_C_INLINE void SbLogFormatF(const char* format, ...)
    SB_PRINTF_FORMAT(1, 2);
void SbLogFormatF(const char* format, ...) {
  va_list args;
  va_start(args, format);
  SbLogFormat(format, args);
  va_end(args);
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>
        <code>format</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code></code><br>
        <code>...</code></td>
    <td> </td>
  </tr>
</table>

### SbLogIsTty

**Description**

Indicates whether the log output goes to a TTY or is being redirected.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbLogIsTty-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbLogIsTty-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbLogIsTty-declaration">
<pre>
SB_EXPORT bool SbLogIsTty();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbLogIsTty-stub">

```
#include "starboard/log.h"

bool SbLogIsTty() {
  return false;
}
```

  </div>
</div>

### SbLogRaw

**Description**

A bare-bones log output method that is async-signal-safe, i.e. safe to call
from an asynchronous signal handler (e.g. a `SIGSEGV` handler). It should not
do any additional formatting.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbLogRaw-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbLogRaw-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbLogRaw-declaration">
<pre>
SB_EXPORT void SbLogRaw(const char* message);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbLogRaw-stub">

```
#include "starboard/log.h"

void SbLogRaw(const char* /*message*/) {
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>        <code>message</code></td>
    <td>The message to be logged.</td>
  </tr>
</table>

### SbLogRawDumpStack

**Description**

Dumps the stack of the current thread to the log in an async-signal-safe
manner, i.e. safe to call from an asynchronous signal handler (e.g. a
`SIGSEGV` handler). Does not include <code>SbLogRawDumpStack</code> itself.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbLogRawDumpStack-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbLogRawDumpStack-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbLogRawDumpStack-declaration">
<pre>
SB_EXPORT void SbLogRawDumpStack(int frames_to_skip);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbLogRawDumpStack-stub">

```
#include "starboard/log.h"

void SbLogRawDumpStack(int /*frames_to_skip*/) {
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>int</code><br>        <code>frames_to_skip</code></td>
    <td>The number of frames to skip from the top of the stack
when dumping the current thread to the log. This parameter lets you remove
noise from helper functions that might end up on top of every stack dump
so that the stack dump is just the relevant function stack where the
problem occurred.</td>
  </tr>
</table>

### SbLogRawFormat

**Description**

A formatted log output method that is async-signal-safe, i.e. safe to call
from an asynchronous signal handler (e.g. a `SIGSEGV` handler).

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbLogRawFormat-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbLogRawFormat-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbLogRawFormat-declaration">
<pre>
SB_EXPORT void SbLogRawFormat(const char* format, va_list args)
    SB_PRINTF_FORMAT(1, 0);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbLogRawFormat-stub">

```
#include "starboard/log.h"

void SbLogRawFormat(const char* /*format*/, va_list /*arguments*/) {
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>
        <code>format</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>va_list</code><br>
        <code>args</code></td>
    <td> </td>
  </tr>
</table>

### SbLogRawFormatF

**Description**

Inline wrapper of <code><a href="#sblog">SbLog</a></code>Format to convert from ellipsis to va_args.

**Declaration**

```
static SB_C_INLINE void SbLogRawFormatF(const char* format, ...)
    SB_PRINTF_FORMAT(1, 2);
void SbLogRawFormatF(const char* format, ...) {
  va_list args;
  va_start(args, format);
  SbLogRawFormat(format, args);
  va_end(args);
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>
        <code>format</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code></code><br>
        <code>...</code></td>
    <td> </td>
  </tr>
</table>

### SetMinLogLevel

**Declaration**

```
SB_EXPORT void SetMinLogLevel(SbLogPriority level);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbLogPriority</code><br>
        <code>level</code></td>
    <td> </td>
  </tr>
</table>

### causes

**Declaration**

```
// SB_EXPORT'd. Using SB_EXPORT here directly causes issues on Windows because
// it uses std classes which also need to be exported.
class LogMessage {
 public:
  LogMessage(const char* file, int line, SbLogPriority priority);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>issues on Windows</code><br>
        <code>because</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>// it uses std classes which also need to be</code><br>
        <code>exported.</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>class LogMessage</code><br>
        <code>{</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code></code><br>
        <code>public:</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>LogMessage(const char*</code><br>
        <code>file</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>int</code><br>
        <code>line</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbLogPriority</code><br>
        <code>priority</code></td>
    <td> </td>
  </tr>
</table>

### count 

**Declaration**

```
    static int count = 0;                                \
    if (0 == count++) {                                  \
      SbLog(kSbLogPriorityError, SB_NOTIMPLEMENTED_MSG); \
    }                                                    \
  } while (0)
#endif
#endif  // __cplusplus
#endif  // STARBOARD_LOG_H_
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>= 0;                               </code><br>
        <code>\</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>if (0 ==</code><br>
        <code>count++</code></td>
    <td> </td>
  </tr>
</table>

