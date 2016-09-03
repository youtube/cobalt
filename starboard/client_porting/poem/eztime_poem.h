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

// A poem (POsix EMulation) for time functions based on EzTime, which is based
// on ICU and Starboard.

#ifndef STARBOARD_CLIENT_PORTING_POEM_EZTIME_POEM_H_
#define STARBOARD_CLIENT_PORTING_POEM_EZTIME_POEM_H_

// #if defined(POEM_FULL_EMULATION) && (POEM_FULL_EMULATION)

#include "starboard/client_porting/eztime/eztime.h"

#undef time_t
#define time_t EzTimeT

#undef tm
#define tm EzTimeExploded

#undef timeval
#define timeval EzTimeValue

#define gettimeofday(a, b) EzTimeValueGetNow(a, b)
#define gmtime_r(a, b) EzTimeTExplodeUTC(a, b)
#define localtime_r(a, b) EzTimeTExplodeLocal(a, b)
#define mktime(x) EzTimeTImplodeLocal(x)
#define time(x) EzTimeTGetNow(x)
#define timegm(x) EzTimeTImplodeUTC(x)
#define timelocal(x) EzTimeTImplodeLocal(x)

// #endif // POEM_FULL_EMULATION

#endif  // STARBOARD_CLIENT_PORTING_POEM_EZTIME_POEM_H_
