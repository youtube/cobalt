// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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

#if defined(USE_SYSTEM_SSL)
#include <dlfcn.h>
#endif
#include <certdb.h>
#include <hasht.h>
#include <keyhi.h>
#include <nspr.h>
#include <nss.h>
#include <secerr.h>
#include <sechash.h>
#include <ssl.h>
#include <sslerr.h>
#include <pk11pub.h>

#include "base/compiler_specific.h"
#include "base/histogram.h"
#include "base/logging.h"
#include "base/nss_util.h"
#include "base/singleton.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "net/base/address_list.h"
#include "net/base/cert_verifier.h"
#include "net/base/dnsrr_resolver.h"
#include "net/base/dnssec_chain_verifier.h"
#include "net/base/dns_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/ssl_cert_request_info.h"
#include "net/base/ssl_connection_status_flags.h"
#include "net/base/ssl_info.h"
#include "net/base/sys_addrinfo.h"
#include "net/ocsp/nss_ocsp.h"
#include "net/socket/client_socket_handle.h"

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

class NSSSSLInitSingleton {
 public:
  NSSSSLInitSingleton() {
    base::EnsureNSSInit();

    NSS_SetDomesticPolicy();

#if defined(USE_SYSTEM_SSL)
    // Use late binding to avoid scary but benign warning
    // "Symbol `SSL_ImplementedCiphers' has different size in shared object,
    //  consider re-linking"
    // TODO(wtc): Use the new SSL_GetImplementedCiphers and
    // SSL_GetNumImplementedCiphers functions when we require NSS 3.12.6.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=496993.
    const PRUint16* pSSL_ImplementedCiphers = static_cast<const PRUint16*>(
        dlsym(RTLD_DEFAULT, "SSL_ImplementedCiphers"));
    if (pSSL_ImplementedCiphers == NULL) {
      NOTREACHED() << "Can't get list of supported ciphers";
      return;
    }
#else
#define pSSL_ImplementedCiphers SSL_ImplementedCiphers
#endif

    // Explicitly enable exactly those ciphers with keys of at least 80 bits
    for (int i = 0; i < SSL_NumImplementedCiphers; i++) {
      SSLCipherSuiteInfo info;
      if (SSL_GetCipherSuiteInfo(pSSL_ImplementedCiphers[i], &info,
                                 sizeof(info)) == SECSuccess) {
        SSL_CipherPrefSetDefault(pSSL_ImplementedCiphers[i],
                                 (info.effectiveKeyBits >= 80));
      }
    }

    // Enable SSL.
    SSL_OptionSetDefault(SSL_SECURITY, PR_TRUE);

    // All other SSL options are set per-session by SSLClientSocket.
  }

  ~NSSSSLInitSingleton() {
    // Have to clear the cache, or NSS_Shutdown fails with SEC_ERROR_BUSY.
    SSL_ClearSessionCache();
  }
};

// Initialize the NSS SSL library if it isn't already initialized.  This must
// be called before any other NSS SSL functions.  This function is
// thread-safe, and the NSS SSL library will only ever be initialized once.
// The NSS SSL library will be properly shut down on program exit.
void EnsureNSSSSLInit() {
  Singleton<NSSSSLInitSingleton>::get();
}

// The default error mapping function.
// Maps an NSPR error code to a network error code.
int MapNSPRError(PRErrorCode err) {
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
    case PR_INVALID_ARGUMENT_ERROR:
      return ERR_INVALID_ARGUMENT;

    case SEC_ERROR_INVALID_ARGS:
      return ERR_INVALID_ARGUMENT;

    case SSL_ERROR_SSL_DISABLED:
      return ERR_NO_SSL_VERSIONS_ENABLED;
    case SSL_ERROR_NO_CYPHER_OVERLAP:
    case SSL_ERROR_UNSUPPORTED_VERSION:
      return ERR_SSL_VERSION_OR_CIPHER_MISMATCH;
    case SSL_ERROR_HANDSHAKE_FAILURE_ALERT:
    case SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT:
    case SSL_ERROR_ILLEGAL_PARAMETER_ALERT:
      return ERR_SSL_PROTOCOL_ERROR;
    case SSL_ERROR_DECOMPRESSION_FAILURE_ALERT:
      return ERR_SSL_DECOMPRESSION_FAILURE_ALERT;
    case SSL_ERROR_BAD_MAC_ALERT:
      return ERR_SSL_BAD_RECORD_MAC_ALERT;
    case SSL_ERROR_UNSAFE_NEGOTIATION:
      return ERR_SSL_UNSAFE_NEGOTIATION;
    case SSL_ERROR_WEAK_SERVER_KEY:
      return ERR_SSL_WEAK_SERVER_EPHEMERAL_DH_KEY;

    default: {
      if (IS_SSL_ERROR(err)) {
        LOG(WARNING) << "Unknown SSL error " << err <<
            " mapped to net::ERR_SSL_PROTOCOL_ERROR";
        return ERR_SSL_PROTOCOL_ERROR;
      }
      LOG(WARNING) << "Unknown error " << err <<
          " mapped to net::ERR_FAILED";
      return ERR_FAILED;
    }
  }
}

// Context-sensitive error mapping functions.

int MapHandshakeError(PRErrorCode err) {
  switch (err) {
    // If the server closed on us, it is a protocol error.
    // Some TLS-intolerant servers do this when we request TLS.
    case PR_END_OF_FILE_ERROR:
    // The handshake may fail because some signature (for example, the
    // signature in the ServerKeyExchange message for an ephemeral
    // Diffie-Hellman cipher suite) is invalid.
    case SEC_ERROR_BAD_SIGNATURE:
      return ERR_SSL_PROTOCOL_ERROR;
    default:
      return MapNSPRError(err);
  }
}

#if defined(OS_WIN)

