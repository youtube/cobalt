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

#include <string>

#include "starboard/log.h"
#include "starboard/shared/nouser/user_internal.h"
#include "starboard/shared/uwp/winrt_workaround.h"
#include "starboard/shared/win32/wchar_utils.h"
#include "starboard/string.h"
#include "starboard/system.h"

using Windows::Storage::ApplicationData;

namespace sbwin32 = starboard::shared::win32;

namespace starboard {
namespace shared {
namespace nouser {

bool GetHomeDirectory(SbUser user, char* out_path, int path_size) {
  if (user != SbUserGetCurrent()) {
    return false;
  }
  std::string home_directory =
      sbwin32::platformStringToString(
          ApplicationData::Current->LocalFolder->Path);
  return SbStringCopy(out_path, home_directory.c_str(), path_size);
}

}  // namespace nouser
}  // namespace shared
}  // namespace starboard
