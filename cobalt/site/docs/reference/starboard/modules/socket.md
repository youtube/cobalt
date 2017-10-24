---
layout: doc
title: "Starboard Module Reference: socket.h"
---

Defines Starboard socket I/O functions. Starboard supports IPv4 and IPv6,
TCP and UDP, server and client sockets. Some platforms may not support IPv6,
some may not support listening sockets, and some may not support any kind
of sockets at all (or only allow them in debug builds).<br>
Starboard ONLY supports non-blocking socket I/O, so all sockets are
non-blocking at creation time.<br>
Note that, on some platforms, API calls on one end of a socket connection
may not be instantaneously aware of manipulations on the socket at the other
end of the connection, thus requiring use of either an SbSocketWaiter or
spin-polling.<br>
TODO: For platforms that do not support sockets at all, they must
support at least a high-level HTTP client API (to be defined later).

## Enums

### SbSocketAddressType

All possible address types.

**Values**

*   `kSbSocketAddressTypeIpv4` - An IPv4 address, using only the first 4 entries of the address buffer.
*   `kSbSocketAddressTypeIpv6` - An IPv6 address, which uses 16 entries of the address buffer.

### SbSocketError

Enumeration of all Starboard socket operation results. Despite the enum
name, note that the value actually describes the outcome of an operation,
which is not always an error.

**Values**

*   `kSbSocketOk` - The operation succeeded.
*   `kSbSocketPending` - The operation is blocked on I/O. Either try again "later," or be veryclever and wait on it with a SbSocketWaiter.
*   `kSbSocketErrorFailed` - The operation failed for some reason.<br>TODO: It's unclear if we actually care about why, so leaving the restof this enumeration blank until it becomes clear that we do.

### SbSocketProtocol

All possible IP socket types.

**Values**

*   `kSbSocketProtocolTcp` - The TCP/IP protocol, a reliable, stateful, streaming protocol.
*   `kSbSocketProtocolUdp` - The UDP/IP protocol, an unreliable, connectionless, discrete packet(datagram) protocol.

### SbSocketResolveFilter

Bits that can be set when calling SbSocketResolve to filter the results.

**Values**

*   `kSbSocketResolveFilterNone` - No filters, include everything.
*   `kSbSocketResolveFilterIpv4` - Include Ipv4 addresses.
*   `kSbSocketResolveFilterIpv6` - Include Ipv6 addresses.

## Structs

### SbSocketAddress

A representation of any possible supported address type.

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>uint8_t</code><br>        <code>address[16]</code></td>    <td>The storage for the address. For IPv4, only the first 4 bytes make up the
address. For IPv6, the entire buffer of 16 bytes is meaningful. An address
of all zeros means that the address is unspecified.</td>  </tr>
  <tr>
    <td><code>SbSocketAddressType</code><br>        <code>type</code></td>    <td>The type of address this represents (IPv4 vs IPv6).</td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>port</code></td>    <td>The port component of this socket address. If not specified, it will be
zero, which is officially undefined.</td>  </tr>
</table>

### SbSocketPrivate

Private structure representing a socket, which may or may not be connected or
listening.

### SbSocketResolution

The result of a host name resolution.

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>int</code><br>        <code>address_count</code></td>    <td>The length of the <code>addresses</code> array.</td>  </tr>
</table>

## Functions

### SbSocketAccept

**Description**

Accepts a pending connection on `socket` and returns a new SbSocket
representing that connection. This function sets the error on `socket`
and returns `kSbSocketInvalid` if it is unable to accept a new connection.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketAccept-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketAccept-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketAccept-declaration">
<pre>
SB_EXPORT SbSocket SbSocketAccept(SbSocket socket);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketAccept-stub">

```
#include "starboard/socket.h"

SbSocket SbSocketAccept(SbSocket /*socket*/) {
  return kSbSocketInvalid;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocket</code><br>        <code>socket</code></td>
    <td>The SbSocket that is accepting a pending connection.</td>
  </tr>
</table>

### SbSocketBind

**Description**

Binds `socket` to a specific local interface and port specified by
`local_address`. This function sets and returns the socket error if it
is unable to bind to `local_address`.<br></li></ul>

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketBind-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketBind-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketBind-declaration">
<pre>
SB_EXPORT SbSocketError SbSocketBind(SbSocket socket,
                                     const SbSocketAddress* local_address);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketBind-stub">

```
#include "starboard/socket.h"

