// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_factory.h"

#include "base/singleton.h"
#include "build/build_config.h"
#include "net/socket/client_socket_handle.h"
#if defined(OS_WIN)
#include "net/socket/ssl_client_socket_win.h"
#elif defined(USE_OPENSSL)
#include "net/socket/ssl_client_socket_openssl.h"
#elif defined(USE_NSS)
#include "net/socket/ssl_client_socket_nss.h"
#elif defined(OS_MACOSX)
#include "net/socket/ssl_client_socket_mac.h"
#include "net/socket/ssl_client_socket_nss.h"
#endif
#include "net/socket/tcp_client_socket.h"

namespace net {

namespace {

SSLClientSocket* DefaultSSLClientSocketFactory(
    ClientSocketHandle* transport_socket,
    const std::string& hostname,
    const SSLConfig& ssl_config) {
#if defined(OS_WIN)
  return new SSLClientSocketWin(transport_socket, hostname, ssl_config);
#elif defined(USE_OPENSSL)
  return new SSLClientSocketOpenSSL(transport_socket, hostname, ssl_config);
#elif defined(USE_NSS)
  return new SSLClientSocketNSS(transport_socket, hostname, ssl_config);
#elif defined(OS_MACOSX)
  // TODO(wtc): SSLClientSocketNSS can't do SSL client authentication using
  // Mac OS X CDSA/CSSM yet (http://crbug.com/45369), so fall back on
  // SSLClientSocketMac.
  if (ssl_config.send_client_cert)
    return new SSLClientSocketMac(transport_socket, hostname, ssl_config);

  return new SSLClientSocketNSS(transport_socket, hostname, ssl_config);
#else
  NOTIMPLEMENTED();
  return NULL;
#endif
}

SSLClientSocketFactory g_ssl_factory = DefaultSSLClientSocketFactory;

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
      const std::string& hostname,
      const SSLConfig& ssl_config) {
    return g_ssl_factory(transport_socket, hostname, ssl_config);
  }
};

}  // namespace

// static
ClientSocketFactory* ClientSocketFactory::GetDefaultFactory() {
  return Singleton<DefaultClientSocketFactory>::get();
}

// static
void ClientSocketFactory::SetSSLClientSocketFactory(
    SSLClientSocketFactory factory) {
  g_ssl_factory = factory;
}

// Deprecated function (http://crbug.com/37810) that takes a ClientSocket.
SSLClientSocket* ClientSocketFactory::CreateSSLClientSocket(
    ClientSocket* transport_socket,
    const std::string& hostname,
    const SSLConfig& ssl_config) {
  ClientSocketHandle* socket_handle = new ClientSocketHandle();
  socket_handle->set_socket(transport_socket);
  return CreateSSLClientSocket(socket_handle, hostname, ssl_config);
}

}  // namespace net