// A certificate for COMODO EV SGC CA, issued by AddTrust External CA Root,
// causes CertGetCertificateChain to report CERT_TRUST_IS_NOT_VALID_FOR_USAGE.
// It seems to be caused by the szOID_APPLICATION_CERT_POLICIES extension in
// that certificate.
//
// This function is used in the workaround for http://crbug.com/43538
bool IsProblematicComodoEVCACert(const CERTCertificate& cert) {
  // Issuer:
  // CN = AddTrust External CA Root
  // OU = AddTrust External TTP Network
  // O = AddTrust AB
  // C = SE
  static const uint8 kIssuer[] = {
    0x30, 0x6f, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04,
    0x06, 0x13, 0x02, 0x53, 0x45, 0x31, 0x14, 0x30, 0x12, 0x06,
    0x03, 0x55, 0x04, 0x0a, 0x13, 0x0b, 0x41, 0x64, 0x64, 0x54,
    0x72, 0x75, 0x73, 0x74, 0x20, 0x41, 0x42, 0x31, 0x26, 0x30,
    0x24, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x13, 0x1d, 0x41, 0x64,
    0x64, 0x54, 0x72, 0x75, 0x73, 0x74, 0x20, 0x45, 0x78, 0x74,
    0x65, 0x72, 0x6e, 0x61, 0x6c, 0x20, 0x54, 0x54, 0x50, 0x20,
    0x4e, 0x65, 0x74, 0x77, 0x6f, 0x72, 0x6b, 0x31, 0x22, 0x30,
    0x20, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x19, 0x41, 0x64,
    0x64, 0x54, 0x72, 0x75, 0x73, 0x74, 0x20, 0x45, 0x78, 0x74,
    0x65, 0x72, 0x6e, 0x61, 0x6c, 0x20, 0x43, 0x41, 0x20, 0x52,
    0x6f, 0x6f, 0x74
  };

  // Serial number: 79:0A:83:4D:48:40:6B:AB:6C:35:2A:D5:1F:42:83:FE.
  static const uint8 kSerialNumber[] = {
    0x79, 0x0a, 0x83, 0x4d, 0x48, 0x40, 0x6b, 0xab, 0x6c, 0x35,
    0x2a, 0xd5, 0x1f, 0x42, 0x83, 0xfe
  };

  return cert.derIssuer.len == sizeof(kIssuer) &&
         memcmp(cert.derIssuer.data, kIssuer, cert.derIssuer.len) == 0 &&
         cert.serialNumber.len == sizeof(kSerialNumber) &&
         memcmp(cert.serialNumber.data, kSerialNumber,
                cert.serialNumber.len) == 0;
}

// This callback is intended to be used with CertFindChainInStore. In addition
// to filtering by extended/enhanced key usage, we do not show expired
// certificates and require digital signature usage in the key usage
// extension.
//
// This matches our behavior on Mac OS X and that of NSS. It also matches the
// default behavior of IE8. See http://support.microsoft.com/kb/890326 and
// http://blogs.msdn.com/b/askie/archive/2009/06/09/my-expired-client-certificates-no-longer-display-when-connecting-to-my-web-server-using-ie8.aspx
BOOL WINAPI ClientCertFindCallback(PCCERT_CONTEXT cert_context,
                                   void* find_arg) {
  LOG(INFO) << "Calling ClientCertFindCallback from _nss";
  // Verify the certificate's KU is good.
  BYTE key_usage;
  if (CertGetIntendedKeyUsage(X509_ASN_ENCODING, cert_context->pCertInfo,
                              &key_usage, 1)) {
    if (!(key_usage & CERT_DIGITAL_SIGNATURE_KEY_USAGE))
      return FALSE;
  } else {
    DWORD err = GetLastError();
    // If |err| is non-zero, it's an actual error. Otherwise the extension
    // just isn't present, and we treat it as if everything was allowed.
    if (err) {
      DLOG(ERROR) << "CertGetIntendedKeyUsage failed: " << err;
      return FALSE;
    }
  }

  // Verify the current time is within the certificate's validity period.
  if (CertVerifyTimeValidity(NULL, cert_context->pCertInfo) != 0)
    return FALSE;

  return TRUE;
}

#endif

}  // namespace

#if defined(OS_WIN)
// static
HCERTSTORE SSLClientSocketNSS::cert_store_ = NULL;
#endif

SSLClientSocketNSS::SSLClientSocketNSS(ClientSocketHandle* transport_socket,
                                       const std::string& hostname,
                                       const SSLConfig& ssl_config)
    : ALLOW_THIS_IN_INITIALIZER_LIST(buffer_send_callback_(
          this, &SSLClientSocketNSS::BufferSendComplete)),
      ALLOW_THIS_IN_INITIALIZER_LIST(buffer_recv_callback_(
          this, &SSLClientSocketNSS::BufferRecvComplete)),
      transport_send_busy_(false),
      transport_recv_busy_(false),
      corked_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(handshake_io_callback_(
          this, &SSLClientSocketNSS::OnHandshakeIOComplete)),
      transport_(transport_socket),
      hostname_(hostname),
      ssl_config_(ssl_config),
      user_connect_callback_(NULL),
      user_read_callback_(NULL),
      user_write_callback_(NULL),
      user_read_buf_len_(0),
      user_write_buf_len_(0),
      server_cert_nss_(NULL),
      client_auth_cert_needed_(false),
      handshake_callback_called_(false),
      completed_handshake_(false),
      dnssec_provider_(NULL),
      next_handshake_state_(STATE_NONE),
      nss_fd_(NULL),
      nss_bufs_(NULL),
      net_log_(transport_socket->socket()->NetLog()) {
  EnterFunction("");
}

SSLClientSocketNSS::~SSLClientSocketNSS() {
  EnterFunction("");
  Disconnect();
  LeaveFunction("");
}

