// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

// Module Overview: Starboard Socket module
//
// Defines Starboard socket I/O functions. Starboard supports IPv4 and IPv6,
// TCP and UDP, server and client sockets. Some platforms may not support IPv6,
// some may not support listening sockets, and some may not support any kind
// of sockets at all (or only allow them in debug builds).
//
// Starboard ONLY supports non-blocking socket I/O, so all sockets are
// non-blocking at creation time.
//
// Note that, on some platforms, API calls on one end of a socket connection
// may not be instantaneously aware of manipulations on the socket at the other
// end of the connection, thus requiring use of either an SbSocketWaiter or
// spin-polling.
//
// TODO: For platforms that do not support sockets at all, they must
// support at least a high-level HTTP client API (to be defined later).

#ifndef STARBOARD_SOCKET_H_
#define STARBOARD_SOCKET_H_

#include "starboard/export.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Private structure representing a socket, which may or may not be connected or
// listening.
typedef struct SbSocketPrivate SbSocketPrivate;

// A handle to a socket.
typedef SbSocketPrivate* SbSocket;

// Enumeration of all Starboard socket operation results. Despite the enum
// name, note that the value actually describes the outcome of an operation,
// which is not always an error.
typedef enum SbSocketError {
  // The operation succeeded.
  kSbSocketOk = 0,

  // The operation is blocked on I/O. Either try again "later," or be very
  // clever and wait on it with a SbSocketWaiter.
  kSbSocketPending,

  // This socket error is generated when the connection is reset unexpectedly
  // and the connection is now invalid.
  // This might happen for example if an read packet has the "TCP RST" bit set.
  kSbSocketErrorConnectionReset,

  // The operation failed for some other reason not specified above.
  kSbSocketErrorFailed,
} SbSocketError;

// All possible IP socket types.
typedef enum SbSocketProtocol {
  // The TCP/IP protocol, a reliable, stateful, streaming protocol.
  kSbSocketProtocolTcp,

  // The UDP/IP protocol, an unreliable, connectionless, discrete packet
  // (datagram) protocol.
  kSbSocketProtocolUdp,
} SbSocketProtocol;

// All possible address types.
typedef enum SbSocketAddressType {
  // An IPv4 address, using only the first 4 entries of the address buffer.
  kSbSocketAddressTypeIpv4,

  // An IPv6 address, which uses 16 entries of the address buffer.
  kSbSocketAddressTypeIpv6,
} SbSocketAddressType;

// A representation of any possible supported address type.
typedef struct SbSocketAddress {
  // The storage for the address. For IPv4, only the first 4 bytes make up the
  // address. For IPv6, the entire buffer of 16 bytes is meaningful. An address
  // of all zeros means that the address is unspecified.
  uint8_t address[16];

  // The type of address this represents (IPv4 vs IPv6).
  SbSocketAddressType type;

  // The port component of this socket address. If not specified, it will be
  // zero, which is officially undefined.
  int port;
} SbSocketAddress;

// The result of a host name resolution.
typedef struct SbSocketResolution {
  // An array of addresses associated with the host name.
  SbSocketAddress* addresses;

  // The length of the |addresses| array.
  int address_count;
} SbSocketResolution;

// Well-defined value for an invalid socket handle.
#define kSbSocketInvalid ((SbSocket)NULL)

// Returns whether the given socket handle is valid.
static inline bool SbSocketIsValid(SbSocket socket) {
  return socket != kSbSocketInvalid;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SOCKET_H_
