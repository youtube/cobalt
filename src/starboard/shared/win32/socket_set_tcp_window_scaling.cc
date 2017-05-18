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

#include "starboard/socket.h"

#include <windows.h>
#include <winsock2.h>

// Note that windows.h and winsock2.h must be included before these.
#include <mswsock.h>
#include <sdkddkver.h>

#include "starboard/shared/win32/socket_internal.h"

bool SbSocketSetTcpWindowScaling(SbSocket socket, bool enable_window_scaling) {
  // From
  // https://msdn.microsoft.com/en-us/library/windows/desktop/cc136103(v=vs.85).aspx:
  // "When the TargetOsVersion member is set to a value for Windows Vista or
  // later, receive window auto-tuning is enabled and the TCP window scale
  // factor is reduced to 2 from the default value of 8."

  const ULONG target_os_version =
      enable_window_scaling ? NTDDI_VISTA : NTDDI_WS03;
  WSA_COMPATIBILITY_MODE compatibility_mode = {WsaBehaviorAutoTuning,
                                               target_os_version};

  DWORD kZero = 0;
  int return_value = WSAIoctl(
      socket->socket_handle, SIO_SET_COMPATIBILITY_MODE, &compatibility_mode,
      sizeof(WSA_COMPATIBILITY_MODE), nullptr, 0, &kZero, nullptr, nullptr);

  return return_value != SOCKET_ERROR;
}
