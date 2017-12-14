---
layout: doc
title: "Starboard Module Reference: socket.h"
---

Defines Starboard socket I/O functions. Starboard supports IPv4 and IPv6, TCP
and UDP, server and client sockets. Some platforms may not support IPv6, some
may not support listening sockets, and some may not support any kind of sockets
at all (or only allow them in debug builds).

Starboard ONLY supports non-blocking socket I/O, so all sockets are non-blocking
at creation time.

Note that, on some platforms, API calls on one end of a socket connection may
not be instantaneously aware of manipulations on the socket at the other end of
the connection, thus requiring use of either an SbSocketWaiter or spin-polling.

TODO: For platforms that do not support sockets at all, they must support at
least a high-level HTTP client API (to be defined later).

## Macros ##

### kSbSocketInvalid ###

Well-defined value for an invalid socket handle.

## Enums ##

### SbSocketAddressType ###

All possible address types.

#### Values ####

*   `kSbSocketAddressTypeIpv4`

    An IPv4 address, using only the first 4 entries of the address buffer.
*   `kSbSocketAddressTypeIpv6`

    An IPv6 address, which uses 16 entries of the address buffer.

### SbSocketError ###

Enumeration of all Starboard socket operation results. Despite the enum name,
note that the value actually describes the outcome of an operation, which is not
always an error.

#### Values ####

*   `kSbSocketOk`

    The operation succeeded.
*   `kSbSocketPending`

    The operation is blocked on I/O. Either try again "later," or be very clever
    and wait on it with a SbSocketWaiter.
*   `kSbSocketErrorFailed`

    The operation failed for some reason.

    TODO: It's unclear if we actually care about why, so leaving the rest of
    this enumeration blank until it becomes clear that we do.

### SbSocketProtocol ###

All possible IP socket types.

#### Values ####

*   `kSbSocketProtocolTcp`

    The TCP/IP protocol, a reliable, stateful, streaming protocol.
*   `kSbSocketProtocolUdp`

    The UDP/IP protocol, an unreliable, connectionless, discrete packet
    (datagram) protocol.

### SbSocketResolveFilter ###

Bits that can be set when calling SbSocketResolve to filter the results.

#### Values ####

*   `kSbSocketResolveFilterNone`

    No filters, include everything.
*   `kSbSocketResolveFilterIpv4`

    Include Ipv4 addresses.
*   `kSbSocketResolveFilterIpv6`

    Include Ipv6 addresses.

## Typedefs ##

### SbSocket ###

A handle to a socket.

#### Definition ####

```
typedef SbSocketPrivate* SbSocket
```

## Structs ##

### SbSocketAddress ###

A representation of any possible supported address type.

#### Members ####

*   `uint8_t address`

    The storage for the address. For IPv4, only the first 4 bytes make up the
    address. For IPv6, the entire buffer of 16 bytes is meaningful. An address
    of all zeros means that the address is unspecified.
*   `SbSocketAddressType type`

    The type of address this represents (IPv4 vs IPv6).
*   `int port`

    The port component of this socket address. If not specified, it will be
    zero, which is officially undefined.

### SbSocketResolution ###

The result of a host name resolution.

#### Members ####

*   `SbSocketAddress* addresses`

    An array of addresses associated with the host name.
*   `int address_count`

    The length of the `addresses` array.

## Functions ##

### SbSocketAccept ###

Accepts a pending connection on `socket` and returns a new SbSocket representing
that connection. This function sets the error on `socket` and returns
`kSbSocketInvalid` if it is unable to accept a new connection.

`socket`: The SbSocket that is accepting a pending connection.

#### Declaration ####

```
SbSocket SbSocketAccept(SbSocket socket)
```

### SbSocketBind ###

Binds `socket` to a specific local interface and port specified by
`local_address`. This function sets and returns the socket error if it is unable
to bind to `local_address`.

`socket`: The SbSocket to be bound to the local interface. `local_address`: The
local address to which the socket is to be bound. This value must not be `NULL`.

