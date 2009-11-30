// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file includes code SSLClientSocketNSS::DoVerifyCertComplete() derived
// from AuthCertificateCallback() in
// mozilla/security/manager/ssl/src/nsNSSCallbacks.cpp.

/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ian McGreer <mcgreer@netscape.com>
 *   Javier Delgadillo <javi@netscape.com>
 *   Kai Engert <kengert@redhat.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "net/socket/ssl_client_socket_nss.h"

#include <certdb.h>
#include <keyhi.h>
#include <nspr.h>
#include <nss.h>
#include <secerr.h>
// Work around https://bugzilla.mozilla.org/show_bug.cgi?id=455424
// until NSS 3.12.2 comes out and we update to it.
#define Lock FOO_NSS_Lock
#include <ssl.h>
#include <sslerr.h>
#include <pk11pub.h>
#undef Lock

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/nss_init.h"
#include "base/string_util.h"
#include "net/base/cert_verifier.h"
#include "net/base/io_buffer.h"
#include "net/base/load_log.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_cert_request_info.h"
#include "net/base/ssl_info.h"
#include "net/ocsp/nss_ocsp.h"

static const int kRecvBufferSize = 4096;

namespace net {

// State machines are easier to debug if you log state transitions.
// Enable these if you want to see what's going on.
#if 1
#define EnterFunction(x)
#define LeaveFunction(x)
#define GotoState(s) next_handshake_state_ = s
#define LogData(s, len)
#else
#define EnterFunction(x)  LOG(INFO) << (void *)this << " " << __FUNCTION__ << \
                           " enter " << x << \
                           "; next_handshake_state " << next_handshake_state_
#define LeaveFunction(x)  LOG(INFO) << (void *)this << " " << __FUNCTION__ << \
                           " leave " << x << \
                           "; next_handshake_state " << next_handshake_state_
#define GotoState(s) do { LOG(INFO) << (void *)this << " " << __FUNCTION__ << \
                           " jump to state " << s; \
                           next_handshake_state_ = s; } while (0)
#define LogData(s, len)   LOG(INFO) << (void *)this << " " << __FUNCTION__ << \
                           " data [" << std::string(s, len) << "]";

#endif

namespace {

int NetErrorFromNSPRError(PRErrorCode err) {
  // TODO(port): fill this out as we learn what's important
  switch (err) {
    case PR_WOULD_BLOCK_ERROR:
      return ERR_IO_PENDING;
    case PR_ADDRESS_NOT_SUPPORTED_ERROR:  // For connect.
    case PR_NO_ACCESS_RIGHTS_ERROR:
      return ERR_ACCESS_DENIED;
    case PR_IO_TIMEOUT_ERROR:
      return ERR_TIMED_OUT;
    case PR_CONNECT_RESET_ERROR:
      return ERR_CONNECTION_RESET;
    case PR_CONNECT_ABORTED_ERROR:
      return ERR_CONNECTION_ABORTED;
    case PR_CONNECT_REFUSED_ERROR:
      return ERR_CONNECTION_REFUSED;
    case PR_HOST_UNREACHABLE_ERROR:
    case PR_NETWORK_UNREACHABLE_ERROR:
      return ERR_ADDRESS_UNREACHABLE;
    case PR_ADDRESS_NOT_AVAILABLE_ERROR:
      return ERR_ADDRESS_INVALID;

    case SSL_ERROR_NO_CYPHER_OVERLAP:
      return ERR_SSL_VERSION_OR_CIPHER_MISMATCH;
    case SSL_ERROR_BAD_CERT_DOMAIN:
      return ERR_CERT_COMMON_NAME_INVALID;
    case SEC_ERROR_EXPIRED_CERTIFICATE:
      return ERR_CERT_DATE_INVALID;
    case SEC_ERROR_BAD_SIGNATURE:
      return ERR_CERT_INVALID;
    case SSL_ERROR_REVOKED_CERT_ALERT:
    case SEC_ERROR_REVOKED_CERTIFICATE:
    case SEC_ERROR_REVOKED_KEY:
      return ERR_CERT_REVOKED;
    case SEC_ERROR_CA_CERT_INVALID:
    case SEC_ERROR_UNKNOWN_ISSUER:
    case SEC_ERROR_UNTRUSTED_CERT:
    case SEC_ERROR_UNTRUSTED_ISSUER:
      return ERR_CERT_AUTHORITY_INVALID;
    case SSL_ERROR_HANDSHAKE_FAILURE_ALERT:
      return ERR_SSL_PROTOCOL_ERROR;

    default: {
      if (IS_SSL_ERROR(err)) {
        LOG(WARNING) << "Unknown SSL error " << err <<
            " mapped to net::ERR_SSL_PROTOCOL_ERROR";
        return ERR_SSL_PROTOCOL_ERROR;
      }
      if (IS_SEC_ERROR(err)) {
        // TODO(port): Probably not the best mapping
        LOG(WARNING) << "Unknown SEC error " << err <<
            " mapped to net::ERR_CERT_INVALID";
        return ERR_CERT_INVALID;
      }
      LOG(WARNING) << "Unknown error " << err <<
          " mapped to net::ERR_FAILED";
      return ERR_FAILED;
    }
  }
}

}  // namespace

