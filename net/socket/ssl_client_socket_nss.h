// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_CLIENT_SOCKET_NSS_H_
#define NET_SOCKET_SSL_CLIENT_SOCKET_NSS_H_

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

namespace base {
class SequencedTaskRunner;
}

namespace net {

class BoundNetLog;
class CertVerifier;
class ClientSocketHandle;
class ServerBoundCertService;
class SingleRequestCertVerifier;
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
  //
  // Because calls to NSS may block, such as due to needing to access slow
  // hardware or needing to synchronously unlock protected tokens, calls to
  // NSS may optionally be run on a dedicated thread. If synchronous/blocking
  // behaviour is desired, for performance or compatibility, the current task
  // runner should be supplied instead.
  SSLClientSocketNSS(base::SequencedTaskRunner* nss_task_runner,
                     ClientSocketHandle* transport_socket,
                     const HostPortPair& host_and_port,
                     const SSLConfig& ssl_config,
                     const SSLClientSocketContext& context);
  virtual ~SSLClientSocketNSS();

  // SSLClientSocket implementation.
  virtual void GetSSLCertRequestInfo(
      SSLCertRequestInfo* cert_request_info) OVERRIDE;
  virtual NextProtoStatus GetNextProto(std::string* proto,
                                       std::string* server_protos) OVERRIDE;

  // SSLSocket implementation.
  virtual int ExportKeyingMaterial(const base::StringPiece& label,
                                   bool has_context,
                                   const base::StringPiece& context,
                                   unsigned char* out,
                                   unsigned int outlen) OVERRIDE;
  virtual int GetTLSUniqueChannelBinding(std::string* out) OVERRIDE;

  // StreamSocket implementation.
  virtual int Connect(const CompletionCallback& callback) OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual bool IsConnected() const OVERRIDE;
  virtual bool IsConnectedAndIdle() const OVERRIDE;
  virtual int GetPeerAddress(IPEndPoint* address) const OVERRIDE;
  virtual int GetLocalAddress(IPEndPoint* address) const OVERRIDE;
  virtual const BoundNetLog& NetLog() const OVERRIDE;
  virtual void SetSubresourceSpeculation() OVERRIDE;
  virtual void SetOmniboxSpeculation() OVERRIDE;
  virtual bool WasEverUsed() const OVERRIDE;
  virtual bool UsingTCPFastOpen() const OVERRIDE;
  virtual int64 NumBytesRead() const OVERRIDE;
  virtual base::TimeDelta GetConnectTimeMicros() const OVERRIDE;
  virtual bool GetSSLInfo(SSLInfo* ssl_info) OVERRIDE;

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
  // Helper class to handle marshalling any NSS interaction to and from the
  // NSS and network task runners. Not every call needs to happen on the Core
  class Core;

  enum State {
    STATE_NONE,
    STATE_HANDSHAKE,
    STATE_HANDSHAKE_COMPLETE,
    STATE_VERIFY_DNSSEC,
    STATE_VERIFY_CERT,
    STATE_VERIFY_CERT_COMPLETE,
  };

  int Init();
  void InitCore();

  // Initializes NSS SSL options.  Returns a net error code.
  int InitializeSSLOptions();

  // Initializes the socket peer name in SSL.  Returns a net error code.
  int InitializeSSLPeerName();

  void DoConnectCallback(int result);
  void OnHandshakeIOComplete(int result);

  int DoHandshakeLoop(int last_io_result);
  int DoHandshake();
  int DoHandshakeComplete(int result);
  int DoVerifyDNSSEC(int result);
  int DoVerifyCert(int result);
  int DoVerifyCertComplete(int result);

  void LogConnectionTypeMetrics() const;

  // The following methods are for debugging bug 65948. Will remove this code
  // after fixing bug 65948.
  void EnsureThreadIdAssigned() const;
  bool CalledOnValidThread() const;

  // The task runner used to perform NSS operations.
  scoped_refptr<base::SequencedTaskRunner> nss_task_runner_;
  scoped_ptr<ClientSocketHandle> transport_;
  HostPortPair host_and_port_;
  SSLConfig ssl_config_;

  scoped_refptr<Core> core_;

  CompletionCallback user_connect_callback_;

  CertVerifyResult server_cert_verify_result_;
  HashValueVector side_pinned_public_keys_;

  CertVerifier* const cert_verifier_;
  scoped_ptr<SingleRequestCertVerifier> verifier_;

  // The service for retrieving Channel ID keys.  May be NULL.
  ServerBoundCertService* server_bound_cert_service_;

  // ssl_session_cache_shard_ is an opaque string that partitions the SSL
  // session cache. i.e. sessions created with one value will not attempt to
  // resume on the socket with a different value.
  const std::string ssl_session_cache_shard_;

  // True if the SSL handshake has been completed.
  bool completed_handshake_;

  State next_handshake_state_;

  // The NSS SSL state machine. This is owned by |core_|.
  // TODO(rsleevi): http://crbug.com/130616 - Remove this member once
  // ExportKeyingMaterial is updated to be asynchronous.
  PRFileDesc* nss_fd_;

  BoundNetLog net_log_;

  base::TimeTicks start_cert_verification_time_;

  TransportSecurityState* transport_security_state_;

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
