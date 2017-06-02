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

// Not breaking these functions up because however one is implemented, the
// others should be implemented similarly.

#include "starboard/shared/win32/error_utils.h"

#include "starboard/log.h"
#include "starboard/shared/win32/wchar_utils.h"

namespace starboard {
namespace shared {
namespace win32 {

std::ostream& operator<<(std::ostream& os, const Win32ErrorCode& error_code) {
  LPWSTR error_message;
  int message_size = FormatMessage(
      FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr,  // Unused with FORMAT_MESSAGE_FROM_SYSTEM.
      error_code.GetHRESULT(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPTSTR)&error_message,
      0,  // Minimum size for output buffer.
      nullptr);
  SB_DCHECK(message_size);
  os << wchar_tToUTF8(error_message);
  LocalFree(error_message);

  return os;
}

void DebugLogWinError() {
#if defined(_DEBUG)
  DWORD error_code = GetLastError();
  if (!error_code)
    return;

  SB_LOG(ERROR) << Win32ErrorCode(error_code);
#endif  // defined(_DEBUG)
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
