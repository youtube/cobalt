// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_CLIENT_SOCKET_MAC_H_
#define NET_SOCKET_SSL_CLIENT_SOCKET_MAC_H_

#include <Security/Security.h>

#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "net/base/cert_verify_result.h"
#include "net/base/completion_callback.h"
#include "net/base/ssl_config_service.h"
#include "net/socket/ssl_client_socket.h"

namespace net {

class CertVerifier;
class LoadLog;

// An SSL client socket implemented with Secure Transport.
class SSLClientSocketMac : public SSLClientSocket {
 public:
  // Takes ownership of the transport_socket, which may already be connected.
  // The given hostname will be compared with the name(s) in the server's
  // certificate during the SSL handshake. ssl_config specifies the SSL
  // settings.
  SSLClientSocketMac(ClientSocket* transport_socket,
                     const std::string& hostname,
                     const SSLConfig& ssl_config);
  ~SSLClientSocketMac();

  // SSLClientSocket methods:
  virtual void GetSSLInfo(SSLInfo* ssl_info);
  virtual void GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info);

  // ClientSocket methods:
  virtual int Connect(CompletionCallback* callback, LoadLog* load_log);
  virtual void Disconnect();
  virtual bool IsConnected() const;
  virtual bool IsConnectedAndIdle() const;

  // Socket methods:
  virtual int Read(IOBuffer* buf, int buf_len, CompletionCallback* callback);
  virtual int Write(IOBuffer* buf, int buf_len, CompletionCallback* callback);
  virtual bool SetReceiveBufferSize(int32 size);
  virtual bool SetSendBufferSize(int32 size);

 private:
  // Initializes the SSLContext.  Returns a net error code.
  int InitializeSSLContext();

  void DoConnectCallback(int result);
  void DoReadCallback(int result);
  void DoWriteCallback(int result);
  void OnHandshakeIOComplete(int result);
  void OnTransportReadComplete(int result);
  void OnTransportWriteComplete(int result);

  int DoHandshakeLoop(int last_io_result);

  int DoPayloadRead();
  int DoPayloadWrite();
  int DoHandshakeStart();
  int DoVerifyCert();
  int DoVerifyCertComplete(int result);
  int DoHandshakeFinish();

  static OSStatus SSLReadCallback(SSLConnectionRef connection,
                                  void* data,
                                  size_t* data_length);
  static OSStatus SSLWriteCallback(SSLConnectionRef connection,
                                   const void* data,
                                   size_t* data_length);

  CompletionCallbackImpl<SSLClientSocketMac> handshake_io_callback_;
  CompletionCallbackImpl<SSLClientSocketMac> transport_read_callback_;
  CompletionCallbackImpl<SSLClientSocketMac> transport_write_callback_;

  scoped_ptr<ClientSocket> transport_;
  std::string hostname_;
  SSLConfig ssl_config_;

  CompletionCallback* user_connect_callback_;
  CompletionCallback* user_read_callback_;
  CompletionCallback* user_write_callback_;

  // Used by Read function.
  scoped_refptr<IOBuffer> user_read_buf_;
  int user_read_buf_len_;

  // Used by Write function.
  scoped_refptr<IOBuffer> user_write_buf_;
  int user_write_buf_len_;

  enum State {
    STATE_NONE,
    STATE_HANDSHAKE_START,
    STATE_VERIFY_CERT,
    STATE_VERIFY_CERT_COMPLETE,
    STATE_HANDSHAKE_FINISH,
  };
  State next_handshake_state_;

  scoped_refptr<X509Certificate> server_cert_;
  std::vector<scoped_refptr<X509Certificate> > intermediate_certs_;
  scoped_ptr<CertVerifier> verifier_;
  CertVerifyResult server_cert_verify_result_;

  bool completed_handshake_;
  bool handshake_interrupted_;
  SSLContextRef ssl_context_;

  // These are buffers for holding data during I/O. The "slop" is the amount of
  // space at the ends of the receive buffer that are allocated for holding data
  // but don't (yet).
  std::vector<char> send_buffer_;
  int pending_send_error_;
  std::vector<char> recv_buffer_;
  int recv_buffer_head_slop_;
  int recv_buffer_tail_slop_;

  // This buffer holds data for Read() operations on the underlying transport
  // (ClientSocket::Read()).
  scoped_refptr<IOBuffer> read_io_buf_;

  scoped_refptr<LoadLog> load_log_;
};

}  // namespace net

#endif  // NET_SOCKET_SSL_CLIENT_SOCKET_MAC_H_
