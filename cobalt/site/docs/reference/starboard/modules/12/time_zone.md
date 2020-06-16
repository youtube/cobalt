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

### SbTimeZoneGetName ###

Gets a string representation of the current timezone. Note that the string
representation can either be standard or daylight saving time. The output can be
of the form: 1) A three-letter abbreviation such as "PST" or "PDT" (preferred).
2) A time zone identifier such as "America/Los_Angeles" 3) An un-abbreviated
name such as "Pacific Standard Time".

#### Declaration ####

```
const char* SbTimeZoneGetName()
```

