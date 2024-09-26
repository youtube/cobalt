// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_P2P_IPC_SOCKET_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_P2P_IPC_SOCKET_FACTORY_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_persistent.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/webrtc/api/packet_socket_factory.h"

namespace blink {

class P2PSocketDispatcher;

// IpcPacketSocketFactory implements rtc::PacketSocketFactory
// interface for libjingle using IPC-based P2P sockets. The class must
// be created and used on a thread that is a libjingle thread (implements
// rtc::Thread) and also has associated base::MessageLoop. Each
// socket created by the factory must be used on the thread it was
// created on.
class IpcPacketSocketFactory : public rtc::PacketSocketFactory {
 public:
  PLATFORM_EXPORT explicit IpcPacketSocketFactory(
      P2PSocketDispatcher* socket_dispatcher,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);
  IpcPacketSocketFactory(const IpcPacketSocketFactory&) = delete;
  IpcPacketSocketFactory& operator=(const IpcPacketSocketFactory&) = delete;
  ~IpcPacketSocketFactory() override;

  rtc::AsyncPacketSocket* CreateUdpSocket(
      const rtc::SocketAddress& local_address,
      uint16_t min_port,
      uint16_t max_port) override;
  rtc::AsyncListenSocket* CreateServerTcpSocket(
      const rtc::SocketAddress& local_address,
      uint16_t min_port,
      uint16_t max_port,
      int opts) override;
  rtc::AsyncPacketSocket* CreateClientTcpSocket(
      const rtc::SocketAddress& local_address,
      const rtc::SocketAddress& remote_address,
      const rtc::ProxyInfo& proxy_info,
      const std::string& user_agent,
      const rtc::PacketSocketTcpOptions& opts) override;
  rtc::AsyncResolverInterface* CreateAsyncResolver() override;

 private:
  // `P2PSocketDispatcher` is owned by the main thread, and must be accessed in
  // a thread-safe way. `this` is indirectly owned by
  // `PeerConnectionDependencyFactory`, which holds a hard reference to the
  // dispatcher, so this should be never be null (with the possible exception of
  // the dtor).
  CrossThreadWeakPersistent<P2PSocketDispatcher> socket_dispatcher_;
  const net::NetworkTrafficAnnotationTag traffic_annotation_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_P2P_IPC_SOCKET_FACTORY_H_