int SSLClientSocketNSS::Init() {
  EnterFunction("");
  // Initialize the NSS SSL library in a threadsafe way.  This also
  // initializes the NSS base library.
  EnsureNSSSSLInit();
  if (!NSS_IsInitialized())
    return ERR_UNEXPECTED;
#if !defined(OS_MACOSX) && !defined(OS_WIN)
  // We must call EnsureOCSPInit() here, on the IO thread, to get the IO loop
  // by MessageLoopForIO::current().
  // X509Certificate::Verify() runs on a worker thread of CertVerifier.
  EnsureOCSPInit();
#endif

  LeaveFunction("");
  return OK;
}

int SSLClientSocketNSS::Connect(CompletionCallback* callback) {
  EnterFunction("");
  DCHECK(transport_.get());
  DCHECK(next_handshake_state_ == STATE_NONE);
  DCHECK(!user_read_callback_);
  DCHECK(!user_write_callback_);
  DCHECK(!user_connect_callback_);
  DCHECK(!user_read_buf_);
  DCHECK(!user_write_buf_);

  net_log_.BeginEvent(NetLog::TYPE_SSL_CONNECT, NULL);

  int rv = Init();
  if (rv != OK) {
    net_log_.EndEvent(NetLog::TYPE_SSL_CONNECT, NULL);
    return rv;
  }

  rv = InitializeSSLOptions();
  if (rv != OK) {
    net_log_.EndEvent(NetLog::TYPE_SSL_CONNECT, NULL);
    return rv;
  }

  GotoState(STATE_HANDSHAKE);
  rv = DoHandshakeLoop(OK);
  if (rv == ERR_IO_PENDING) {
    user_connect_callback_ = callback;
  } else {
    net_log_.EndEvent(NetLog::TYPE_SSL_CONNECT, NULL);
  }

  LeaveFunction("");
  return rv > OK ? OK : rv;
}

int SSLClientSocketNSS::InitializeSSLOptions() {
  // Transport connected, now hook it up to nss
  // TODO(port): specify rx and tx buffer sizes separately
  nss_fd_ = memio_CreateIOLayer(kRecvBufferSize);
  if (nss_fd_ == NULL) {
    return ERR_OUT_OF_MEMORY;  // TODO(port): map NSPR error code.
  }

  // Tell NSS who we're connected to
  AddressList peer_address;
  int err = transport_->socket()->GetPeerAddress(&peer_address);
  if (err != OK)
    return err;

  const struct addrinfo* ai = peer_address.head();

  PRNetAddr peername;
  memset(&peername, 0, sizeof(peername));
  DCHECK_LE(ai->ai_addrlen, sizeof(peername));
  size_t len = std::min(static_cast<size_t>(ai->ai_addrlen), sizeof(peername));
  memcpy(&peername, ai->ai_addr, len);

  // Adjust the address family field for BSD, whose sockaddr
  // structure has a one-byte length and one-byte address family
  // field at the beginning.  PRNetAddr has a two-byte address
  // family field at the beginning.
  peername.raw.family = ai->ai_addr->sa_family;

  memio_SetPeerName(nss_fd_, &peername);

  // Grab pointer to buffers
  nss_bufs_ = memio_GetSecret(nss_fd_);

  /* Create SSL state machine */
  /* Push SSL onto our fake I/O socket */
  nss_fd_ = SSL_ImportFD(NULL, nss_fd_);
  if (nss_fd_ == NULL) {
      return ERR_OUT_OF_MEMORY;  // TODO(port): map NSPR/NSS error code.
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
  // Some web servers have been found to break if TLS is used *or* if DEFLATE
  // is advertised. Thus, if TLS is disabled (probably because we are doing
  // SSLv3 fallback), we disable DEFLATE also.
  // See http://crbug.com/31628
  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_DEFLATE, ssl_config_.tls1_enabled);
  if (rv != SECSuccess)
     LOG(INFO) << "SSL_ENABLE_DEFLATE failed.  Old system nss?";
#endif

#ifdef SSL_ENABLE_FALSE_START
  rv = SSL_OptionSet(
      nss_fd_, SSL_ENABLE_FALSE_START,
      ssl_config_.false_start_enabled &&
      !SSLConfigService::IsKnownFalseStartIncompatibleServer(hostname_));
  if (rv != SECSuccess)
    LOG(INFO) << "SSL_ENABLE_FALSE_START failed.  Old system nss?";
#endif

#ifdef SSL_ENABLE_RENEGOTIATION
  if (SSLConfigService::IsKnownStrictTLSServer(hostname_) &&
      !ssl_config_.mitm_proxies_allowed) {
    rv = SSL_OptionSet(nss_fd_, SSL_REQUIRE_SAFE_NEGOTIATION, PR_TRUE);
    if (rv != SECSuccess)
       LOG(INFO) << "SSL_REQUIRE_SAFE_NEGOTIATION failed.";
    rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_RENEGOTIATION,
                       SSL_RENEGOTIATE_REQUIRES_XTN);
  } else {
    // We allow servers to request renegotiation. Since we're a client,
    // prohibiting this is rather a waste of time. Only servers are in a
    // position to prevent renegotiation attacks.
    // http://extendedsubset.com/?p=8

    rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_RENEGOTIATION,
                       SSL_RENEGOTIATE_TRANSITIONAL);
  }
  if (rv != SECSuccess)
     LOG(INFO) << "SSL_ENABLE_RENEGOTIATION failed.";
#endif  // SSL_ENABLE_RENEGOTIATION

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
  // TODO(wtc): port in |peer_address| is not the server's port when a proxy is
  // used.
  std::string peer_id = StringPrintf("%s:%d", hostname_.c_str(),
                                     peer_address.GetPort());
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
  transport_->socket()->Disconnect();

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
  if (server_cert_nss_) {
    CERT_DestroyCertificate(server_cert_nss_);
    server_cert_nss_     = NULL;
  }
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
  bool ret = completed_handshake_ && transport_->socket()->IsConnected();
  LeaveFunction("");
  return ret;
}

