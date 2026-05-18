Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Starboard Module Reference: `time_zone.h`

Provides access to the system time zone information.

## Typedefs

### SbTimeZone

The number of minutes west of the Greenwich Prime Meridian, NOT including
Daylight Savings Time adjustments.

For example: America/Los_Angeles is 480 minutes (28800 seconds, 8 hours).

#### Definition

```
typedef int SbTimeZone
```

## Functions

### SbTimeZoneGetCurrent

Gets the system's current SbTimeZone in minutes.

#### Declaration

```
SbTimeZone SbTimeZoneGetCurrent()
```

### SbTimeZoneGetName

Gets a string representation of the current timezone. The format should be in
the IANA format [https://data.iana.org/time-zones/theory.html#naming](https://data.iana.org/time-zones/theory.html#naming)) .
Names normally have the form AREA/LOCATION, where AREA is a continent or ocean,
and LOCATION is a specific location within the area. Typical names are
'Africa/Cairo', 'America/New_York', and 'Pacific/Honolulu'.

#### Declaration

```
const char* SbTimeZoneGetName()
```

