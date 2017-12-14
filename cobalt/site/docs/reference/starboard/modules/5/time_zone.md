---
layout: doc
title: "Starboard Module Reference: time_zone.h"
---

Provides access to the system time zone information.

## Typedefs ##

### SbTimeZone ###

The number of minutes west of the Greenwich Prime Meridian, NOT including
Daylight Savings Time adjustments.

For example: PST/PDT is 480 minutes (28800 seconds, 8 hours).

#### Definition ####

```
typedef int SbTimeZone
```

## Functions ##

### SbTimeZoneGetCurrent ###

Gets the system's current SbTimeZone in minutes.

#### Declaration ####

```
SbTimeZone SbTimeZoneGetCurrent()
```

### SbTimeZoneGetDstName ###

Gets the three-letter code of the current timezone in Daylight Savings Time,
regardless of current DST status. (e.g. "PDT")

#### Declaration ####

```
const char* SbTimeZoneGetDstName()
```

### SbTimeZoneGetName ###

Gets the three-letter code of the current timezone in standard time, regardless
of current Daylight Savings Time status. (e.g. "PST")

#### Declaration ####

```
const char* SbTimeZoneGetName()
```

