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
#include "base/timer.h"
#include "net/base/cert_verify_result.h"
#include "net/base/completion_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_log.h"
#include "net/base/nss_memio.h"
#include "net/base/ssl_config_service.h"
#include "net/base/x509_certificate.h"
#include "net/socket/ssl_client_socket.h"

namespace net {

class BoundNetLog;
class CertVerifier;
class ClientSocketHandle;
class DnsCertProvenanceChecker;
class SingleRequestCertVerifier;
class SSLHostInfo;
class X509Certificate;

// An SSL client socket implemented with Mozilla NSS.
class SSLClientSocketNSS : public SSLClientSocket {
 public:
  // Takes ownership of the |transport_socket|, which must already be connected.
  // The hostname specified in |host_and_port| will be compared with the name(s)
  // in the server's certificate during the SSL handshake.  If SSL client
  // authentication is requested, the host_and_port field of SSLCertRequestInfo
  // will be populated with |host_and_port|.  |ssl_config| specifies
  // the SSL settings.
  SSLClientSocketNSS(ClientSocketHandle* transport_socket,
                     const HostPortPair& host_and_port,
                     const SSLConfig& ssl_config,
                     SSLHostInfo* ssl_host_info,
                     CertVerifier* cert_verifier,
                     DnsCertProvenanceChecker* dnsrr_resolver);
  ~SSLClientSocketNSS();

  // For tests
  static void ClearSessionCache();

  // SSLClientSocket methods:
  virtual void GetSSLInfo(SSLInfo* ssl_info);
  virtual void GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info);
  virtual NextProtoStatus GetNextProto(std::string* proto);
  virtual void UseDNSSEC(DNSSECProvider* provider);

  // ClientSocket methods:
  virtual int Connect(CompletionCallback* callback);
  virtual void Disconnect();
  virtual bool IsConnected() const;
  virtual bool IsConnectedAndIdle() const;
  virtual int GetPeerAddress(AddressList* address) const;
  virtual const BoundNetLog& NetLog() const;
  virtual void SetSubresourceSpeculation();
  virtual void SetOmniboxSpeculation();
  virtual bool WasEverUsed() const;
  virtual bool UsingTCPFastOpen() const;

  // Socket methods:
  virtual int Read(IOBuffer* buf, int buf_len, CompletionCallback* callback);
  virtual int Write(IOBuffer* buf, int buf_len, CompletionCallback* callback);
  virtual bool SetReceiveBufferSize(int32 size);
  virtual bool SetSendBufferSize(int32 size);

 private:
  enum State {
    STATE_NONE,
    STATE_SNAP_START_LOAD_INFO,
    STATE_SNAP_START_WAIT_FOR_WRITE,
    STATE_HANDSHAKE,
    STATE_VERIFY_DNSSEC,
    STATE_VERIFY_DNSSEC_COMPLETE,
    STATE_VERIFY_CERT,
    STATE_VERIFY_CERT_COMPLETE,
  };

  int Init();

  // Initializes NSS SSL options.  Returns a net error code.
  int InitializeSSLOptions();

  // Initializes the socket peer name in SSL.  Returns a net error code.
  int InitializeSSLPeerName();

#if defined(OS_MACOSX) || defined(OS_WIN)
  // Creates an OS certificate from a DER-encoded certificate.
  static X509Certificate::OSCertHandle CreateOSCert(const SECItem& der_cert);
#endif
  X509Certificate* UpdateServerCert();
  void UpdateConnectionStatus();
  void DoReadCallback(int result);
  void DoWriteCallback(int result);
  void DoConnectCallback(int result);
  void OnHandshakeIOComplete(int result);
  void OnSendComplete(int result);
  void OnRecvComplete(int result);

  int DoHandshakeLoop(int last_io_result);
  int DoReadLoop(int result);
  int DoWriteLoop(int result);

  int DoSnapStartLoadInfo();
  int DoSnapStartWaitForWrite();
  int DoHandshake();

  int DoVerifyDNSSEC(int result);
  int DoVerifyDNSSECComplete(int result);
  int DoVerifyCert(int result);
  int DoVerifyCertComplete(int result);
  int DoPayloadRead();
  int DoPayloadWrite();
  void LogConnectionTypeMetrics() const;
  void SaveSnapStartInfo();
  bool LoadSnapStartInfo();
  bool IsNPNProtocolMispredicted();
  void UncorkAfterTimeout();