bool SSLClientSocketNSS::nss_options_initialized_ = false;

SSLClientSocketNSS::SSLClientSocketNSS(ClientSocket* transport_socket,
                                       const std::string& hostname,
                                       const SSLConfig& ssl_config)
    :
      buffer_send_callback_(this, &SSLClientSocketNSS::BufferSendComplete),
      buffer_recv_callback_(this, &SSLClientSocketNSS::BufferRecvComplete),
      transport_send_busy_(false),
      transport_recv_busy_(false),
      handshake_io_callback_(this, &SSLClientSocketNSS::OnHandshakeIOComplete),
      transport_(transport_socket),
      hostname_(hostname),
      ssl_config_(ssl_config),
      user_connect_callback_(NULL),
      user_read_callback_(NULL),
      user_write_callback_(NULL),
      user_read_buf_len_(0),
      user_write_buf_len_(0),
      client_auth_cert_needed_(false),
      completed_handshake_(false),
      next_handshake_state_(STATE_NONE),
      nss_fd_(NULL),
      nss_bufs_(NULL) {
  EnterFunction("");
}

SSLClientSocketNSS::~SSLClientSocketNSS() {
  EnterFunction("");
  Disconnect();
  LeaveFunction("");
}

int SSLClientSocketNSS::Init() {
  EnterFunction("");
  // Initialize NSS in a threadsafe way.
  base::EnsureNSSInit();
  // We must call EnsureOCSPInit() here, on the IO thread, to get the IO loop
  // by MessageLoopForIO::current().
  // X509Certificate::Verify() runs on a worker thread of CertVerifier.
  EnsureOCSPInit();

  LeaveFunction("");
  return OK;
}

int SSLClientSocketNSS::Connect(CompletionCallback* callback,
                                LoadLog* load_log) {
  EnterFunction("");
  DCHECK(transport_.get());
  DCHECK(next_handshake_state_ == STATE_NONE);
  DCHECK(!user_read_callback_);
  DCHECK(!user_write_callback_);
  DCHECK(!user_connect_callback_);
  DCHECK(!user_read_buf_);
  DCHECK(!user_write_buf_);

  LoadLog::BeginEvent(load_log, LoadLog::TYPE_SSL_CONNECT);

  if (Init() != OK) {
    NOTREACHED() << "Couldn't initialize nss";
  }

  int rv = InitializeSSLOptions();
  if (rv != OK) {
    LoadLog::EndEvent(load_log, LoadLog::TYPE_SSL_CONNECT);
    return rv;
  }

  GotoState(STATE_HANDSHAKE);
  rv = DoHandshakeLoop(OK);
  if (rv == ERR_IO_PENDING) {
    user_connect_callback_ = callback;
    load_log_ = load_log;
  } else {
    LoadLog::EndEvent(load_log, LoadLog::TYPE_SSL_CONNECT);
  }

  LeaveFunction("");
  return rv > OK ? OK : rv;
}

int SSLClientSocketNSS::InitializeSSLOptions() {
  // Transport connected, now hook it up to nss
  // TODO(port): specify rx and tx buffer sizes separately
  nss_fd_ = memio_CreateIOLayer(kRecvBufferSize);
  if (nss_fd_ == NULL) {
    return 9999;  // TODO(port): real error
  }

  // Tell NSS who we're connected to
  PRNetAddr peername;
  socklen_t len = sizeof(PRNetAddr);
  int err = transport_->GetPeerName((struct sockaddr *)&peername, &len);
  if (err) {
    DLOG(ERROR) << "GetPeerName failed";
    return 9999;  // TODO(port): real error
  }
  memio_SetPeerName(nss_fd_, &peername);

  // Grab pointer to buffers
  nss_bufs_ = memio_GetSecret(nss_fd_);

  /* Create SSL state machine */
  /* Push SSL onto our fake I/O socket */
  nss_fd_ = SSL_ImportFD(NULL, nss_fd_);
  if (nss_fd_ == NULL) {
      return ERR_SSL_PROTOCOL_ERROR;  // TODO(port): real error
  }
  // TODO(port): set more ssl options!  Check errors!

  int rv;

  rv = SSL_OptionSet(nss_fd_, SSL_SECURITY, PR_TRUE);
  if (rv != SECSuccess)
     return ERR_UNEXPECTED;

  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_SSL2, ssl_config_.ssl2_enabled);
  if (rv != SECSuccess)
     return ERR_UNEXPECTED;

  // SNI is enabled automatically if TLS is enabled -- as long as
  // SSL_V2_COMPATIBLE_HELLO isn't.
  // So don't do V2 compatible hellos unless we're really using SSL2,
  // to avoid errors like
  // "common name `mail.google.com' != requested host name `gmail.com'"
  rv = SSL_OptionSet(nss_fd_, SSL_V2_COMPATIBLE_HELLO,
                     ssl_config_.ssl2_enabled);
  if (rv != SECSuccess)
     return ERR_UNEXPECTED;

  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_SSL3, ssl_config_.ssl3_enabled);
  if (rv != SECSuccess)
     return ERR_UNEXPECTED;

  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_TLS, ssl_config_.tls1_enabled);
  if (rv != SECSuccess)
     return ERR_UNEXPECTED;

