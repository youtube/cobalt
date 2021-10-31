// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_CLIENT_PORTING_EZTIME_TEST_CONSTANTS_H_
#define STARBOARD_CLIENT_PORTING_EZTIME_TEST_CONSTANTS_H_

#if defined(STARBOARD)

#include "starboard/client_porting/eztime/eztime.h"

namespace starboard {
namespace client_porting {
namespace eztime {

static const EzTimeT kTestEzTimeTYear = 525600 * kEzTimeTMinute;

// 1443121328 in POSIX time is
// Thursday, 9/24/2015 19:02:08 UTC
static const EzTimeT kTestTimePositive = SB_INT64_C(1443121328);

// 0 in POSIX time is
// Thursday, 1/1/1970 00:00:00 UTC
static const EzTimeT kTestTimePosixZero = SB_INT64_C(0);

// -771942639 in POSIX time is
// Monday, 7/16/1945 11:29:21 UTC
static const EzTimeT kTestTimePosixNegative = SB_INT64_C(-771942639);

// 0 in Windows time is
// Monday, 1/1/1601 00:00:00 UTC
static const EzTimeT kTestTimeWindowsZero =
    kTestTimePosixZero - kEzTimeTToWindowsDelta;

// -12212553600 in POSIX time is
// Saturday, 1/1/1583 00:00:00 UTC
// which is after the cutover to the Gregorian calendar, so most time system
// agree.
static const EzTimeT kTestTimeWindowsNegative = SB_INT64_C(-12212553600);

// 1600970155 in POSIX time is
// Thursday, 9/24/2020 17:55:55 UTC
// NOTE: Update this value once every 25 or so years.
static const EzTimeT kTestTimeWritten = SB_INT64_C(1600970155);

// 25 years after the time this test was written.
static const EzTimeT kTestTimePastWritten =
    (kTestTimeWritten + (25 * kTestEzTimeTYear));

// 4133980800 in POSIX time is
// Saturday, 01 Jan 2101 00:00:00 UTC
// NOTE: Update this value once every 100 or so years.
static const EzTimeT kTestTimeNextCentury = SB_INT64_C(4133980800);

}  // namespace eztime
}  // namespace client_porting
}  // namespace starboard

#endif  // STARBOARD

#endif  // STARBOARD_CLIENT_PORTING_EZTIME_TEST_CONSTANTS_H_