SbSocketError SbSocketBind(SbSocket /*socket*/,
                           const SbSocketAddress* /*local_address*/) {
  return kSbSocketErrorFailed;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocket</code><br>        <code>socket</code></td>
    <td>The SbSocket to be bound to the local interface.</td>
  </tr>
  <tr>
    <td><code>const SbSocketAddress*</code><br>        <code>local_address</code></td>
    <td>The local address to which the socket is to be bound.
This value must not be <code>NULL</code>.
<ul><li>Setting the local address to port <code>0</code> (or not specifying a port) indicates
that the function should choose a port for you.
</li><li>Setting the IP address to <code>0.0.0.0</code> means that the socket should be bound
to all interfaces.</td>
  </tr>
</table>

### SbSocketClearLastError

**Description**

Clears the last error set on `socket`. The return value indicates whether
the socket error was cleared.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketClearLastError-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketClearLastError-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketClearLastError-declaration">
<pre>
SB_EXPORT bool SbSocketClearLastError(SbSocket socket);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketClearLastError-stub">

```
#include "starboard/socket.h"

bool SbSocketClearLastError(SbSocket /*socket*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocket</code><br>
        <code>socket</code></td>
    <td> </td>
  </tr>
</table>

### SbSocketConnect

**Description**

Opens a connection of `socket`'s type to the host and port specified by
`address`. This function sets and returns the socket error if it is unable
to connect to `address`. (It returns `kSbSocketOk` if it creates the
connection successfully.)

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketConnect-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketConnect-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketConnect-declaration">
<pre>
SB_EXPORT SbSocketError SbSocketConnect(SbSocket socket,
                                        const SbSocketAddress* address);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketConnect-stub">

```
#include "starboard/socket.h"

SbSocketError SbSocketConnect(SbSocket /*socket*/,
                              const SbSocketAddress* /*address*/) {
  return kSbSocketErrorFailed;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocket</code><br>        <code>socket</code></td>
    <td>The type of connection that should be opened.</td>
  </tr>
  <tr>
    <td><code>const SbSocketAddress*</code><br>        <code>address</code></td>
    <td>The host and port to which the socket should connect.</td>
  </tr>
</table>

### SbSocketCreate

**Description**

Creates a new non-blocking socket for protocol `protocol` using address
family `address_type`.<br>
<ul><li>If successful, this function returns the newly created handle.
</li><li>If unsuccessful, this function returns `kSbSocketInvalid` and also sets
the last system error appropriately.</li></ul>

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketCreate-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketCreate-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketCreate-declaration">
<pre>
SB_EXPORT SbSocket SbSocketCreate(SbSocketAddressType address_type,
                                  SbSocketProtocol protocol);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketCreate-stub">

```
#include "starboard/socket.h"

SbSocket SbSocketCreate(SbSocketAddressType /*address_type*/,
                        SbSocketProtocol /*protocol*/) {
  return kSbSocketInvalid;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocketAddressType</code><br>        <code>address_type</code></td>
    <td>The type of IP address to use for the socket.</td>
  </tr>
  <tr>
    <td><code>SbSocketProtocol</code><br>        <code>protocol</code></td>
    <td>The protocol to use for the socket.</td>
  </tr>
</table>

### SbSocketDestroy

**Description**

Destroys the `socket` by flushing it, closing any connection that may be
active on it, and reclaiming any resources associated with it, including
any registration with an `SbSocketWaiter`.<br>
The return value indicates whether the destruction was successful. However,
even if this function returns `false`, you should not be able to use the
socket any more.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketDestroy-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketDestroy-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketDestroy-declaration">
<pre>
SB_EXPORT bool SbSocketDestroy(SbSocket socket);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketDestroy-stub">

```
#include "starboard/socket.h"

bool SbSocketDestroy(SbSocket /*socket*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocket</code><br>        <code>socket</code></td>
    <td>The SbSocket to be destroyed.</td>
  </tr>
</table>

### SbSocketFreeResolution

**Description**

Frees a resolution allocated by <code><a href="#sbsocketresolve">SbSocketResolve</a></code>.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketFreeResolution-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketFreeResolution-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketFreeResolution-declaration">
<pre>
SB_EXPORT void SbSocketFreeResolution(SbSocketResolution* resolution);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketFreeResolution-stub">

```
#include "starboard/socket.h"

void SbSocketFreeResolution(SbSocketResolution* /*resolution*/) {
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocketResolution*</code><br>        <code>resolution</code></td>
    <td>The resolution to be freed.</td>
  </tr>
</table>

### SbSocketGetInterfaceAddress

**Description**

Gets the source address and the netmask that would be used to connect to the
destination.  The netmask parameter is optional, and only populated if a
non-NULL parameter is passed in.  To determine which source IP will be used,
the kernel takes into account the protocol, routes, destination
ip, etc.  The subnet mask, aka netmask, is used to find the routing prefix.
In IPv6, this should be derived from the prefix value.<br>
Returns whether it was possible to determine the source address and the
netmask (if non-NULL value is passed) to be used to connect to the
destination.  This function could fail if the destination is not reachable,
if it an invalid address, etc.<br>
If the destination address is 0.0.0.0, and its `type` is
`kSbSocketAddressTypeIpv4`, then any IPv4 local interface that is up and not
a loopback interface is a valid return value.<br>
If the destination address is ::, and its `type` is
`kSbSocketAddressTypeIpv6` then any IPv6 local interface that is up and not
loopback or a link-local IP is a valid return value.  However, in the case of
IPv6, the address with the biggest scope must be returned.  E.g., a globally
scoped and routable IP is prefered over a unique local address (ULA).  Also,
the IP address that is returned must be permanent.<br>
If destination address is NULL, then any IP address that is valid for
`destination` set to 0.0.0.0 (IPv4) or :: (IPv6) can be returned.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketGetInterfaceAddress-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketGetInterfaceAddress-stub" class="mdl-tabs__tab">stub</a>
    <a href="#SbSocketGetInterfaceAddress-linux" class="mdl-tabs__tab">linux</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketGetInterfaceAddress-declaration">
<pre>
SB_EXPORT bool SbSocketGetInterfaceAddress(
    const SbSocketAddress* const destination,
    SbSocketAddress* out_source_address,
    SbSocketAddress* out_netmask);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketGetInterfaceAddress-stub">

```
#include "starboard/socket.h"

bool SbSocketGetInterfaceAddress(const SbSocketAddress* const /*destination*/,
                                 SbSocketAddress* /*out_source_address*/,
                                 SbSocketAddress* /*out_netmask*/) {
  return false;
}
```

  </div>
  <div class="mdl-tabs__panel" id="SbSocketGetInterfaceAddress-linux">

```
#include "starboard/socket.h"

// linux/if.h assumes the symbols for structs defined in ifaddrs.h are
// already present. These includes must be above <linux/if.h> below.
#include <arpa/inet.h>
#include <ifaddrs.h>

#if SB_HAS_QUIRK(SOCKET_BSD_HEADERS)
#include <errno.h>
#include <net/if.h>
#include <net/if_dl.h>
#else
#include <linux/if.h>
#include <linux/if_addr.h>
#include <netdb.h>
#endif

#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "starboard/byte_swap.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/shared/posix/socket_internal.h"

namespace sbposix = starboard::shared::posix;

namespace {

// TODO: Move this constant to socket.h.
const int kIPv6AddressSize = 16;

bool IsAnyAddress(const SbSocketAddress& address) {
  switch (address.type) {
    case kSbSocketAddressTypeIpv4:
      return (address.address[0] == 0 && address.address[1] == 0 &&
              address.address[2] == 0 && address.address[3] == 0);
#if SB_HAS(IPV6)
    case kSbSocketAddressTypeIpv6: {
      bool found_nonzero = false;
      for (std::size_t i = 0; i != kIPv6AddressSize; ++i) {
        found_nonzero |= (address.address[i] != 0);
      }
      return !found_nonzero;
    }
#endif
    default:
      SB_NOTREACHED() << "Invalid address type " << address.type;
      break;
  }

  return false;
}

template <typename T, int source_size>
void CopyIntoObjectFromArray(T* out_destination,
                             const unsigned char(&source)[source_size]) {
  SB_COMPILE_ASSERT(sizeof(T) <= source_size, destination_is_too_small);
  SbMemoryCopy(out_destination, source, sizeof(T));
}

bool GetPotentialMatch(const sockaddr* input_addr,
                       const in_addr** out_interface_addr) {
  if (!input_addr || input_addr->sa_family != AF_INET) {
    *out_interface_addr = NULL;
    return false;
  }

  const sockaddr_in* v4input_addr =
      reinterpret_cast<const sockaddr_in*>(input_addr);
  *out_interface_addr = &(v4input_addr->sin_addr);
  return true;
}

bool GetPotentialMatch(const sockaddr* input_addr,
                       const in6_addr** out_interface_addr) {
  if (!input_addr || input_addr->sa_family != AF_INET6) {
    *out_interface_addr = NULL;
    return false;
  }

  const sockaddr_in6* v6input_addr =
      reinterpret_cast<const sockaddr_in6*>(input_addr);
  *out_interface_addr = &(v6input_addr->sin6_addr);
  return true;
}

template <typename in_addr_type>
bool GetNetmaskForInterfaceAddress(const SbSocketAddress& interface_address,
                                   SbSocketAddress* out_netmask) {
  SB_DCHECK((interface_address.type == kSbSocketAddressTypeIpv4) ||
            (interface_address.type == kSbSocketAddressTypeIpv6));
  struct ifaddrs* interface_addrs = NULL;

  int retval = getifaddrs(&interface_addrs);
  if (retval != 0) {
    return false;
  }

  in_addr_type to_match;
  CopyIntoObjectFromArray(&to_match, interface_address.address);

  bool found_netmask = false;
  for (struct ifaddrs* interface = interface_addrs; interface != NULL;
       interface = interface->ifa_next) {
    if (!(IFF_UP & interface->ifa_flags) ||
        (IFF_LOOPBACK & interface->ifa_flags)) {
      continue;
    }

    const in_addr_type* potential_match;
    if (!GetPotentialMatch(interface->ifa_addr, &potential_match))
      continue;

    if (SbMemoryCompare(&to_match, potential_match, sizeof(in_addr_type)) !=
        0) {
      continue;
    }

    sbposix::SockAddr sock_addr;
    sock_addr.FromSockaddr(interface->ifa_netmask);
    if (sock_addr.ToSbSocketAddress(out_netmask)) {
      found_netmask = true;
      break;
    }
  }

  freeifaddrs(interface_addrs);

  return found_netmask;
}

bool GetNetMaskForInterfaceAddress(const SbSocketAddress& interface_address,
                                   SbSocketAddress* out_netmask) {
  SB_DCHECK(out_netmask);

  switch (interface_address.type) {
    case kSbSocketAddressTypeIpv4:
      return GetNetmaskForInterfaceAddress<in_addr>(interface_address,
                                                    out_netmask);
#if SB_HAS(IPV6)
    case kSbSocketAddressTypeIpv6:
      return GetNetmaskForInterfaceAddress<in6_addr>(interface_address,
                                                     out_netmask);
#endif
    default:
      SB_NOTREACHED() << "Invalid address type " << interface_address.type;
      break;
  }

  return false;
}

bool FindIPv4InterfaceIP(SbSocketAddress* out_interface_ip,
                         SbSocketAddress* out_netmask) {
  if (out_interface_ip == NULL) {
    SB_NOTREACHED() << "out_interface_ip must be specified";
    return false;
  }
  struct ifaddrs* interface_addrs = NULL;

  int retval = getifaddrs(&interface_addrs);
  if (retval != 0) {
    return false;
  }

  bool success = false;
  for (struct ifaddrs* interface = interface_addrs; interface != NULL;
       interface = interface->ifa_next) {
    if (!(IFF_UP & interface->ifa_flags) ||
        (IFF_LOOPBACK & interface->ifa_flags)) {
      continue;
    }

    const struct sockaddr* addr = interface->ifa_addr;
    const struct sockaddr* netmask = interface->ifa_netmask;
    if (!addr || !netmask || (addr->sa_family != AF_INET)) {
      // IPv4 addresses only.
      continue;
    }

    sbposix::SockAddr sock_addr;
    sock_addr.FromSockaddr(addr);
    if (sock_addr.ToSbSocketAddress(out_interface_ip)) {
      if (out_netmask) {
        sbposix::SockAddr netmask_addr;
        netmask_addr.FromSockaddr(netmask);
        if (!netmask_addr.ToSbSocketAddress(out_netmask)) {
          continue;
        }
      }

      success = true;
      break;
    }
  }

  freeifaddrs(interface_addrs);

  return success;
}

#if SB_HAS(IPV6)
bool IsUniqueLocalAddress(const unsigned char ip[16]) {
  // Unique Local Addresses are in fd08::/8.
  return ip[0] == 0xfd && ip[1] == 0x08;
}

bool FindIPv6InterfaceIP(SbSocketAddress* out_interface_ip,
                         SbSocketAddress* out_netmask) {
  if (!out_interface_ip) {
    SB_NOTREACHED() << "out_interface_ip must be specified";
    return false;
  }
  struct ifaddrs* interface_addrs = NULL;

  int retval = getifaddrs(&interface_addrs);
  if (retval != 0) {
    return false;
  }

  int max_scope_interface_value = -1;

  bool ip_found = false;
  SbSocketAddress temp_interface_ip;
  SbSocketAddress temp_netmask;

  for (struct ifaddrs* interface = interface_addrs; interface != NULL;
       interface = interface->ifa_next) {
    if (!(IFF_UP & interface->ifa_flags) ||
        (IFF_LOOPBACK & interface->ifa_flags)) {
      continue;
    }

    const struct sockaddr* addr = interface->ifa_addr;
    const struct sockaddr* netmask = interface->ifa_netmask;
    if (!addr || !netmask || addr->sa_family != AF_INET6) {
      // IPv6 addresses only.
      continue;
    }

    const in6_addr* potential_match;
    if (!GetPotentialMatch(interface->ifa_addr, &potential_match))
      continue;

    // Check the IP for loopback again, just in case flags were incorrect.
    if (IN6_IS_ADDR_LOOPBACK(potential_match) ||
        IN6_IS_ADDR_LINKLOCAL(potential_match)) {
      continue;
    }

    const sockaddr_in6* v6addr =
        reinterpret_cast<const sockaddr_in6*>(interface->ifa_addr);
    if (!v6addr) {
      continue;
    }

    int current_interface_scope = v6addr->sin6_scope_id;

    if (IsUniqueLocalAddress(v6addr->sin6_addr.s6_addr)) {
      // ULAs have global scope, but not globally routable.  So prefer
      // non ULA addresses with global scope by adjusting their "scope"
      current_interface_scope -= 1;
    }

    if (current_interface_scope <= max_scope_interface_value) {
      continue;
    }
    max_scope_interface_value = current_interface_scope;

    sbposix::SockAddr sock_addr;
    sock_addr.FromSockaddr(addr);
    if (sock_addr.ToSbSocketAddress(&temp_interface_ip)) {
      if (netmask) {
        sbposix::SockAddr netmask_addr;
        netmask_addr.FromSockaddr(netmask);
        if (!netmask_addr.ToSbSocketAddress(&temp_netmask)) {
          continue;
        }
      }

      ip_found = true;
    }
  }

  freeifaddrs(interface_addrs);

  if (!ip_found) {
    return false;
  }

  SbMemoryCopy(out_interface_ip, &temp_interface_ip, sizeof(SbSocketAddress));
  if (out_netmask != NULL) {
    SbMemoryCopy(out_netmask, &temp_netmask, sizeof(SbSocketAddress));
  }

  return true;
}
#endif

bool FindInterfaceIP(const SbSocketAddressType type,
                     SbSocketAddress* out_interface_ip,
                     SbSocketAddress* out_netmask) {
  switch (type) {
    case kSbSocketAddressTypeIpv4:
      return FindIPv4InterfaceIP(out_interface_ip, out_netmask);
#if SB_HAS(IPV6)
    case kSbSocketAddressTypeIpv6:
      return FindIPv6InterfaceIP(out_interface_ip, out_netmask);
#endif
    default:
      SB_NOTREACHED() << "Invalid socket address type " << type;
  }

  return false;
}

bool FindSourceAddressForDestination(const SbSocketAddress& destination,
                                     SbSocketAddress* out_source_address) {
  SbSocket socket = SbSocketCreate(destination.type, kSbSocketProtocolUdp);
  if (!SbSocketIsValid(socket)) {
    return false;
  }

  SbSocketError connect_retval = SbSocketConnect(socket, &destination);
  if (connect_retval != kSbSocketOk) {
    bool socket_destroyed = SbSocketDestroy(socket);
    SB_DCHECK(socket_destroyed);
    return false;
  }

  bool success = SbSocketGetLocalAddress(socket, out_source_address);
  bool socket_destroyed = SbSocketDestroy(socket);
  SB_DCHECK(socket_destroyed);
  return success;
}

}  // namespace

bool SbSocketGetInterfaceAddress(const SbSocketAddress* const destination,
                                 SbSocketAddress* out_source_address,
                                 SbSocketAddress* out_netmask) {
  if (!out_source_address) {
    return false;
  }

  if (destination == NULL) {
#if SB_HAS(IPV6)
    // Return either a v4 or a v6 address.  Per spec.
    return (FindIPv4InterfaceIP(out_source_address, out_netmask) ||
            FindIPv6InterfaceIP(out_source_address, out_netmask));
#else
    return FindIPv4InterfaceIP(out_source_address, out_netmask);
#endif

  } else if (IsAnyAddress(*destination)) {
    return FindInterfaceIP(destination->type, out_source_address, out_netmask);
  } else {
    return (FindSourceAddressForDestination(*destination, out_source_address) &&
            GetNetMaskForInterfaceAddress(*out_source_address, out_netmask));
  }

  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const SbSocketAddress* const</code><br>        <code>destination</code></td>
    <td>The destination IP to be connected to.  If IP addresses is not
0.0.0.0 or ::, then temporary addresses may be returned.</td>
  </tr>
  <tr>
    <td><code>SbSocketAddress*</code><br>        <code>out_source_address</code></td>
    <td>This function places the address of the local interface
in this output variable.</td>
  </tr>
  <tr>
    <td><code>SbSocketAddress*</code><br>        <code>out_netmask</code></td>
    <td>This parameter is optional.  If a non-NULL value is passed in,
this function places the netmask associated with the source address in this
output variable.</td>
  </tr>
</table>

### SbSocketGetLastError

**Description**

Returns the last error set on `socket`. If `socket` is not valid, this
function returns `kSbSocketErrorFailed`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketGetLastError-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketGetLastError-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketGetLastError-declaration">
<pre>
SB_EXPORT SbSocketError SbSocketGetLastError(SbSocket socket);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketGetLastError-stub">

```
#include "starboard/socket.h"

SbSocketError SbSocketGetLastError(SbSocket /*socket*/) {
  return kSbSocketErrorFailed;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocket</code><br>        <code>socket</code></td>
    <td>The SbSocket that the last error is returned for.</td>
  </tr>
</table>

### SbSocketGetLocalAddress

**Description**

Gets the address that this socket is bound to locally, if the socket is
connected. The return value indicates whether the address was retrieved
successfully.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketGetLocalAddress-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketGetLocalAddress-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketGetLocalAddress-declaration">
<pre>
SB_EXPORT bool SbSocketGetLocalAddress(SbSocket socket,
                                       SbSocketAddress* out_address);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketGetLocalAddress-stub">

```
#include "starboard/socket.h"

bool SbSocketGetLocalAddress(SbSocket /*socket*/,
                             SbSocketAddress* /*out_address*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocket</code><br>        <code>socket</code></td>
    <td>The SbSocket for which the local address is retrieved.</td>
  </tr>
  <tr>
    <td><code>SbSocketAddress*</code><br>        <code>out_address</code></td>
    <td>The SbSocket's local address.</td>
  </tr>
</table>

### SbSocketIsConnected

**Description**

Indicates whether `socket` is connected to anything. Invalid sockets are not
connected.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketIsConnected-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketIsConnected-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketIsConnected-declaration">
<pre>
SB_EXPORT bool SbSocketIsConnected(SbSocket socket);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketIsConnected-stub">

```
#include "starboard/socket.h"

bool SbSocketIsConnected(SbSocket /*socket*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocket</code><br>        <code>socket</code></td>
    <td>The SbSocket to be checked.</td>
  </tr>
</table>

### SbSocketIsConnectedAndIdle

**Description**

Returns whether `socket` is connected to anything, and, if so, whether it is
receiving any data.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketIsConnectedAndIdle-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketIsConnectedAndIdle-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketIsConnectedAndIdle-declaration">
<pre>
SB_EXPORT bool SbSocketIsConnectedAndIdle(SbSocket socket);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketIsConnectedAndIdle-stub">

```
#include "starboard/socket.h"

bool SbSocketIsConnectedAndIdle(SbSocket /*socket*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocket</code><br>        <code>socket</code></td>
    <td>The SbSocket to be checked.</td>
  </tr>
</table>

### SbSocketIsValid

**Description**

Returns whether the given socket handle is valid.

**Declaration**

```
static SB_C_INLINE bool SbSocketIsValid(SbSocket socket) {
  return socket != kSbSocketInvalid;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocket</code><br>
        <code>socket</code></td>
    <td> </td>
  </tr>
</table>

### SbSocketJoinMulticastGroup

**Description**

Joins `socket` to an IP multicast group identified by `address`. The
equivalent of IP_ADD_MEMBERSHIP. The return value indicates whether the
socket was joined to the group successfully.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketJoinMulticastGroup-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketJoinMulticastGroup-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketJoinMulticastGroup-declaration">
<pre>
SB_EXPORT bool SbSocketJoinMulticastGroup(SbSocket socket,
                                          const SbSocketAddress* address);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketJoinMulticastGroup-stub">

```
#include "starboard/socket.h"

bool SbSocketJoinMulticastGroup(SbSocket /*socket*/,
                                const SbSocketAddress* /*address*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocket</code><br>        <code>socket</code></td>
    <td>The SbSocket to be joined to the IP multicast group.</td>
  </tr>
  <tr>
    <td><code>const SbSocketAddress*</code><br>        <code>address</code></td>
    <td>The location of the IP multicast group.</td>
  </tr>
</table>

### SbSocketListen

**Description**

Causes `socket` to listen on the local address that `socket` was previously
bound to by <code><a href="#sbsocketbind">SbSocketBind</a></code>. This function sets and returns the socket error if
it is unable to listen for some reason. (It returns `kSbSocketOk` if it
creates the connection successfully.)

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketListen-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketListen-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketListen-declaration">
<pre>
SB_EXPORT SbSocketError SbSocketListen(SbSocket socket);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketListen-stub">

```
#include "starboard/socket.h"

SbSocketError SbSocketListen(SbSocket /*socket*/) {
  return kSbSocketErrorFailed;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocket</code><br>        <code>socket</code></td>
    <td>The SbSocket on which the function operates.</td>
  </tr>
</table>

### SbSocketReceiveFrom

**Description**

Reads up to `data_size` bytes from `socket` into `out_data` and places the
source address of the packet in `out_source` if out_source is not NULL.
Returns the number of bytes read, or a negative number if there is an error,
in which case <code><a href="#sbsocketgetlasterror">SbSocketGetLastError</a></code> can provide the precise error encountered.<br>
Note that this function is NOT specified to make a best effort to read all
data on all platforms, but it MAY still do so. It is specified to read
however many bytes are available conveniently, meaning that it should avoid
blocking until there is data. It can be run in a loop until
SbSocketGetLastError returns `kSbSocketPending` to make it a best-effort
read (but still only up to not blocking, unless you want to spin).<br>
The primary use of `out_source` is to receive datagram packets from
multiple sources on a UDP server socket. TCP has two endpoints connected
persistently, so the address is unnecessary, but allowed.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketReceiveFrom-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketReceiveFrom-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketReceiveFrom-declaration">
<pre>
SB_EXPORT int SbSocketReceiveFrom(SbSocket socket,
                                  char* out_data,
                                  int data_size,
                                  SbSocketAddress* out_source);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketReceiveFrom-stub">

```
#include "starboard/socket.h"

int SbSocketReceiveFrom(SbSocket /*socket*/,
                        char* /*out_data*/,
                        int /*data_size*/,
                        SbSocketAddress* /*out_source*/) {
  return -1;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocket</code><br>        <code>socket</code></td>
    <td>The SbSocket from which data is read.</td>
  </tr>
  <tr>
    <td><code>char*</code><br>        <code>out_data</code></td>
    <td>The data read from the socket.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>data_size</code></td>
    <td>The number of bytes to read.</td>
  </tr>
  <tr>
    <td><code>SbSocketAddress*</code><br>        <code>out_source</code></td>
    <td>The source address of the packet.</td>
  </tr>
</table>

### SbSocketResolve

**Description**

Synchronously resolves `hostname` into the returned SbSocketResolution,
which must be freed with <code><a href="#sbsocketfreeresolution">SbSocketFreeResolution</a></code>. The function returns
`NULL` if it is unable to resolve `hostname`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketResolve-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketResolve-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketResolve-declaration">
<pre>
SB_EXPORT SbSocketResolution* SbSocketResolve(const char* hostname,
                                              int filters);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketResolve-stub">

```
#include "starboard/socket.h"

SbSocketResolution* SbSocketResolve(const char* /*hostname*/, int /*filters*/) {
  return NULL;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>        <code>hostname</code></td>
    <td>The hostname to be resolved.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>filters</code></td>
    <td>A mask of <code>SbSocketResolve</code>Filter values used to filter the
resolution. If <code>filters</code> does not specify an IP address family filter,
all address families are included. However, if one IP address family filter
is specified, only that address family is included. The function ignores
unrecognized filter bits.</td>
  </tr>
</table>

### SbSocketSendTo

**Description**

Writes up to `data_size` bytes of `data` to `destination` via
`socket`. Returns the number of bytes written, or a negative number if
there is an error, in which case `SbSocketGetLastError` can provide the
precise error encountered.<br>
Note that this function is NOT specified to make a best effort to write all
data on all platforms, but it MAY still do so. It is specified to write
however many bytes are available conveniently. It can be run in a loop
until <code><a href="#sbsocketgetlasterror">SbSocketGetLastError</a></code> returns `kSbSocketPending` to make it a
best-effort write (but still only up to not blocking, unless you want to
spin).<br>
The primary use of `destination` is to send datagram packets, which can
go out to multiple sources from a single UDP server socket. TCP has two
endpoints connected persistently, so setting `destination` when sending
to a TCP socket will cause an error.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketSendTo-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketSendTo-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketSendTo-declaration">
<pre>
SB_EXPORT int SbSocketSendTo(SbSocket socket,
                             const char* data,
                             int data_size,
                             const SbSocketAddress* destination);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketSendTo-stub">

```
#include "starboard/socket.h"

int SbSocketSendTo(SbSocket /*socket*/,
                   const char* /*data*/,
                   int /*data_size*/,
                   const SbSocketAddress* /*destination*/) {
  return -1;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocket</code><br>        <code>socket</code></td>
    <td>The SbSocket to use to write data.</td>
  </tr>
  <tr>
    <td><code>const char*</code><br>        <code>data</code></td>
    <td>The data read from the socket.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>data_size</code></td>
    <td>The number of bytes of <code>data</code> to write.</td>
  </tr>
  <tr>
    <td><code>const SbSocketAddress*</code><br>        <code>destination</code></td>
    <td>The location to which data is written. This value must be
<code>NULL</code> for TCP connections, which can only have a single endpoint.</td>
  </tr>
</table>

### SbSocketSetBroadcast

**Description**

Sets the `SO_BROADCAST`, or equivalent, option to `value` on `socket`. The
return value indicates whether the option was actually set.<br>
This option is only meaningful for UDP sockets and allows the socket to
send to the broadcast address.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketSetBroadcast-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketSetBroadcast-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketSetBroadcast-declaration">
<pre>
SB_EXPORT bool SbSocketSetBroadcast(SbSocket socket, bool value);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketSetBroadcast-stub">

```
#include "starboard/socket.h"

bool SbSocketSetBroadcast(SbSocket /*socket*/, bool /*value*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocket</code><br>        <code>socket</code></td>
    <td>The SbSocket for which the option is set.</td>
  </tr>
  <tr>
    <td><code>bool</code><br>        <code>value</code></td>
    <td>The new value for the option.</td>
  </tr>
</table>

### SbSocketSetReceiveBufferSize

**Description**

Sets the `SO_RCVBUF`, or equivalent, option to `size` on `socket`. The
return value indicates whether the option was actually set.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketSetReceiveBufferSize-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketSetReceiveBufferSize-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketSetReceiveBufferSize-declaration">
<pre>
SB_EXPORT bool SbSocketSetReceiveBufferSize(SbSocket socket, int32_t size);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketSetReceiveBufferSize-stub">

```
#include "starboard/socket.h"

bool SbSocketSetReceiveBufferSize(SbSocket /*socket*/, int32_t /*size*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocket</code><br>        <code>socket</code></td>
    <td>The SbSocket for which the option is set.</td>
  </tr>
  <tr>
    <td><code>int32_t</code><br>        <code>size</code></td>
    <td>The value for the option.</td>
  </tr>
</table>

### SbSocketSetReuseAddress

**Description**

Sets the `SO_REUSEADDR`, or equivalent, option to `value` on `socket`.
The return value indicates whether the option was actually set.<br>
This option allows a bound address to be reused if a socket isn't actively
bound to it.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketSetReuseAddress-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketSetReuseAddress-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketSetReuseAddress-declaration">
<pre>
SB_EXPORT bool SbSocketSetReuseAddress(SbSocket socket, bool value);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketSetReuseAddress-stub">

```
#include "starboard/socket.h"

bool SbSocketSetReuseAddress(SbSocket /*socket*/, bool /*value*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocket</code><br>        <code>socket</code></td>
    <td>The SbSocket for which the option is set.</td>
  </tr>
  <tr>
    <td><code>bool</code><br>        <code>value</code></td>
    <td>The new value for the option.</td>
  </tr>
</table>

### SbSocketSetSendBufferSize

**Description**

Sets the `SO_SNDBUF`, or equivalent, option to `size` on `socket`. The
return value indicates whether the option was actually set.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketSetSendBufferSize-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketSetSendBufferSize-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketSetSendBufferSize-declaration">
<pre>
SB_EXPORT bool SbSocketSetSendBufferSize(SbSocket socket, int32_t size);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketSetSendBufferSize-stub">

```
#include "starboard/socket.h"

bool SbSocketSetSendBufferSize(SbSocket /*socket*/, int32_t /*size*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocket</code><br>        <code>socket</code></td>
    <td>The SbSocket for which the option is set.</td>
  </tr>
  <tr>
    <td><code>int32_t</code><br>        <code>size</code></td>
    <td>The value for the option.</td>
  </tr>
</table>

### SbSocketSetTcpKeepAlive

**Description**

Sets the `SO_KEEPALIVE`, or equivalent, option to `value` on `socket`. The
return value indicates whether the option was actually set.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketSetTcpKeepAlive-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketSetTcpKeepAlive-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketSetTcpKeepAlive-declaration">
<pre>
SB_EXPORT bool SbSocketSetTcpKeepAlive(SbSocket socket,
                                       bool value,
                                       SbTime period);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketSetTcpKeepAlive-stub">

```
#include "starboard/socket.h"

bool SbSocketSetTcpKeepAlive(SbSocket /*socket*/,
                             bool /*value*/,
                             SbTime /*period*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocket</code><br>        <code>socket</code></td>
    <td>The SbSocket for which the option is set.</td>
  </tr>
  <tr>
    <td><code>bool</code><br>        <code>value</code></td>
    <td>If set to <code>true</code>, then <code>period</code> specifies the minimum time
(SbTime) is always in microseconds) between keep-alive packets. If
set to <code>false</code>, <code>period</code> is ignored.</td>
  </tr>
  <tr>
    <td><code>SbTime</code><br>        <code>period</code></td>
    <td>The time between keep-alive packets. This value is only relevant
if <code>value</code> is <code>true</code>.</td>
  </tr>
</table>

### SbSocketSetTcpNoDelay

**Description**

Sets the `TCP_NODELAY`, or equivalent, option to `value` on `socket`. The
return value indicates whether the option was actually set.<br>
This function disables the Nagle algorithm for reducing the number of
packets sent when converting from a stream to packets. Disabling Nagle
generally puts the data for each Send call into its own packet, but does
not guarantee that behavior.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketSetTcpNoDelay-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketSetTcpNoDelay-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketSetTcpNoDelay-declaration">
<pre>
SB_EXPORT bool SbSocketSetTcpNoDelay(SbSocket socket, bool value);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketSetTcpNoDelay-stub">

```
#include "starboard/socket.h"

bool SbSocketSetTcpNoDelay(SbSocket /*socket*/, bool /*value*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocket</code><br>        <code>socket</code></td>
    <td>The SbSocket for which the option is set.</td>
  </tr>
  <tr>
    <td><code>bool</code><br>        <code>value</code></td>
    <td>Indicates whether the Nagle algorithm should be disabled
(<code>value</code>=<code>true</code>).</td>
  </tr>
</table>

### SbSocketSetTcpWindowScaling

**Description**

Sets the `SO_WINSCALE`, or equivalent, option to `value` on `socket`. The
return value indicates whether the option was actually set.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSocketSetTcpWindowScaling-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSocketSetTcpWindowScaling-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSocketSetTcpWindowScaling-declaration">
<pre>
SB_EXPORT bool SbSocketSetTcpWindowScaling(SbSocket socket, bool value);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSocketSetTcpWindowScaling-stub">

```
#include "starboard/socket.h"

bool SbSocketSetTcpWindowScaling(SbSocket /*socket*/, bool /*value*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSocket</code><br>        <code>socket</code></td>
    <td>The SbSocket for which the option is set.</td>
  </tr>
  <tr>
    <td><code>bool</code><br>        <code>value</code></td>
    <td>The value for the option.</td>
  </tr>
</table>

