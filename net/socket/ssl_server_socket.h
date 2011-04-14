// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_SERVER_SOCKET_H_
#define NET_SOCKET_SSL_SERVER_SOCKET_H_

#include "base/basictypes.h"
#include "net/base/completion_callback.h"
#include "net/socket/socket.h"

namespace crypto {
class RSAPrivateKey;
}  // namespace base

namespace net {

class IOBuffer;
struct SSLConfig;
class X509Certificate;

// SSLServerSocket takes an already connected socket and performs SSL on top of
// it.
//
// This class is designed to work in a peer-to-peer connection and is not
// intended to be used as a standalone SSL server.
class SSLServerSocket : public Socket {
 public:
  virtual ~SSLServerSocket() {}

  // Performs an SSL server handshake on the existing socket. The given socket
  // must have already been connected.
  //
  // Accept either returns ERR_IO_PENDING, in which case the given callback
  // will be called in the future with the real result, or it completes
  // synchronously, returning the result immediately.
  virtual int Accept(CompletionCallback* callback) = 0;
};

// Creates an SSL server socket using an already connected socket. A certificate
// and private key needs to be provided.
//
// This created server socket will take ownership of |socket|. However |key|
// is copied.
// TODO(hclam): Defines ServerSocketFactory to create SSLServerSocket. This will
// make mocking easier.
SSLServerSocket* CreateSSLServerSocket(
    Socket* socket, X509Certificate* certificate, crypto::RSAPrivateKey* key,
    const SSLConfig& ssl_config);

}  // namespace net

#endif  // NET_SOCKET_SSL_SERVER_SOCKET_NSS_H_
