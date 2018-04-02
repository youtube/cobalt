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
#include "starboard/shared/uwp/log_file_impl.h"
#include "starboard/string.h"

namespace sbuwp = starboard::shared::uwp;

namespace {

void OutputToDebugConsole(const char* message) {
  // OutputDebugStringA may stall for multiple seconds if the output string is
  // too long. Split |message| into shorter strings for output.
  char buffer[512];
  for (;;) {
    errno_t result = strncpy_s(buffer, message, _TRUNCATE);
    if (result == 0) {
      OutputDebugStringA(buffer);
      break;
    } else if (result == STRUNCATE) {
      OutputDebugStringA(buffer);
      message += sizeof(buffer) - 1;
    } else {
      break;
    }
  }
}

}  // namespace.

void SbLogRaw(const char* message) {
  fprintf(stderr, "%s", message);
  sbuwp::WriteToLogFile(
      message, static_cast<int>(SbStringGetLength(message)));

  OutputToDebugConsole(message);
  starboard::shared::starboard::NetLogWrite(message);
}
