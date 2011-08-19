// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "net/socket/ssl_server_socket.h"

namespace net {

// TODO(bulach): Rather than disable components which call
// CreateSSLServerSocket when building for OpenSSL rather than NSS, just
// provide a stub for it for now.
SSLServerSocket* CreateSSLServerSocket(StreamSocket* socket,
                                       X509Certificate* certificate,
                                       crypto::RSAPrivateKey* key,
                                       const SSLConfig& ssl_config) {
  NOTIMPLEMENTED();
  delete socket;
  return NULL;
}

}  // namespace net
