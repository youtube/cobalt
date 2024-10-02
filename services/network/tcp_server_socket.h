// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TCP_SERVER_SOCKET_H_
#define SERVICES_NETWORK_TCP_SERVER_SOCKET_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/ip_endpoint.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "services/network/public/mojom/socket_connection_tracker.mojom.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace net {
class NetLog;
class ServerSocket;
class StreamSocket;
}  // namespace net

namespace network {
class TCPConnectedSocket;

class COMPONENT_EXPORT(NETWORK_SERVICE) TCPServerSocket
    : public mojom::TCPServerSocket {
 public:
  // A delegate interface that is notified when new connections are established.
  class Delegate {
   public:
    Delegate() {}
    ~Delegate() {}

    // Invoked when a new connection is accepted. The delegate should take
    // ownership of |socket| and set up binding for |receiver|.
    virtual void OnAccept(
        std::unique_ptr<TCPConnectedSocket> socket,
        mojo::PendingReceiver<mojom::TCPConnectedSocket> receiver) = 0;
  };

  // Constructs a TCPServerSocket. |delegate| must outlive |this|. When a new
  // connection is accepted, |delegate| will be notified to take ownership of
  // the connection.
  TCPServerSocket(Delegate* delegate,
                  net::NetLog* net_log,
                  const net::NetworkTrafficAnnotationTag& traffic_annotation);

  // As above, but takes an already listening socket.
  TCPServerSocket(std::unique_ptr<net::ServerSocket> server_socket,
                  int backlog,
                  Delegate* delegate,
                  const net::NetworkTrafficAnnotationTag& traffic_annotation);

  TCPServerSocket(const TCPServerSocket&) = delete;
  TCPServerSocket& operator=(const TCPServerSocket&) = delete;

  ~TCPServerSocket() override;

  base::expected<net::IPEndPoint, int32_t> Listen(
      const net::IPEndPoint& local_addr,
      int backlog,
      absl::optional<bool> ipv6_only);

  // TCPServerSocket implementation.
  void Accept(mojo::PendingRemote<mojom::SocketObserver> observer,
              AcceptCallback callback) override;

#if BUILDFLAG(IS_CHROMEOS)
  void AttachConnectionTracker(
      mojo::PendingRemote<mojom::SocketConnectionTracker>);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Replaces the underlying socket implementation with |socket| in tests.
  void SetSocketForTest(std::unique_ptr<net::ServerSocket> socket);

 private:
  struct PendingAccept {
    PendingAccept(AcceptCallback callback,
                  mojo::PendingRemote<mojom::SocketObserver> observer);
    ~PendingAccept();

    AcceptCallback callback;
    mojo::PendingRemote<mojom::SocketObserver> observer;
  };
  // Invoked when socket_->Accept() completes.
  void OnAcceptCompleted(int result);
  // Process the next Accept() from |pending_accepts_queue_|.
  void ProcessNextAccept();

  const raw_ptr<Delegate> delegate_;
  std::unique_ptr<net::ServerSocket> socket_;
  int backlog_;
  std::vector<std::unique_ptr<PendingAccept>> pending_accepts_queue_;
  std::unique_ptr<net::StreamSocket> accepted_socket_;
  net::IPEndPoint accepted_address_;
  net::NetworkTrafficAnnotationTag traffic_annotation_;

#if BUILDFLAG(IS_CHROMEOS)
  mojo::PendingRemote<mojom::SocketConnectionTracker> connection_tracker_;
#endif  // BUILDFLAG(IS_CHROMEOS)

  base::WeakPtrFactory<TCPServerSocket> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_TCP_SERVER_SOCKET_H_
