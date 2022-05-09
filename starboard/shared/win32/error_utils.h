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

// Not breaking these functions up because however one is implemented, the
// others should be implemented similarly.

#ifndef STARBOARD_SHARED_WIN32_ERROR_UTILS_H_
#define STARBOARD_SHARED_WIN32_ERROR_UTILS_H_

#include <windows.h>

#include <ostream>
#include <string>

#include "starboard/common/log.h"

namespace starboard {
namespace shared {
namespace win32 {

class Win32ErrorCode {
 public:
  explicit Win32ErrorCode(DWORD error_code) : error_code_(error_code) {}

  HRESULT GetHRESULT() const { return HRESULT_FROM_WIN32(error_code_); }

 private:
  DWORD error_code_;
};

std::ostream& operator<<(std::ostream& os, const Win32ErrorCode& error_code);

// Checks for system errors and logs a human-readable error if GetLastError()
// returns an error code. Noops on non-debug builds.
void DebugLogWinError();

std::string HResultToString(HRESULT hr);

// The following macro evaluates statement in every build config, and checks the
// result in debug & devel builds only.
#define CheckResult(statement)                                          \
  do {                                                                  \
    auto check_result_hr = (statement);                                 \
    SB_DCHECK(SUCCEEDED(check_result_hr))                               \
        << "HRESULT was " << std::hex << check_result_hr                \
        << " which translates to\n---> \""                              \
        << ::starboard::shared::win32::HResultToString(check_result_hr) \
        << "\"";                                                        \
  } while (0)

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_ERROR_UTILS_H_