#ifdef SSL_ENABLE_SESSION_TICKETS
  // Support RFC 5077
  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_SESSION_TICKETS, PR_TRUE);
  if (rv != SECSuccess)
     LOG(INFO) << "SSL_ENABLE_SESSION_TICKETS failed.  Old system nss?";
#else
  #error "You need to install NSS-3.12 or later to build chromium"
#endif

#ifdef SSL_ENABLE_DEFLATE
  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_DEFLATE, PR_TRUE);
  if (rv != SECSuccess)
     LOG(INFO) << "SSL_ENABLE_DEFLATE failed.  Old system nss?";
#endif

#ifdef SSL_NEXT_PROTO_NEGOTIATED
  if (!ssl_config_.next_protos.empty()) {
    rv = SSL_SetNextProtoNego(
       nss_fd_,
       reinterpret_cast<const unsigned char *>(ssl_config_.next_protos.data()),
       ssl_config_.next_protos.size());
    if (rv != SECSuccess)
       LOG(INFO) << "SSL_SetNextProtoNego failed.";
  }
#endif

  rv = SSL_OptionSet(nss_fd_, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE);
  if (rv != SECSuccess)
     return ERR_UNEXPECTED;

  rv = SSL_AuthCertificateHook(nss_fd_, OwnAuthCertHandler, this);
  if (rv != SECSuccess)
     return ERR_UNEXPECTED;

  rv = SSL_GetClientAuthDataHook(nss_fd_, ClientAuthHandler, this);
  if (rv != SECSuccess)
     return ERR_UNEXPECTED;

  rv = SSL_HandshakeCallback(nss_fd_, HandshakeCallback, this);
  if (rv != SECSuccess)
    return ERR_UNEXPECTED;

  // Tell SSL the hostname we're trying to connect to.
  SSL_SetURL(nss_fd_, hostname_.c_str());

  // Set the peer ID for session reuse.  This is necessary when we create an
  // SSL tunnel through a proxy -- GetPeerName returns the proxy's address
  // rather than the destination server's address in that case.
  // TODO(wtc): port in peername is not the server's port when a proxy is used.
  std::string peer_id = StringPrintf("%s:%d", hostname_.c_str(),
                                     PR_ntohs(PR_NetAddrInetPort(&peername)));
  rv = SSL_SetSockPeerID(nss_fd_, const_cast<char*>(peer_id.c_str()));
  if (rv != SECSuccess)
    LOG(INFO) << "SSL_SetSockPeerID failed: peer_id=" << peer_id;

  // Tell SSL we're a client; needed if not letting NSPR do socket I/O
  SSL_ResetHandshake(nss_fd_, 0);

  return OK;
}

void SSLClientSocketNSS::InvalidateSessionIfBadCertificate() {
  if (UpdateServerCert() != NULL &&
      ssl_config_.IsAllowedBadCert(server_cert_)) {
    SSL_InvalidateSession(nss_fd_);
  }
}

void SSLClientSocketNSS::Disconnect() {
  EnterFunction("");

  // TODO(wtc): Send SSL close_notify alert.
  if (nss_fd_ != NULL) {
    InvalidateSessionIfBadCertificate();
    PR_Close(nss_fd_);
    nss_fd_ = NULL;
  }

  // Shut down anything that may call us back (through buffer_send_callback_,
  // buffer_recv_callback, or handshake_io_callback_).
  verifier_.reset();
  transport_->Disconnect();

  // Reset object state
  transport_send_busy_   = false;
  transport_recv_busy_   = false;
  user_connect_callback_ = NULL;
  user_read_callback_    = NULL;
  user_write_callback_   = NULL;
  user_read_buf_         = NULL;
  user_read_buf_len_     = 0;
  user_write_buf_        = NULL;
  user_write_buf_len_    = 0;
  server_cert_           = NULL;
  server_cert_verify_result_.Reset();
  completed_handshake_   = false;
  nss_bufs_              = NULL;
  client_certs_.clear();
  client_auth_cert_needed_ = false;

  LeaveFunction("");
}

bool SSLClientSocketNSS::IsConnected() const {
  // Ideally, we should also check if we have received the close_notify alert
  // message from the server, and return false in that case.  We're not doing
  // that, so this function may return a false positive.  Since the upper
  // layer (HttpNetworkTransaction) needs to handle a persistent connection
  // closed by the server when we send a request anyway, a false positive in
  // exchange for simpler code is a good trade-off.
  EnterFunction("");
  bool ret = completed_handshake_ && transport_->IsConnected();
  LeaveFunction("");
  return ret;
}

bool SSLClientSocketNSS::IsConnectedAndIdle() const {
  // Unlike IsConnected, this method doesn't return a false positive.
  //
  // Strictly speaking, we should check if we have received the close_notify
  // alert message from the server, and return false in that case.  Although
  // the close_notify alert message means EOF in the SSL layer, it is just
  // bytes to the transport layer below, so transport_->IsConnectedAndIdle()
  // returns the desired false when we receive close_notify.
  EnterFunction("");
  bool ret = completed_handshake_ && transport_->IsConnectedAndIdle();
  LeaveFunction("");
  return ret;
}

