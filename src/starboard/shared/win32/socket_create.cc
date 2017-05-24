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

#include <winsock2.h>
#include <mswsock.h>

#include "starboard/log.h"
#include "starboard/shared/win32/set_non_blocking_internal.h"
#include "starboard/shared/win32/socket_internal.h"

namespace sbwin32 = starboard::shared::win32;

SbSocket SbSocketCreate(SbSocketAddressType address_type,
                        SbSocketProtocol protocol) {
  int socket_domain;
  switch (address_type) {
    case kSbSocketAddressTypeIpv4:
      socket_domain = AF_INET;
      break;
    case kSbSocketAddressTypeIpv6:
      socket_domain = AF_INET6;
      break;
    default:
      SB_NOTREACHED();
      return kSbSocketInvalid;
  }

  int socket_type;
  int socket_protocol;
  switch (protocol) {
    case kSbSocketProtocolTcp:
      socket_type = SOCK_STREAM;
      socket_protocol = IPPROTO_TCP;
      break;
    case kSbSocketProtocolUdp:
      socket_type = SOCK_DGRAM;
      socket_protocol = IPPROTO_UDP;
      break;
    default:
      SB_NOTREACHED();
      return kSbSocketInvalid;
  }

  // WSASocket with dwFlags=0, instead of socket() creates sockets that do not
  // support overlapped IO.
  SOCKET socket_handle =
      WSASocketW(socket_domain, socket_type, socket_protocol, nullptr, 0, 0);
  if (socket_handle == INVALID_SOCKET) {
    return kSbSocketInvalid;
  }

  // From
  // https://msdn.microsoft.com/en-us/library/windows/desktop/cc136103(v=vs.85).aspx:
  // "When the TargetOsVersion member is set to a value for Windows Vista or
  // later, reductions to the TCP receive buffer size on this socket using the
  // SO_RCVBUF socket option are allowed even after a TCP connection has been
  // establishment."
  // "When the TargetOsVersion member is set to a value for Windows Vista or
  // later, receive window auto-tuning is enabled and the TCP window scale
  // factor is reduced to 2 from the default value of 8."

  // The main impetus for this change:

  // "The WsaBehaviorAutoTuning option is needed on Windows Vista for some
  // Internet gateway devices and firewalls that do not correctly support data
  // flows for TCP connections that use the WSopt extension and a windows scale
  // factor. On Windows Vista, a receiver by default negotiates a window scale
  // factor of 8 for a maximum true window size of 16,776,960 bytes. When data
  // begins to flow on a fast link, Windows initially starts with a 64 Kilobyte
  // true window size by setting the Window field of the TCP header to 256 and
  // setting the window scale factor to 8 in the TCP options (256*2^8=64KB).
  // Some Internet gateway devices and firewalls ignore the window scale factor
  // and only look at the advertised Window field in the TCP header specified as
  // 256, and drop incoming packets for the connection that contain more than
  // 256 bytes of TCP data. To support TCP receive window scaling, a gateway
  // device or firewall must monitor the TCP handshake and track the negotiated
  // window scale factor as part of the TCP connection data. Also some
  // applications and TCP stack implementations on other platforms ignore the
  // TCP WSopt extension and the window scaling factor. So the remote host
  // sending the data may send data at the rate advertised in the Window field
  // of the TCP header (256 bytes). This can result in data being received very
  // slowly by the receiver."

  if (protocol == kSbSocketProtocolTcp) {
    WSA_COMPATIBILITY_MODE compatibility_mode = {WsaBehaviorAll, NTDDI_VISTA};

    DWORD kZero = 0;
    int return_value = WSAIoctl(
        socket_handle, SIO_SET_COMPATIBILITY_MODE, &compatibility_mode,
        sizeof(WSA_COMPATIBILITY_MODE), nullptr, 0, &kZero, nullptr, nullptr);
    if (return_value == SOCKET_ERROR) {
      closesocket(socket_handle);
      return kSbSocketInvalid;
    }
  }

  // All Starboard sockets are non-blocking, so let's ensure it.
  if (!sbwin32::SetNonBlocking(socket_handle)) {
    // Something went wrong, we'll clean up and return failure.
    closesocket(socket_handle);
    return kSbSocketInvalid;
  }

  return new SbSocketPrivate(address_type, protocol, socket_handle,
                             SbSocketPrivate::BindTarget::kUnbound);
}
