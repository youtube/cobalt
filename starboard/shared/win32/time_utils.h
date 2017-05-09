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

#ifndef STARBOARD_SHARED_WIN32_TIME_UTILS_H_
#define STARBOARD_SHARED_WIN32_TIME_UTILS_H_

namespace starboard {
namespace shared {
namespace win32 {

inline SbTime ConvertFileTimeTicksToSbTime(const LARGE_INTEGER ticks) {
  // According to
  // https://msdn.microsoft.com/en-us/library/windows/desktop/ms724284(v=vs.85).aspx
  // FILETIME format is "Contains a 64-bit value representing the number of
  // 100-nanosecond intervals since January 1, 1601 (UTC)"
  const uint64_t kNumber100nanosecondTicksInMicrosecond = 10;
  return ticks.QuadPart / kNumber100nanosecondTicksInMicrosecond;
}

inline SbTime ConvertFileTimeToSbTime(const FILETIME file_time) {
  // According to
  // https://msdn.microsoft.com/en-us/library/windows/desktop/ms724284(v=vs.85).aspx
  // FILETIME format is "Contains a 64-bit value representing the number of
  // 100-nanosecond intervals since January 1, 1601 (UTC)"
  LARGE_INTEGER ticks;
  ticks.QuadPart = (static_cast<LONGLONG>(file_time.dwHighDateTime) << 32) |
                   file_time.dwLowDateTime;
  return ConvertFileTimeTicksToSbTime(ticks);
}

inline SbTime ConvertSystemTimeToSbTime(const SYSTEMTIME system_time) {
  FILETIME file_time = {0};
  SystemTimeToFileTime(&system_time, &file_time);
  return ConvertFileTimeToSbTime(file_time);
}

// Many Win32 calls take millis, but SbTime is microseconds.
// Many nplb tests assume waits are at least as long as requested, so
// round up.
inline DWORD ConvertSbTimeToMillisRoundUp(SbTime duration) {
  return (duration + kSbTimeMillisecond - 1) / kSbTimeMillisecond;
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_TIME_UTILS_H_
