// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_NPLB_POSIX_COMPLIANCE_POSIX_TIMEZONE_TEST_HELPERS_H_
#define STARBOARD_NPLB_POSIX_COMPLIANCE_POSIX_TIMEZONE_TEST_HELPERS_H_

#include <gtest/gtest.h>
#include <stdlib.h>
#include <time.h>

#include <optional>
#include <string>
#include <type_traits>

#include "starboard/common/source_location.h"

namespace nplb {

// The declarations for tzname, timezone, and daylight are provided by <time.h>.
// We use static_assert to ensure they have the expected types as defined by
// POSIX.

// POSIX defines tzname as "char *tzname[]", but both musl and glibc define it
// as "char *tzname[2]". This check allows either.
static_assert(std::is_same_v<decltype(tzname), char* [2]> ||
                  std::is_same_v<decltype(tzname), char*[]>,
              "Type of tzname must be 'char* [2]' or 'char* []'.");

static_assert(std::is_same_v<decltype(timezone), long>,
              "Type of timezone must be 'long'.");
static_assert(std::is_same_v<decltype(daylight), int>,
              "Type of daylight must be 'int'.");

// Number of seconds in an hour.
constexpr int kSecondsInHour = 3600;
// Number of seconds in a minute.
constexpr int kSecondsInMinute = 60;

// Helper to create a time_t for a specific UTC date.
time_t CreateTime(int year, int month, int day);

struct TimeSamples {
  std::optional<time_t> standard;
  std::optional<time_t> daylight;
};

// Helper to deterministically find a sample of standard and daylight time
// for a given year.
TimeSamples GetTimeSamples(int year);

// Helper to assert the values of a tm struct.
void AssertTM(
    const tm& tminfo,
    long offset,
    const char* std_zone,
    const std::optional<const char*>& dst_zone,
    const char* tz,
    starboard::SourceLocation location = starboard::SourceLocation::current());

}  // namespace nplb

#endif  // STARBOARD_NPLB_POSIX_COMPLIANCE_POSIX_TIMEZONE_TEST_HELPERS_H_
