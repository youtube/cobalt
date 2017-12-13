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

// Enumeration of all Starboard socket operation results. Despite the enum
// name, note that the value actually describes the outcome of an operation,
// which is not always an error.
typedef enum SbSocketError {
  // The operation succeeded.
  kSbSocketOk = 0,

  // The operation is blocked on I/O. Either try again "later," or be very
  // clever and wait on it with a SbSocketWaiter.
  kSbSocketPending,

#if SB_API_VERSION >= SB_ADDITIONAL_SOCKET_CONNECTION_ERRORS_API_VERSION
  // This socket error is generated when the connection is reset unexpectedly
  // and the connection is now invalid.
  // This might happen for example if an read packet has the "TCP RST" bit set.
  kSbSocketErrorConnectionReset,
#endif  // SB_API_VERSION >= SB_ADDITIONAL_SOCKET_CONNECTION_ERRORS_API_VERSION

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
// family |address_type|.
//
// - If successful, this function returns the newly created handle.
// - If unsuccessful, this function returns |kSbSocketInvalid| and also sets
//   the last system error appropriately.
//
// |address_type|: The type of IP address to use for the socket.
// |protocol|: The protocol to use for the socket.
SB_EXPORT SbSocket SbSocketCreate(SbSocketAddressType address_type,
                                  SbSocketProtocol protocol);

// Destroys the |socket| by flushing it, closing any connection that may be
// active on it, and reclaiming any resources associated with it, including
// any registration with an |SbSocketWaiter|.
//
// The return value indicates whether the destruction was successful. However,
// even if this function returns |false|, you should not be able to use the
// socket any more.
//
// |socket|: The SbSocket to be destroyed.
SB_EXPORT bool SbSocketDestroy(SbSocket socket);

// Opens a connection of |socket|'s type to the host and port specified by
// |address|. This function sets and returns the socket error if it is unable
// to connect to |address|. (It returns |kSbSocketOk| if it creates the
// connection successfully.)
//
// |socket|: The type of connection that should be opened.
// |address|: The host and port to which the socket should connect.
SB_EXPORT SbSocketError SbSocketConnect(SbSocket socket,
                                        const SbSocketAddress* address);

// Binds |socket| to a specific local interface and port specified by
// |local_address|. This function sets and returns the socket error if it
// is unable to bind to |local_address|.
//
// |socket|: The SbSocket to be bound to the local interface.
// |local_address|: The local address to which the socket is to be bound.
//   This value must not be |NULL|.
// - Setting the local address to port |0| (or not specifying a port) indicates
//   that the function should choose a port for you.
// - Setting the IP address to |0.0.0.0| means that the socket should be bound
//   to all interfaces.
SB_EXPORT SbSocketError SbSocketBind(SbSocket socket,
                                     const SbSocketAddress* local_address);

// Causes |socket| to listen on the local address that |socket| was previously
// bound to by SbSocketBind. This function sets and returns the socket error if
// it is unable to listen for some reason. (It returns |kSbSocketOk| if it
// creates the connection successfully.)
//
// |socket|: The SbSocket on which the function operates.
SB_EXPORT SbSocketError SbSocketListen(SbSocket socket);

// Accepts a pending connection on |socket| and returns a new SbSocket
// representing that connection. This function sets the error on |socket|
// and returns |kSbSocketInvalid| if it is unable to accept a new connection.
//
// |socket|: The SbSocket that is accepting a pending connection.
SB_EXPORT SbSocket SbSocketAccept(SbSocket socket);

// Indicates whether |socket| is connected to anything. Invalid sockets are not
// connected.
//
// |socket|: The SbSocket to be checked.
SB_EXPORT bool SbSocketIsConnected(SbSocket socket);

// Returns whether |socket| is connected to anything, and, if so, whether it is
// receiving any data.
//
// |socket|: The SbSocket to be checked.
SB_EXPORT bool SbSocketIsConnectedAndIdle(SbSocket socket);

// Returns the last error set on |socket|. If |socket| is not valid, this
// function returns |kSbSocketErrorFailed|.
//
// |socket|: The SbSocket that the last error is returned for.
SB_EXPORT SbSocketError SbSocketGetLastError(SbSocket socket);

// Clears the last error set on |socket|. The return value indicates whether
// the socket error was cleared.
SB_EXPORT bool SbSocketClearLastError(SbSocket socket);

// Gets the address that this socket is bound to locally, if the socket is
// connected. The return value indicates whether the address was retrieved
// successfully.
//
// |socket|: The SbSocket for which the local address is retrieved.
// |out_address|: The SbSocket's local address.
SB_EXPORT bool SbSocketGetLocalAddress(SbSocket socket,
                                       SbSocketAddress* out_address);

// Gets the source address and the netmask that would be used to connect to the
// destination.  The netmask parameter is optional, and only populated if a
// non-NULL parameter is passed in.  To determine which source IP will be used,
// the kernel takes into account the protocol, routes, destination
// ip, etc.  The subnet mask, aka netmask, is used to find the routing prefix.
// In IPv6, this should be derived from the prefix value.
//
// Returns whether it was possible to determine the source address and the
// netmask (if non-NULL value is passed) to be used to connect to the
// destination.  This function could fail if the destination is not reachable,
// if it an invalid address, etc.
//
// |destination|: The destination IP to be connected to.  If IP addresses is not
// 0.0.0.0 or ::, then temporary addresses may be returned.
//
// If the destination address is 0.0.0.0, and its |type| is
// |kSbSocketAddressTypeIpv4|, then any IPv4 local interface that is up and not
// a loopback interface is a valid return value.
//
// If the destination address is ::, and its |type| is
// |kSbSocketAddressTypeIpv6| then any IPv6 local interface that is up and not
// loopback or a link-local IP is a valid return value.  However, in the case of
// IPv6, the address with the biggest scope must be returned.  E.g., a globally
// scoped and routable IP is prefered over a unique local address (ULA).  Also,
// the IP address that is returned must be permanent.
//
// If destination address is NULL, then any IP address that is valid for
// |destination| set to 0.0.0.0 (IPv4) or :: (IPv6) can be returned.
//
// |out_source_address|: This function places the address of the local interface
// in this output variable.
// |out_netmask|: This parameter is optional.  If a non-NULL value is passed in,
// this function places the netmask associated with the source address in this
// output variable.
SB_EXPORT bool SbSocketGetInterfaceAddress(
    const SbSocketAddress* const destination,
    SbSocketAddress* out_source_address,
    SbSocketAddress* out_netmask);

// Reads up to |data_size| bytes from |socket| into |out_data| and places the
// source address of the packet in |out_source| if out_source is not NULL.
// Returns the number of bytes read, or a negative number if there is an error,
// in which case SbSocketGetLastError can provide the precise error encountered.
//
// Note that this function is NOT specified to make a best effort to read all
// data on all platforms, but it MAY still do so. It is specified to read
// however many bytes are available conveniently, meaning that it should avoid
// blocking until there is data. It can be run in a loop until
// SbSocketGetLastError returns |kSbSocketPending| to make it a best-effort
// read (but still only up to not blocking, unless you want to spin).
//
// The primary use of |out_source| is to receive datagram packets from
// multiple sources on a UDP server socket. TCP has two endpoints connected
// persistently, so the address is unnecessary, but allowed.
//
// |socket|: The SbSocket from which data is read.
// |out_data|: The data read from the socket.
// |data_size|: The number of bytes to read.
// |out_source|: The source address of the packet.
SB_EXPORT int SbSocketReceiveFrom(SbSocket socket,
                                  char* out_data,
                                  int data_size,
                                  SbSocketAddress* out_source);

// Writes up to |data_size| bytes of |data| to |destination| via
// |socket|. Returns the number of bytes written, or a negative number if
// there is an error, in which case |SbSocketGetLastError| can provide the
// precise error encountered.
//
// Note that this function is NOT specified to make a best effort to write all
// data on all platforms, but it MAY still do so. It is specified to write
// however many bytes are available conveniently. It can be run in a loop
// until SbSocketGetLastError returns |kSbSocketPending| to make it a
// best-effort write (but still only up to not blocking, unless you want to
// spin).
//
// |socket|: The SbSocket to use to write data.
// |data|: The data read from the socket.
// |data_size|: The number of bytes of |data| to write.
// |destination|: The location to which data is written. This value must be
//   |NULL| for TCP connections, which can only have a single endpoint.
//
//   The primary use of |destination| is to send datagram packets, which can
//   go out to multiple sources from a single UDP server socket. TCP has two
//   endpoints connected persistently, so setting |destination| when sending
//   to a TCP socket will cause an error.
SB_EXPORT int SbSocketSendTo(SbSocket socket,
                             const char* data,
                             int data_size,
                             const SbSocketAddress* destination);

// Sets the |SO_BROADCAST|, or equivalent, option to |value| on |socket|. The
// return value indicates whether the option was actually set.
//
// This option is only meaningful for UDP sockets and allows the socket to
// send to the broadcast address.
//
// |socket|: The SbSocket for which the option is set.
// |value|: The new value for the option.
SB_EXPORT bool SbSocketSetBroadcast(SbSocket socket, bool value);

// Sets the |SO_REUSEADDR|, or equivalent, option to |value| on |socket|.
// The return value indicates whether the option was actually set.
//
// This option allows a bound address to be reused if a socket isn't actively
// bound to it.
//
// |socket|: The SbSocket for which the option is set.
// |value|: The new value for the option.
SB_EXPORT bool SbSocketSetReuseAddress(SbSocket socket, bool value);

// Sets the |SO_RCVBUF|, or equivalent, option to |size| on |socket|. The
// return value indicates whether the option was actually set.
//
// |socket|: The SbSocket for which the option is set.
// |size|: The value for the option.
SB_EXPORT bool SbSocketSetReceiveBufferSize(SbSocket socket, int32_t size);

// Sets the |SO_SNDBUF|, or equivalent, option to |size| on |socket|. The
// return value indicates whether the option was actually set.
//
// |socket|: The SbSocket for which the option is set.
// |size|: The value for the option.
SB_EXPORT bool SbSocketSetSendBufferSize(SbSocket socket, int32_t size);

// Sets the |SO_KEEPALIVE|, or equivalent, option to |value| on |socket|. The
// return value indicates whether the option was actually set.
//
// |socket|: The SbSocket for which the option is set.
// |value|: If set to |true|, then |period| specifies the minimum time
//   (SbTime) is always in microseconds) between keep-alive packets. If
//   set to |false|, |period| is ignored.
// |period|: The time between keep-alive packets. This value is only relevant
//   if |value| is |true|.
SB_EXPORT bool SbSocketSetTcpKeepAlive(SbSocket socket,
                                       bool value,
                                       SbTime period);

// Sets the |TCP_NODELAY|, or equivalent, option to |value| on |socket|. The
// return value indicates whether the option was actually set.
//
// This function disables the Nagle algorithm for reducing the number of
// packets sent when converting from a stream to packets. Disabling Nagle
// generally puts the data for each Send call into its own packet, but does
// not guarantee that behavior.
//
// |socket|: The SbSocket for which the option is set.
// |value|: Indicates whether the Nagle algorithm should be disabled
//   (|value|=|true|).
SB_EXPORT bool SbSocketSetTcpNoDelay(SbSocket socket, bool value);

// Sets the |SO_WINSCALE|, or equivalent, option to |value| on |socket|. The
// return value indicates whether the option was actually set.
//
// |socket|: The SbSocket for which the option is set.
// |value|: The value for the option.
SB_EXPORT bool SbSocketSetTcpWindowScaling(SbSocket socket, bool value);

// Joins |socket| to an IP multicast group identified by |address|. The
// equivalent of IP_ADD_MEMBERSHIP. The return value indicates whether the
// socket was joined to the group successfully.
//
// |socket|: The SbSocket to be joined to the IP multicast group.
// |address|: The location of the IP multicast group.
SB_EXPORT bool SbSocketJoinMulticastGroup(SbSocket socket,
                                          const SbSocketAddress* address);

// Synchronously resolves |hostname| into the returned SbSocketResolution,
// which must be freed with SbSocketFreeResolution. The function returns
// |NULL| if it is unable to resolve |hostname|.
//
// |hostname|: The hostname to be resolved.
// |filters|: A mask of SbSocketResolveFilter values used to filter the
//   resolution. If |filters| does not specify an IP address family filter,
//   all address families are included. However, if one IP address family filter
//   is specified, only that address family is included. The function ignores
//   unrecognized filter bits.
SB_EXPORT SbSocketResolution* SbSocketResolve(const char* hostname,
                                              int filters);

// Frees a resolution allocated by SbSocketResolve.
//
// |resolution|: The resolution to be freed.
SB_EXPORT void SbSocketFreeResolution(SbSocketResolution* resolution);

#ifdef __cplusplus
}  // extern "C"
#endif