bool SSLClientSocketNSS::IsConnectedAndIdle() const {
  // Unlike IsConnected, this method doesn't return a false positive.
  //
  // Strictly speaking, we should check if we have received the close_notify
  // alert message from the server, and return false in that case.  Although
  // the close_notify alert message means EOF in the SSL layer, it is just
  // bytes to the transport layer below, so
  // transport_->socket()->IsConnectedAndIdle() returns the desired false
  // when we receive close_notify.
  EnterFunction("");
  bool ret = completed_handshake_ && transport_->socket()->IsConnectedAndIdle();
  LeaveFunction("");
  return ret;
}

int SSLClientSocketNSS::GetPeerAddress(AddressList* address) const {
  return transport_->socket()->GetPeerAddress(address);
}

void SSLClientSocketNSS::SetSubresourceSpeculation() {
  if (transport_.get() && transport_->socket()) {
    transport_->socket()->SetSubresourceSpeculation();
  } else {
    NOTREACHED();
  }
}

void SSLClientSocketNSS::SetOmniboxSpeculation() {
  if (transport_.get() && transport_->socket()) {
    transport_->socket()->SetOmniboxSpeculation();
  } else {
    NOTREACHED();
  }
}

bool SSLClientSocketNSS::WasEverUsed() const {
  if (transport_.get() && transport_->socket()) {
    return transport_->socket()->WasEverUsed();
  }
  NOTREACHED();
  return false;
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

  if (rv == ERR_IO_PENDING) {
    user_read_callback_ = callback;
  } else {
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

  corked_ = false;
  int rv = DoWriteLoop(OK);

  if (rv == ERR_IO_PENDING) {
    user_write_callback_ = callback;
  } else {
    user_write_buf_ = NULL;
    user_write_buf_len_ = 0;
  }
  LeaveFunction(rv);
  return rv;
}

bool SSLClientSocketNSS::SetReceiveBufferSize(int32 size) {
  return transport_->socket()->SetReceiveBufferSize(size);
}

bool SSLClientSocketNSS::SetSendBufferSize(int32 size) {
  return transport_->socket()->SetSendBufferSize(size);
}

#if defined(OS_WIN)
// static
X509Certificate::OSCertHandle SSLClientSocketNSS::CreateOSCert(
    const SECItem& der_cert) {
  // TODO(wtc): close cert_store_ at shutdown.
  if (!cert_store_)
    cert_store_ = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, NULL, 0, NULL);

  X509Certificate::OSCertHandle cert_handle = NULL;
  BOOL ok = CertAddEncodedCertificateToStore(
      cert_store_, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
      der_cert.data, der_cert.len, CERT_STORE_ADD_USE_EXISTING, &cert_handle);
  return ok ? cert_handle : NULL;
}
#elif defined(OS_MACOSX)
// static
X509Certificate::OSCertHandle SSLClientSocketNSS::CreateOSCert(
    const SECItem& der_cert) {
  return X509Certificate::CreateOSCertHandleFromBytes(
      reinterpret_cast<char*>(der_cert.data), der_cert.len);
}
#endif

X509Certificate *SSLClientSocketNSS::UpdateServerCert() {
  // We set the server_cert_ from OwnAuthCertHandler(), but this handler
  // does not necessarily get called if we are continuing a cached SSL
  // session.
  if (server_cert_ == NULL) {
    server_cert_nss_ = SSL_PeerCertificate(nss_fd_);
    if (server_cert_nss_) {
#if defined(OS_MACOSX) || defined(OS_WIN)
      // Get each of the intermediate certificates in the server's chain.
      // These will be added to the server's X509Certificate object, making
      // them available to X509Certificate::Verify() for chain building.
      X509Certificate::OSCertHandles intermediate_ca_certs;
      X509Certificate::OSCertHandle cert_handle = NULL;
      CERTCertList* cert_list = CERT_GetCertChainFromCert(
          server_cert_nss_, PR_Now(), certUsageSSLCA);
      if (cert_list) {
        for (CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
             !CERT_LIST_END(node, cert_list);
             node = CERT_LIST_NEXT(node)) {
          if (node->cert == server_cert_nss_)
            continue;
#if defined(OS_WIN)
          // Work around http://crbug.com/43538 by not importing the
          // problematic COMODO EV SGC CA certificate.  CryptoAPI will
          // download a good certificate for that CA, issued by COMODO
          // Certification Authority, using the AIA extension in the server
          // certificate.
          if (IsProblematicComodoEVCACert(*node->cert))
            continue;
#endif
          cert_handle = CreateOSCert(node->cert->derCert);
          DCHECK(cert_handle);
          intermediate_ca_certs.push_back(cert_handle);
        }
        CERT_DestroyCertList(cert_list);
      }

      // Finally create the X509Certificate object.
      cert_handle = CreateOSCert(server_cert_nss_->derCert);
      DCHECK(cert_handle);
      server_cert_ = X509Certificate::CreateFromHandle(
          cert_handle,
          X509Certificate::SOURCE_FROM_NETWORK,
          intermediate_ca_certs);
      X509Certificate::FreeOSCertHandle(cert_handle);
      for (size_t i = 0; i < intermediate_ca_certs.size(); ++i)
        X509Certificate::FreeOSCertHandle(intermediate_ca_certs[i]);
#else
      server_cert_ = X509Certificate::CreateFromHandle(
          server_cert_nss_,
          X509Certificate::SOURCE_FROM_NETWORK,
          X509Certificate::OSCertHandles());
#endif
    }
  }
  return server_cert_;
}

