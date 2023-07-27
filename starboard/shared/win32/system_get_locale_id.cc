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

#include "starboard/system.h"

#include <string>

#include "starboard/common/log.h"
#include "starboard/once.h"

#include "starboard/shared/win32/error_utils.h"
#include "starboard/shared/win32/wchar_utils.h"

using starboard::shared::win32::DebugLogWinError;
using starboard::shared::win32::wchar_tToUTF8;

namespace {
class LocaleString {
 public:
  static LocaleString* Get();
  const char* value() const { return value_.c_str(); }

 private:
  LocaleString() {
    wchar_t name[LOCALE_NAME_MAX_LENGTH];
    int result = GetUserDefaultLocaleName(name, LOCALE_NAME_MAX_LENGTH);
    if (result != 0) {
      value_ = wchar_tToUTF8(name);
    } else {
      SB_LOG(ERROR) << "Error retrieving GetUserDefaultLocaleName";
      DebugLogWinError();
      value_ = "en-US";
    }
  }
  std::string value_;
};

SB_ONCE_INITIALIZE_FUNCTION(LocaleString, LocaleString::Get);
}  // namespace

const char* SbSystemGetLocaleId() {
  return LocaleString::Get()->value();
}
