// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// These functions use ICU to generate test values based on specific and local
// time zones.

#ifndef BASE_TEST_TIME_HELPERS_H_
#define BASE_TEST_TIME_HELPERS_H_

#include <string>

#include "base/time/time.h"

namespace base {
namespace test {
namespace time_helpers {

// Some TimeZone values that can be used in tests.
enum TimeZone {
  kTimeZoneGMT,
  kTimeZoneLocal,
  kTimeZonePacific,
};

// Converts a set of fields in a timezone to a time.
// |timezone| is one of the enumerated timezones.
// |year| is a full 4-digit year
// |month| is a 1-based month (1 = January)
// |date| is a 1-based day-of-month (first day of month is 1)
// |hour| is the 0-based 24-hour clock hour-of-the-day
//        (midnight is 0, noon is 12, 3pm is 15)
// |minute| is the 0-based clock minute-of-the-hour (0-59)
// |second| is the 0-based clock second-of-the-minute (0-59)
base::Time FieldsToTime(TimeZone timezone,
                        int year,
                        int month,
                        int date,
                        int hour,
                        int minute,
                        int second);

// Defines a standard date to use in tests.
// Mon, Oct 15 12:45:00 PDT 2007
base::Time TestDateToTime(TimeZone timezone);

// Formats the given time into a UTC something parsable by
// base::Time::FromString().
std::string TimeFormatUTC(base::Time time);

// Formats the given time into a local something parsable by
// base::Time::FromString().
std::string TimeFormatLocal(base::Time time);

}  // namespace time_helpers
}  // namespace test
}  // namespace base
#endif  // BASE_TEST_TIME_HELPERS_H_
