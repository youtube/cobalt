// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#if SB_HAS_IPV6

#include <string.h>

#include "starboard/common/log.h"

namespace {
bool ProbeIpv6Support() {
  SbSocket probe_socket = SbSocketCreate(kSbSocketAddressTypeIpv6, kSbSocketProtocolTcp);
  if (!SbSocketIsValid(probe_socket)) {
    SB_LOG(WARNING) << "IPv6 Probe: Failed to create socket.";
    return false;
  }

  // Set reuse address to mirror original behavior
  if (!SbSocketSetReuseAddress(probe_socket, true)) {
    SB_LOG(WARNING) << "IPv6 Probe: Failed to set reuse address.";
  }

  SbSocketAddress probe_address; memset(&probe_address, 0, sizeof(probe_address));
  probe_address.type = kSbSocketAddressTypeIpv6;

  SbSocketError bind_status = SbSocketBind(probe_socket, &probe_address);
  
  if (bind_status != kSbSocketOk) {
    SB_LOG(WARNING) << "IPv6 Probe: Failed to bind test socket. status=" << bind_status;
  }

  SbSocketDestroy(probe_socket);

  return (bind_status == kSbSocketOk);
}
}  // namespace

#endif  // SB_HAS_IPV6

bool SbSocketIsIpv6Supported() {
#if SB_HAS_IPV6
  return ProbeIpv6Support();
#else
  return false;
#endif
}
