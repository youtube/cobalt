#include "time_zone_with_expect_value.h"

#if !defined(_WIN32)

const std::array<TimeZoneWithExpectValue, 12> timeZonesWithExpectedTimeValues{
    TimeZoneWithExpectValue("UTC", 0, 0),
    TimeZoneWithExpectValue("America/Puerto_Rico", 240, 240),
    TimeZoneWithExpectValue("America/New_York", 300, 300),
    TimeZoneWithExpectValue("US/Eastern", 300, 300),
    TimeZoneWithExpectValue("America/Chicago", 360, 360),
    TimeZoneWithExpectValue("US/Mountain", 420, 420),
    TimeZoneWithExpectValue("US/Pacific", 480, 480),
    TimeZoneWithExpectValue("US/Alaska", 540, 540),
    TimeZoneWithExpectValue("Pacific/Honolulu", 600, 600),
    TimeZoneWithExpectValue("US/Samoa", 660, 660),
    TimeZoneWithExpectValue("Australia/South", -570, -570),
    TimeZoneWithExpectValue("Pacific/Guam", -600, -600)};

#endif