*   Setting the local address to port `0` (or not specifying a port) indicates
    that the function should choose a port for you.

*   Setting the IP address to `0.0.0.0` means that the socket should be bound to
    all interfaces.

#### Declaration ####

```
SbSocketError SbSocketBind(SbSocket socket, const SbSocketAddress *local_address)
```

### SbSocketClearLastError ###

Clears the last error set on `socket`. The return value indicates whether the
socket error was cleared.

#### Declaration ####

```
bool SbSocketClearLastError(SbSocket socket)
```

### SbSocketConnect ###

Opens a connection of `socket`'s type to the host and port specified by
`address`. This function sets and returns the socket error if it is unable to
connect to `address`. (It returns `kSbSocketOk` if it creates the connection
successfully.)

`socket`: The type of connection that should be opened. `address`: The host and
port to which the socket should connect.

#### Declaration ####

```
SbSocketError SbSocketConnect(SbSocket socket, const SbSocketAddress *address)
```

### SbSocketCreate ###

Creates a new non-blocking socket for protocol `protocol` using address family
`address_type`.

*   If successful, this function returns the newly created handle.

*   If unsuccessful, this function returns `kSbSocketInvalid` and also sets the
    last system error appropriately.

`address_type`: The type of IP address to use for the socket. `protocol`: The
protocol to use for the socket.

#### Declaration ####

```
SbSocket SbSocketCreate(SbSocketAddressType address_type, SbSocketProtocol protocol)
```

### SbSocketDestroy ###

Destroys the `socket` by flushing it, closing any connection that may be active
on it, and reclaiming any resources associated with it, including any
registration with an `SbSocketWaiter`.

The return value indicates whether the destruction was successful. However, even
if this function returns `false`, you should not be able to use the socket any
more.

`socket`: The SbSocket to be destroyed.

#### Declaration ####

```
bool SbSocketDestroy(SbSocket socket)
```

### SbSocketFreeResolution ###

Frees a resolution allocated by SbSocketResolve.

`resolution`: The resolution to be freed.

#### Declaration ####

```
void SbSocketFreeResolution(SbSocketResolution *resolution)
```

### SbSocketGetInterfaceAddress ###

Gets the source address and the netmask that would be used to connect to the
destination. The netmask parameter is optional, and only populated if a non-NULL
parameter is passed in. To determine which source IP will be used, the kernel
takes into account the protocol, routes, destination ip, etc. The subnet mask,
aka netmask, is used to find the routing prefix. In IPv6, this should be derived
from the prefix value.

Returns whether it was possible to determine the source address and the netmask
(if non-NULL value is passed) to be used to connect to the destination. This
function could fail if the destination is not reachable, if it an invalid
address, etc.

`destination`: The destination IP to be connected to. If IP addresses is not
0.0.0.0 or ::, then temporary addresses may be returned.

If the destination address is 0.0.0.0, and its `type` is
`kSbSocketAddressTypeIpv4`, then any IPv4 local interface that is up and not a
loopback interface is a valid return value.

If the destination address is ::, and its `type` is `kSbSocketAddressTypeIpv6`
then any IPv6 local interface that is up and not loopback or a link-local IP is
a valid return value. However, in the case of IPv6, the address with the biggest
scope must be returned. E.g., a globally scoped and routable IP is prefered over
a unique local address (ULA). Also, the IP address that is returned must be
permanent.

If destination address is NULL, then any IP address that is valid for
`destination` set to 0.0.0.0 (IPv4) or :: (IPv6) can be returned.

`out_source_address`: This function places the address of the local interface in
this output variable. `out_netmask`: This parameter is optional. If a non-NULL
value is passed in, this function places the netmask associated with the source
address in this output variable.

#### Declaration ####

```
bool SbSocketGetInterfaceAddress(const SbSocketAddress *const destination, SbSocketAddress *out_source_address, SbSocketAddress *out_netmask)
```

### SbSocketGetLastError ###

Returns the last error set on `socket`. If `socket` is not valid, this function
returns `kSbSocketErrorFailed`.

