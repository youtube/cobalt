// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_P2P_SOCKET_MANAGER_H_
#define SERVICES_NETWORK_P2P_SOCKET_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_change_notifier.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/datagram_client_socket.h"
#include "services/network/p2p/socket.h"
#include "services/network/p2p/socket_throttler.h"
#include "services/network/public/cpp/p2p_socket_type.h"
#include "services/network/public/mojom/p2p.mojom.h"
#include "services/network/public/mojom/p2p_trusted.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {
class URLRequestContext;
}

namespace network {
class ProxyResolvingClientSocketFactory;
}

namespace network {

struct DefaultLocalAddresses {
  DefaultLocalAddresses();
  ~DefaultLocalAddresses();

  absl::optional<net::IPAddress> default_ipv4_local_address;
  absl::optional<net::IPAddress> default_ipv6_local_address;
};

// Owns all the P2P socket instances and dispatches Mojo calls from the
// (untrusted) child and (trusted) browser process.
class COMPONENT_EXPORT(NETWORK_SERVICE) P2PSocketManager
    : public net::NetworkChangeNotifier::NetworkChangeObserver,
      public mojom::P2PSocketManager,
      public mojom::P2PTrustedSocketManager,
      public P2PSocket::Delegate {
 public:
  using DeleteCallback =
      base::OnceCallback<void(P2PSocketManager* socket_manager)>;

  // |delete_callback| tells the P2PSocketManager's owner to destroy the
  // P2PSocketManager. The P2PSocketManager must be destroyed before the
  // |url_request_context|.
  P2PSocketManager(
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojo::PendingRemote<mojom::P2PTrustedSocketManagerClient>
          trusted_socket_manager_client,
      mojo::PendingReceiver<mojom::P2PTrustedSocketManager>
          trusted_socket_manager_receiver,
      mojo::PendingReceiver<mojom::P2PSocketManager> socket_manager_receiver,
      DeleteCallback delete_callback,
      net::URLRequestContext* url_request_context);

  P2PSocketManager(const P2PSocketManager&) = delete;
  P2PSocketManager& operator=(const P2PSocketManager&) = delete;

  ~P2PSocketManager() override;

  // net::NetworkChangeNotifier::NetworkChangeObserver overrides.
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  void PauseNetworkChangeNotifications() override;
  void ResumeNetworkChangeNotifications() override;

 private:
  class DnsRequest;

  static net::NetworkInterfaceList DoGetNetworkList();

  using GetDefaultCallback = base::OnceCallback<void(net::IPAddress)>;
  void DoGetDefaultLocalAddresses(net::NetworkInterfaceList list);
  void MaybeFinishDoGetDefaultLocalAddresses(
      DefaultLocalAddresses* default_local_addresses,
      net::NetworkInterfaceList list,
      int family,
      net::IPAddress addr);
  // This connects a UDP socket to a public IP address and gets local
  // address. Since it binds to the "any" address (0.0.0.0 or ::) internally, it
  // retrieves the default local address.
  void GetDefaultLocalAddress(int family, GetDefaultCallback callback);
  void FinishGetDefaultLocalAddress(
      std::unique_ptr<net::DatagramClientSocket> socket,
      GetDefaultCallback callback,
      int result);
  void SendNetworkList(const net::NetworkInterfaceList& list,
                       net::IPAddress default_ipv4_local_address,
                       net::IPAddress default_ipv6_local_address);

  // P2PSocket::Delegate.
  void AddAcceptedConnection(
      std::unique_ptr<P2PSocket> accepted_connection) override;
  void DestroySocket(P2PSocket* socket) override;
  void DumpPacket(base::span<const uint8_t> data, bool incoming) override;

  // mojom::P2PSocketManager overrides:
  void StartNetworkNotifications(
      mojo::PendingRemote<mojom::P2PNetworkNotificationClient> client) override;
  void GetHostAddress(
      const std::string& host_name,
      bool enable_mdns,
      mojom::P2PSocketManager::GetHostAddressCallback callback) override;
  void GetHostAddressWithFamily(
      const std::string& host_name,
      int address_family,
      bool enable_mdns,
      mojom::P2PSocketManager::GetHostAddressCallback callback) override;
  void CreateSocket(
      P2PSocketType type,
      const net::IPEndPoint& local_address,
      const P2PPortRange& port_range,
      const P2PHostAndIPEndPoint& remote_address,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingRemote<mojom::P2PSocketClient> client,
      mojo::PendingReceiver<mojom::P2PSocket> receiver) override;

  // mojom::P2PTrustedSocketManager overrides:
  void StartRtpDump(bool incoming, bool outgoing) override;
  void StopRtpDump(bool incoming, bool outgoing) override;

  void NetworkNotificationClientConnectionError();

  void DoGetHostAddress(
      const std::string& host_name,
      absl::optional<int> address_family,
      bool enable_mdns,
      mojom::P2PSocketManager::GetHostAddressCallback callback);

  void OnAddressResolved(
      DnsRequest* request,
      mojom::P2PSocketManager::GetHostAddressCallback callback,
      const net::IPAddressList& addresses);

  void OnConnectionError();

  DeleteCallback delete_callback_;
  raw_ptr<net::URLRequestContext> url_request_context_;
  const net::NetworkAnonymizationKey network_anonymization_key_;

  std::unique_ptr<ProxyResolvingClientSocketFactory>
      proxy_resolving_socket_factory_;

  base::flat_map<P2PSocket*, std::unique_ptr<P2PSocket>> sockets_;

  std::set<std::unique_ptr<DnsRequest>, base::UniquePtrComparator>
      dns_requests_;
  P2PMessageThrottler throttler_;

  bool dump_incoming_rtp_packet_ = false;
  bool dump_outgoing_rtp_packet_ = false;

  bool notifications_paused_ = false;
  bool pending_network_change_notification_ = false;

  // Used to call DoGetNetworkList, which may briefly block since getting the
  // default local address involves creating a dummy socket.
  const scoped_refptr<base::SequencedTaskRunner> network_list_task_runner_;

  mojo::Remote<mojom::P2PTrustedSocketManagerClient>
      trusted_socket_manager_client_;
  mojo::Receiver<mojom::P2PTrustedSocketManager>
      trusted_socket_manager_receiver_;
  mojo::Receiver<mojom::P2PSocketManager> socket_manager_receiver_;

  mojo::Remote<mojom::P2PNetworkNotificationClient>
      network_notification_client_;

  base::WeakPtrFactory<P2PSocketManager> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_P2P_SOCKET_MANAGER_H_