int SSLClientSocketNSS::Read(IOBuffer* buf, int buf_len,
                             CompletionCallback* callback) {
  EnterFunction(buf_len);
  DCHECK(completed_handshake_);
  DCHECK(next_handshake_state_ == STATE_NONE);
  DCHECK(!user_read_callback_);
  DCHECK(!user_connect_callback_);
  DCHECK(!user_read_buf_);
  DCHECK(nss_bufs_);

  user_read_buf_ = buf;
  user_read_buf_len_ = buf_len;

  int rv = DoReadLoop(OK);

  if (rv == ERR_IO_PENDING)
    user_read_callback_ = callback;
  else {
    user_read_buf_ = NULL;
    user_read_buf_len_ = 0;
  }
  LeaveFunction(rv);
  return rv;
}

int SSLClientSocketNSS::Write(IOBuffer* buf, int buf_len,
                              CompletionCallback* callback) {
  EnterFunction(buf_len);
  DCHECK(completed_handshake_);
  DCHECK(next_handshake_state_ == STATE_NONE);
  DCHECK(!user_write_callback_);
  DCHECK(!user_connect_callback_);
  DCHECK(!user_write_buf_);
  DCHECK(nss_bufs_);

  user_write_buf_ = buf;
  user_write_buf_len_ = buf_len;

  int rv = DoWriteLoop(OK);

  if (rv == ERR_IO_PENDING)
    user_write_callback_ = callback;
  else {
    user_write_buf_ = NULL;
    user_write_buf_len_ = 0;
  }
  LeaveFunction(rv);
  return rv;
}

bool SSLClientSocketNSS::SetReceiveBufferSize(int32 size) {
  return transport_->SetReceiveBufferSize(size);
}

bool SSLClientSocketNSS::SetSendBufferSize(int32 size) {
  return transport_->SetSendBufferSize(size);
}

X509Certificate *SSLClientSocketNSS::UpdateServerCert() {
  // We set the server_cert_ from OwnAuthCertHandler(), but this handler
  // does not necessarily get called if we are continuing a cached SSL
  // session.
  if (server_cert_ == NULL) {
    X509Certificate::OSCertHandle nss_cert = SSL_PeerCertificate(nss_fd_);
    if (nss_cert) {
      server_cert_ = X509Certificate::CreateFromHandle(
          nss_cert, X509Certificate::SOURCE_FROM_NETWORK);
    }
  }
  return server_cert_;
}

void SSLClientSocketNSS::GetSSLInfo(SSLInfo* ssl_info) {
  EnterFunction("");
  ssl_info->Reset();
  if (!server_cert_)
    return;

  SSLChannelInfo channel_info;
  SECStatus ok = SSL_GetChannelInfo(nss_fd_,
                                    &channel_info, sizeof(channel_info));
  if (ok == SECSuccess &&
      channel_info.length == sizeof(channel_info) &&
      channel_info.cipherSuite) {
    SSLCipherSuiteInfo cipher_info;
    ok = SSL_GetCipherSuiteInfo(channel_info.cipherSuite,
                                &cipher_info, sizeof(cipher_info));
    if (ok == SECSuccess) {
      ssl_info->security_bits = cipher_info.effectiveKeyBits;
    } else {
      ssl_info->security_bits = -1;
      LOG(DFATAL) << "SSL_GetCipherSuiteInfo returned " << PR_GetError()
                  << " for cipherSuite " << channel_info.cipherSuite;
    }
    UpdateServerCert();
  }
  ssl_info->cert_status = server_cert_verify_result_.cert_status;
  DCHECK(server_cert_ != NULL);
  ssl_info->cert = server_cert_;

#ifdef SSL_NEXT_PROTO_NEGOTIATED
  unsigned char npn_buf[255];
  unsigned npn_len;
  int npn_status;
  SECStatus rv = SSL_GetNextProto(nss_fd_, &npn_status, npn_buf, &npn_len,
                                  sizeof(npn_buf));
  if (rv != SECSuccess) {
    npn_status = SSL_NEXT_PROTO_NO_SUPPORT;
  }

  if (npn_status == SSL_NEXT_PROTO_NO_SUPPORT) {
    ssl_info->next_proto_status = SSLInfo::kNextProtoUnsupported;
    ssl_info->next_proto.clear();
  } else {
    ssl_info->next_proto =
      std::string(reinterpret_cast<const char *>(npn_buf), npn_len);
    switch (npn_status) {
      case SSL_NEXT_PROTO_NEGOTIATED:
        ssl_info->next_proto_status = SSLInfo::kNextProtoNegotiated;
        break;
      case SSL_NEXT_PROTO_NO_OVERLAP:
        ssl_info->next_proto_status = SSLInfo::kNextProtoNoOverlap;
        break;
      default:
        LOG(ERROR) << "Unknown npn_status: " << npn_status;
        ssl_info->next_proto_status = SSLInfo::kNextProtoNoOverlap;
        break;
    }
  }
#endif

  LeaveFunction("");
}

void SSLClientSocketNSS::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  EnterFunction("");
  cert_request_info->host_and_port = hostname_;
  cert_request_info->client_certs = client_certs_;
  LeaveFunction(cert_request_info->client_certs.size());
}

