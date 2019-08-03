// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "net/socket/ssl_server_socket.h"

// TODO(bulach): Provide simple stubs for EnableSSLServerSockets and
// CreateSSLServerSocket so that when building for OpenSSL rather than NSS,
// so that the code using SSL server sockets can be compiled and disabled
// programatically rather than requiring to be carved out from the compile.

namespace net {

void EnableSSLServerSockets() {
  NOTIMPLEMENTED();
}

SSLServerSocket* CreateSSLServerSocket(StreamSocket* socket,
                                       X509Certificate* certificate,
                                       crypto::RSAPrivateKey* key,
                                       const SSLConfig& ssl_config) {
  NOTIMPLEMENTED();
  delete socket;
  return NULL;
}

}  // namespace net
