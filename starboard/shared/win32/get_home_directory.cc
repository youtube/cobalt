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

#include <objbase.h>
#include <shlobj.h>
#include <shlwapi.h>

// Windows defines GetCommandLine as a macro to GetCommandLineW which
// breaks our Application::GetCommandLine call below; thus we undefine
// it after including our Windows headers.
#undef GetCommandLine

#include <cstdlib>
#include <cstring>
#include <string>

#include "starboard/log.h"
#include "starboard/shared/nouser/user_internal.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/command_line.h"
#include "starboard/shared/win32/application_win32.h"
#include "starboard/shared/win32/error_utils.h"
#include "starboard/shared/win32/wchar_utils.h"
#include "starboard/string.h"
#include "starboard/system.h"

namespace sbwin32 = starboard::shared::win32;

using starboard::shared::starboard::CommandLine;

namespace starboard {
namespace shared {
namespace nouser {

bool GetHomeDirectory(SbUser user, char* out_path, int path_size) {
  if (user != SbUserGetCurrent()) {
    return false;
  }

  PWSTR local_app_data_path = nullptr;

  if (S_OK == SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr,
                                   &local_app_data_path)) {
    TCHAR wide_path[MAX_PATH];
    wcscpy(wide_path, local_app_data_path);
    CoTaskMemFree(local_app_data_path);
    // Instead of using the raw local AppData directory, we create a program
    // app directory if it doesn't exist already.
    const CommandLine* command_line =
        sbwin32::ApplicationWin32::Get()->GetCommandLine();
    SB_DCHECK(command_line);
    const std::string program_name = command_line->argv()[0];
    PathAppend(
        wide_path,
        sbwin32::CStringToWString(program_name.c_str()).c_str());
    if (!PathFileExists(wide_path)) {
      SECURITY_ATTRIBUTES security_attributes = {sizeof(SECURITY_ATTRIBUTES),
                                                 NULL, TRUE};
      const BOOL created_directory =
          CreateDirectory(wide_path, &security_attributes);
      if (!created_directory) {
        SB_LOG(ERROR) << "Failed to create home directory";
        sbwin32::DebugLogWinError();
        return false;
      }
    }

    const size_t actual_path_length = wcslen(wide_path);
    if (path_size < actual_path_length) {
      SB_LOG(ERROR) << "Home directory length exceeds max path size";
      return false;
    }
    std::wcstombs(out_path, wide_path, actual_path_length + 1);
    return true;
  }
  SB_LOG(ERROR) << "Unable to open local AppData as home directory.";
  sbwin32::DebugLogWinError();
  return false;
}

}  // namespace nouser
}  // namespace shared
}  // namespace starboard
