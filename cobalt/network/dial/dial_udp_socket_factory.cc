// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/network/dial/dial_udp_socket_factory.h"

#if !defined(STARBOARD)
#include <arpa/inet.h>
#endif

#include <memory>

#include "base/logging.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_string_util.h"

#if defined(STARBOARD)
#include "starboard/common/socket.h"
#include "starboard/types.h"
#endif

namespace cobalt {
namespace network {

std::unique_ptr<net::UDPSocket> UdpSocketFactory::CreateAndBind(
    const net::IPEndPoint& address) {
  std::unique_ptr<net::UDPSocket> sock(new net::UDPSocket(
      net::DatagramSocket::DEFAULT_BIND, nullptr, net::NetLogSource()));
  if (sock->Open(address.GetFamily()) != net::OK) {
    return NULL;
  }

  int set_socket_reuse_success = sock->AllowAddressReuse();
  if (set_socket_reuse_success != net::OK) {
    LOG(WARNING) << "AllowAddressReuse failed on DIAL UDP socket with code "
                 << set_socket_reuse_success;
  }

  if (sock->Bind(address) != net::OK) {
    LOG(WARNING) << "DIAL UDP socket failed to bind to address";
    sock->Close();
    return NULL;
  }

  SetupSocketAfterBind(sock.get());

  return sock;
}
// Enable Multicast and join group if multicast is enabled.
void DialUdpSocketFactory::SetupSocketAfterBind(net::UDPSocket* sock) {
  net::IPAddress address(239, 255, 255, 250);
  if (sock->JoinGroup(address) != net::OK) {
    LOG(WARNING) << "JoinGroup failed on DIAL UDP socket.";
  }
}

}  // namespace network
}  // namespace cobalt