`socket`: The SbSocket that the last error is returned for.

#### Declaration ####

```
SbSocketError SbSocketGetLastError(SbSocket socket)
```

### SbSocketGetLocalAddress ###

Gets the address that this socket is bound to locally, if the socket is
connected. The return value indicates whether the address was retrieved
successfully.

`socket`: The SbSocket for which the local address is retrieved. `out_address`:
The SbSocket's local address.

#### Declaration ####

```
bool SbSocketGetLocalAddress(SbSocket socket, SbSocketAddress *out_address)
```

### SbSocketIsConnected ###

Indicates whether `socket` is connected to anything. Invalid sockets are not
connected.

`socket`: The SbSocket to be checked.

#### Declaration ####

```
bool SbSocketIsConnected(SbSocket socket)
```

### SbSocketIsConnectedAndIdle ###

Returns whether `socket` is connected to anything, and, if so, whether it is
receiving any data.

`socket`: The SbSocket to be checked.

#### Declaration ####

```
bool SbSocketIsConnectedAndIdle(SbSocket socket)
```

### SbSocketIsValid ###

Returns whether the given socket handle is valid.

#### Declaration ####

```
static bool SbSocketIsValid(SbSocket socket)
```

### SbSocketJoinMulticastGroup ###

Joins `socket` to an IP multicast group identified by `address`. The equivalent
of IP_ADD_MEMBERSHIP. The return value indicates whether the socket was joined
to the group successfully.

`socket`: The SbSocket to be joined to the IP multicast group. `address`: The
location of the IP multicast group.

#### Declaration ####

```
bool SbSocketJoinMulticastGroup(SbSocket socket, const SbSocketAddress *address)
```

### SbSocketListen ###

Causes `socket` to listen on the local address that `socket` was previously
bound to by SbSocketBind. This function sets and returns the socket error if it
is unable to listen for some reason. (It returns `kSbSocketOk` if it creates the
connection successfully.)

`socket`: The SbSocket on which the function operates.

#### Declaration ####

```
SbSocketError SbSocketListen(SbSocket socket)
```

### SbSocketReceiveFrom ###

Reads up to `data_size` bytes from `socket` into `out_data` and places the
source address of the packet in `out_source` if out_source is not NULL. Returns
the number of bytes read, or a negative number if there is an error, in which
case SbSocketGetLastError can provide the precise error encountered.

Note that this function is NOT specified to make a best effort to read all data
on all platforms, but it MAY still do so. It is specified to read however many
bytes are available conveniently, meaning that it should avoid blocking until
there is data. It can be run in a loop until SbSocketGetLastError returns
`kSbSocketPending` to make it a best-effort read (but still only up to not
blocking, unless you want to spin).

The primary use of `out_source` is to receive datagram packets from multiple
sources on a UDP server socket. TCP has two endpoints connected persistently, so
the address is unnecessary, but allowed.

`socket`: The SbSocket from which data is read. `out_data`: The data read from
the socket. `data_size`: The number of bytes to read. `out_source`: The source
address of the packet.

#### Declaration ####

```
int SbSocketReceiveFrom(SbSocket socket, char *out_data, int data_size, SbSocketAddress *out_source)
```

### SbSocketResolve ###

Synchronously resolves `hostname` into the returned SbSocketResolution , which
must be freed with SbSocketFreeResolution. The function returns `NULL` if it is
unable to resolve `hostname`.

`hostname`: The hostname to be resolved. `filters`: A mask of
SbSocketResolveFilter values used to filter the resolution. If `filters` does
not specify an IP address family filter, all address families are included.
However, if one IP address family filter is specified, only that address family
is included. The function ignores unrecognized filter bits.

#### Declaration ####

```
SbSocketResolution* SbSocketResolve(const char *hostname, int filters)
```

### SbSocketSendTo ###

Writes up to `data_size` bytes of `data` to `destination` via `socket`. Returns
the number of bytes written, or a negative number if there is an error, in which
case `SbSocketGetLastError` can provide the precise error encountered.