void SSLClientSocketNSS::DoReadCallback(int rv) {
  EnterFunction(rv);
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(user_read_callback_);

  // Since Run may result in Read being called, clear |user_read_callback_|
  // up front.
  CompletionCallback* c = user_read_callback_;
  user_read_callback_ = NULL;
  user_read_buf_ = NULL;
  user_read_buf_len_ = 0;
  c->Run(rv);
  LeaveFunction("");
}

void SSLClientSocketNSS::DoWriteCallback(int rv) {
  EnterFunction(rv);
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(user_write_callback_);

  // Since Run may result in Write being called, clear |user_write_callback_|
  // up front.
  CompletionCallback* c = user_write_callback_;
  user_write_callback_ = NULL;
  user_write_buf_ = NULL;
  user_write_buf_len_ = 0;
  c->Run(rv);
  LeaveFunction("");
}

// As part of Connect(), the SSLClientSocketNSS object performs an SSL
// handshake. This requires network IO, which in turn calls
// BufferRecvComplete() with a non-zero byte count. This byte count eventually
// winds its way through the state machine and ends up being passed to the
// callback. For Read() and Write(), that's what we want. But for Connect(),
// the caller expects OK (i.e. 0) for success.
//
void SSLClientSocketNSS::DoConnectCallback(int rv) {
  EnterFunction(rv);
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(user_connect_callback_);

  CompletionCallback* c = user_connect_callback_;
  user_connect_callback_ = NULL;
  c->Run(rv > OK ? OK : rv);
  LeaveFunction("");
}

void SSLClientSocketNSS::OnHandshakeIOComplete(int result) {
  EnterFunction(result);
  int rv = DoHandshakeLoop(result);
  if (rv != ERR_IO_PENDING) {
    LoadLog::EndEvent(load_log_, net::LoadLog::TYPE_SSL_CONNECT);
    load_log_ = NULL;
    DoConnectCallback(rv);
  }
  LeaveFunction("");
}

void SSLClientSocketNSS::OnSendComplete(int result) {
  EnterFunction(result);
  if (next_handshake_state_ != STATE_NONE) {
    // In handshake phase.
    OnHandshakeIOComplete(result);
    LeaveFunction("");
    return;
  }

  // OnSendComplete may need to call DoPayloadRead while the renegotiation
  // handshake is in progress.
  int rv_read = ERR_IO_PENDING;
  int rv_write = ERR_IO_PENDING;
  bool network_moved;
  do {
      if (user_read_buf_)
          rv_read = DoPayloadRead();
      if (user_write_buf_)
          rv_write = DoPayloadWrite();
      network_moved = DoTransportIO();
  } while (rv_read == ERR_IO_PENDING &&
           rv_write == ERR_IO_PENDING &&
           network_moved);

  if (user_read_buf_ && rv_read != ERR_IO_PENDING)
      DoReadCallback(rv_read);
  if (user_write_buf_ && rv_write != ERR_IO_PENDING)
      DoWriteCallback(rv_write);

  LeaveFunction("");
}

void SSLClientSocketNSS::OnRecvComplete(int result) {
  EnterFunction(result);
  if (next_handshake_state_ != STATE_NONE) {
    // In handshake phase.
    OnHandshakeIOComplete(result);
    LeaveFunction("");
    return;
  }

  // Network layer received some data, check if client requested to read
  // decrypted data.
  if (!user_read_buf_) {
    LeaveFunction("");
    return;
  }

  int rv = DoReadLoop(result);
  if (rv != ERR_IO_PENDING)
    DoReadCallback(rv);
  LeaveFunction("");
}

// Map a Chromium net error code to an NSS error code.
// See _MD_unix_map_default_error in the NSS source
// tree for inspiration.
static PRErrorCode MapErrorToNSS(int result) {
  if (result >=0)
    return result;

  switch (result) {
    case ERR_IO_PENDING:
      return PR_WOULD_BLOCK_ERROR;
    case ERR_ACCESS_DENIED:
      // For connect, this could be mapped to PR_ADDRESS_NOT_SUPPORTED_ERROR.
      return PR_NO_ACCESS_RIGHTS_ERROR;
    case ERR_INTERNET_DISCONNECTED:  // Equivalent to ENETDOWN.
      return PR_NETWORK_UNREACHABLE_ERROR;  // Best approximation.
    case ERR_CONNECTION_TIMED_OUT:
    case ERR_TIMED_OUT:
      return PR_IO_TIMEOUT_ERROR;
    case ERR_CONNECTION_RESET:
      return PR_CONNECT_RESET_ERROR;
    case ERR_CONNECTION_ABORTED:
      return PR_CONNECT_ABORTED_ERROR;
    case ERR_CONNECTION_REFUSED:
      return PR_CONNECT_REFUSED_ERROR;
    case ERR_ADDRESS_UNREACHABLE:
      return PR_HOST_UNREACHABLE_ERROR;  // Also PR_NETWORK_UNREACHABLE_ERROR.
    case ERR_ADDRESS_INVALID:
      return PR_ADDRESS_NOT_AVAILABLE_ERROR;
    default:
      LOG(WARNING) << "MapErrorToNSS " << result
                   << " mapped to PR_UNKNOWN_ERROR";
      return PR_UNKNOWN_ERROR;
  }
}