#ifdef __cplusplus
// An inline C++ wrapper to SbSocket.
class Socket {
 public:
  Socket(SbSocketAddressType address_type, SbSocketProtocol protocol)
      : socket_(SbSocketCreate(address_type, protocol)) {}
  explicit Socket(SbSocketAddressType address_type)
      : socket_(SbSocketCreate(address_type, kSbSocketProtocolTcp)) {}
  explicit Socket(SbSocketProtocol protocol)
      : socket_(SbSocketCreate(kSbSocketAddressTypeIpv4, protocol)) {}
  Socket()
      : socket_(
            SbSocketCreate(kSbSocketAddressTypeIpv4, kSbSocketProtocolTcp)) {}
  ~Socket() { SbSocketDestroy(socket_); }
  bool IsValid() { return SbSocketIsValid(socket_); }

  SbSocketError Connect(const SbSocketAddress* address) {
    return SbSocketConnect(socket_, address);
  }
  SbSocketError Bind(const SbSocketAddress* local_address) {
    return SbSocketBind(socket_, local_address);
  }
  SbSocketError Listen() { return SbSocketListen(socket_); }
  Socket* Accept() {
    SbSocket accepted_socket = SbSocketAccept(socket_);
    if (SbSocketIsValid(accepted_socket)) {
      return new Socket(accepted_socket);
    }

    return NULL;
  }

