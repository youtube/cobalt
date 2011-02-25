// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_factory.h"

#include "base/lazy_instance.h"
#include "build/build_config.h"
#include "net/socket/client_socket_handle.h"
#if defined(OS_WIN)
#include "net/socket/ssl_client_socket_nss.h"
#include "net/socket/ssl_client_socket_win.h"
#elif defined(USE_OPENSSL)
#include "net/socket/ssl_client_socket_openssl.h"
#elif defined(USE_NSS)
#include "net/socket/ssl_client_socket_nss.h"
#elif defined(OS_MACOSX)
#include "net/socket/ssl_client_socket_mac.h"
#include "net/socket/ssl_client_socket_nss.h"
#endif
#include "net/socket/ssl_host_info.h"
#include "net/socket/tcp_client_socket.h"

namespace net {

namespace {

bool g_use_system_ssl = false;

class DefaultClientSocketFactory : public ClientSocketFactory {
 public:
  virtual ClientSocket* CreateTCPClientSocket(
      const AddressList& addresses,
      NetLog* net_log,
      const NetLog::Source& source) {
    return new TCPClientSocket(addresses, net_log, source);
  }

  virtual SSLClientSocket* CreateSSLClientSocket(
      ClientSocketHandle* transport_socket,
      const HostPortPair& host_and_port,
      const SSLConfig& ssl_config,
      SSLHostInfo* ssl_host_info,
      CertVerifier* cert_verifier,
      DnsCertProvenanceChecker* dns_cert_checker) {
    scoped_ptr<SSLHostInfo> shi(ssl_host_info);
#if defined(OS_WIN)
    if (g_use_system_ssl) {
      return new SSLClientSocketWin(transport_socket, host_and_port,
                                    ssl_config, cert_verifier);
    }
    return new SSLClientSocketNSS(transport_socket, host_and_port, ssl_config,
                                  shi.release(), cert_verifier,
                                  dns_cert_checker);
#elif defined(USE_OPENSSL)
    return new SSLClientSocketOpenSSL(transport_socket, host_and_port,
                                      ssl_config, cert_verifier);
#elif defined(USE_NSS)
    return new SSLClientSocketNSS(transport_socket, host_and_port, ssl_config,
                                  shi.release(), cert_verifier,
                                  dns_cert_checker);
#elif defined(OS_MACOSX)
    if (g_use_system_ssl) {
      return new SSLClientSocketMac(transport_socket, host_and_port,
                                    ssl_config, cert_verifier);
    }
    return new SSLClientSocketNSS(transport_socket, host_and_port, ssl_config,
                                  shi.release(), cert_verifier,
                                  dns_cert_checker);
#else
    NOTIMPLEMENTED();
    return NULL;
#endif
  }

  // TODO(rch): This is only implemented for the NSS SSL library, which is the
  /// default for Windows, Mac and Linux, but we should implement it everywhere.
  void ClearSSLSessionCache() {
#if defined(OS_WIN)
    if (!g_use_system_ssl)
      SSLClientSocketNSS::ClearSessionCache();
#elif defined(USE_OPENSSL)
    // no-op
#elif defined(USE_NSS)
    SSLClientSocketNSS::ClearSessionCache();
#elif defined(OS_MACOSX)
    if (!g_use_system_ssl)
      SSLClientSocketNSS::ClearSessionCache();
#else
    NOTIMPLEMENTED();
#endif
  }

};

static base::LazyInstance<DefaultClientSocketFactory>
    g_default_client_socket_factory(base::LINKER_INITIALIZED);

}  // namespace

// Deprecated function (http://crbug.com/37810) that takes a ClientSocket.
SSLClientSocket* ClientSocketFactory::CreateSSLClientSocket(
    ClientSocket* transport_socket,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config,
    SSLHostInfo* ssl_host_info,
    CertVerifier* cert_verifier) {
  ClientSocketHandle* socket_handle = new ClientSocketHandle();
  socket_handle->set_socket(transport_socket);
  return CreateSSLClientSocket(socket_handle, host_and_port, ssl_config,
                               ssl_host_info, cert_verifier,
                               NULL /* DnsCertProvenanceChecker */);
}

// static
ClientSocketFactory* ClientSocketFactory::GetDefaultFactory() {
  return g_default_client_socket_factory.Pointer();
}

// static
void ClientSocketFactory::UseSystemSSL() {
  g_use_system_ssl = true;
}

}  // namespace net