// Do network I/O between the given buffer and the given socket.
// Return true if some I/O performed, false otherwise (error or ERR_IO_PENDING)
bool SSLClientSocketNSS::DoTransportIO() {
  EnterFunction("");
  bool network_moved = false;
  if (nss_bufs_ != NULL) {
    int nsent = BufferSend();
    int nreceived = BufferRecv();
    network_moved = (nsent > 0 || nreceived >= 0);
  }
  LeaveFunction(network_moved);
  return network_moved;
}

// Return 0 for EOF,
// > 0 for bytes transferred immediately,
// < 0 for error (or the non-error ERR_IO_PENDING).
int SSLClientSocketNSS::BufferSend(void) {
  if (transport_send_busy_) return ERR_IO_PENDING;

  const char *buf;
  int nb = memio_GetWriteParams(nss_bufs_, &buf);
  EnterFunction(nb);

  int rv;
  if (!nb) {
    rv = OK;
  } else {
    scoped_refptr<IOBuffer> send_buffer = new IOBuffer(nb);
    memcpy(send_buffer->data(), buf, nb);
    rv = transport_->Write(send_buffer, nb, &buffer_send_callback_);
    if (rv == ERR_IO_PENDING)
      transport_send_busy_ = true;
    else
      memio_PutWriteResult(nss_bufs_, MapErrorToNSS(rv));
  }

  LeaveFunction(rv);
  return rv;
}

void SSLClientSocketNSS::BufferSendComplete(int result) {
  EnterFunction(result);
  memio_PutWriteResult(nss_bufs_, MapErrorToNSS(result));
  transport_send_busy_ = false;
  OnSendComplete(result);
  LeaveFunction("");
}


int SSLClientSocketNSS::BufferRecv(void) {
  if (transport_recv_busy_) return ERR_IO_PENDING;

  char *buf;
  int nb = memio_GetReadParams(nss_bufs_, &buf);
  EnterFunction(nb);
  int rv;
  if (!nb) {
    // buffer too full to read into, so no I/O possible at moment
    rv = ERR_IO_PENDING;
  } else {
    recv_buffer_ = new IOBuffer(nb);
    rv = transport_->Read(recv_buffer_, nb, &buffer_recv_callback_);
    if (rv == ERR_IO_PENDING) {
      transport_recv_busy_ = true;
    } else {
      if (rv > 0)
        memcpy(buf, recv_buffer_->data(), rv);
      memio_PutReadResult(nss_bufs_, MapErrorToNSS(rv));
      recv_buffer_ = NULL;
    }
  }
  LeaveFunction(rv);
  return rv;
}

void SSLClientSocketNSS::BufferRecvComplete(int result) {
  EnterFunction(result);
  if (result > 0) {
    char *buf;
    memio_GetReadParams(nss_bufs_, &buf);
    memcpy(buf, recv_buffer_->data(), result);
  }
  recv_buffer_ = NULL;
  memio_PutReadResult(nss_bufs_, MapErrorToNSS(result));
  transport_recv_busy_ = false;
  OnRecvComplete(result);
  LeaveFunction("");
}

int SSLClientSocketNSS::DoHandshakeLoop(int last_io_result) {
  EnterFunction(last_io_result);
  bool network_moved;
  int rv = last_io_result;
  do {
    // Default to STATE_NONE for next state.
    // (This is a quirk carried over from the windows
    // implementation.  It makes reading the logs a bit harder.)
    // State handlers can and often do call GotoState just
    // to stay in the current state.
    State state = next_handshake_state_;
    GotoState(STATE_NONE);
    switch (state) {
      case STATE_NONE:
        // we're just pumping data between the buffer and the network
        break;
      case STATE_HANDSHAKE:
        rv = DoHandshake();
        break;
      case STATE_VERIFY_CERT:
        DCHECK(rv == OK);
        rv = DoVerifyCert(rv);
        break;
      case STATE_VERIFY_CERT_COMPLETE:
        rv = DoVerifyCertComplete(rv);
        break;
      default:
        rv = ERR_UNEXPECTED;
        NOTREACHED() << "unexpected state";
        break;
    }

    // Do the actual network I/O
    network_moved = DoTransportIO();
  } while ((rv != ERR_IO_PENDING || network_moved) &&
            next_handshake_state_ != STATE_NONE);
  LeaveFunction("");
  return rv;
}

int SSLClientSocketNSS::DoReadLoop(int result) {
  EnterFunction("");
  DCHECK(completed_handshake_);
  DCHECK(next_handshake_state_ == STATE_NONE);

  if (result < 0)
    return result;

  if (!nss_bufs_)
    return ERR_UNEXPECTED;

  bool network_moved;
  int rv;
  do {
    rv = DoPayloadRead();
    network_moved = DoTransportIO();
  } while (rv == ERR_IO_PENDING && network_moved);

  LeaveFunction("");
  return rv;
}

