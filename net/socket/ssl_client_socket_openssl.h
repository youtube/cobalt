// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_CLIENT_SOCKET_OPENSSL_H_
#define NET_SOCKET_SSL_CLIENT_SOCKET_OPENSSL_H_
#pragma once

#include "base/scoped_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/ssl_config_service.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/client_socket_handle.h"

typedef struct bio_st BIO;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_st SSL;

namespace net {

class SSLCertRequestInfo;
class SSLConfig;
class SSLInfo;

// An SSL client socket implemented with OpenSSL.
class SSLClientSocketOpenSSL : public SSLClientSocket {
 public:
  // Takes ownership of the transport_socket, which may already be connected.
  // The given hostname will be compared with the name(s) in the server's
  // certificate during the SSL handshake.  ssl_config specifies the SSL
  // settings.
  SSLClientSocketOpenSSL(ClientSocketHandle* transport_socket,
                         const std::string& hostname,
                         const SSLConfig& ssl_config);
  ~SSLClientSocketOpenSSL();

  // SSLClientSocket methods:
  virtual void GetSSLInfo(SSLInfo* ssl_info);
  virtual void GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info);
  virtual NextProtoStatus GetNextProto(std::string* proto);

  // ClientSocket methods:
  virtual int Connect(CompletionCallback* callback);
  virtual void Disconnect();
  virtual bool IsConnected() const;
  virtual bool IsConnectedAndIdle() const;
  virtual int GetPeerAddress(AddressList*) const;
  virtual const BoundNetLog& NetLog() const;
  virtual void SetSubresourceSpeculation();
  virtual void SetOmniboxSpeculation();
  virtual bool WasEverUsed() const;

  // Socket methods:
  virtual int Read(IOBuffer* buf, int buf_len, CompletionCallback* callback);
  virtual int Write(IOBuffer* buf, int buf_len, CompletionCallback* callback);
  virtual bool SetReceiveBufferSize(int32 size);
  virtual bool SetSendBufferSize(int32 size);

 private:
  bool InitOpenSSL();
  bool Init();
  void DoReadCallback(int result);
  void DoWriteCallback(int result);

  bool DoTransportIO();
  int DoHandshake();
  void DoConnectCallback(int result);

  void OnHandshakeIOComplete(int result);
  void OnSendComplete(int result);
  void OnRecvComplete(int result);

  int DoHandshakeLoop(int last_io_result);
  int DoReadLoop(int result);
  int DoWriteLoop(int result);
  int DoPayloadRead();
  int DoPayloadWrite();

  int BufferSend();
  int BufferRecv();
  void BufferSendComplete(int result);
  void BufferRecvComplete(int result);
  void TransportWriteComplete(int result);
  void TransportReadComplete(int result);

  CompletionCallbackImpl<SSLClientSocketOpenSSL> buffer_send_callback_;
  CompletionCallbackImpl<SSLClientSocketOpenSSL> buffer_recv_callback_;
  bool transport_send_busy_;
  scoped_refptr<DrainableIOBuffer> send_buffer_;
  bool transport_recv_busy_;
  scoped_refptr<IOBuffer> recv_buffer_;

  CompletionCallback* user_connect_callback_;
  CompletionCallback* user_read_callback_;
  CompletionCallback* user_write_callback_;

  // Used by Read function.
  scoped_refptr<IOBuffer> user_read_buf_;
  int user_read_buf_len_;

  // Used by Write function.
  scoped_refptr<IOBuffer> user_write_buf_;
  int user_write_buf_len_;

  // Stores client authentication information between ClientAuthHandler and
  // GetSSLCertRequestInfo calls.
  std::vector<scoped_refptr<X509Certificate> > client_certs_;
  bool client_auth_cert_needed_;

  // OpenSSL stuff
  static SSL_CTX* g_ctx;
  SSL* ssl_;
  BIO* transport_bio_;

  scoped_ptr<ClientSocketHandle> transport_;
  std::string hostname_;
  SSLConfig ssl_config_;

  bool completed_handshake_;

  enum State {
    STATE_NONE,
    STATE_HANDSHAKE,
    STATE_VERIFY_CERT,
    STATE_VERIFY_CERT_COMPLETE,
  };
  State next_handshake_state_;
  BoundNetLog net_log_;
};

}  // namespace net

#endif // NET_SOCKET_SSL_CLIENT_SOCKET_OPENSSL_H_

