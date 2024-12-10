#include "time_zone_with_expect_value.h"

#if defined(_WIN32)

const std::array<TimeZoneWithExpectValue, 12> timeZonesWithExpectedTimeValues{
    TimeZoneWithExpectValue("UTC", 0, 0),
    TimeZoneWithExpectValue("Atlantic Standard Time", 240, 180),
    TimeZoneWithExpectValue("Eastern Standard Time", 300, 240),
    TimeZoneWithExpectValue("Central Standard Time", 360, 300),
    TimeZoneWithExpectValue("Mountain Standard Time", 420, 360),
    TimeZoneWithExpectValue("Pacific Standard Time", 480, 420),
    TimeZoneWithExpectValue("Yukon Standard Time", 420, 420),
    TimeZoneWithExpectValue("Samoa Standard Time", -840, -840),
    TimeZoneWithExpectValue("China Standard Time", -480, -480),
    TimeZoneWithExpectValue("Central European Standard Time", -60, -120),
    TimeZoneWithExpectValue("Omsk Standard Time", -360, -360),
    TimeZoneWithExpectValue("Cen. Australia Standard Time", -570, -630)};

#endif
