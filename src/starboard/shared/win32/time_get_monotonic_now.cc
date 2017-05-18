// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/time.h"

#include <windows.h>

#include "starboard/log.h"

SbTimeMonotonic SbTimeGetMonotonicNow() {
  LARGE_INTEGER counts;
  bool success;

  success = QueryPerformanceCounter(&counts);

  // "On systems that run Windows XP or later,
  // the function will always succeed and will thus never return zero."
  // https://msdn.microsoft.com/en-us/library/windows/desktop/ms644904(v=vs.85).aspx
  SB_DCHECK(success);

  LARGE_INTEGER countsPerSecond;
  success = QueryPerformanceFrequency(&countsPerSecond);
  // "On systems that run Windows XP or later,
  // the function will always succeed and will thus never return zero."
  // https://msdn.microsoft.com/en-us/library/windows/desktop/ms644905(v=vs.85).aspx
  SB_DCHECK(success);

  // An observed value of countsPerSecond on a desktop x86 machine is
  // ~2.5e6. With this frequency, it will take ~37500 days to exceed
  // 2^53, which is the mantissa precision of a double.
  // Hence, we can safely convert to a double here without losing precision.
  double result = static_cast<double>(counts.QuadPart);
  result *= (1000.0 * 1000.0) / countsPerSecond.QuadPart;

  return static_cast<SbTimeMonotonic>(result);
}
