// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_SERVER_SOCKET_NSS_H_
#define NET_SOCKET_SSL_SERVER_SOCKET_NSS_H_
#pragma once

#include <certt.h>
#include <keyt.h>
#include <nspr.h>
#include <nss.h>

#include "base/memory/scoped_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_log.h"
#include "net/base/nss_memio.h"
#include "net/base/ssl_config_service.h"
#include "net/socket/ssl_server_socket.h"

namespace net {

class SSLServerSocketNSS : public SSLServerSocket {
 public:
  // This object takes ownership of the following parameters:
  // |socket| - A socket that is already connected.
  // |cert| - The certificate to be used by the server.
  //
  // The following parameters are copied in the constructor.
  // |ssl_config| - Options for SSL socket.
  // |key| - The private key used by the server.
  SSLServerSocketNSS(Socket* transport_socket,
                     scoped_refptr<X509Certificate> cert,
                     crypto::RSAPrivateKey* key,
                     const SSLConfig& ssl_config);
  virtual ~SSLServerSocketNSS();

  // SSLServerSocket implementation.
  virtual int Accept(CompletionCallback* callback);
  virtual int Read(IOBuffer* buf, int buf_len,
                   CompletionCallback* callback);
  virtual int Write(IOBuffer* buf, int buf_len,
                    CompletionCallback* callback);
  virtual bool SetReceiveBufferSize(int32 size);
  virtual bool SetSendBufferSize(int32 size);

 private:
  enum State {
    STATE_NONE,
    STATE_HANDSHAKE,
  };

  int InitializeSSLOptions();

  void OnSendComplete(int result);
  void OnRecvComplete(int result);
  void OnHandshakeIOComplete(int result);

  int BufferSend();
  void BufferSendComplete(int result);
  int BufferRecv();
  void BufferRecvComplete(int result);
  bool DoTransportIO();
  int DoPayloadRead();
  int DoPayloadWrite();

  int DoHandshakeLoop(int last_io_result);
  int DoReadLoop(int result);
  int DoWriteLoop(int result);
  int DoHandshake();
  void DoAcceptCallback(int result);
  void DoReadCallback(int result);
  void DoWriteCallback(int result);

  static SECStatus OwnAuthCertHandler(void* arg,
                                      PRFileDesc* socket,
                                      PRBool checksig,
                                      PRBool is_server);
  static void HandshakeCallback(PRFileDesc* socket, void* arg);

  virtual int Init();

  // Members used to send and receive buffer.
  CompletionCallbackImpl<SSLServerSocketNSS> buffer_send_callback_;
  CompletionCallbackImpl<SSLServerSocketNSS> buffer_recv_callback_;
  bool transport_send_busy_;
  bool transport_recv_busy_;

  scoped_refptr<IOBuffer> recv_buffer_;

  BoundNetLog net_log_;

  CompletionCallback* user_accept_callback_;
  CompletionCallback* user_read_callback_;
  CompletionCallback* user_write_callback_;

  // Used by Read function.
  scoped_refptr<IOBuffer> user_read_buf_;
  int user_read_buf_len_;

  // Used by Write function.
  scoped_refptr<IOBuffer> user_write_buf_;
  int user_write_buf_len_;

  // The NSS SSL state machine
  PRFileDesc* nss_fd_;

  // Buffers for the network end of the SSL state machine
  memio_Private* nss_bufs_;

  // Socket for sending and receiving data.
  scoped_ptr<Socket> transport_socket_;

  // Options for the SSL socket.
  // TODO(hclam): This memeber is currently not used. Should make use of this
  // member to configure the socket.
  SSLConfig ssl_config_;

  // Certificate for the server.
  scoped_refptr<X509Certificate> cert_;

  // Private key used by the server.
  scoped_ptr<crypto::RSAPrivateKey> key_;

  State next_handshake_state_;
  bool completed_handshake_;

  DISALLOW_COPY_AND_ASSIGN(SSLServerSocketNSS);
};

}  // namespace net

#endif  // NET_SOCKET_SSL_SERVER_SOCKET_NSS_H_
