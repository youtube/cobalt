// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_CLIENT_SOCKET_WIN_H_
#define NET_SOCKET_SSL_CLIENT_SOCKET_WIN_H_
#pragma once

#define SECURITY_WIN32  // Needs to be defined before including security.h

#include <windows.h>
#include <wincrypt.h>
#include <security.h>

#include <string>

#include "base/memory/scoped_ptr.h"
#include "net/base/cert_verify_result.h"
#include "net/base/completion_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_log.h"
#include "net/base/ssl_config_service.h"
#include "net/socket/ssl_client_socket.h"

namespace net {

class BoundNetLog;
class CertVerifier;
class ClientSocketHandle;
class HostPortPair;
class SingleRequestCertVerifier;

// An SSL client socket implemented with the Windows Schannel.
class SSLClientSocketWin : public SSLClientSocket {
 public:
  // Takes ownership of the |transport_socket|, which must already be connected.
  // The hostname specified in |host_and_port| will be compared with the name(s)
  // in the server's certificate during the SSL handshake.  If SSL client
  // authentication is requested, the host_and_port field of SSLCertRequestInfo
  // will be populated with |host_and_port|.  |ssl_config| specifies
  // the SSL settings.
  SSLClientSocketWin(ClientSocketHandle* transport_socket,
                     const HostPortPair& host_and_port,
                     const SSLConfig& ssl_config,
                     const SSLClientSocketContext& context);
  ~SSLClientSocketWin();

  // SSLClientSocket methods:
  virtual void GetSSLInfo(SSLInfo* ssl_info);
  virtual void GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info);
  virtual int ExportKeyingMaterial(const base::StringPiece& label,
                                   const base::StringPiece& context,
                                   unsigned char *out,
                                   unsigned int outlen);
  virtual NextProtoStatus GetNextProto(std::string* proto,
                                       std::string* server_protos);

  // StreamSocket methods:
  virtual int Connect(OldCompletionCallback* callback);
  virtual void Disconnect();
  virtual bool IsConnected() const;
  virtual bool IsConnectedAndIdle() const;
  virtual int GetPeerAddress(AddressList* address) const;
  virtual int GetLocalAddress(IPEndPoint* address) const;
  virtual const BoundNetLog& NetLog() const { return net_log_; }
  virtual void SetSubresourceSpeculation();
  virtual void SetOmniboxSpeculation();
  virtual bool WasEverUsed() const;
  virtual bool UsingTCPFastOpen() const;
  virtual int64 NumBytesRead() const;
  virtual base::TimeDelta GetConnectTimeMicros() const;

  // Socket methods:
  virtual int Read(IOBuffer* buf, int buf_len, OldCompletionCallback* callback);
  virtual int Write(IOBuffer* buf, int buf_len, OldCompletionCallback* callback);

  virtual bool SetReceiveBufferSize(int32 size);
  virtual bool SetSendBufferSize(int32 size);

 private:
  bool completed_handshake() const {
    return next_state_ == STATE_COMPLETED_HANDSHAKE;
  }

  // Initializes the SSL options and security context. Returns a net error code.
  int InitializeSSLContext();

  void OnHandshakeIOComplete(int result);
  void OnReadComplete(int result);
  void OnWriteComplete(int result);

  int DoLoop(int last_io_result);
  int DoHandshakeRead();
  int DoHandshakeReadComplete(int result);
  int DoHandshakeWrite();
  int DoHandshakeWriteComplete(int result);
  int DoVerifyCert();
  int DoVerifyCertComplete(int result);

  int DoPayloadRead();
  int DoPayloadReadComplete(int result);
  int DoPayloadDecrypt();
  int DoPayloadEncrypt();
  int DoPayloadWrite();
  int DoPayloadWriteComplete(int result);
  int DoCompletedRenegotiation(int result);

  int DidCallInitializeSecurityContext();
  int DidCompleteHandshake();
  void DidCompleteRenegotiation();
  void LogConnectionTypeMetrics() const;
  void FreeSendBuffer();

  // Internal callbacks as async operations complete.
  OldCompletionCallbackImpl<SSLClientSocketWin> handshake_io_callback_;
  OldCompletionCallbackImpl<SSLClientSocketWin> read_callback_;
  OldCompletionCallbackImpl<SSLClientSocketWin> write_callback_;

