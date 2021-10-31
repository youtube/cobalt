// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_NPLB_TIME_CONSTANTS_H_
#define STARBOARD_NPLB_TIME_CONSTANTS_H_

#include "starboard/time.h"

namespace starboard {
namespace nplb {

static const SbTime kExtraMilliseconds = 123 * kSbTimeMillisecond;
static const SbTime kTestSbTimeYear = 525949 * kSbTimeMinute;

// 1443121328 in POSIX time is
// Thursday, 9/24/2015 19:02:08 UTC
static const SbTime kTestTimePositive = SbTimeFromPosix(
    SB_INT64_C(1443121328) * kSbTimeSecond + kExtraMilliseconds);

// 0 in POSIX time is
// Thursday, 1/1/1970 00:00:00 UTC
static const SbTime kTestTimePosixZero =
    SbTimeFromPosix(SB_INT64_C(0) * kSbTimeSecond);

// -771942639 in POSIX time is
// Monday, 7/16/1945 11:29:21 UTC
static const SbTime kTestTimePosixNegative =
    SbTimeFromPosix(SB_INT64_C(-771942639) * kSbTimeSecond);

// 0 in Windows time is
// Monday, 1/1/1601 00:00:00 UTC
static const SbTime kTestTimeWindowsZero = 0;

// -15065654400 in POSIX time is
// Wednesday, 8/3/1492 00:00:00 UTC
static const SbTime kTestTimeWindowsNegative =
    SbTimeFromPosix(SB_INT64_C(-15065654400) * kSbTimeSecond);

// 1600970155 in POSIX time is
// Thursday, 9/24/2020 17:55:55 UTC
// NOTE: Update this value once every 25 or so years.
static const SbTime kTestTimeWritten =
    SbTimeFromPosix(SB_INT64_C(1600970155) * kSbTimeSecond);

// 25 years after the time this test was written.
static const SbTime kTestTimePastWritten =
    SbTimeFromPosix(kTestTimeWritten + (25 * kTestSbTimeYear));

}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_TIME_CONSTANTS_H_
