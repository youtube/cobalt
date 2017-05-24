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

#include <windows.h>

#include <WinSock2.h>

#include <string>
#include <vector>

#include "starboard/configuration.h"
#include "starboard/shared/uwp/application_uwp.h"
#include "starboard/shared/win32/thread_private.h"
#include "starboard/shared/win32/wchar_utils.h"

using starboard::shared::win32::wchar_tToUTF8;

int main(Platform::Array<Platform::String^>^ args) {
  if (!IsDebuggerPresent()) {
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  }

  const int kWinSockVersionMajor = 2;
  const int kWinSockVersionMinor = 2;
  WSAData wsaData;
  int init_result = WSAStartup(
      MAKEWORD(kWinSockVersionMajor, kWinSockVersionMajor), &wsaData);

  SB_CHECK(init_result == 0);
  // WSAStartup returns the highest version that is supported up to the version
  // we request.
  SB_CHECK(LOBYTE(wsaData.wVersion) == kWinSockVersionMajor &&
           HIBYTE(wsaData.wVersion) == kWinSockVersionMinor);

  starboard::shared::win32::RegisterMainThread();

  std::vector<std::string> string_args;
  for (auto it = args->begin(); it != args->end(); ++it) {
    Platform::String^ s = *it;
    string_args.push_back(wchar_tToUTF8(s->Data(), s->Length()));
  }

  std::vector<const char*> utf8_args;
  for (auto it = string_args.begin(); it != string_args.end(); ++it) {
    utf8_args.push_back(it->data());
  }

  starboard::shared::uwp::ApplicationUwp application;
  int return_value = application.Run(static_cast<int>(utf8_args.size()),
                                     const_cast<char**>(utf8_args.data()));

  WSACleanup();

  return return_value;
}