// Log an informational message if the server does not support secure
// renegotiation (RFC 5746).
void SSLClientSocketNSS::CheckSecureRenegotiation() const {
  // SSL_HandshakeNegotiatedExtension was added in NSS 3.12.6.
  // Since SSL_MAX_EXTENSIONS was added at the same time, we can test
  // SSL_MAX_EXTENSIONS for the presence of SSL_HandshakeNegotiatedExtension.
#if defined(SSL_MAX_EXTENSIONS)
  PRBool received_renego_info;
  if (SSL_HandshakeNegotiatedExtension(nss_fd_, ssl_renegotiation_info_xtn,
                                       &received_renego_info) == SECSuccess &&
      !received_renego_info) {
    LOG(INFO) << "The server " << hostname_
              << " does not support the TLS renegotiation_info extension.";
  }
#endif
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
    ssl_info->connection_status |=
        (((int)channel_info.cipherSuite) & SSL_CONNECTION_CIPHERSUITE_MASK) <<
        SSL_CONNECTION_CIPHERSUITE_SHIFT;

    ssl_info->connection_status |=
        (((int)channel_info.compressionMethod) &
         SSL_CONNECTION_COMPRESSION_MASK) <<
        SSL_CONNECTION_COMPRESSION_SHIFT;

    UpdateServerCert();
  }
  ssl_info->cert_status = server_cert_verify_result_.cert_status;
  DCHECK(server_cert_ != NULL);
  ssl_info->cert = server_cert_;

  PRBool peer_supports_renego_ext;
  ok = SSL_HandshakeNegotiatedExtension(nss_fd_, ssl_renegotiation_info_xtn,
                                        &peer_supports_renego_ext);
  if (ok == SECSuccess) {
    if (!peer_supports_renego_ext)
      ssl_info->connection_status |= SSL_CONNECTION_NO_RENEGOTIATION_EXTENSION;
    UMA_HISTOGRAM_ENUMERATION("Net.RenegotiationExtensionSupported",
                              (int)peer_supports_renego_ext, 2);
  }

  if (ssl_config_.ssl3_fallback)
    ssl_info->connection_status |= SSL_CONNECTION_SSL3_FALLBACK;

  LeaveFunction("");
}

void SSLClientSocketNSS::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  EnterFunction("");
  cert_request_info->host_and_port = hostname_;  // TODO(wtc): no port!
  cert_request_info->client_certs = client_certs_;
  LeaveFunction(cert_request_info->client_certs.size());
}

SSLClientSocket::NextProtoStatus
SSLClientSocketNSS::GetNextProto(std::string* proto) {
#if defined(SSL_NEXT_PROTO_NEGOTIATED)
  unsigned char buf[255];
  int state;
  unsigned len;
  SECStatus rv = SSL_GetNextProto(nss_fd_, &state, buf, &len, sizeof(buf));
  if (rv != SECSuccess) {
    NOTREACHED() << "Error return from SSL_GetNextProto: " << rv;
    proto->clear();
    return kNextProtoUnsupported;
  }
  // We don't check for truncation because sizeof(buf) is large enough to hold
  // the maximum protocol size.
  switch (state) {
    case SSL_NEXT_PROTO_NO_SUPPORT:
      proto->clear();
      return kNextProtoUnsupported;
    case SSL_NEXT_PROTO_NEGOTIATED:
      *proto = std::string(reinterpret_cast<char*>(buf), len);
      return kNextProtoNegotiated;
    case SSL_NEXT_PROTO_NO_OVERLAP:
      *proto = std::string(reinterpret_cast<char*>(buf), len);
      return kNextProtoNoOverlap;
    default:
      NOTREACHED() << "Unknown status from SSL_GetNextProto: " << state;
      proto->clear();
      return kNextProtoUnsupported;
  }
#else
  // No NPN support in the libssl that we are building with.
  proto->clear();
  return kNextProtoUnsupported;
#endif
}

void SSLClientSocketNSS::UseDNSSEC(DNSSECProvider* provider) {
  dnssec_provider_ = provider;
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
    net_log_.EndEvent(net::NetLog::TYPE_SSL_CONNECT, NULL);
    DoConnectCallback(rv);
  }
  LeaveFunction("");
}