  bool DoTransportIO();
  int BufferSend(void);
  void BufferSendComplete(int result);
  int BufferRecv(void);
  void BufferRecvComplete(int result);

  // Handles an NSS error generated while handshaking or performing IO.
  // Returns a network error code mapped from the original NSS error.
  int HandleNSSError(PRErrorCode error, bool handshake_error);

  // NSS calls this when checking certificates. We pass 'this' as the first
  // argument.
  static SECStatus OwnAuthCertHandler(void* arg, PRFileDesc* socket,
                                      PRBool checksig, PRBool is_server);
  // NSS calls this when client authentication is requested.
#if defined(NSS_PLATFORM_CLIENT_AUTH)
  static SECStatus PlatformClientAuthHandler(void* arg,
                                             PRFileDesc* socket,
                                             CERTDistNames* ca_names,
                                             CERTCertList** result_certs,
                                             void** result_private_key);
#else
  static SECStatus ClientAuthHandler(void* arg,
                                     PRFileDesc* socket,
                                     CERTDistNames* ca_names,
                                     CERTCertificate** result_certificate,
                                     SECKEYPrivateKey** result_private_key);
#endif
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
  // uncork_timer_ is used to limit the amount of time that we'll delay the
  // Finished message while waiting for a Write.
  base::OneShotTimer<SSLClientSocketNSS> uncork_timer_;
  scoped_refptr<IOBuffer> recv_buffer_;

  CompletionCallbackImpl<SSLClientSocketNSS> handshake_io_callback_;
  scoped_ptr<ClientSocketHandle> transport_;
  HostPortPair host_and_port_;
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
  // |server_cert_verify_result_| points at the verification result, which may,
  // or may not be, |&local_server_cert_verify_result_|, depending on whether
  // we used an SSLHostInfo's verification.
  const CertVerifyResult* server_cert_verify_result_;
  CertVerifyResult local_server_cert_verify_result_;
  int ssl_connection_status_;

  // Stores client authentication information between ClientAuthHandler and
  // GetSSLCertRequestInfo calls.
  std::vector<scoped_refptr<X509Certificate> > client_certs_;
  bool client_auth_cert_needed_;

  CertVerifier* const cert_verifier_;
  scoped_ptr<SingleRequestCertVerifier> verifier_;

  // True if NSS has called HandshakeCallback.
  bool handshake_callback_called_;

  // True if the SSL handshake has been completed.
  bool completed_handshake_;

  // True if we are lying about being connected in order to merge the first
  // Write call into a Snap Start handshake.
  bool pseudo_connected_;

  // True iff we believe that the user has an ESET product intercepting our
  // HTTPS connections.
  bool eset_mitm_detected_;

  // True iff |ssl_host_info_| contained a predicted certificate chain and
  // that we found the prediction to be correct.
  bool predicted_cert_chain_correct_;

  // True if the peer name has been initialized.
  bool peername_initialized_;

  // This pointer is owned by the caller of UseDNSSEC.
  DNSSECProvider* dnssec_provider_;
  // The time when we started waiting for DNSSEC records.
  base::Time dnssec_wait_start_time_;

  State next_handshake_state_;

  // The NSS SSL state machine
  PRFileDesc* nss_fd_;

  // Buffers for the network end of the SSL state machine
  memio_Private* nss_bufs_;

  BoundNetLog net_log_;

  // When performing Snap Start we need to predict the NPN protocol which the
  // server is going to speak before we actually perform the handshake. Thus
  // the last NPN protocol used is serialised in |ssl_host_info_|
  // and kept in these fields:
  SSLClientSocket::NextProtoStatus predicted_npn_status_;
  std::string predicted_npn_proto_;
  bool predicted_npn_proto_used_;

  base::TimeTicks start_cert_verification_time_;

  scoped_ptr<SSLHostInfo> ssl_host_info_;
  DnsCertProvenanceChecker* const dns_cert_checker_;
};

}  // namespace net

#endif  // NET_SOCKET_SSL_CLIENT_SOCKET_NSS_H_