int SSLClientSocketNSS::DoWriteLoop(int result) {
  EnterFunction("");
  DCHECK(completed_handshake_);
  DCHECK(next_handshake_state_ == STATE_NONE);

  if (result < 0)
    return result;

  if (!nss_bufs_)
    return ERR_UNEXPECTED;

  bool network_moved;
  int rv;
  do {
    rv = DoPayloadWrite();
    network_moved = DoTransportIO();
  } while (rv == ERR_IO_PENDING && network_moved);

  LeaveFunction("");
  return rv;
}

// static
// NSS calls this if an incoming certificate needs to be verified.
// Do nothing but return SECSuccess.
// This is called only in full handshake mode.
// Peer certificate is retrieved in HandshakeCallback() later, which is called
// in full handshake mode or in resumption handshake mode.
SECStatus SSLClientSocketNSS::OwnAuthCertHandler(void* arg,
                                                 PRFileDesc* socket,
                                                 PRBool checksig,
                                                 PRBool is_server) {
  // Tell NSS to not verify the certificate.
  return SECSuccess;
}

// static
// NSS calls this if a client certificate is needed.
// Based on Mozilla's NSS_GetClientAuthData.
SECStatus SSLClientSocketNSS::ClientAuthHandler(
    void* arg,
    PRFileDesc* socket,
    CERTDistNames* ca_names,
    CERTCertificate** result_certificate,
    SECKEYPrivateKey** result_private_key) {
  SSLClientSocketNSS* that = reinterpret_cast<SSLClientSocketNSS*>(arg);

  that->client_auth_cert_needed_ = !that->ssl_config_.send_client_cert;

  CERTCertificate* cert = NULL;
  SECKEYPrivateKey* privkey = NULL;
  void* wincx  = SSL_RevealPinArg(socket);

  // Second pass: a client certificate should have been selected.
  if (that->ssl_config_.send_client_cert) {
    if (that->ssl_config_.client_cert) {
      cert = CERT_DupCertificate(
          that->ssl_config_.client_cert->os_cert_handle());
      privkey = PK11_FindKeyByAnyCert(cert, wincx);
      if (privkey) {
        // TODO(jsorianopastor): We should wait for server certificate
        // verification before sending our credentials.  See
        // http://crbug.com/13934.
        *result_certificate = cert;
        *result_private_key = privkey;
        return SECSuccess;
      }
      LOG(WARNING) << "Client cert found without private key";
    }
    // Send no client certificate.
    return SECFailure;
  }

  CERTCertNicknames* names = CERT_GetCertNicknames(
      CERT_GetDefaultCertDB(), SEC_CERT_NICKNAMES_USER, wincx);
  if (names) {
    for (int i = 0; i < names->numnicknames; ++i) {
      cert = CERT_FindUserCertByUsage(
          CERT_GetDefaultCertDB(), names->nicknames[i],
          certUsageSSLClient, PR_FALSE, wincx);
      if (!cert)
        continue;
      // Only check unexpired certs.
      if (CERT_CheckCertValidTimes(cert, PR_Now(), PR_TRUE) ==
          secCertTimeValid &&
          NSS_CmpCertChainWCANames(cert, ca_names) == SECSuccess) {
        privkey = PK11_FindKeyByAnyCert(cert, wincx);
        if (privkey) {
          X509Certificate* x509_cert = X509Certificate::CreateFromHandle(
              cert, X509Certificate::SOURCE_LONE_CERT_IMPORT);
          that->client_certs_.push_back(x509_cert);
          SECKEY_DestroyPrivateKey(privkey);
          continue;
        }
      }
      CERT_DestroyCertificate(cert);
    }
    CERT_FreeNicknames(names);
  }

  return SECFailure;
}

// static
// NSS calls this when handshake is completed.
// After the SSL handshake is finished, use CertVerifier to verify
// the saved server certificate.
void SSLClientSocketNSS::HandshakeCallback(PRFileDesc* socket,
                                           void* arg) {
  SSLClientSocketNSS* that = reinterpret_cast<SSLClientSocketNSS*>(arg);

  that->UpdateServerCert();
}

int SSLClientSocketNSS::DoHandshake() {
  EnterFunction("");
  int net_error = net::OK;
  SECStatus rv = SSL_ForceHandshake(nss_fd_);

  if (client_auth_cert_needed_) {
    net_error = ERR_SSL_CLIENT_AUTH_CERT_NEEDED;
    // If the handshake already succeeded (because the server requests but
    // doesn't require a client cert), we need to invalidate the SSL session
    // so that we won't try to resume the non-client-authenticated session in
    // the next handshake.  This will cause the server to ask for a client
    // cert again.
    if (rv == SECSuccess && SSL_InvalidateSession(nss_fd_) != SECSuccess) {
      LOG(WARNING) << "Couldn't invalidate SSL session: " << PR_GetError();
    }
  } else if (rv == SECSuccess) {
    // SSL handshake is completed.  Let's verify the certificate.
    GotoState(STATE_VERIFY_CERT);
    // Done!
  } else {
    PRErrorCode prerr = PR_GetError();

    // If the server closed on us, it is a protocol error.
    // Some TLS-intolerant servers do this when we request TLS.
    if (prerr == PR_END_OF_FILE_ERROR) {
      net_error = ERR_SSL_PROTOCOL_ERROR;
    } else {
      net_error = NetErrorFromNSPRError(prerr);
    }

    // If not done, stay in this state
    if (net_error == ERR_IO_PENDING) {
      GotoState(STATE_HANDSHAKE);
    } else {
      LOG(ERROR) << "handshake failed; NSS error code " << prerr
                 << ", net_error " << net_error;
    }
  }

  LeaveFunction("");
  return net_error;
}

