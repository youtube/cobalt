#include "starboard/time_zone.h"

#include <array>
#include <gmock/gmock.h>

struct TimeZoneWithExpectValue {
  TimeZoneWithExpectValue(std::string timeZoneName_,
                          SbTimeZone expectedStandardValue_,
                          SbTimeZone expectedDaylightValue_)
      : timeZoneName{timeZoneName_},
        expectedStandardValue{expectedStandardValue_},
        expectedDaylightValue{expectedDaylightValue_} {}

  std::string timeZoneName;

  SbTimeZone expectedStandardValue;
  SbTimeZone expectedDaylightValue;
};

extern const std::array<TimeZoneWithExpectValue, 12> timeZonesWithExpectedTimeValues;