  bool IsConnected() { return SbSocketIsConnected(socket_); }
  bool IsConnectedAndIdle() { return SbSocketIsConnectedAndIdle(socket_); }

  bool IsPending() { return GetLastError() == kSbSocketPending; }
  SbSocketError GetLastError() { return SbSocketGetLastError(socket_); }
  void ClearLastError() { SbSocketClearLastError(socket_); }

  int ReceiveFrom(char* out_data, int data_size, SbSocketAddress* out_source) {
    return SbSocketReceiveFrom(socket_, out_data, data_size, out_source);
  }

  int SendTo(const char* data,
             int data_size,
             const SbSocketAddress* destination) {
    return SbSocketSendTo(socket_, data, data_size, destination);
  }

  bool GetLocalAddress(SbSocketAddress* out_address) {
    return SbSocketGetLocalAddress(socket_, out_address);
  }

  bool SetBroadcast(bool value) { return SbSocketSetBroadcast(socket_, value); }

  bool SetReuseAddress(bool value) {
    return SbSocketSetReuseAddress(socket_, value);
  }

  bool SetReceiveBufferSize(int32_t size) {
    return SbSocketSetReceiveBufferSize(socket_, size);
  }

  bool SetSendBufferSize(int32_t size) {
    return SbSocketSetSendBufferSize(socket_, size);
  }

