/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_PACKET_SOCKET_FACTORY_H_
#define API_PACKET_SOCKET_FACTORY_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "api/async_dns_resolver.h"
#include "api/environment/environment.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/checks.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

struct PacketSocketTcpOptions {
  PacketSocketTcpOptions() = default;
  ~PacketSocketTcpOptions() = default;

  int opts = 0;
  std::vector<std::string> tls_alpn_protocols;
  std::vector<std::string> tls_elliptic_curves;
  // An optional custom SSL certificate verifier that an API user can provide to
  // inject their own certificate verification logic (not available to users
  // outside of the WebRTC repo).
  SSLCertificateVerifier* tls_cert_verifier = nullptr;
};

class RTC_EXPORT PacketSocketFactory {
 public:
  enum Options {
    OPT_STUN = 0x04,

    // The TLS options below are mutually exclusive.
    OPT_TLS = 0x02,           // Real and secure TLS.
    OPT_TLS_FAKE = 0x01,      // Fake TLS with a dummy SSL handshake.
    OPT_TLS_INSECURE = 0x08,  // Insecure TLS without certificate validation.

    // Deprecated, use OPT_TLS_FAKE.
    OPT_SSLTCP = OPT_TLS_FAKE,
  };

  PacketSocketFactory() = default;

  PacketSocketFactory(const PacketSocketFactory&) = delete;
  PacketSocketFactory& operator=(const PacketSocketFactory&) = delete;

  virtual ~PacketSocketFactory() = default;

  // TODO: bugs.webrtc.org/42223992 - after Oct 10, 2025 make Create*Socket
  // functions that accept Environment pure virtual, and delete legacy
  // Create*Socket functions.
  virtual std::unique_ptr<AsyncPacketSocket> CreateUdpSocket(
      const Environment& /*env*/,
      const SocketAddress& address,
      uint16_t min_port,
      uint16_t max_port) {
    return absl::WrapUnique(CreateUdpSocket(address, min_port, max_port));
  }

  virtual std::unique_ptr<AsyncListenSocket> CreateServerTcpSocket(
      const Environment& /*env*/,
      const SocketAddress& local_address,
      uint16_t min_port,
      uint16_t max_port,
      int opts) {
    return absl::WrapUnique(
        CreateServerTcpSocket(local_address, min_port, max_port, opts));
  }

  virtual std::unique_ptr<AsyncPacketSocket> CreateClientTcpSocket(
      const Environment& /*env*/,
      const SocketAddress& local_address,
      const SocketAddress& remote_address,
      const PacketSocketTcpOptions& tcp_options) {
    return absl::WrapUnique(
        CreateClientTcpSocket(local_address, remote_address, tcp_options));
  }

  virtual std::unique_ptr<AsyncDnsResolverInterface>
  CreateAsyncDnsResolver() = 0;

 private:
  virtual AsyncPacketSocket* CreateUdpSocket(const SocketAddress& address,
                                             uint16_t min_port,
                                             uint16_t max_port) {
    RTC_DCHECK_NOTREACHED();
    return nullptr;
  }
  virtual AsyncListenSocket* CreateServerTcpSocket(
      const SocketAddress& local_address,
      uint16_t min_port,
      uint16_t max_port,
      int opts) {
    RTC_DCHECK_NOTREACHED();
    return nullptr;
  }

  virtual AsyncPacketSocket* CreateClientTcpSocket(
      const SocketAddress& local_address,
      const SocketAddress& remote_address,
      const PacketSocketTcpOptions& tcp_options) {
    RTC_DCHECK_NOTREACHED();
    return nullptr;
  }
};

}  //  namespace webrtc


#endif  // API_PACKET_SOCKET_FACTORY_H_
