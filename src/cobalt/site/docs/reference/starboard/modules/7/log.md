---
layout: doc
title: "Starboard Module Reference: log.h"
---

Defines debug logging functions.

## Enums ##

### SbLogPriority ###

The priority at which a message should be logged. The platform may be configured
to filter logs by priority, or render them differently.

#### Values ####

*   `kSbLogPriorityUnknown`
*   `kSbLogPriorityInfo`
*   `kSbLogPriorityWarning`
*   `kSbLogPriorityError`
*   `kSbLogPriorityFatal`

## Functions ##

### SbLog ###

Writes `message` to the platform's debug output log.

`priority`: The SbLogPriority at which the message should be logged. Note that
passing `kSbLogPriorityFatal` does not terminate the program. Such a policy must
be enforced at the application level. In fact, `priority` may be completely
ignored on many platforms. `message`: The message to be logged. No formatting is
required to be done on the value, including newline termination. That said,
platforms can adjust the message to be more suitable for their output method by
wrapping the text, stripping unprintable characters, etc.

#### Declaration ####

```
void SbLog(SbLogPriority priority, const char *message)
```

### SbLogFlush ###

Flushes the log buffer on some platforms.

#### Declaration ####

```
void SbLogFlush()
```

### SbLogFormat ###

A log output method that additionally performs a string format on the data being
logged.

#### Declaration ####

```
void SbLogFormat(const char *format, va_list args) SB_PRINTF_FORMAT(1
```

### SbLogFormatF ###

Inline wrapper of SbLogFormat that converts from ellipsis to va_args.

#### Declaration ####

```
void static void void SbLogFormatF(const char *format,...) SB_PRINTF_FORMAT(1
```

### SbLogIsTty ###

Indicates whether the log output goes to a TTY or is being redirected.

#### Declaration ####

```
bool SbLogIsTty()
```

### SbLogRaw ###

A bare-bones log output method that is async-signal-safe, i.e. safe to call from
an asynchronous signal handler (e.g. a `SIGSEGV` handler). It should not do any
additional formatting.

`message`: The message to be logged.

#### Declaration ####

```
void SbLogRaw(const char *message)
```

### SbLogRawDumpStack ###

Dumps the stack of the current thread to the log in an async-signal-safe manner,
i.e. safe to call from an asynchronous signal handler (e.g. a `SIGSEGV`
handler). Does not include SbLogRawDumpStack itself.

`frames_to_skip`: The number of frames to skip from the top of the stack when
dumping the current thread to the log. This parameter lets you remove noise from
helper functions that might end up on top of every stack dump so that the stack
dump is just the relevant function stack where the problem occurred.

#### Declaration ####

```
void SbLogRawDumpStack(int frames_to_skip)
```

### SbLogRawFormat ###

A formatted log output method that is async-signal-safe, i.e. safe to call from
an asynchronous signal handler (e.g. a `SIGSEGV` handler).

#### Declaration ####

```
void SbLogRawFormat(const char *format, va_list args) SB_PRINTF_FORMAT(1
```

### SbLogRawFormatF ###

Inline wrapper of SbLogFormat to convert from ellipsis to va_args.

#### Declaration ####

```
void static void void SbLogRawFormatF(const char *format,...) SB_PRINTF_FORMAT(1
```