void SSLClientSocketNSS::OnSendComplete(int result) {
  EnterFunction(result);
  if (next_handshake_state_ == STATE_HANDSHAKE) {
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
  if (next_handshake_state_ == STATE_HANDSHAKE) {
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
    case ERR_NAME_NOT_RESOLVED:
      return PR_DIRECTORY_LOOKUP_ERROR;
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
  if (transport_send_busy_)
    return ERR_IO_PENDING;

  EnterFunction("");
  const char* buf1;
  const char* buf2;
  unsigned int len1, len2;
  memio_GetWriteParams(nss_bufs_, &buf1, &len1, &buf2, &len2);
  const unsigned int len = len1 + len2;

  if (corked_ && len < kRecvBufferSize / 2)
    return 0;

  int rv = 0;
  if (len) {
    scoped_refptr<IOBuffer> send_buffer = new IOBuffer(len);
    memcpy(send_buffer->data(), buf1, len1);
    memcpy(send_buffer->data() + len1, buf2, len2);
    rv = transport_->socket()->Write(send_buffer, len,
                                     &buffer_send_callback_);
    if (rv == ERR_IO_PENDING) {
      transport_send_busy_ = true;
    } else {
      memio_PutWriteResult(nss_bufs_, MapErrorToNSS(rv));
    }
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
    rv = transport_->socket()->Read(recv_buffer_, nb, &buffer_recv_callback_);
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
      case STATE_VERIFY_DNSSEC:
        rv = DoVerifyDNSSEC(rv);
        break;
      case STATE_VERIFY_DNSSEC_COMPLETE:
        rv = DoVerifyDNSSECComplete(rv);
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
#ifdef SSL_ENABLE_FALSE_START
  // In the event that we are False Starting this connection, we wish to send
  // out the Finished message and first application data record in the same
  // packet. This prevents non-determinism when talking to False Start
  // intolerant servers which, otherwise, might see the two messages in
  // different reads or not, depending on network conditions.
  PRBool false_start = 0;
  SECStatus rv = SSL_OptionGet(socket, SSL_ENABLE_FALSE_START, &false_start);
  if (rv != SECSuccess)
    NOTREACHED();
  if (false_start) {
    SSLClientSocketNSS* that = reinterpret_cast<SSLClientSocketNSS*>(arg);
    if (!that->handshake_callback_called_)
      that->corked_ = true;
  }
#endif

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

#if defined(OS_WIN)
  if (that->ssl_config_.send_client_cert) {
    // TODO(wtc): SSLClientSocketNSS can't do SSL client authentication using
    // CryptoAPI yet (http://crbug.com/37560), so client_cert must be NULL.
    DCHECK(!that->ssl_config_.client_cert);
    // Send no client certificate.
    return SECFailure;
  }

  that->client_certs_.clear();

  std::vector<CERT_NAME_BLOB> issuer_list(ca_names->nnames);
  for (int i = 0; i < ca_names->nnames; ++i) {
    issuer_list[i].cbData = ca_names->names[i].len;
    issuer_list[i].pbData = ca_names->names[i].data;
  }

  // Client certificates of the user are in the "MY" system certificate store.
  HCERTSTORE my_cert_store = CertOpenSystemStore(NULL, L"MY");
  if (!my_cert_store) {
    LOG(ERROR) << "Could not open the \"MY\" system certificate store: "
               << GetLastError();
    return SECFailure;
  }

  // Enumerate the client certificates.
  CERT_CHAIN_FIND_BY_ISSUER_PARA find_by_issuer_para;
  memset(&find_by_issuer_para, 0, sizeof(find_by_issuer_para));
  find_by_issuer_para.cbSize = sizeof(find_by_issuer_para);
  find_by_issuer_para.pszUsageIdentifier = szOID_PKIX_KP_CLIENT_AUTH;
  find_by_issuer_para.cIssuer = ca_names->nnames;
  find_by_issuer_para.rgIssuer = ca_names->nnames ? &issuer_list[0] : NULL;
  find_by_issuer_para.pfnFindCallback = ClientCertFindCallback;

  PCCERT_CHAIN_CONTEXT chain_context = NULL;

  // TODO(wtc): close cert_store_ at shutdown.
  if (!cert_store_)
    cert_store_ = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, NULL, 0, NULL);

  for (;;) {
    // Find a certificate chain.
    chain_context = CertFindChainInStore(my_cert_store,
                                         X509_ASN_ENCODING,
                                         0,
                                         CERT_CHAIN_FIND_BY_ISSUER,
                                         &find_by_issuer_para,
                                         chain_context);
    if (!chain_context) {
      DWORD err = GetLastError();
      if (err != CRYPT_E_NOT_FOUND)
        DLOG(ERROR) << "CertFindChainInStore failed: " << err;
      break;
    }

    // Get the leaf certificate.
    PCCERT_CONTEXT cert_context =
        chain_context->rgpChain[0]->rgpElement[0]->pCertContext;
    // Copy it to our own certificate store, so that we can close the "MY"
    // certificate store before returning from this function.
    PCCERT_CONTEXT cert_context2;
    BOOL ok = CertAddCertificateContextToStore(cert_store_, cert_context,
                                               CERT_STORE_ADD_USE_EXISTING,
                                               &cert_context2);
    if (!ok) {
      NOTREACHED();
      continue;
    }
    scoped_refptr<X509Certificate> cert = X509Certificate::CreateFromHandle(
        cert_context2, X509Certificate::SOURCE_LONE_CERT_IMPORT,
        X509Certificate::OSCertHandles());
    X509Certificate::FreeOSCertHandle(cert_context2);
    that->client_certs_.push_back(cert);
  }

  BOOL ok = CertCloseStore(my_cert_store, CERT_CLOSE_STORE_CHECK_FLAG);
  DCHECK(ok);

  // Tell NSS to suspend the client authentication.  We will then abort the
  // handshake by returning ERR_SSL_CLIENT_AUTH_CERT_NEEDED.
  return SECWouldBlock;
#elif defined(OS_MACOSX)
  if (that->ssl_config_.send_client_cert) {
    // TODO(wtc): SSLClientSocketNSS can't do SSL client authentication using
    // CDSA/CSSM yet (http://crbug.com/45369), so client_cert must be NULL.
    DCHECK(!that->ssl_config_.client_cert);
    // Send no client certificate.
    return SECFailure;
  }

  that->client_certs_.clear();

  // First, get the cert issuer names allowed by the server.
  std::vector<CertPrincipal> valid_issuers;
  int n = ca_names->nnames;
  for (int i = 0; i < n; i++) {
    // Parse each name into a CertPrincipal object.
    CertPrincipal p;
    if (p.ParseDistinguishedName(ca_names->names[i].data,
                                 ca_names->names[i].len)) {
      valid_issuers.push_back(p);
    }
  }

  // Now get the available client certs whose issuers are allowed by the server.
  X509Certificate::GetSSLClientCertificates(that->hostname_,
                                            valid_issuers,
                                            &that->client_certs_);

  // Tell NSS to suspend the client authentication.  We will then abort the
  // handshake by returning ERR_SSL_CLIENT_AUTH_CERT_NEEDED.
  return SECWouldBlock;
#else
  void* wincx  = SSL_RevealPinArg(socket);

  // Second pass: a client certificate should have been selected.
  if (that->ssl_config_.send_client_cert) {
    if (that->ssl_config_.client_cert) {
      CERTCertificate* cert = CERT_DupCertificate(
          that->ssl_config_.client_cert->os_cert_handle());
      SECKEYPrivateKey* privkey = PK11_FindKeyByAnyCert(cert, wincx);
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

  // Iterate over all client certificates.
  CERTCertList* client_certs = CERT_FindUserCertsByUsage(
      CERT_GetDefaultCertDB(), certUsageSSLClient,
      PR_FALSE, PR_FALSE, wincx);
  if (client_certs) {
    for (CERTCertListNode* node = CERT_LIST_HEAD(client_certs);
         !CERT_LIST_END(node, client_certs);
         node = CERT_LIST_NEXT(node)) {
      // Only offer unexpired certificates.
      if (CERT_CheckCertValidTimes(node->cert, PR_Now(), PR_TRUE) !=
          secCertTimeValid)
        continue;
      // Filter by issuer.
      //
      // TODO(davidben): This does a binary comparison of the DER-encoded
      // issuers. We should match according to RFC 5280 sec. 7.1. We should find
      // an appropriate NSS function or add one if needbe.
      if (ca_names->nnames &&
          NSS_CmpCertChainWCANames(node->cert, ca_names) != SECSuccess)
        continue;
      X509Certificate* x509_cert = X509Certificate::CreateFromHandle(
          node->cert, X509Certificate::SOURCE_LONE_CERT_IMPORT,
          net::X509Certificate::OSCertHandles());
      that->client_certs_.push_back(x509_cert);
    }
    CERT_DestroyCertList(client_certs);
  }

  // Tell NSS to suspend the client authentication.  We will then abort the
  // handshake by returning ERR_SSL_CLIENT_AUTH_CERT_NEEDED.
  return SECWouldBlock;
#endif
}

// static
// NSS calls this when handshake is completed.
// After the SSL handshake is finished, use CertVerifier to verify
// the saved server certificate.
void SSLClientSocketNSS::HandshakeCallback(PRFileDesc* socket,
                                           void* arg) {
  SSLClientSocketNSS* that = reinterpret_cast<SSLClientSocketNSS*>(arg);

  that->set_handshake_callback_called();

  that->UpdateServerCert();

  that->CheckSecureRenegotiation();
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
    if (handshake_callback_called_) {
      // SSL handshake is completed.  Let's verify the certificate.
      GotoState(STATE_VERIFY_DNSSEC);
      // Done!
    } else {
      // SSL_ForceHandshake returned SECSuccess prematurely.
      rv = SECFailure;
      net_error = ERR_SSL_PROTOCOL_ERROR;
    }
  } else {
    PRErrorCode prerr = PR_GetError();
    net_error = MapHandshakeError(prerr);

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

// DNSValidationResult enumerates the possible outcomes from processing a
// set of DNS records.
enum DNSValidationResult {
  DNSVR_SUCCESS,  // the cert is immediately acceptable.
  DNSVR_FAILURE,  // the cert is unconditionally rejected.
  DNSVR_CONTINUE, // perform CA validation as usual.
};

// VerifyTXTRecords processes the RRDATA for a number of DNS TXT records and
// checks them against the given certificate.
//   dnssec: if true then the TXT records are DNSSEC validated. In this case,
//       DNSVR_SUCCESS may be returned.
//    server_cert_nss: the certificate to validate
//    rrdatas: the TXT records for the current domain.
static DNSValidationResult VerifyTXTRecords(
    bool dnssec,
    CERTCertificate* server_cert_nss,
    const std::vector<base::StringPiece>& rrdatas) {
  bool found_well_formed_record = false;
  bool matched_record = false;

  for (std::vector<base::StringPiece>::const_iterator
       i = rrdatas.begin(); i != rrdatas.end(); ++i) {
    std::map<std::string, std::string> m(
        DNSSECChainVerifier::ParseTLSTXTRecord(*i));
    if (m.empty())
      continue;

    std::map<std::string, std::string>::const_iterator j;
    j = m.find("v");
    if (j == m.end() || j->second != "tls1")
      continue;

    j = m.find("ha");

    HASH_HashType hash_algorithm;
    unsigned hash_length;
    if (j == m.end() || j->second == "sha1") {
      hash_algorithm = HASH_AlgSHA1;
      hash_length = SHA1_LENGTH;
    } else if (j->second == "sha256") {
      hash_algorithm = HASH_AlgSHA256;
      hash_length = SHA256_LENGTH;
    } else {
      continue;
    }

    j = m.find("h");
    if (j == m.end())
      continue;

    std::vector<uint8> given_hash;
    if (!base::HexStringToBytes(j->second, &given_hash))
      continue;

    if (given_hash.size() != hash_length)
      continue;

    uint8 calculated_hash[SHA256_LENGTH];  // SHA256 is the largest.
    SECStatus rv;

    j = m.find("hr");
    if (j == m.end() || j->second == "pubkey") {
      rv = HASH_HashBuf(hash_algorithm, calculated_hash,
                        server_cert_nss->derPublicKey.data,
                        server_cert_nss->derPublicKey.len);
    } else if (j->second == "cert") {
      rv = HASH_HashBuf(hash_algorithm, calculated_hash,
                        server_cert_nss->derCert.data,
                        server_cert_nss->derCert.len);
    } else {
      continue;
    }

    if (rv != SECSuccess)
      NOTREACHED();

    found_well_formed_record = true;

    if (memcmp(calculated_hash, &given_hash[0], hash_length) == 0) {
      matched_record = true;
      if (dnssec)
        return DNSVR_SUCCESS;
    }
  }

  if (found_well_formed_record && !matched_record)
    return DNSVR_FAILURE;

  return DNSVR_CONTINUE;
}


// CheckDNSSECChain tries to validate a DNSSEC chain embedded in
// |server_cert_nss_|. It returns true iff a chain is found that proves the
// value of a TXT record that contains a valid public key fingerprint.
static DNSValidationResult CheckDNSSECChain(
    const std::string& hostname,
    CERTCertificate* server_cert_nss) {
  if (!server_cert_nss)
    return DNSVR_CONTINUE;

  // CERT_FindCertExtensionByOID isn't exported so we have to install an OID,
  // get a tag for it and find the extension by using that tag.
  static SECOidTag dnssec_chain_tag;
  static bool dnssec_chain_tag_valid;
  if (!dnssec_chain_tag_valid) {
    // It's harmless if multiple threads enter this block concurrently.
    static const uint8 kDNSSECChainOID[] =
        // 1.3.6.1.4.1.11129.13172
        // (iso.org.dod.internet.private.enterprises.google.13172)
        {0x2b, 0x06, 0x01, 0x04, 0x01, 0xd6, 0x79, 0xe6, 0x74};
    SECOidData oid_data;
    memset(&oid_data, 0, sizeof(oid_data));
    oid_data.oid.data = const_cast<uint8*>(kDNSSECChainOID);
    oid_data.oid.len = sizeof(kDNSSECChainOID);
    oid_data.desc = "DNSSEC chain";
    oid_data.supportedExtension = SUPPORTED_CERT_EXTENSION;
    dnssec_chain_tag = SECOID_AddEntry(&oid_data);
    DCHECK_NE(SEC_OID_UNKNOWN, dnssec_chain_tag);
    dnssec_chain_tag_valid = true;
  }

  SECItem dnssec_embedded_chain;
  SECStatus rv = CERT_FindCertExtension(server_cert_nss,
      dnssec_chain_tag, &dnssec_embedded_chain);
  if (rv != SECSuccess)
    return DNSVR_CONTINUE;

  base::StringPiece chain(
      reinterpret_cast<char*>(dnssec_embedded_chain.data),
      dnssec_embedded_chain.len);
  std::string dns_hostname;
  if (!DNSDomainFromDot(hostname, &dns_hostname))
    return DNSVR_CONTINUE;
  DNSSECChainVerifier verifier(dns_hostname, chain);
  DNSSECChainVerifier::Error err = verifier.Verify();
  if (err != DNSSECChainVerifier::OK) {
    LOG(ERROR) << "DNSSEC chain verification failed: " << err;
    return DNSVR_CONTINUE;
  }

  if (verifier.rrtype() != kDNS_TXT)
    return DNSVR_CONTINUE;

  DNSValidationResult r = VerifyTXTRecords(
      true /* DNSSEC verified */, server_cert_nss, verifier.rrdatas());
  SECITEM_FreeItem(&dnssec_embedded_chain, PR_FALSE);
  return r;
}

int SSLClientSocketNSS::DoVerifyDNSSEC(int result) {
  if (ssl_config_.dnssec_enabled) {
    DNSValidationResult r = CheckDNSSECChain(hostname_, server_cert_nss_);
    if (r == DNSVR_SUCCESS) {
      GotoState(STATE_VERIFY_CERT_COMPLETE);
      return OK;
    }
  }

  if (dnssec_provider_ == NULL) {
    GotoState(STATE_VERIFY_CERT);
    return OK;
  }

  GotoState(STATE_VERIFY_DNSSEC_COMPLETE);
  RRResponse* response;
  dnssec_wait_start_time_ = base::Time::Now();
  return dnssec_provider_->GetDNSSECRecords(&response, &handshake_io_callback_);
}

int SSLClientSocketNSS::DoVerifyDNSSECComplete(int result) {
  RRResponse* response;
  int err = dnssec_provider_->GetDNSSECRecords(&response, NULL);
  DCHECK_EQ(err, OK);

  const base::TimeDelta elapsed = base::Time::Now() - dnssec_wait_start_time_;
  HISTOGRAM_TIMES("Net.DNSSECWaitTime", elapsed);

  GotoState(STATE_VERIFY_CERT);
  if (!response || response->rrdatas.empty())
    return OK;

  std::vector<base::StringPiece> records;
  records.resize(response->rrdatas.size());
  for (unsigned i = 0; i < response->rrdatas.size(); i++)
    records[i] = base::StringPiece(response->rrdatas[i]);
  DNSValidationResult r =
      VerifyTXTRecords(response->dnssec, server_cert_nss_, records);

  if (!ssl_config_.dnssec_enabled) {
    // If DNSSEC is not enabled we don't take any action based on the result,
    // except to record the latency, above.
    GotoState(STATE_VERIFY_CERT);
    return OK;
  }

  switch (r) {
    case DNSVR_FAILURE:
      GotoState(STATE_VERIFY_CERT_COMPLETE);
      return ERR_CERT_NOT_IN_DNS;
    case DNSVR_CONTINUE:
      GotoState(STATE_VERIFY_CERT);
      break;
    case DNSVR_SUCCESS:
      GotoState(STATE_VERIFY_CERT_COMPLETE);
      break;
    default:
      NOTREACHED();
      GotoState(STATE_VERIFY_CERT);
  }

  return OK;
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
        server_cert_nss_, PR_Now(), certUsageSSLCA);
    if (cert_list) {
      for (CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
           !CERT_LIST_END(node, cert_list);
           node = CERT_LIST_NEXT(node)) {
        if (node->cert->slot || node->cert->isRoot || node->cert->isperm ||
            node->cert == server_cert_nss_) {
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
  // TODO(ukai): we may not need this call because it is now harmless to have a
  // session with a bad cert.
  InvalidateSessionIfBadCertificate();
  // Exit DoHandshakeLoop and return the result to the caller to Connect.
  DCHECK(next_handshake_state_ == STATE_NONE);
  return result;
}

int SSLClientSocketNSS::DoPayloadRead() {
  EnterFunction(user_read_buf_len_);
  DCHECK(user_read_buf_);
  DCHECK_GT(user_read_buf_len_, 0);
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
  return MapNSPRError(prerr);
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
    LeaveFunction("");
    return ERR_IO_PENDING;
  }
  LeaveFunction("");
  return MapNSPRError(prerr);
}

}  // namespace net
