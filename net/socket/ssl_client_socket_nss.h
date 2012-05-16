// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "base/timer.h"
#include "net/base/cert_verify_result.h"
#include "net/base/completion_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/base/net_log.h"
#include "net/base/nss_memio.h"
#include "net/base/server_bound_cert_service.h"
#include "net/base/ssl_config_service.h"
#include "net/base/x509_certificate.h"
#include "net/socket/ssl_client_socket.h"

namespace net {

class BoundNetLog;
class CertVerifier;
class ClientSocketHandle;
class ServerBoundCertService;
class SingleRequestCertVerifier;
class SSLHostInfo;
class TransportSecurityState;
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
                     const SSLClientSocketContext& context);
  virtual ~SSLClientSocketNSS();

  // SSLClientSocket implementation.
  virtual void GetSSLInfo(SSLInfo* ssl_info) OVERRIDE;
  virtual void GetSSLCertRequestInfo(
      SSLCertRequestInfo* cert_request_info) OVERRIDE;
  virtual int ExportKeyingMaterial(const base::StringPiece& label,
                                   bool has_context,
                                   const base::StringPiece& context,
                                   unsigned char* out,
                                   unsigned int outlen) OVERRIDE;
  virtual NextProtoStatus GetNextProto(std::string* proto,
                                       std::string* server_protos) OVERRIDE;

  // StreamSocket implementation.
  virtual int Connect(const CompletionCallback& callback) OVERRIDE;
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

  // Socket implementation.
  virtual int Read(IOBuffer* buf,
                   int buf_len,
                   const CompletionCallback& callback) OVERRIDE;
  virtual int Write(IOBuffer* buf,
                    int buf_len,
                    const CompletionCallback& callback) OVERRIDE;
  virtual bool SetReceiveBufferSize(int32 size) OVERRIDE;
  virtual bool SetSendBufferSize(int32 size) OVERRIDE;
  virtual ServerBoundCertService* GetServerBoundCertService() const OVERRIDE;

 private:
  enum State {
    STATE_NONE,
    STATE_LOAD_SSL_HOST_INFO,
    STATE_HANDSHAKE,
    STATE_GET_DOMAIN_BOUND_CERT_COMPLETE,
    STATE_VERIFY_DNSSEC,
    STATE_VERIFY_CERT,
    STATE_VERIFY_CERT_COMPLETE,
  };

  int Init();

  // Initializes NSS SSL options.  Returns a net error code.
  int InitializeSSLOptions();

  // Initializes the socket peer name in SSL.  Returns a net error code.
  int InitializeSSLPeerName();

  void UpdateServerCert();
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

  bool LoadSSLHostInfo();
  int DoLoadSSLHostInfo();

  int DoHandshake();

  // ImportDBCertAndKey is a helper function for turning a DER-encoded cert and
  // key into a CERTCertificate and SECKEYPrivateKey. Returns OK upon success
  // and an error code otherwise.
  // Requires |domain_bound_private_key_| and |domain_bound_cert_| to have been
  // set by a call to ServerBoundCertService->GetDomainBoundCert. The caller
  // takes ownership of the |*cert| and |*key|.
  int ImportDBCertAndKey(CERTCertificate** cert, SECKEYPrivateKey** key);
  int DoGetDBCertComplete(int result);
  int DoVerifyDNSSEC(int result);
  int DoVerifyCert(int result);
  int DoVerifyCertComplete(int result);
  int DoPayloadRead();
  int DoPayloadWrite();
  void LogConnectionTypeMetrics() const;
  void SaveSSLHostInfo();

  bool DoTransportIO();
  int BufferSend();
  void BufferSendComplete(int result);
  int BufferRecv();
  void BufferRecvComplete(int result);

  // Handles an NSS error generated while handshaking or performing IO.
  // Returns a network error code mapped from the original NSS error.
  int HandleNSSError(PRErrorCode error, bool handshake_error);

  // NSS calls this when checking certificates. We pass 'this' as the first
  // argument.
  static SECStatus OwnAuthCertHandler(void* arg, PRFileDesc* socket,
                                      PRBool checksig, PRBool is_server);
  // Returns true if connection negotiated the domain bound cert extension.
  static bool DomainBoundCertNegotiated(PRFileDesc* socket);
  // Domain bound cert client auth handler.
  // Returns the value the ClientAuthHandler function should return.
  SECStatus DomainBoundClientAuthHandler(
      const SECItem* cert_types,
      CERTCertificate** result_certificate,
      SECKEYPrivateKey** result_private_key);
