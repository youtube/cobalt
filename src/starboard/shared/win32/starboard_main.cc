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

#include "starboard/shared/win32/starboard_main.h"

#include <Objbase.h>
#include <WinSock2.h>
#include <mfapi.h>
#include <windows.h>

#include <cstdio>

#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "starboard/shared/starboard/command_line.h"
#include "starboard/shared/starboard/net_log.h"
#include "starboard/shared/win32/application_win32.h"
#include "starboard/shared/win32/thread_private.h"

using starboard::shared::starboard::CommandLine;
using starboard::shared::starboard::kNetLogCommandSwitchWait;
using starboard::shared::starboard::NetLogFlushThenClose;
using starboard::shared::starboard::NetLogWaitForClientConnected;
using starboard::shared::win32::ApplicationWin32;

namespace {

void WaitForNetLogIfNecessary(const CommandLine& cmd_line) {
  if (cmd_line.HasSwitch(kNetLogCommandSwitchWait)) {
    NetLogWaitForClientConnected();
  }
}

}  // namespace.

extern "C" int StarboardMain(int argc, char** argv) {
  WSAData wsaData;
  const int kWinSockVersionMajor = 2;
  const int kWinSockVersionMinor = 2;
  const int init_result = WSAStartup(
      MAKEWORD(kWinSockVersionMajor, kWinSockVersionMajor), &wsaData);

  SB_CHECK(init_result == 0);
  // WSAStartup returns the highest version that is supported up to the version
  // we request.
  SB_CHECK(LOBYTE(wsaData.wVersion) == kWinSockVersionMajor &&
           HIBYTE(wsaData.wVersion) == kWinSockVersionMinor);

  // Initialize COM for XAudio2 APIs.
  CoInitialize(nullptr);
  SbAudioSinkPrivate::Initialize();
  starboard::shared::win32::RegisterMainThread();
  // TODO: Do this with SbOnce when media is first used instead.
  HRESULT hr = MFStartup(MF_VERSION);
  SB_DCHECK(SUCCEEDED(hr));

  WaitForNetLogIfNecessary(CommandLine(argc, argv));
  ApplicationWin32 application;
  // This will run the message loop.
  const int main_return_value = application.Run(argc, argv);
  NetLogFlushThenClose();

  MFShutdown();
  WSACleanup();
  return main_return_value;
}
