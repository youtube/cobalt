// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_CLIENT_SOCKET_NSS_H_
#define NET_SOCKET_SSL_CLIENT_SOCKET_NSS_H_
#pragma once

#include <certt.h>
#include <keyt.h>
#include <nspr.h>
#include <nss.h>

#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "base/time.h"
#include "net/base/cert_verify_result.h"
#include "net/base/completion_callback.h"
#include "net/base/net_log.h"
#include "net/base/nss_memio.h"
#include "net/base/ssl_config_service.h"
#include "net/base/x509_certificate.h"
#include "net/socket/ssl_client_socket.h"

namespace net {

class BoundNetLog;
class CertVerifier;
class ClientSocketHandle;
class X509Certificate;

// An SSL client socket implemented with Mozilla NSS.
class SSLClientSocketNSS : public SSLClientSocket {
 public:
  // Takes ownership of the |transport_socket|, which must already be connected.
  // The given hostname will be compared with the name(s) in the server's
  // certificate during the SSL handshake.  ssl_config specifies the SSL
  // settings.
  SSLClientSocketNSS(ClientSocketHandle* transport_socket,
                     const std::string& hostname,
                     const SSLConfig& ssl_config);
  ~SSLClientSocketNSS();

  // SSLClientSocket methods:
  virtual void GetSSLInfo(SSLInfo* ssl_info);
  virtual void GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info);
  virtual NextProtoStatus GetNextProto(std::string* proto);
  virtual void UseDNSSEC(DNSSECProvider*);

  // ClientSocket methods:
  virtual int Connect(CompletionCallback* callback);
  virtual void Disconnect();
  virtual bool IsConnected() const;
  virtual bool IsConnectedAndIdle() const;
  virtual int GetPeerAddress(AddressList* address) const;
  virtual const BoundNetLog& NetLog() const { return net_log_; }
  virtual void SetSubresourceSpeculation();
  virtual void SetOmniboxSpeculation();
  virtual bool WasEverUsed() const;

  // Socket methods:
  virtual int Read(IOBuffer* buf, int buf_len, CompletionCallback* callback);
  virtual int Write(IOBuffer* buf, int buf_len, CompletionCallback* callback);
  virtual bool SetReceiveBufferSize(int32 size);
  virtual bool SetSendBufferSize(int32 size);

  void set_handshake_callback_called() { handshake_callback_called_ = true; }

 private:
  // Initializes NSS SSL options.  Returns a net error code.
  int InitializeSSLOptions();

  void InvalidateSessionIfBadCertificate();
#if defined(OS_MACOSX) || defined(OS_WIN)
  // Creates an OS certificate from a DER-encoded certificate.
  static X509Certificate::OSCertHandle CreateOSCert(const SECItem& der_cert);
#endif
  X509Certificate* UpdateServerCert();
  void CheckSecureRenegotiation() const;
  void DoReadCallback(int result);
  void DoWriteCallback(int result);
  void DoConnectCallback(int result);
  void OnHandshakeIOComplete(int result);
  void OnSendComplete(int result);
  void OnRecvComplete(int result);

  int DoHandshakeLoop(int last_io_result);
  int DoReadLoop(int result);
  int DoWriteLoop(int result);

  int DoHandshake();

  int DoVerifyDNSSEC(int result);
  int DoVerifyDNSSECComplete(int result);
  int DoVerifyCert(int result);
  int DoVerifyCertComplete(int result);
  int DoPayloadRead();
  int DoPayloadWrite();
  int Init();

  bool DoTransportIO();
  int BufferSend(void);
  int BufferRecv(void);
  void BufferSendComplete(int result);
  void BufferRecvComplete(int result);

  // NSS calls this when checking certificates. We pass 'this' as the first
  // argument.
  static SECStatus OwnAuthCertHandler(void* arg, PRFileDesc* socket,
                                      PRBool checksig, PRBool is_server);
  // NSS calls this when client authentication is requested.
  static SECStatus ClientAuthHandler(void* arg,
                                     PRFileDesc* socket,
                                     CERTDistNames* ca_names,
                                     CERTCertificate** result_certificate,
                                     SECKEYPrivateKey** result_private_key);
  // NSS calls this when handshake is completed.  We pass 'this' as the second
  // argument.
  static void HandshakeCallback(PRFileDesc* socket, void* arg);

  CompletionCallbackImpl<SSLClientSocketNSS> buffer_send_callback_;
  CompletionCallbackImpl<SSLClientSocketNSS> buffer_recv_callback_;
  bool transport_send_busy_;
  bool transport_recv_busy_;
  // corked_ is true if we are currently suspending writes to the network. This
  // is named after the similar kernel flag, TCP_CORK.
  bool corked_;
  scoped_refptr<IOBuffer> recv_buffer_;

  CompletionCallbackImpl<SSLClientSocketNSS> handshake_io_callback_;
  scoped_ptr<ClientSocketHandle> transport_;
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

  // Set when handshake finishes.  The server certificate is first received
  // from NSS as an NSS certificate handle (server_cert_nss_), and then
  // converted into an X509Certificate object (server_cert_).
  scoped_refptr<X509Certificate> server_cert_;
  CERTCertificate* server_cert_nss_;
  CertVerifyResult server_cert_verify_result_;

  // Stores client authentication information between ClientAuthHandler and
  // GetSSLCertRequestInfo calls.
  std::vector<scoped_refptr<X509Certificate> > client_certs_;
  bool client_auth_cert_needed_;

  scoped_ptr<CertVerifier> verifier_;

  // True if NSS has called HandshakeCallback.
  bool handshake_callback_called_;

  // True if the SSL handshake has been completed.
  bool completed_handshake_;

  // This pointer is owned by the caller of UseDNSSEC.
  DNSSECProvider* dnssec_provider_;
  // The time when we started waiting for DNSSEC records.
  base::Time dnssec_wait_start_time_;

  enum State {
    STATE_NONE,
    STATE_HANDSHAKE,
    STATE_VERIFY_DNSSEC,
    STATE_VERIFY_DNSSEC_COMPLETE,
    STATE_VERIFY_CERT,
    STATE_VERIFY_CERT_COMPLETE,
  };
  State next_handshake_state_;

  // The NSS SSL state machine
  PRFileDesc* nss_fd_;

  // Buffers for the network end of the SSL state machine
  memio_Private* nss_bufs_;

  BoundNetLog net_log_;

#if defined(OS_WIN)
  // A CryptoAPI in-memory certificate store.  We use it for two purposes:
  // 1. Import server certificates into this store so that we can verify and
  //    display the certificates using CryptoAPI.
  // 2. Copy client certificates from the "MY" system certificate store into
  //    this store so that we can close the system store when we finish
  //    searching for client certificates.
  static HCERTSTORE cert_store_;
#endif
};

}  // namespace net

#endif  // NET_SOCKET_SSL_CLIENT_SOCKET_NSS_H_