Note that this function is NOT specified to make a best effort to write all data
on all platforms, but it MAY still do so. It is specified to write however many
bytes are available conveniently. It can be run in a loop until
SbSocketGetLastError returns `kSbSocketPending` to make it a best-effort write
(but still only up to not blocking, unless you want to spin).

`socket`: The SbSocket to use to write data. `data`: The data read from the
socket. `data_size`: The number of bytes of `data` to write. `destination`: The
location to which data is written. This value must be `NULL` for TCP
connections, which can only have a single endpoint.

The primary use of `destination` is to send datagram packets, which can go out
to multiple sources from a single UDP server socket. TCP has two endpoints
connected persistently, so setting `destination` when sending to a TCP socket
will cause an error.

#### Declaration ####

```
int SbSocketSendTo(SbSocket socket, const char *data, int data_size, const SbSocketAddress *destination)
```

### SbSocketSetBroadcast ###

Sets the `SO_BROADCAST`, or equivalent, option to `value` on `socket`. The
return value indicates whether the option was actually set.

This option is only meaningful for UDP sockets and allows the socket to send to
the broadcast address.

`socket`: The SbSocket for which the option is set. `value`: The new value for
the option.

#### Declaration ####

```
bool SbSocketSetBroadcast(SbSocket socket, bool value)
```

### SbSocketSetReceiveBufferSize ###

Sets the `SO_RCVBUF`, or equivalent, option to `size` on `socket`. The return
value indicates whether the option was actually set.

`socket`: The SbSocket for which the option is set. `size`: The value for the
option.

#### Declaration ####

```
bool SbSocketSetReceiveBufferSize(SbSocket socket, int32_t size)
```

### SbSocketSetReuseAddress ###

Sets the `SO_REUSEADDR`, or equivalent, option to `value` on `socket`. The
return value indicates whether the option was actually set.

This option allows a bound address to be reused if a socket isn't actively bound
to it.

`socket`: The SbSocket for which the option is set. `value`: The new value for
the option.

#### Declaration ####

```
bool SbSocketSetReuseAddress(SbSocket socket, bool value)
```

### SbSocketSetSendBufferSize ###

Sets the `SO_SNDBUF`, or equivalent, option to `size` on `socket`. The return
value indicates whether the option was actually set.

`socket`: The SbSocket for which the option is set. `size`: The value for the
option.

#### Declaration ####

```
bool SbSocketSetSendBufferSize(SbSocket socket, int32_t size)
```

### SbSocketSetTcpKeepAlive ###

Sets the `SO_KEEPALIVE`, or equivalent, option to `value` on `socket`. The
return value indicates whether the option was actually set.

`socket`: The SbSocket for which the option is set. `value`: If set to `true`,
then `period` specifies the minimum time (SbTime) is always in microseconds)
between keep-alive packets. If set to `false`, `period` is ignored. `period`:
The time between keep-alive packets. This value is only relevant if `value` is
`true`.

#### Declaration ####

```
bool SbSocketSetTcpKeepAlive(SbSocket socket, bool value, SbTime period)
```

### SbSocketSetTcpNoDelay ###

Sets the `TCP_NODELAY`, or equivalent, option to `value` on `socket`. The return
value indicates whether the option was actually set.

This function disables the Nagle algorithm for reducing the number of packets
sent when converting from a stream to packets. Disabling Nagle generally puts
the data for each Send call into its own packet, but does not guarantee that
behavior.

`socket`: The SbSocket for which the option is set. `value`: Indicates whether
the Nagle algorithm should be disabled (`value`=`true`).

#### Declaration ####

```
bool SbSocketSetTcpNoDelay(SbSocket socket, bool value)
```

### SbSocketSetTcpWindowScaling ###

Sets the `SO_WINSCALE`, or equivalent, option to `value` on `socket`. The return
value indicates whether the option was actually set.

`socket`: The SbSocket for which the option is set. `value`: The value for the
option.

#### Declaration ####

```
bool SbSocketSetTcpWindowScaling(SbSocket socket, bool value)
```

