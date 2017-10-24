---
layout: doc
title: "Starboard Module Reference: time_zone.h"
---

Provides access to the system time zone information.

## Functions

### SbTimeZoneGetCurrent

**Description**

Gets the system's current SbTimeZone in minutes.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbTimeZoneGetCurrent-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbTimeZoneGetCurrent-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbTimeZoneGetCurrent-declaration">
<pre>
SB_EXPORT SbTimeZone SbTimeZoneGetCurrent();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbTimeZoneGetCurrent-stub">

```
#include "starboard/time_zone.h"

SbTimeZone SbTimeZoneGetCurrent() {
  return 0;
}
```

  </div>
</div>

### SbTimeZoneGetDstName

**Description**

Gets the three-letter code of the current timezone in Daylight Savings Time,
regardless of current DST status. (e.g. "PDT")

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbTimeZoneGetDstName-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbTimeZoneGetDstName-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbTimeZoneGetDstName-declaration">
<pre>
SB_EXPORT const char* SbTimeZoneGetDstName();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbTimeZoneGetDstName-stub">

```
#include "starboard/time_zone.h"

#if SB_API_VERSION < 6
const char* SbTimeZoneGetDstName() {
  return "GMT";
}
#endif  // SB_API_VERSION < 6
```

  </div>
</div>

### SbTimeZoneGetName

**Description**

Gets the three-letter code of the current timezone in standard time,
regardless of current Daylight Savings Time status. (e.g. "PST")
Gets a string representation of the current timezone. Note that the string
representation can either be standard or daylight saving time. The output
can be of the form:
1) A three-letter abbreviation such as "PST" or "PDT" (preferred).
2) A time zone identifier such as "America/Los_Angeles"
3) An un-abbreviated name such as "Pacific Standard Time".

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbTimeZoneGetName-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbTimeZoneGetName-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbTimeZoneGetName-declaration">
<pre>
SB_EXPORT const char* SbTimeZoneGetName();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbTimeZoneGetName-stub">

```
#include "starboard/time_zone.h"

const char* SbTimeZoneGetName() {
  return "GMT";
}
```

  </div>
</div>

