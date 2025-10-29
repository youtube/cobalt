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

#include <netinet/tcp.h>

#include "starboard/shared/posix/socket_internal.h"

namespace sbposix = starboard::shared::posix;

bool SbSocketSetTcpKeepAlive(SbSocket socket, bool value, int64_t period) {
  bool result = sbposix::SetBooleanSocketOption(
      socket, SOL_SOCKET, SO_KEEPALIVE, "SO_KEEPALIVE", value);
  if (!result) {
    return false;
  }

  // For reference:
  // https://stackoverflow.com/questions/15860127/how-to-configure-tcp-keepalive-under-mac-os-x
  int period_seconds = static_cast<int>(period / 1000000);
  return sbposix::SetIntegerSocketOption(socket, IPPROTO_TCP, TCP_KEEPALIVE,
                                         "TCP_KEEPALIVE", period_seconds);
}
