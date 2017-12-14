---
layout: doc
title: "Starboard Module Reference: time.h"
---

Provides access to system time and timers.

## Macros ##

### kSbTimeDay ###

One day in SbTime units (microseconds).

### kSbTimeHour ###

One hour in SbTime units (microseconds).

### kSbTimeMax ###

The maximum value of an SbTime.

### kSbTimeMillisecond ###

One millisecond in SbTime units (microseconds).

### kSbTimeMinute ###

One minute in SbTime units (microseconds).

### kSbTimeNanosecondsPerMicrosecond ###

How many nanoseconds in one SbTime unit (microseconds).

### kSbTimeSecond ###

One second in SbTime units (microseconds).

### kSbTimeToPosixDelta ###

A term that can be added to an SbTime to convert it into the number of
microseconds since the POSIX epoch.

## Typedefs ##

### SbTime ###

The number of microseconds since the epoch of January 1, 1601 UTC, or the number
of microseconds between two times. Always microseconds, ALWAYS UTC.

#### Definition ####

```
typedef int64_t SbTime
```

### SbTimeMonotonic ###

A number of microseconds from some point. The main property of this time is that
it increases monotonically. It should also be as high-resolution a timer as we
can get on a platform. So, it is good for measuring the time between two calls
without worrying about a system clock adjustment. It's not good for getting the
wall clock time.

#### Definition ####

```
typedef int64_t SbTimeMonotonic
```

## Functions ##

### SbTimeFromPosix ###

Converts microseconds from the POSIX epoch into an `SbTime`.

`time`: A time that measures the number of microseconds since January 1, 1970,
00:00:00, UTC.

#### Declaration ####

```
static SbTime SbTimeFromPosix(int64_t time)
```

### SbTimeGetMonotonicNow ###

Gets a monotonically increasing time representing right now.

#### Declaration ####

```
SbTimeMonotonic SbTimeGetMonotonicNow()
```

### SbTimeGetMonotonicThreadNow ###

Gets a monotonically increasing time representing how long the current thread
has been in the executing state (i.e. not pre-empted nor waiting on an event).
This is not necessarily total time and is intended to allow measuring thread
execution time between two timestamps. If this is not available then
SbTimeGetMonotonicNow() should be used.

#### Declaration ####

```
SbTimeMonotonic SbTimeGetMonotonicThreadNow()
```

### SbTimeGetNow ###

Gets the current system time as an `SbTime`.

#### Declaration ####

```
SbTime SbTimeGetNow()
```

### SbTimeNarrow ###

Safely narrows a number from a more precise unit to a less precise one. This
function rounds negative values toward negative infinity.

#### Declaration ####

```
static int64_t SbTimeNarrow(int64_t time, int64_t divisor)
```

### SbTimeToPosix ###

Converts `SbTime` into microseconds from the POSIX epoch.

`time`: A time that is either measured in microseconds since the epoch of
January 1, 1601, UTC, or that measures the number of microseconds between two
times.

#### Declaration ####

```
static int64_t SbTimeToPosix(SbTime time)
```