  scoped_ptr<ClientSocketHandle> transport_;
  HostPortPair host_and_port_;
  SSLConfig ssl_config_;

  // User function to callback when the Connect() completes.
  OldCompletionCallback* user_connect_callback_;

  // User function to callback when a Read() completes.
  OldCompletionCallback* user_read_callback_;
  scoped_refptr<IOBuffer> user_read_buf_;
  int user_read_buf_len_;

  // User function to callback when a Write() completes.
  OldCompletionCallback* user_write_callback_;
  scoped_refptr<IOBuffer> user_write_buf_;
  int user_write_buf_len_;

  // Used to Read and Write using transport_.
  scoped_refptr<IOBuffer> transport_read_buf_;
  scoped_refptr<IOBuffer> transport_write_buf_;

  enum State {
    STATE_NONE,
    STATE_HANDSHAKE_READ,
    STATE_HANDSHAKE_READ_COMPLETE,
    STATE_HANDSHAKE_WRITE,
    STATE_HANDSHAKE_WRITE_COMPLETE,
    STATE_VERIFY_CERT,
    STATE_VERIFY_CERT_COMPLETE,
    STATE_COMPLETED_RENEGOTIATION,
    STATE_COMPLETED_HANDSHAKE
    // After the handshake, the socket remains
    // in the STATE_COMPLETED_HANDSHAKE state,
    // unless a renegotiate handshake occurs.
  };
  State next_state_;

  SecPkgContext_StreamSizes stream_sizes_;
  scoped_refptr<X509Certificate> server_cert_;
  CertVerifier* const cert_verifier_;
  scoped_ptr<SingleRequestCertVerifier> verifier_;
  CertVerifyResult server_cert_verify_result_;

  CredHandle* creds_;
  CtxtHandle ctxt_;
  SecBuffer in_buffers_[2];  // Input buffers for InitializeSecurityContext.
  SecBuffer send_buffer_;  // Output buffer for InitializeSecurityContext.
  SECURITY_STATUS isc_status_;  // Return value of InitializeSecurityContext.
  scoped_array<char> payload_send_buffer_;
  int payload_send_buffer_len_;
  int bytes_sent_;

  // recv_buffer_ holds the received ciphertext.  Since Schannel decrypts
  // data in place, sometimes recv_buffer_ may contain decrypted plaintext and
  // any undecrypted ciphertext.  (Ciphertext is decrypted one full SSL record
  // at a time.)
  //
  // If bytes_decrypted_ is 0, the received ciphertext is at the beginning of
  // recv_buffer_, ready to be passed to DecryptMessage.
  scoped_array<char> recv_buffer_;
  char* decrypted_ptr_;  // Points to the decrypted plaintext in recv_buffer_
  int bytes_decrypted_;  // The number of bytes of decrypted plaintext.
  char* received_ptr_;  // Points to the received ciphertext in recv_buffer_
  int bytes_received_;  // The number of bytes of received ciphertext.

  // True if we're writing the first token (handshake message) to the server,
  // false if we're writing a subsequent token.  After we have written a token
  // successfully, DoHandshakeWriteComplete checks this member to set the next
  // state.
  bool writing_first_token_;

  // Only used in the STATE_HANDSHAKE_READ_COMPLETE and
  // STATE_PAYLOAD_READ_COMPLETE states.  True if a 'result' argument of OK
  // should be ignored, to prevent it from being interpreted as EOF.
  //
  // The reason we need this flag is that OK means not only "0 bytes of data
  // were read" but also EOF.  We set ignore_ok_result_ to true when we need
  // to continue processing previously read data without reading more data.
  // We have to pass a 'result' of OK to the DoLoop method, and don't want it
  // to be interpreted as EOF.
  bool ignore_ok_result_;

  // Renegotiation is in progress.
  bool renegotiating_;

  // True when the decrypter needs more data in order to decrypt.
  bool need_more_data_;

  BoundNetLog net_log_;
};

}  // namespace net

#endif  // NET_SOCKET_SSL_CLIENT_SOCKET_WIN_H_
