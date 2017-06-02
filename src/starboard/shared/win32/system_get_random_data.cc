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

#include "starboard/system.h"

#include <windows.h>

// This must be included after windows.h
#include <bcrypt.h>

#include "starboard/log.h"
#include "starboard/shared/win32/error_utils.h"

namespace sbwin32 = starboard::shared::win32;

void SbSystemGetRandomData(void* out_buffer, int buffer_size) {
  // Note: this might not be secure before Windows 10, as
  // BCRYPT_RNG_DUAL_EC_ALGORITHM might be used.  See:
  // https://msdn.microsoft.com/en-us/library/windows/desktop/aa375534%28v=vs.85%29.aspx
  // "Windows 10:  Beginning with Windows 10, the dual elliptic curve random
  // number generator algorithm has been removed. Existing uses of this
  // algorithm will continue to work; however, the random number generator is
  // based on the AES counter mode specified in the NIST SP 800-90 standard. New
  // code should use BCRYPT_RNG_ALGORITHM, and it is recommended that existing
  // code be changed to use BCRYPT_RNG_ALGORITHM."
  NTSTATUS status =
      BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(out_buffer),
                      buffer_size, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
  SB_DCHECK(status == 0) << "Error while calling CryptGenRandom: "
                         << sbwin32::Win32ErrorCode(GetLastError());
}
