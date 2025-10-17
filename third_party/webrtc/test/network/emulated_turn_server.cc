/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/network/emulated_turn_server.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "api/async_dns_resolver.h"
#include "api/packet_socket_factory.h"
#include "api/sequence_checker.h"
#include "api/test/network_emulation/network_emulation_interfaces.h"
#include "api/test/network_emulation_manager.h"
#include "p2p/base/port_interface.h"
#include "p2p/test/turn_server.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/task_queue_for_test.h"
#include "rtc_base/thread.h"

namespace {

static const char kTestRealm[] = "example.org";
static const char kTestSoftware[] = "TestTurnServer";

// A wrapper class for webrtc::TurnServer to allocate sockets.
class PacketSocketFactoryWrapper : public webrtc::PacketSocketFactory {
 public:
  explicit PacketSocketFactoryWrapper(
      webrtc::test::EmulatedTURNServer* turn_server)
      : turn_server_(turn_server) {}
  ~PacketSocketFactoryWrapper() override {}

  // This method is called from TurnServer when making a TURN ALLOCATION.
  // It will create a socket on the `peer_` endpoint.
  webrtc::AsyncPacketSocket* CreateUdpSocket(
      const webrtc::SocketAddress& address,
      uint16_t min_port,
      uint16_t max_port) override {
    return turn_server_->CreatePeerSocket();
  }

  webrtc::AsyncListenSocket* CreateServerTcpSocket(
      const webrtc::SocketAddress& local_address,
      uint16_t min_port,
      uint16_t max_port,
      int opts) override {
    return nullptr;
  }
  webrtc::AsyncPacketSocket* CreateClientTcpSocket(
      const webrtc::SocketAddress& local_address,
      const webrtc::SocketAddress& remote_address,
      const webrtc::PacketSocketTcpOptions& tcp_options) override {
    return nullptr;
  }
  std::unique_ptr<webrtc::AsyncDnsResolverInterface> CreateAsyncDnsResolver()
      override {
    return nullptr;
  }

 private:
  webrtc::test::EmulatedTURNServer* turn_server_;
};

}  //  namespace

namespace webrtc {
namespace test {

// A wrapper class for copying data between an AsyncPacketSocket and a
// EmulatedEndpoint. This is used by the webrtc::TurnServer when
// sending data back into the emulated network.
class EmulatedTURNServer::AsyncPacketSocketWrapper : public AsyncPacketSocket {
 public:
  AsyncPacketSocketWrapper(test::EmulatedTURNServer* turn_server,
                           EmulatedEndpoint* endpoint,
                           uint16_t port)
      : turn_server_(turn_server),
        endpoint_(endpoint),
        local_address_(SocketAddress(endpoint_->GetPeerLocalAddress(), port)) {}
  ~AsyncPacketSocketWrapper() { turn_server_->Unbind(local_address_); }

  SocketAddress GetLocalAddress() const override { return local_address_; }
  SocketAddress GetRemoteAddress() const override { return SocketAddress(); }
  int Send(const void* pv,
           size_t cb,
           const AsyncSocketPacketOptions& options) override {
    RTC_CHECK(false) << "TCP not implemented";
    return -1;
  }
  int SendTo(const void* pv,
             size_t cb,
             const SocketAddress& addr,
             const AsyncSocketPacketOptions& options) override {
    // Copy from webrtc::AsyncPacketSocket to EmulatedEndpoint.
    CopyOnWriteBuffer buf(reinterpret_cast<const char*>(pv), cb);
    endpoint_->SendPacket(local_address_, addr, buf);
    return cb;
  }
  int Close() override { return 0; }
  void NotifyPacketReceived(const ReceivedIpPacket& packet) {
    AsyncPacketSocket::NotifyPacketReceived(packet);
  }

  AsyncPacketSocket::State GetState() const override {
    return AsyncPacketSocket::STATE_BOUND;
  }
  int GetOption(Socket::Option opt, int* value) override { return 0; }
  int SetOption(Socket::Option opt, int value) override { return 0; }
  int GetError() const override { return 0; }
  void SetError(int error) override {}

 private:
  test::EmulatedTURNServer* const turn_server_;
  EmulatedEndpoint* const endpoint_;
  const SocketAddress local_address_;
};

EmulatedTURNServer::EmulatedTURNServer(const EmulatedTURNServerConfig& config,
                                       std::unique_ptr<Thread> thread,
                                       EmulatedEndpoint* client,
                                       EmulatedEndpoint* peer)
    : thread_(std::move(thread)), client_(client), peer_(peer) {
  ice_config_.username = "keso";
  ice_config_.password = "keso";
  SendTask(thread_.get(), [this, enable_permission_checks =
                                     config.enable_permission_checks]() {
    RTC_DCHECK_RUN_ON(thread_.get());
    turn_server_ = std::make_unique<TurnServer>(thread_.get());
    turn_server_->set_realm(kTestRealm);
    turn_server_->set_realm(kTestSoftware);
    turn_server_->set_auth_hook(this);
    turn_server_->set_enable_permission_checks(enable_permission_checks);

    auto client_socket = Wrap(client_);
    turn_server_->AddInternalSocket(client_socket, PROTO_UDP);
    turn_server_->SetExternalSocketFactory(new PacketSocketFactoryWrapper(this),
                                           SocketAddress());
    client_address_ = client_socket->GetLocalAddress();
    char buf[256];
    SimpleStringBuilder str(buf);
    str.AppendFormat("turn:%s?transport=udp",
                     client_address_.ToString().c_str());
    ice_config_.url = str.str();
  });
}

void EmulatedTURNServer::Stop() {
  SendTask(thread_.get(), [this]() {
    RTC_DCHECK_RUN_ON(thread_.get());
    sockets_.clear();
  });
}

EmulatedTURNServer::~EmulatedTURNServer() {
  SendTask(thread_.get(), [this]() {
    RTC_DCHECK_RUN_ON(thread_.get());
    turn_server_.reset(nullptr);
  });
}

AsyncPacketSocket* EmulatedTURNServer::Wrap(EmulatedEndpoint* endpoint) {
  RTC_DCHECK_RUN_ON(thread_.get());
  auto port = endpoint->BindReceiver(0, this).value();
  auto socket = new AsyncPacketSocketWrapper(this, endpoint, port);
  sockets_[SocketAddress(endpoint->GetPeerLocalAddress(), port)] = socket;
  return socket;
}

void EmulatedTURNServer::OnPacketReceived(EmulatedIpPacket packet) {
  // Copy from EmulatedEndpoint to webrtc::AsyncPacketSocket.
  thread_->PostTask([this, packet(std::move(packet))]() {
    RTC_DCHECK_RUN_ON(thread_.get());
    auto it = sockets_.find(packet.to);
    if (it != sockets_.end()) {
      it->second->NotifyPacketReceived(
          ReceivedIpPacket(packet.data, packet.from, packet.arrival_time));
    }
  });
}

void EmulatedTURNServer::Unbind(SocketAddress address) {
  RTC_DCHECK_RUN_ON(thread_.get());
  if (GetClientEndpoint()->GetPeerLocalAddress() == address.ipaddr()) {
    GetClientEndpoint()->UnbindReceiver(address.port());
  } else {
    GetPeerEndpoint()->UnbindReceiver(address.port());
  }
  sockets_.erase(address);
}

}  // namespace test
}  // namespace webrtc
