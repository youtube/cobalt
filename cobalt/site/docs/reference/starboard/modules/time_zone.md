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

### SbTimeZoneGetName

Gets a string representation of the current timezone in [IANA format](https://data.iana.org/time-zones/theory.html#naming). Names typically
follow the `AREA/LOCATION` format, where `AREA` is a continent or ocean, and
`LOCATION` is a specific location within that area (for example, 'Africa/Cairo',
'America/New_York', or 'Pacific/Honolulu').

#### Declaration

```
const char * SbTimeZoneGetName()
```