int SSLClientSocketNSS::DoVerifyCert(int result) {
  DCHECK(server_cert_);
  GotoState(STATE_VERIFY_CERT_COMPLETE);
  int flags = 0;
  if (ssl_config_.rev_checking_enabled)
    flags |= X509Certificate::VERIFY_REV_CHECKING_ENABLED;
  if (ssl_config_.verify_ev_cert)
    flags |= X509Certificate::VERIFY_EV_CERT;
  verifier_.reset(new CertVerifier);
  return verifier_->Verify(server_cert_, hostname_, flags,
                           &server_cert_verify_result_,
                           &handshake_io_callback_);
}

// Derived from AuthCertificateCallback() in
// mozilla/source/security/manager/ssl/src/nsNSSCallbacks.cpp.
int SSLClientSocketNSS::DoVerifyCertComplete(int result) {
  DCHECK(verifier_.get());
  verifier_.reset();

  if (result == OK) {
    // Remember the intermediate CA certs if the server sends them to us.
    //
    // We used to remember the intermediate CA certs in the NSS database
    // persistently.  However, NSS opens a connection to the SQLite database
    // during NSS initialization and doesn't close the connection until NSS
    // shuts down.  If the file system where the database resides is gone,
    // the database connection goes bad.  What's worse, the connection won't
    // recover when the file system comes back.  Until this NSS or SQLite bug
    // is fixed, we need to  avoid using the NSS database for non-essential
    // purposes.  See https://bugzilla.mozilla.org/show_bug.cgi?id=508081 and
    // http://crbug.com/15630 for more info.
    CERTCertList* cert_list = CERT_GetCertChainFromCert(
        server_cert_->os_cert_handle(), PR_Now(), certUsageSSLCA);
    if (cert_list) {
      for (CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
           !CERT_LIST_END(node, cert_list);
           node = CERT_LIST_NEXT(node)) {
        if (node->cert->slot || node->cert->isRoot || node->cert->isperm ||
            node->cert == server_cert_->os_cert_handle()) {
          // Some certs we don't want to remember are:
          // - found on a token.
          // - the root cert.
          // - already stored in perm db.
          // - the server cert itself.
          continue;
        }

        // We have found a CA cert that we want to remember.
        // TODO(wtc): Remember the intermediate CA certs in a std::set
        // temporarily (http://crbug.com/15630).
      }
      CERT_DestroyCertList(cert_list);
    }
  }

  // If we have been explicitly told to accept this certificate, override the
  // result of verifier_.Verify.
  // Eventually, we should cache the cert verification results so that we don't
  // need to call verifier_.Verify repeatedly.  But for now we need to do this.
  // Alternatively, we could use the cert's status that we stored along with
  // the cert in the allowed_bad_certs vector.
  if (IsCertificateError(result) &&
      ssl_config_.IsAllowedBadCert(server_cert_)) {
    LOG(INFO) << "accepting bad SSL certificate, as user told us to";
    result = OK;
  }

  completed_handshake_ = true;
  // TODO(ukai): we may not need this call because it is now harmless to have an
  // session with a bad cert.
  InvalidateSessionIfBadCertificate();
  // Exit DoHandshakeLoop and return the result to the caller to Connect.
  DCHECK(next_handshake_state_ == STATE_NONE);
  return result;
}

int SSLClientSocketNSS::DoPayloadRead() {
  EnterFunction(user_read_buf_len_);
  DCHECK(user_read_buf_);
  DCHECK(user_read_buf_len_ > 0);
  int rv = PR_Read(nss_fd_, user_read_buf_->data(), user_read_buf_len_);
  if (client_auth_cert_needed_) {
    // We don't need to invalidate the non-client-authenticated SSL session
    // because the server will renegotiate anyway.
    LeaveFunction("");
    return ERR_SSL_CLIENT_AUTH_CERT_NEEDED;
  }
  if (rv >= 0) {
    LogData(user_read_buf_->data(), rv);
    LeaveFunction("");
    return rv;
  }
  PRErrorCode prerr = PR_GetError();
  if (prerr == PR_WOULD_BLOCK_ERROR) {
    LeaveFunction("");
    return ERR_IO_PENDING;
  }
  LeaveFunction("");
  return NetErrorFromNSPRError(prerr);
}

int SSLClientSocketNSS::DoPayloadWrite() {
  EnterFunction(user_write_buf_len_);
  DCHECK(user_write_buf_);
  int rv = PR_Write(nss_fd_, user_write_buf_->data(), user_write_buf_len_);
  if (rv >= 0) {
    LogData(user_write_buf_->data(), rv);
    LeaveFunction("");
    return rv;
  }
  PRErrorCode prerr = PR_GetError();
  if (prerr == PR_WOULD_BLOCK_ERROR) {
    return ERR_IO_PENDING;
  }
  LeaveFunction("");
  return NetErrorFromNSPRError(prerr);
}

}  // namespace net
