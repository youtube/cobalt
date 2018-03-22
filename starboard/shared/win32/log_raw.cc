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

#include "starboard/log.h"

#include <stdio.h>
#include <windows.h>

#include "starboard/shared/starboard/net_log.h"
#include "starboard/shared/win32/log_file_impl.h"
#include "starboard/string.h"

namespace sbwin32 = starboard::shared::win32;

void SbLogRaw(const char* message) {
  fprintf(stderr, "%s", message);
  OutputDebugStringA(message);
  sbwin32::WriteToLogFile(
      message, static_cast<int>(SbStringGetLength(message)));

  starboard::shared::starboard::NetLogWrite(message);
}