#if defined(NSS_PLATFORM_CLIENT_AUTH)
  // On platforms where we use the native certificate store, NSS calls this
  // instead when client authentication is requested.  At most one of
  // (result_certs, result_private_key) or
  // (result_nss_certificate, result_nss_private_key) should be set.
  static SECStatus PlatformClientAuthHandler(
      void* arg,
      PRFileDesc* socket,
      CERTDistNames* ca_names,
      CERTCertList** result_certs,
      void** result_private_key,
      CERTCertificate** result_nss_certificate,
      SECKEYPrivateKey** result_nss_private_key);
#else
  // NSS calls this when client authentication is requested.
  static SECStatus ClientAuthHandler(void* arg,
                                     PRFileDesc* socket,
                                     CERTDistNames* ca_names,
                                     CERTCertificate** result_certificate,
                                     SECKEYPrivateKey** result_private_key);
#endif
  // Record histograms for DBC support.  The histogram will only be updated if
  // this socket did a full handshake.
  void RecordDomainBoundCertSupport() const;

  // NSS calls this when handshake is completed.  We pass 'this' as the second
  // argument.
  static void HandshakeCallback(PRFileDesc* socket, void* arg);

  static SECStatus NextProtoCallback(void* arg,
                                     PRFileDesc* fd,
                                     const unsigned char* protos,
                                     unsigned int protos_len,
                                     unsigned char* proto_out,
                                     unsigned int* proto_out_len,
                                     unsigned int proto_max_len);

  // The following methods are for debugging bug 65948. Will remove this code
  // after fixing bug 65948.
  void EnsureThreadIdAssigned() const;
  bool CalledOnValidThread() const;

  bool transport_send_busy_;
  bool transport_recv_busy_;
  bool transport_recv_eof_;
  scoped_refptr<IOBuffer> recv_buffer_;

  scoped_ptr<ClientSocketHandle> transport_;
  HostPortPair host_and_port_;
  SSLConfig ssl_config_;

  CompletionCallback user_connect_callback_;
  CompletionCallback user_read_callback_;
  CompletionCallback user_write_callback_;

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
  std::vector<SHA1Fingerprint> side_pinned_public_keys_;
  int ssl_connection_status_;

  // Stores client authentication information between ClientAuthHandler and
  // GetSSLCertRequestInfo calls.
  std::vector<scoped_refptr<X509Certificate> > client_certs_;
  bool client_auth_cert_needed_;

  CertVerifier* const cert_verifier_;
  scoped_ptr<SingleRequestCertVerifier> verifier_;

  // For domain bound certificates in client auth.
  bool domain_bound_cert_xtn_negotiated_;
  ServerBoundCertService* server_bound_cert_service_;
  SSLClientCertType domain_bound_cert_type_;
  std::string domain_bound_private_key_;
  std::string domain_bound_cert_;
  ServerBoundCertService::RequestHandle domain_bound_cert_request_handle_;

  // True if NSS has called HandshakeCallback.
  bool handshake_callback_called_;

  // True if the SSL handshake has been completed.
  bool completed_handshake_;

  // ssl_session_cache_shard_ is an opaque string that partitions the SSL
  // session cache. i.e. sessions created with one value will not attempt to
  // resume on the socket with a different value.
  const std::string ssl_session_cache_shard_;

  // True iff we believe that the user has an ESET product intercepting our
  // HTTPS connections.
  bool eset_mitm_detected_;

  // True iff |ssl_host_info_| contained a predicted certificate chain and
  // that we found the prediction to be correct.
  bool predicted_cert_chain_correct_;

  State next_handshake_state_;

  // The NSS SSL state machine
  PRFileDesc* nss_fd_;

  // Buffers for the network end of the SSL state machine
  memio_Private* nss_bufs_;

  BoundNetLog net_log_;

  base::TimeTicks start_cert_verification_time_;

  scoped_ptr<SSLHostInfo> ssl_host_info_;

  TransportSecurityState* transport_security_state_;

  // next_proto_ is the protocol that we selected by NPN.
  std::string next_proto_;
  NextProtoStatus next_proto_status_;
  // Server's NPN advertised protocols.
  std::string server_protos_;

  // The following two variables are added for debugging bug 65948. Will
  // remove this code after fixing bug 65948.
  // Added the following code Debugging in release mode.
  mutable base::Lock lock_;
  // This is mutable so that CalledOnValidThread can set it.
  // It's guarded by |lock_|.
  mutable base::PlatformThreadId valid_thread_id_;
};

}  // namespace net

#endif  // NET_SOCKET_SSL_CLIENT_SOCKET_NSS_H_
