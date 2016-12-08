// Copyright 2015 Google Inc. All Rights Reserved.
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

// Starboard socket I/O. Starboard supports IPv4 and IPv6, TCP and UDP, server
// and client sockets. Some platforms may not support IPv6, some may not support
// listening sockets, and some may not support any kind of sockets at all (or
// only allow them in debug builds). Starboard ONLY supports non-blocking socket
// I/O, so all sockets are non-blocking at creation time.
// Note that, on some platforms, API calls on one end of a socket connection may
// not be instantaneously aware of manipulations on the socket at the other end
// of the connection, thus requiring use of either an SbSocketWaiter or
// spin-polling.
//
// TODO: For platforms that do not support sockets at all, they must
// support at least a high-level HTTP client API (to be defined later).

#ifndef STARBOARD_SOCKET_H_
#define STARBOARD_SOCKET_H_

#ifdef __cplusplus
#include <iomanip>
#include <iostream>
#endif

#include "starboard/export.h"
#include "starboard/time.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Private structure representing a socket, which may or may not be connected or
// listening.
typedef struct SbSocketPrivate SbSocketPrivate;

// A handle to a socket.
typedef SbSocketPrivate* SbSocket;

// Enumeration of all Starboard socket operation results.
typedef enum SbSocketError {
  // The operation succeeded. Yep.
  kSbSocketOk = 0,

  // The operation is blocked on I/O. Either try again "later," or be very
  // clever and wait on it with a SbSocketWaiter.
  kSbSocketPending,

  // The operation failed for some reason.
  //
  // TODO: It's unclear if we actually care about why, so leaving the rest
  // of this enumeration blank until it becomes clear that we do.
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

// Bits that can be set when calling SbSocketResolve to filter the results.
typedef enum SbSocketResolveFilter {
  // No filters, include everything.
  kSbSocketResolveFilterNone = 0,

  // Include Ipv4 addresses.
  kSbSocketResolveFilterIpv4 = 1 << 0,

  // Include Ipv6 addresses.
  kSbSocketResolveFilterIpv6 = 1 << 1,
} SbSocketResolveFilter;

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
static SB_C_INLINE bool SbSocketIsValid(SbSocket socket) {
  return socket != kSbSocketInvalid;
}

// Creates a new non-blocking socket for protocol |protocol| using address
// family |address_type|. Returns the newly created handle, if successful, or
// kSbSocketInvalid, if not. If unsuccessful, sets the last system error
// appropriately.
SB_EXPORT SbSocket SbSocketCreate(SbSocketAddressType address_type,
                                  SbSocketProtocol protocol);

// Destroys |socket|. Flushes the socket, closes any connection that may be
// active on |socket|, and reclaims any resources associated with |socket|,
// including any registration with an SbSocketWaiter. Returns whether the
// destruction was successful. Even if this function returns false, you
// shouldn't be able to use the socket anymore.
SB_EXPORT bool SbSocketDestroy(SbSocket socket);

// Opens a connection of |socket|'s type to the host and port specified by
// |address|. Sets and returns the socket error if unable to bind to
// |local_address|.
SB_EXPORT SbSocketError SbSocketConnect(SbSocket socket,
                                        const SbSocketAddress* address);

// Binds |socket| to a specific local interface and port specified by
// |local_address|, which must not be NULL. Port 0 means choose a port for me
// and IP address 0.0.0.0 means bind to all interfaces. Sets and returns the
// socket error if unable to bind to |local_address|.
SB_EXPORT SbSocketError SbSocketBind(SbSocket socket,
                                     const SbSocketAddress* local_address);

// Causes |socket| to listen on |local_address|. Sets and returns the socket
// error if unable to listen for some reason.
SB_EXPORT SbSocketError SbSocketListen(SbSocket socket);

// Accepts a pending connection on |socket|, returning a new SbSocket
// representing that connection. Sets the error on |socket| and returns
// kSbSocketInvalid if unable to accept a new connection.
SB_EXPORT SbSocket SbSocketAccept(SbSocket socket);

// Returns whether |socket| is connected to anything. Invalid sockets are not
// connected.
SB_EXPORT bool SbSocketIsConnected(SbSocket socket);

// Returns whether |socket| is connected to anything, and whether it is
// receiving any data.
SB_EXPORT bool SbSocketIsConnectedAndIdle(SbSocket socket);

// Returns the last error set on |socket|. If |socket| is not valid, always
// returns kSbSocketErrorFailed.
SB_EXPORT SbSocketError SbSocketGetLastError(SbSocket socket);

// Clears the last error set on |socket|. Returns whether the socket error was
// cleared.
SB_EXPORT bool SbSocketClearLastError(SbSocket socket);

// Gets the address that this socket is bound to locally, if the socket is
// connected. Returns whether getting the address was successful.
SB_EXPORT bool SbSocketGetLocalAddress(SbSocket socket,
                                       SbSocketAddress* out_address);

// Reads up to |data_size| bytes from |socket| into |out_data|. Also places the
// source address of the packet in |out_source|, if out_source is not NULL.
// Returns the number of bytes read, or a negative number on error.
// SbSocketGetLastError can provide the precise error encountered.
//
// Note that this function is NOT specified to (but MAY) make a best effort to
// read all data on all platforms, it just reads however many bytes are
// available conveniently. Can be run in a loop until pending to make it a
// best-effort read (but still only up to not blocking, unless you want to
// spin).
//
// The primary use of |out_source| is for receiving datagram packets from
// multiple sources on a UDP server socket. TCP has two endpoints connected
// persistently, so the address is unnecessary, but allowed.
SB_EXPORT int SbSocketReceiveFrom(SbSocket socket,
                                  char* out_data,
                                  int data_size,
                                  SbSocketAddress* out_source);

// Writes up to |data_size| bytes of |data| to |destination| via
// |socket|. Returns the number of bytes written, or a negative number on
// error. SbSocketGetLastError can provide the precise error encountered.
// |destination| must be NULL for TCP connections, which can only have a single
// endpoint.
//
// Note that this function is NOT specified to (but MAY) make a best effort to
// write all data on all platforms, it just writes however many bytes that are
// conveniently. Can be run in a loop until pending to make it a best-effort
// write (but still only up to not blocking, unless you want to spin).
//
// The primary use of |destination| is to send datagram packets, which can go
// out to multiple sources from a single UDP server socket. TCP has two
// endpoints connected persistently, so |destination| cannot be set when sending
// to a TCP socket -- it will cause an error.
SB_EXPORT int SbSocketSendTo(SbSocket socket,
                             const char* data,
                             int data_size,
                             const SbSocketAddress* destination);

// Sets the SO_BROADCAST, or equivalent, option to |value| on |socket|. Returns
// whether the option was actually set.
//
// This option is only meaningful for UDP sockets, and will allow the socket to
// send to the broadcast address.
SB_EXPORT bool SbSocketSetBroadcast(SbSocket socket, bool value);

// Sets the SO_REUSEADDR, or equivalent, option to |value| on |socket|. Returns
// whether the option was actually set.
//
// This option allows a bound address to be reused if a socket isn't actively
// bound to it.
SB_EXPORT bool SbSocketSetReuseAddress(SbSocket socket, bool value);

// Sets SO_RCVBUF, or equivalent, option to |size| on |socket|.
SB_EXPORT bool SbSocketSetReceiveBufferSize(SbSocket socket, int32_t size);

// Sets SO_SNDBUF, or equivalent, option to |size| on |socket|.
SB_EXPORT bool SbSocketSetSendBufferSize(SbSocket socket, int32_t size);

// Sets the SO_KEEPALIVE, or equivalent, option to |value| on |socket|. If
// |value| is true, then |period| specifies the minimum time (SbTime is always
// in microseconds) between keep-alive packets, otherwise |period| is
// ignored. Returns whether the option was actually set.
SB_EXPORT bool SbSocketSetTcpKeepAlive(SbSocket socket,
                                       bool value,
                                       SbTime period);

// Sets the TCP_NODELAY, or equivalent, option to |value| on |socket|.  Returns
// whether the option was actually set.
//
// This disables the Nagle algorithm for reducing the number of packets sent
// when converting from a stream to packets. Disabling Nagle will generally put
// the data for each Send call into its own packet, but does not guarantee that
// behavior.
SB_EXPORT bool SbSocketSetTcpNoDelay(SbSocket socket, bool value);

// Sets SO_WINSCALE, or equivalent, option to |value| on |socket|.
SB_EXPORT bool SbSocketSetTcpWindowScaling(SbSocket socket, bool value);

// Joins |socket| to an IP multicast group identified by |address|. The
// equivalent of IP_ADD_MEMBERSHIP.
SB_EXPORT bool SbSocketJoinMulticastGroup(SbSocket socket,
                                          const SbSocketAddress* address);

// Gets the address of the local IPv4 network interface. Does not include
// loopback (or IPv6) addresses.
SB_EXPORT bool SbSocketGetLocalInterfaceAddress(SbSocketAddress* out_address);

// Synchronously resolves |hostname| into an SbSocketResolution, filtered by
// |filters|, which is a mask of SbSocketResolveFilter values. If no IP address
// family filter is specified in |filters|, all address families will be
// included, but if one is specified, only that address familiy will be
// included. Unrecognized filter bits are ignored. Returns NULL if unable to
// resolve |hostname|. The returned SbSocketResolution must be freed with
// SbSocketFreeResolution.
SB_EXPORT SbSocketResolution* SbSocketResolve(const char* hostname,
                                              int filters);

// Frees a resolution allocated by SbSocketResolve.
SB_EXPORT void SbSocketFreeResolution(SbSocketResolution* resolution);

#ifdef __cplusplus
}  // extern "C"
#endif

#ifdef __cplusplus
// Let SbSocketAddresses be output to log streams.
inline std::ostream& operator<<(std::ostream& os,
                                const SbSocketAddress& address) {
  if (address.type == kSbSocketAddressTypeIpv6) {
    os << std::hex << "[";
    const uint16_t* fields = reinterpret_cast<const uint16_t*>(address.address);
    for (int i = 0; i < 8; ++i) {
      if (i != 0) {
        os << ":";
      }
      os << fields[i];
    }
    os << "]" << std::dec;
  } else {
    for (int i = 0; i < 4; ++i) {
      if (i != 0) {
        os << ".";
      }
      os << static_cast<int>(address.address[i]);
    }
  }
  os << ":" << address.port;
  return os;
}
#endif

#endif  // STARBOARD_SOCKET_H_