  bool SetTcpKeepAlive(bool value, SbTime period) {
    return SbSocketSetTcpKeepAlive(socket_, value, period);
  }

  bool SetTcpNoDelay(bool value) {
    return SbSocketSetTcpNoDelay(socket_, value);
  }

  bool SetTcpWindowScaling(bool value) {
    return SbSocketSetTcpWindowScaling(socket_, value);
  }

  bool JoinMulticastGroup(const SbSocketAddress* address) {
    return SbSocketJoinMulticastGroup(socket_, address);
  }

  SbSocket socket() { return socket_; }

 private:
  explicit Socket(SbSocket socket) : socket_(socket) {}

  SbSocket socket_;
};
#endif

#ifdef __cplusplus
// Let SbSocketAddresses be output to log streams.
inline std::ostream& operator<<(std::ostream& os,
                                const SbSocketAddress& address) {
  if (address.type == kSbSocketAddressTypeIpv6) {
    os << std::hex << "[";
    const uint16_t* fields = reinterpret_cast<const uint16_t*>(address.address);
    int i = 0;
    while (fields[i] == 0) {
      if (i == 0) {
        os << ":";
      }
      ++i;
      if (i == 8) {
        os << ":";
      }
    }
    for (; i < 8; ++i) {
      if (i != 0) {
        os << ":";
      }
#if SB_IS(LITTLE_ENDIAN)
      const uint8_t* fields8 = reinterpret_cast<const uint8_t*>(&fields[i]);
      const uint16_t value = static_cast<uint16_t>(fields8[0]) * 256 |
                             static_cast<uint16_t>(fields8[1]);
      os << value;
#else
      os << fields[i];
#endif
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
