// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_PERFORMANCE_HIGH_RESOLUTION_TIME_H_
#define COBALT_DOM_PERFORMANCE_HIGH_RESOLUTION_TIME_H_

#include "base/time/time.h"

namespace cobalt {
namespace dom {

// The definition of DOMHighResTimeStamp.
//   https://w3c.github.io/hr-time/#dom-domhighrestimestamp
typedef double DOMHighResTimeStamp;

// Time converstion helper functions.
inline DOMHighResTimeStamp ConvertSecondsToDOMHighResTimeStamp(double seconds) {
  return static_cast<DOMHighResTimeStamp>(seconds * 1000.0);
}

inline double ConvertDOMHighResTimeStampToSeconds(
    DOMHighResTimeStamp milliseconds) {
  return milliseconds / 1000.0;
}

inline DOMHighResTimeStamp ConvertTimeTicksToDOMHighResTimeStamp(
    base::TimeTicks time) {
  return (time - base::TimeTicks()).InMillisecondsF();
}

// Clamp customized minimum clock resolution in milliseconds.
//   https://w3c.github.io/hr-time/#clock-resolution
inline DOMHighResTimeStamp ClampTimeStampMinimumResolution(
    base::TimeTicks ticks,
  int64_t min_resolution_in_microseconds) {
  int64_t microseconds = ticks.since_origin().InMicroseconds();
  return base::TimeDelta::FromMicroseconds(microseconds -
      (microseconds % min_resolution_in_microseconds)).InMillisecondsF();
}

// Clamp customized minimum clock resolution in milliseconds.
//   https://w3c.github.io/hr-time/#clock-resolution
inline DOMHighResTimeStamp ClampTimeStampMinimumResolution(base::TimeDelta delta,
    int64_t min_resolution_in_microseconds) {
  int64_t microseconds = delta.InMicroseconds();
  return base::TimeDelta::FromMicroseconds(microseconds -
      (microseconds % min_resolution_in_microseconds)).InMillisecondsF();
}

inline DOMHighResTimeStamp
    ClampTimeStampMinimumResolution(DOMHighResTimeStamp time_delta,
                                    int64_t min_resolution_in_microseconds) {
  return time_delta -
      (static_cast<int64_t>(time_delta) % min_resolution_in_microseconds);
}

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_PERFORMANCE_HIGH_RESOLUTION_TIME_H_
