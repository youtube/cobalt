// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#if SB_API_VERSION < 16

#include "starboard/time.h"

#include <windows.h>

SbTime SbTimeGetNow() {
  SYSTEMTIME system_time;
  GetSystemTime(&system_time);

  FILETIME file_time;
  SystemTimeToFileTime(&system_time, &file_time);

  ULARGE_INTEGER large_int;
  large_int.LowPart = file_time.dwLowDateTime;
  large_int.HighPart = file_time.dwHighDateTime;
  return static_cast<SbTime>(large_int.QuadPart) / 10;
}

#endif  // SB_API_VERSION < 16
