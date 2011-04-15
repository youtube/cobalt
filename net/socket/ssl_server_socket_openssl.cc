// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "net/socket/ssl_server_socket.h"

namespace net {

namespace {

class SSLServerSocketOpenSSL : public SSLServerSocket {
 public:
  virtual ~SSLServerSocketOpenSSL() {}

  // SSLServerSocket
  virtual int Accept(CompletionCallback* callback) {
    // TODO(bulach): implement.
    NOTIMPLEMENTED();
    return 0;
  }

  // Socket
  virtual int Read(IOBuffer* buf, int buf_len,
                   CompletionCallback* callback) {
    // TODO(bulach): implement.
    NOTIMPLEMENTED();
    return 0;
  }
  virtual int Write(IOBuffer* buf, int buf_len,
                    CompletionCallback* callback) {
    // TODO(bulach): implement.
    NOTIMPLEMENTED();
    return 0;
  }

  virtual bool SetReceiveBufferSize(int32 size) {
    // TODO(bulach): implement.
    NOTIMPLEMENTED();
    return false;
  }

  virtual bool SetSendBufferSize(int32 size) {
    // TODO(bulach): implement.
    NOTIMPLEMENTED();
    return false;
  }
};

}  // namespace

SSLServerSocket* CreateSSLServerSocket(Socket* socket,
                                       X509Certificate* certificate,
                                       crypto::RSAPrivateKey* key,
                                       const SSLConfig& ssl_config) {
  return new SSLServerSocketOpenSSL();
}

}  // namespace net
