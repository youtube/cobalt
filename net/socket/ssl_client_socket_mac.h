// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_CLIENT_SOCKET_MAC_H_
#define NET_SOCKET_SSL_CLIENT_SOCKET_MAC_H_
#pragma once

#include <Security/Security.h>

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "net/base/cert_verify_result.h"
#include "net/base/completion_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_log.h"
#include "net/base/ssl_config_service.h"
#include "net/socket/ssl_client_socket.h"

namespace net {

class CertVerifier;
class ClientSocketHandle;
class SingleRequestCertVerifier;

// An SSL client socket implemented with Secure Transport.
class SSLClientSocketMac : public SSLClientSocket {
 public:
  // Takes ownership of the |transport_socket|, which must already be connected.
  // The hostname specified in |host_and_port| will be compared with the name(s)
  // in the server's certificate during the SSL handshake.  If SSL client
  // authentication is requested, the host_and_port field of SSLCertRequestInfo
  // will be populated with |host_and_port|.  |ssl_config| specifies
  // the SSL settings.
  SSLClientSocketMac(ClientSocketHandle* transport_socket,
                     const HostPortPair& host_and_port,
                     const SSLConfig& ssl_config,
                     const SSLClientSocketContext& context);
  virtual ~SSLClientSocketMac();

  // SSLClientSocket methods:
  virtual void GetSSLInfo(SSLInfo* ssl_info) OVERRIDE;
  virtual void GetSSLCertRequestInfo(
      SSLCertRequestInfo* cert_request_info) OVERRIDE;
  virtual int ExportKeyingMaterial(const base::StringPiece& label,
                                   const base::StringPiece& context,
                                   unsigned char *out,
                                   unsigned int outlen) OVERRIDE;
  virtual NextProtoStatus GetNextProto(std::string* proto,
                                       std::string* server_protos) OVERRIDE;

  // StreamSocket methods:
  virtual int Connect(OldCompletionCallback* callback) OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual bool IsConnected() const OVERRIDE;
  virtual bool IsConnectedAndIdle() const OVERRIDE;
  virtual int GetPeerAddress(AddressList* address) const OVERRIDE;
  virtual int GetLocalAddress(IPEndPoint* address) const OVERRIDE;
  virtual const BoundNetLog& NetLog() const OVERRIDE;
  virtual void SetSubresourceSpeculation() OVERRIDE;
  virtual void SetOmniboxSpeculation() OVERRIDE;
  virtual bool WasEverUsed() const OVERRIDE;
  virtual bool UsingTCPFastOpen() const OVERRIDE;
  virtual int64 NumBytesRead() const OVERRIDE;
  virtual base::TimeDelta GetConnectTimeMicros() const OVERRIDE;

  // Socket methods:
  virtual int Read(IOBuffer* buf,
                   int buf_len,
                   OldCompletionCallback* callback) OVERRIDE;
  virtual int Write(IOBuffer* buf,
                    int buf_len,
                    OldCompletionCallback* callback) OVERRIDE;
  virtual bool SetReceiveBufferSize(int32 size) OVERRIDE;
  virtual bool SetSendBufferSize(int32 size) OVERRIDE;

 private:
  bool completed_handshake() const {
    return next_handshake_state_ == STATE_COMPLETED_HANDSHAKE;
  }
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
  int DoHandshake();
  int DoVerifyCert();
  int DoVerifyCertComplete(int result);
  int DoCompletedRenegotiation(int result);

  void DidCompleteRenegotiation();
  int DidCompleteHandshake();

  int SetClientCert();

  static OSStatus SSLReadCallback(SSLConnectionRef connection,
                                  void* data,
                                  size_t* data_length);
  static OSStatus SSLWriteCallback(SSLConnectionRef connection,
                                   const void* data,
                                   size_t* data_length);

  OldCompletionCallbackImpl<SSLClientSocketMac> transport_read_callback_;
  OldCompletionCallbackImpl<SSLClientSocketMac> transport_write_callback_;

  scoped_ptr<ClientSocketHandle> transport_;
  HostPortPair host_and_port_;
  SSLConfig ssl_config_;

  OldCompletionCallback* user_connect_callback_;
  OldCompletionCallback* user_read_callback_;
  OldCompletionCallback* user_write_callback_;

  // Used by Read function.
  scoped_refptr<IOBuffer> user_read_buf_;
  int user_read_buf_len_;

  // Used by Write function.
  scoped_refptr<IOBuffer> user_write_buf_;
  int user_write_buf_len_;

  enum State {
    STATE_NONE,
    STATE_HANDSHAKE,
    STATE_VERIFY_CERT,
    STATE_VERIFY_CERT_COMPLETE,
    STATE_COMPLETED_RENEGOTIATION,
    STATE_COMPLETED_HANDSHAKE,
    // After the handshake, the socket remains in the
    // STATE_COMPLETED_HANDSHAKE state until renegotiation is requested by
    // the server. When renegotiation is requested, the state machine
    // restarts at STATE_HANDSHAKE, advances through to
    // STATE_VERIFY_CERT_COMPLETE, and then continues to
    // STATE_COMPLETED_RENEGOTIATION. After STATE_COMPLETED_RENEGOTIATION
    // has been processed, it goes back to STATE_COMPLETED_HANDSHAKE and
    // will remain there until the server requests renegotiation again.
    // During the initial handshake, STATE_COMPLETED_RENEGOTIATION is
    // skipped.
  };
  State next_handshake_state_;

  scoped_refptr<X509Certificate> server_cert_;
  CertVerifier* const cert_verifier_;
  scoped_ptr<SingleRequestCertVerifier> verifier_;
  CertVerifyResult server_cert_verify_result_;

  // The initial handshake has already completed, and the current handshake
  // is server-initiated renegotiation.
  bool renegotiating_;
  bool client_cert_requested_;
  SSLContextRef ssl_context_;

  // During a renegotiation, the amount of application data read following
  // the handshake's completion.
  size_t bytes_read_after_renegotiation_;

  // These buffers hold data retrieved from/sent to the underlying transport
  // before it's fed to the SSL engine.
  std::vector<char> send_buffer_;
  int pending_send_error_;
  std::vector<char> recv_buffer_;

  // These are the IOBuffers used for operations on the underlying transport.
  scoped_refptr<IOBuffer> read_io_buf_;
  scoped_refptr<IOBuffer> write_io_buf_;

  BoundNetLog net_log_;
};

}  // namespace net

#endif  // NET_SOCKET_SSL_CLIENT_SOCKET_MAC_H_
