// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// OpenSSL binding for SSLClientSocket. The class layout and general principle
// of operation is derived from SSLClientSocketNSS.

#include "net/socket/ssl_client_socket_openssl.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "base/metrics/histogram.h"
#include "base/openssl_util.h"
#include "base/singleton.h"
#include "base/synchronization/lock.h"
#include "net/base/cert_verifier.h"
#include "net/base/net_errors.h"
#include "net/base/openssl_private_key_store.h"
#include "net/base/ssl_cert_request_info.h"
#include "net/base/ssl_connection_status_flags.h"
#include "net/base/ssl_info.h"
#include "net/socket/ssl_error_params.h"

namespace net {

namespace {

// Enable this to see logging for state machine state transitions.
#if 0
#define GotoState(s) do { DVLOG(2) << (void *)this << " " << __FUNCTION__ << \
                           " jump to state " << s; \
                           next_handshake_state_ = s; } while (0)
#else
#define GotoState(s) next_handshake_state_ = s
#endif

const size_t kMaxRecvBufferSize = 4096;
const int kSessionCacheTimeoutSeconds = 60 * 60;
const size_t kSessionCacheMaxEntires = 1024;

// This method doesn't seemed to have made it into the OpenSSL headers.
unsigned long SSL_CIPHER_get_id(const SSL_CIPHER* cipher) { return cipher->id; }

// Used for encoding the |connection_status| field of an SSLInfo object.
int EncodeSSLConnectionStatus(int cipher_suite,
                              int compression,
                              int version) {
  return ((cipher_suite & SSL_CONNECTION_CIPHERSUITE_MASK) <<
          SSL_CONNECTION_CIPHERSUITE_SHIFT) |
         ((compression & SSL_CONNECTION_COMPRESSION_MASK) <<
          SSL_CONNECTION_COMPRESSION_SHIFT) |
         ((version & SSL_CONNECTION_VERSION_MASK) <<
          SSL_CONNECTION_VERSION_SHIFT);
}

// Returns the net SSL version number (see ssl_connection_status_flags.h) for
// this SSL connection.
int GetNetSSLVersion(SSL* ssl) {
  switch (SSL_version(ssl)) {
    case SSL2_VERSION:
      return SSL_CONNECTION_VERSION_SSL2;
    case SSL3_VERSION:
      return SSL_CONNECTION_VERSION_SSL3;
    case TLS1_VERSION:
      return SSL_CONNECTION_VERSION_TLS1;
    case 0x0302:
      return SSL_CONNECTION_VERSION_TLS1_1;
    case 0x0303:
      return SSL_CONNECTION_VERSION_TLS1_2;
    default:
      return SSL_CONNECTION_VERSION_UNKNOWN;
  }
}

int MapOpenSSLErrorSSL() {
  // Walk down the error stack to find the SSLerr generated reason.
  unsigned long error_code;
  do {
    error_code = ERR_get_error();
    if (error_code == 0)
      return ERR_SSL_PROTOCOL_ERROR;
  } while (ERR_GET_LIB(error_code) != ERR_LIB_SSL);

  DVLOG(1) << "OpenSSL SSL error, reason: " << ERR_GET_REASON(error_code)
           << ", name: " << ERR_error_string(error_code, NULL);
  switch (ERR_GET_REASON(error_code)) {
    case SSL_R_READ_TIMEOUT_EXPIRED:
      return ERR_TIMED_OUT;
    case SSL_R_BAD_RESPONSE_ARGUMENT:
      return ERR_INVALID_ARGUMENT;
    case SSL_R_UNKNOWN_CERTIFICATE_TYPE:
    case SSL_R_UNKNOWN_CIPHER_TYPE:
    case SSL_R_UNKNOWN_KEY_EXCHANGE_TYPE:
    case SSL_R_UNKNOWN_PKEY_TYPE:
    case SSL_R_UNKNOWN_REMOTE_ERROR_TYPE:
    case SSL_R_UNKNOWN_SSL_VERSION:
      return ERR_NOT_IMPLEMENTED;
    case SSL_R_UNSUPPORTED_SSL_VERSION:
    case SSL_R_NO_CIPHER_MATCH:
    case SSL_R_NO_SHARED_CIPHER:
    case SSL_R_TLSV1_ALERT_INSUFFICIENT_SECURITY:
    case SSL_R_TLSV1_ALERT_PROTOCOL_VERSION:
      return ERR_SSL_VERSION_OR_CIPHER_MISMATCH;
    case SSL_R_SSLV3_ALERT_BAD_CERTIFICATE:
    case SSL_R_SSLV3_ALERT_UNSUPPORTED_CERTIFICATE:
    case SSL_R_SSLV3_ALERT_CERTIFICATE_REVOKED:
    case SSL_R_SSLV3_ALERT_CERTIFICATE_EXPIRED:
    case SSL_R_SSLV3_ALERT_CERTIFICATE_UNKNOWN:
    case SSL_R_TLSV1_ALERT_ACCESS_DENIED:
    case SSL_R_TLSV1_ALERT_UNKNOWN_CA:
      return ERR_BAD_SSL_CLIENT_AUTH_CERT;
    case SSL_R_BAD_DECOMPRESSION:
    case SSL_R_SSLV3_ALERT_DECOMPRESSION_FAILURE:
      return ERR_SSL_DECOMPRESSION_FAILURE_ALERT;
    case SSL_R_SSLV3_ALERT_BAD_RECORD_MAC:
      return ERR_SSL_BAD_RECORD_MAC_ALERT;
    case SSL_R_UNSAFE_LEGACY_RENEGOTIATION_DISABLED:
      return ERR_SSL_UNSAFE_NEGOTIATION;
    case SSL_R_WRONG_NUMBER_OF_KEY_BITS:
      return ERR_SSL_WEAK_SERVER_EPHEMERAL_DH_KEY;
    // SSL_R_UNKNOWN_PROTOCOL is reported if premature application data is
    // received (see http://crbug.com/42538), and also if all the protocol
    // versions supported by the server were disabled in this socket instance.
    // Mapped to ERR_SSL_PROTOCOL_ERROR for compatibility with other SSL sockets
    // in the former scenario.
    case SSL_R_UNKNOWN_PROTOCOL:
    case SSL_R_SSL_HANDSHAKE_FAILURE:
    case SSL_R_DECRYPTION_FAILED:
    case SSL_R_DECRYPTION_FAILED_OR_BAD_RECORD_MAC:
    case SSL_R_DH_PUBLIC_VALUE_LENGTH_IS_WRONG:
    case SSL_R_DIGEST_CHECK_FAILED:
    case SSL_R_DUPLICATE_COMPRESSION_ID:
    case SSL_R_ECGROUP_TOO_LARGE_FOR_CIPHER:
    case SSL_R_ENCRYPTED_LENGTH_TOO_LONG:
    case SSL_R_ERROR_IN_RECEIVED_CIPHER_LIST:
    case SSL_R_EXCESSIVE_MESSAGE_SIZE:
    case SSL_R_EXTRA_DATA_IN_MESSAGE:
    case SSL_R_GOT_A_FIN_BEFORE_A_CCS:
    case SSL_R_ILLEGAL_PADDING:
    case SSL_R_INVALID_CHALLENGE_LENGTH:
    case SSL_R_INVALID_COMMAND:
    case SSL_R_INVALID_PURPOSE:
    case SSL_R_INVALID_STATUS_RESPONSE:
    case SSL_R_INVALID_TICKET_KEYS_LENGTH:
    case SSL_R_KEY_ARG_TOO_LONG:
    case SSL_R_READ_WRONG_PACKET_TYPE:
    case SSL_R_SSLV3_ALERT_UNEXPECTED_MESSAGE:
    // TODO(joth): SSL_R_SSLV3_ALERT_HANDSHAKE_FAILURE may be returned from the
    // server after receiving ClientHello if there's no common supported cipher.
    // Ideally we'd map that specific case to ERR_SSL_VERSION_OR_CIPHER_MISMATCH
    // to match the NSS implementation. See also http://goo.gl/oMtZW
    case SSL_R_SSLV3_ALERT_HANDSHAKE_FAILURE:
    case SSL_R_SSLV3_ALERT_NO_CERTIFICATE:
    case SSL_R_SSLV3_ALERT_ILLEGAL_PARAMETER:
    case SSL_R_TLSV1_ALERT_DECODE_ERROR:
    case SSL_R_TLSV1_ALERT_DECRYPTION_FAILED:
    case SSL_R_TLSV1_ALERT_DECRYPT_ERROR:
    case SSL_R_TLSV1_ALERT_EXPORT_RESTRICTION:
    case SSL_R_TLSV1_ALERT_INTERNAL_ERROR:
    case SSL_R_TLSV1_ALERT_NO_RENEGOTIATION:
    case SSL_R_TLSV1_ALERT_RECORD_OVERFLOW:
    case SSL_R_TLSV1_ALERT_USER_CANCELLED:
      return ERR_SSL_PROTOCOL_ERROR;
    default:
      LOG(WARNING) << "Unmapped error reason: " << ERR_GET_REASON(error_code);
      return ERR_FAILED;
  }
}

// Converts an OpenSSL error code into a net error code, walking the OpenSSL
// error stack if needed. Note that |tracer| is not currently used in the
// implementation, but is passed in anyway as this ensures the caller will clear
// any residual codes left on the error stack.
int MapOpenSSLError(int err, const base::OpenSSLErrStackTracer& tracer) {
  switch (err) {
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
      return ERR_IO_PENDING;
    case SSL_ERROR_SYSCALL:
      DVLOG(1) << "OpenSSL SYSCALL error, errno " << errno;
      return ERR_SSL_PROTOCOL_ERROR;
    case SSL_ERROR_SSL:
      return MapOpenSSLErrorSSL();
    default:
      // TODO(joth): Implement full mapping.
      LOG(WARNING) << "Unknown OpenSSL error " << err;
      return ERR_SSL_PROTOCOL_ERROR;
  }
}

// We do certificate verification after handshake, so we disable the default
// by registering a no-op verify function.
int NoOpVerifyCallback(X509_STORE_CTX*, void *) {
  DVLOG(3) << "skipping cert verify";
  return 1;
}

// OpenSSL manages a cache of SSL_SESSION, this class provides the application
// side policy for that cache about session re-use: we retain one session per
// unique HostPortPair.
class SSLSessionCache {
 public:
  SSLSessionCache() {}

  void OnSessionAdded(const HostPortPair& host_and_port, SSL_SESSION* session) {
    // Declare the session cleaner-upper before the lock, so any call into
    // OpenSSL to free the session will happen after the lock is released.
    base::ScopedOpenSSL<SSL_SESSION, SSL_SESSION_free> session_to_free;
    base::AutoLock lock(lock_);

    DCHECK_EQ(0U, session_map_.count(session));
    std::pair<HostPortMap::iterator, bool> res =
        host_port_map_.insert(std::make_pair(host_and_port, session));
    if (!res.second) {  // Already exists: replace old entry.
      session_to_free.reset(res.first->second);
      session_map_.erase(session_to_free.get());
      res.first->second = session;
    }
    DVLOG(2) << "Adding session " << session << " => "
             << host_and_port.ToString() << ", new entry = " << res.second;
    DCHECK(host_port_map_[host_and_port] == session);
    session_map_[session] = res.first;
    DCHECK_EQ(host_port_map_.size(), session_map_.size());
    DCHECK_LE(host_port_map_.size(), kSessionCacheMaxEntires);
  }

  void OnSessionRemoved(SSL_SESSION* session) {
    // Declare the session cleaner-upper before the lock, so any call into
    // OpenSSL to free the session will happen after the lock is released.
    base::ScopedOpenSSL<SSL_SESSION, SSL_SESSION_free> session_to_free;
    base::AutoLock lock(lock_);

    SessionMap::iterator it = session_map_.find(session);
    if (it == session_map_.end())
      return;
    DVLOG(2) << "Remove session " << session << " => "
             << it->second->first.ToString();
    DCHECK(it->second->second == session);
    host_port_map_.erase(it->second);
    session_map_.erase(it);
    session_to_free.reset(session);
    DCHECK_EQ(host_port_map_.size(), session_map_.size());
  }

  // Looks up the host:port in the cache, and if a session is found it is added
  // to |ssl|, returning true on success.
  bool SetSSLSession(SSL* ssl, const HostPortPair& host_and_port) {
    base::AutoLock lock(lock_);
    HostPortMap::iterator it = host_port_map_.find(host_and_port);
    if (it == host_port_map_.end())
      return false;
    DVLOG(2) << "Lookup session: " << it->second << " => "
             << host_and_port.ToString();
    SSL_SESSION* session = it->second;
    DCHECK(session);
    DCHECK(session_map_[session] == it);
    // Ideally we'd release |lock_| before calling into OpenSSL here, however
    // that opens a small risk |session| will go out of scope before it is used.
    // Alternatively we would take a temporary local refcount on |session|,
    // except OpenSSL does not provide a public API for adding a ref (c.f.
    // SSL_SESSION_free which decrements the ref).
    return SSL_set_session(ssl, session) == 1;
  }

 private:
  // A pair of maps to allow bi-directional lookups between host:port and an
  // associated session.
  // TODO(joth): When client certificates are implemented we should key the
  // cache on the client certificate used in addition to the host-port pair.
  typedef std::map<HostPortPair, SSL_SESSION*> HostPortMap;
  typedef std::map<SSL_SESSION*, HostPortMap::iterator> SessionMap;
  HostPortMap host_port_map_;
  SessionMap session_map_;

  // Protects access to both the above maps.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(SSLSessionCache);
};

class SSLContext {
 public:
  static SSLContext* GetInstance() { return Singleton<SSLContext>::get(); }
  SSL_CTX* ssl_ctx() { return ssl_ctx_.get(); }
  SSLSessionCache* session_cache() { return &session_cache_; }

  SSLClientSocketOpenSSL* GetClientSocketFromSSL(SSL* ssl) {
    DCHECK(ssl);
    SSLClientSocketOpenSSL* socket = static_cast<SSLClientSocketOpenSSL*>(
        SSL_get_ex_data(ssl, ssl_socket_data_index_));
    DCHECK(socket);
    return socket;
  }

  bool SetClientSocketForSSL(SSL* ssl, SSLClientSocketOpenSSL* socket) {
    return SSL_set_ex_data(ssl, ssl_socket_data_index_, socket) != 0;
  }

 private:
  friend struct DefaultSingletonTraits<SSLContext>;

  SSLContext() {
    base::EnsureOpenSSLInit();
    ssl_socket_data_index_ = SSL_get_ex_new_index(0, 0, 0, 0, 0);
    DCHECK_NE(ssl_socket_data_index_, -1);
    ssl_ctx_.reset(SSL_CTX_new(SSLv23_client_method()));
    SSL_CTX_set_cert_verify_callback(ssl_ctx_.get(), NoOpVerifyCallback, NULL);
    SSL_CTX_set_session_cache_mode(ssl_ctx_.get(), SSL_SESS_CACHE_CLIENT);
    SSL_CTX_sess_set_new_cb(ssl_ctx_.get(), NewSessionCallbackStatic);
    SSL_CTX_sess_set_remove_cb(ssl_ctx_.get(), RemoveSessionCallbackStatic);
    SSL_CTX_set_timeout(ssl_ctx_.get(), kSessionCacheTimeoutSeconds);
    SSL_CTX_sess_set_cache_size(ssl_ctx_.get(), kSessionCacheMaxEntires);
    SSL_CTX_set_client_cert_cb(ssl_ctx_.get(), ClientCertCallback);
#if defined(OPENSSL_NPN_NEGOTIATED)
    // TODO(kristianm): Only select this if ssl_config_.next_proto is not empty.
    // It would be better if the callback were not a global setting,
    // but that is an OpenSSL issue.
    SSL_CTX_set_next_proto_select_cb(ssl_ctx_.get(), SelectNextProtoCallback,
                                     NULL);
#endif
  }

  static int NewSessionCallbackStatic(SSL* ssl, SSL_SESSION* session) {
    return GetInstance()->NewSessionCallback(ssl, session);
  }

  int NewSessionCallback(SSL* ssl, SSL_SESSION* session) {
    SSLClientSocketOpenSSL* socket = GetClientSocketFromSSL(ssl);
    session_cache_.OnSessionAdded(socket->host_and_port(), session);
    return 1;  // 1 => We took ownership of |session|.
  }

  static void RemoveSessionCallbackStatic(SSL_CTX* ctx, SSL_SESSION* session) {
    return GetInstance()->RemoveSessionCallback(ctx, session);
  }

  void RemoveSessionCallback(SSL_CTX* ctx, SSL_SESSION* session) {
    DCHECK(ctx == ssl_ctx());
    session_cache_.OnSessionRemoved(session);
  }

  static int ClientCertCallback(SSL* ssl, X509** x509, EVP_PKEY** pkey) {
    SSLClientSocketOpenSSL* socket = GetInstance()->GetClientSocketFromSSL(ssl);
    CHECK(socket);
    return socket->ClientCertRequestCallback(ssl, x509, pkey);
  }

  static int SelectNextProtoCallback(SSL* ssl,
                                     unsigned char** out, unsigned char* outlen,
                                     const unsigned char* in,
                                     unsigned int inlen, void* arg) {
    SSLClientSocketOpenSSL* socket = GetInstance()->GetClientSocketFromSSL(ssl);
    return socket->SelectNextProtoCallback(out, outlen, in, inlen);
  }

  // This is the index used with SSL_get_ex_data to retrieve the owner
  // SSLClientSocketOpenSSL object from an SSL instance.
  int ssl_socket_data_index_;

  base::ScopedOpenSSL<SSL_CTX, SSL_CTX_free> ssl_ctx_;
  SSLSessionCache session_cache_;
};

// Utility to construct the appropriate set & clear masks for use the OpenSSL
// options and mode configuration functions. (SSL_set_options etc)
struct SslSetClearMask {
  SslSetClearMask() : set_mask(0), clear_mask(0) {}
  void ConfigureFlag(long flag, bool state) {
    (state ? set_mask : clear_mask) |= flag;
    // Make sure we haven't got any intersection in the set & clear options.
    DCHECK_EQ(0, set_mask & clear_mask) << flag << ":" << state;
  }
  long set_mask;
  long clear_mask;
};

}  // namespace

SSLClientSocketOpenSSL::SSLClientSocketOpenSSL(
    ClientSocketHandle* transport_socket,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config,
    CertVerifier* cert_verifier)
    : ALLOW_THIS_IN_INITIALIZER_LIST(buffer_send_callback_(
          this, &SSLClientSocketOpenSSL::BufferSendComplete)),
      ALLOW_THIS_IN_INITIALIZER_LIST(buffer_recv_callback_(
          this, &SSLClientSocketOpenSSL::BufferRecvComplete)),
      transport_send_busy_(false),
      transport_recv_busy_(false),
      user_connect_callback_(NULL),
      user_read_callback_(NULL),
      user_write_callback_(NULL),
      completed_handshake_(false),
      client_auth_cert_needed_(false),
      cert_verifier_(cert_verifier),
      ALLOW_THIS_IN_INITIALIZER_LIST(handshake_io_callback_(
          this, &SSLClientSocketOpenSSL::OnHandshakeIOComplete)),
      ssl_(NULL),
      transport_bio_(NULL),
      transport_(transport_socket),
      host_and_port_(host_and_port),
      ssl_config_(ssl_config),
      trying_cached_session_(false),
      npn_status_(kNextProtoUnsupported),
      net_log_(transport_socket->socket()->NetLog()) {
}

SSLClientSocketOpenSSL::~SSLClientSocketOpenSSL() {
  Disconnect();
}

bool SSLClientSocketOpenSSL::Init() {
  DCHECK(!ssl_);
  DCHECK(!transport_bio_);

  SSLContext* context = SSLContext::GetInstance();
  base::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  ssl_ = SSL_new(context->ssl_ctx());
  if (!ssl_ || !context->SetClientSocketForSSL(ssl_, this))
    return false;

  if (!SSL_set_tlsext_host_name(ssl_, host_and_port_.host().c_str()))
    return false;

  trying_cached_session_ =
      context->session_cache()->SetSSLSession(ssl_, host_and_port_);

  BIO* ssl_bio = NULL;
  // 0 => use default buffer sizes.
  if (!BIO_new_bio_pair(&ssl_bio, 0, &transport_bio_, 0))
    return false;
  DCHECK(ssl_bio);
  DCHECK(transport_bio_);

  SSL_set_bio(ssl_, ssl_bio, ssl_bio);

  // OpenSSL defaults some options to on, others to off. To avoid ambiguity,
  // set everything we care about to an absolute value.
  SslSetClearMask options;
  options.ConfigureFlag(SSL_OP_NO_SSLv2, true);
  options.ConfigureFlag(SSL_OP_NO_SSLv3, !ssl_config_.ssl3_enabled);
  options.ConfigureFlag(SSL_OP_NO_TLSv1, !ssl_config_.tls1_enabled);

#if defined(SSL_OP_NO_COMPRESSION)
  // If TLS was disabled also disable compression, to provide maximum site
  // compatibility in the case of protocol fallback. See http://crbug.com/31628
  options.ConfigureFlag(SSL_OP_NO_COMPRESSION, !ssl_config_.tls1_enabled);
#endif

  // TODO(joth): Set this conditionally, see http://crbug.com/55410
  options.ConfigureFlag(SSL_OP_LEGACY_SERVER_CONNECT, true);

  SSL_set_options(ssl_, options.set_mask);
  SSL_clear_options(ssl_, options.clear_mask);

  // Same as above, this time for the SSL mode.
  SslSetClearMask mode;

#if defined(SSL_MODE_HANDSHAKE_CUTTHROUGH)
  mode.ConfigureFlag(SSL_MODE_HANDSHAKE_CUTTHROUGH,
                     ssl_config_.false_start_enabled &&
                     !SSLConfigService::IsKnownFalseStartIncompatibleServer(
                         host_and_port_.host()));
#endif

#if defined(SSL_MODE_RELEASE_BUFFERS)
  mode.ConfigureFlag(SSL_MODE_RELEASE_BUFFERS, true);
#endif

#if defined(SSL_MODE_SMALL_BUFFERS)
  mode.ConfigureFlag(SSL_MODE_SMALL_BUFFERS, true);
#endif

  SSL_set_mode(ssl_, mode.set_mask);
  SSL_clear_mode(ssl_, mode.clear_mask);

  // Removing ciphers by ID from OpenSSL is a bit involved as we must use the
  // textual name with SSL_set_cipher_list because there is no public API to
  // directly remove a cipher by ID.
  STACK_OF(SSL_CIPHER)* ciphers = SSL_get_ciphers(ssl_);
  DCHECK(ciphers);
  // See SSLConfig::disabled_cipher_suites for description of the suites
  // disabled by default.
  std::string command("DEFAULT:!NULL:!aNULL:!IDEA:!FZA");
  // Walk through all the installed ciphers, seeing if any need to be
  // appended to the cipher removal |command|.
  for (int i = 0; i < sk_SSL_CIPHER_num(ciphers); ++i) {
    const SSL_CIPHER* cipher = sk_SSL_CIPHER_value(ciphers, i);
    const uint16 id = SSL_CIPHER_get_id(cipher);
    // Remove any ciphers with a strength of less than 80 bits. Note the NSS
    // implementation uses "effective" bits here but OpenSSL does not provide
    // this detail. This only impacts Triple DES: reports 112 vs. 168 bits,
    // both of which are greater than 80 anyway.
    bool disable = SSL_CIPHER_get_bits(cipher, NULL) < 80;
    if (!disable) {
      disable = std::find(ssl_config_.disabled_cipher_suites.begin(),
                          ssl_config_.disabled_cipher_suites.end(), id) !=
                    ssl_config_.disabled_cipher_suites.end();
    }
    if (disable) {
       const char* name = SSL_CIPHER_get_name(cipher);
       DVLOG(3) << "Found cipher to remove: '" << name << "', ID: " << id
                << " strength: " << SSL_CIPHER_get_bits(cipher, NULL);
       command.append(":!");
       command.append(name);
     }
  }
  int rv = SSL_set_cipher_list(ssl_, command.c_str());
  // If this fails (rv = 0) it means there are no ciphers enabled on this SSL.
  // This will almost certainly result in the socket failing to complete the
  // handshake at which point the appropriate error is bubbled up to the client.
  LOG_IF(WARNING, rv != 1) << "SSL_set_cipher_list('" << command << "') "
                              "returned " << rv;
  return true;
}

int SSLClientSocketOpenSSL::ClientCertRequestCallback(SSL* ssl,
                                                      X509** x509,
                                                      EVP_PKEY** pkey) {
  DVLOG(3) << "OpenSSL ClientCertRequestCallback called";
  DCHECK(ssl == ssl_);
  DCHECK(*x509 == NULL);
  DCHECK(*pkey == NULL);

  if (!ssl_config_.send_client_cert) {
    client_auth_cert_needed_ = true;
    return -1;  // Suspends handshake.
  }

  // Second pass: a client certificate should have been selected.
  if (ssl_config_.client_cert) {
    EVP_PKEY* privkey = OpenSSLPrivateKeyStore::GetInstance()->FetchPrivateKey(
        X509_PUBKEY_get(X509_get_X509_PUBKEY(
            ssl_config_.client_cert->os_cert_handle())));
    if (privkey) {
      // TODO(joth): (copied from NSS) We should wait for server certificate
      // verification before sending our credentials. See http://crbug.com/13934
      *x509 = X509Certificate::DupOSCertHandle(
          ssl_config_.client_cert->os_cert_handle());
      *pkey = privkey;
      return 1;
    }
    LOG(WARNING) << "Client cert found without private key";
  }

  // Send no client certificate.
  return 0;
}

// SSLClientSocket methods

void SSLClientSocketOpenSSL::GetSSLInfo(SSLInfo* ssl_info) {
  ssl_info->Reset();
  if (!server_cert_)
    return;

  ssl_info->cert = server_cert_;
  ssl_info->cert_status = server_cert_verify_result_.cert_status;

  const SSL_CIPHER* cipher = SSL_get_current_cipher(ssl_);
  CHECK(cipher);
  ssl_info->security_bits = SSL_CIPHER_get_bits(cipher, NULL);
  const COMP_METHOD* compression = SSL_get_current_compression(ssl_);

  ssl_info->connection_status = EncodeSSLConnectionStatus(
      SSL_CIPHER_get_id(cipher),
      compression ? compression->type : 0,
      GetNetSSLVersion(ssl_));

  bool peer_supports_renego_ext = !!SSL_get_secure_renegotiation_support(ssl_);
  if (!peer_supports_renego_ext)
    ssl_info->connection_status |= SSL_CONNECTION_NO_RENEGOTIATION_EXTENSION;
  UMA_HISTOGRAM_ENUMERATION("Net.RenegotiationExtensionSupported",
                            implicit_cast<int>(peer_supports_renego_ext), 2);

  if (ssl_config_.ssl3_fallback)
    ssl_info->connection_status |= SSL_CONNECTION_SSL3_FALLBACK;

  DVLOG(3) << "Encoded connection status: cipher suite = "
      << SSLConnectionStatusToCipherSuite(ssl_info->connection_status)
      << " compression = "
      << SSLConnectionStatusToCompression(ssl_info->connection_status)
      << " version = "
      << SSLConnectionStatusToVersion(ssl_info->connection_status);
}

void SSLClientSocketOpenSSL::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  cert_request_info->host_and_port = host_and_port_.ToString();
  cert_request_info->client_certs = client_certs_;
}

SSLClientSocket::NextProtoStatus SSLClientSocketOpenSSL::GetNextProto(
    std::string* proto) {
  *proto = npn_proto_;
  return npn_status_;
}

void SSLClientSocketOpenSSL::DoReadCallback(int rv) {
  // Since Run may result in Read being called, clear |user_read_callback_|
  // up front.
  CompletionCallback* c = user_read_callback_;
  user_read_callback_ = NULL;
  user_read_buf_ = NULL;
  user_read_buf_len_ = 0;
  c->Run(rv);
}

void SSLClientSocketOpenSSL::DoWriteCallback(int rv) {
  // Since Run may result in Write being called, clear |user_write_callback_|
  // up front.
  CompletionCallback* c = user_write_callback_;
  user_write_callback_ = NULL;
  user_write_buf_ = NULL;
  user_write_buf_len_ = 0;
  c->Run(rv);
}

// ClientSocket methods

int SSLClientSocketOpenSSL::Connect(CompletionCallback* callback) {
  net_log_.BeginEvent(NetLog::TYPE_SSL_CONNECT, NULL);

  // Set up new ssl object.
  if (!Init()) {
    int result = ERR_UNEXPECTED;
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_SSL_CONNECT, result);
    return result;
  }

  // Set SSL to client mode. Handshake happens in the loop below.
  SSL_set_connect_state(ssl_);

  GotoState(STATE_HANDSHAKE);
  int rv = DoHandshakeLoop(net::OK);
  if (rv == ERR_IO_PENDING) {
    user_connect_callback_ = callback;
  } else {
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_SSL_CONNECT, rv);
  }

  return rv > OK ? OK : rv;
}

void SSLClientSocketOpenSSL::Disconnect() {
  if (ssl_) {
    SSL_free(ssl_);
    ssl_ = NULL;
  }
  if (transport_bio_) {
    BIO_free_all(transport_bio_);
    transport_bio_ = NULL;
  }

  // Shut down anything that may call us back (through buffer_send_callback_,
  // buffer_recv_callback, or handshake_io_callback_).
  verifier_.reset();
  transport_->socket()->Disconnect();

  // Null all callbacks, delete all buffers.
  transport_send_busy_ = false;
  send_buffer_ = NULL;
  transport_recv_busy_ = false;
  recv_buffer_ = NULL;

  user_connect_callback_ = NULL;
  user_read_callback_    = NULL;
  user_write_callback_   = NULL;
  user_read_buf_         = NULL;
  user_read_buf_len_     = 0;
  user_write_buf_        = NULL;
  user_write_buf_len_    = 0;

  server_cert_verify_result_.Reset();
  completed_handshake_ = false;

  client_certs_.clear();
  client_auth_cert_needed_ = false;
}

int SSLClientSocketOpenSSL::DoHandshakeLoop(int last_io_result) {
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
        NOTREACHED() << "unexpected state" << state;
        break;
    }

    // To avoid getting an ERR_IO_PENDING here after handshake complete.
    if (next_handshake_state_ == STATE_NONE)
      break;

    // Do the actual network I/O.
    network_moved = DoTransportIO();
  } while ((rv != ERR_IO_PENDING || network_moved) &&
            next_handshake_state_ != STATE_NONE);
  return rv;
}

int SSLClientSocketOpenSSL::DoHandshake() {
  base::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  int net_error = net::OK;
  int rv = SSL_do_handshake(ssl_);

  if (client_auth_cert_needed_) {
    net_error = ERR_SSL_CLIENT_AUTH_CERT_NEEDED;
    // If the handshake already succeeded (because the server requests but
    // doesn't require a client cert), we need to invalidate the SSL session
    // so that we won't try to resume the non-client-authenticated session in
    // the next handshake.  This will cause the server to ask for a client
    // cert again.
    if (rv == 1) {
      // Remove from session cache but don't clear this connection.
      SSL_SESSION* session = SSL_get_session(ssl_);
      if (session) {
        int rv = SSL_CTX_remove_session(SSL_get_SSL_CTX(ssl_), session);
        LOG_IF(WARNING, !rv) << "Couldn't invalidate SSL session: " << session;
      }
    }
  } else if (rv == 1) {
    if (trying_cached_session_ && logging::DEBUG_MODE) {
      DVLOG(2) << "Result of session reuse for " << host_and_port_.ToString()
               << " is: " << (SSL_session_reused(ssl_) ? "Success" : "Fail");
    }
    // SSL handshake is completed.  Let's verify the certificate.
    const bool got_cert = !!UpdateServerCert();
    DCHECK(got_cert);
    GotoState(STATE_VERIFY_CERT);
  } else {
    int ssl_error = SSL_get_error(ssl_, rv);
    net_error = MapOpenSSLError(ssl_error, err_tracer);

    // If not done, stay in this state
    if (net_error == ERR_IO_PENDING) {
      GotoState(STATE_HANDSHAKE);
    } else {
      LOG(ERROR) << "handshake failed; returned " << rv
                 << ", SSL error code " << ssl_error
                 << ", net_error " << net_error;
      net_log_.AddEvent(
          NetLog::TYPE_SSL_HANDSHAKE_ERROR,
          make_scoped_refptr(new SSLErrorParams(net_error, ssl_error)));
    }
  }
  return net_error;
}

int SSLClientSocketOpenSSL::SelectNextProtoCallback(unsigned char** out,
                                                    unsigned char* outlen,
                                                    const unsigned char* in,
                                                    unsigned int inlen) {
#if defined(OPENSSL_NPN_NEGOTIATED)
  if (ssl_config_.next_protos.empty()) {
    *out = reinterpret_cast<uint8*>(const_cast<char*>("http/1.1"));
    *outlen = 8;
    npn_status_ = SSLClientSocket::kNextProtoUnsupported;
    return SSL_TLSEXT_ERR_OK;
  }

  int status = SSL_select_next_proto(
      out, outlen, in, inlen,
      reinterpret_cast<const unsigned char*>(ssl_config_.next_protos.data()),
      ssl_config_.next_protos.size());

  npn_proto_.assign(reinterpret_cast<const char*>(*out), *outlen);
  switch (status) {
    case OPENSSL_NPN_UNSUPPORTED:
      npn_status_ = SSLClientSocket::kNextProtoUnsupported;
      break;
    case OPENSSL_NPN_NEGOTIATED:
      npn_status_ = SSLClientSocket::kNextProtoNegotiated;
      break;
    case OPENSSL_NPN_NO_OVERLAP:
      npn_status_ = SSLClientSocket::kNextProtoNoOverlap;
      break;
    default:
      NOTREACHED() << status;
      break;
  }
  DVLOG(2) << "next protocol: '" << npn_proto_ << "' status: " << npn_status_;
#endif
  return SSL_TLSEXT_ERR_OK;
}

int SSLClientSocketOpenSSL::DoVerifyCert(int result) {
  DCHECK(server_cert_);
  GotoState(STATE_VERIFY_CERT_COMPLETE);
  int flags = 0;

  if (ssl_config_.rev_checking_enabled)
    flags |= X509Certificate::VERIFY_REV_CHECKING_ENABLED;
  if (ssl_config_.verify_ev_cert)
    flags |= X509Certificate::VERIFY_EV_CERT;
  verifier_.reset(new SingleRequestCertVerifier(cert_verifier_));
  return verifier_->Verify(server_cert_, host_and_port_.host(), flags,
                           &server_cert_verify_result_,
                           &handshake_io_callback_);
}

int SSLClientSocketOpenSSL::DoVerifyCertComplete(int result) {
  verifier_.reset();

  if (result == OK) {
    // TODO(joth): Work out if we need to remember the intermediate CA certs
    // when the server sends them to us, and do so here.
  } else {
    DVLOG(1) << "DoVerifyCertComplete error " << ErrorToString(result)
             << " (" << result << ")";
  }

  // If we have been explicitly told to accept this certificate, override the
  // result of verifier_.Verify.
  // Eventually, we should cache the cert verification results so that we don't
  // need to call verifier_.Verify repeatedly.  But for now we need to do this.
  // Alternatively, we could use the cert's status that we stored along with
  // the cert in the allowed_bad_certs vector.
  if (IsCertificateError(result) &&
      ssl_config_.IsAllowedBadCert(server_cert_)) {
    VLOG(1) << "accepting bad SSL certificate, as user told us to";
    result = OK;
  }

  completed_handshake_ = true;
  // Exit DoHandshakeLoop and return the result to the caller to Connect.
  DCHECK_EQ(STATE_NONE, next_handshake_state_);
  return result;
}

X509Certificate* SSLClientSocketOpenSSL::UpdateServerCert() {
  if (server_cert_)
    return server_cert_;

  base::ScopedOpenSSL<X509, X509_free> cert(SSL_get_peer_certificate(ssl_));
  if (!cert.get()) {
    LOG(WARNING) << "SSL_get_peer_certificate returned NULL";
    return NULL;
  }

  // Unlike SSL_get_peer_certificate, SSL_get_peer_cert_chain does not
  // increment the reference so sk_X509_free does not need to be called.
  STACK_OF(X509)* chain = SSL_get_peer_cert_chain(ssl_);
  X509Certificate::OSCertHandles intermediates;
  if (chain) {
    for (int i = 0; i < sk_X509_num(chain); ++i)
      intermediates.push_back(sk_X509_value(chain, i));
  }
  server_cert_ = X509Certificate::CreateFromHandle(
      cert.get(), X509Certificate::SOURCE_FROM_NETWORK, intermediates);
  DCHECK(server_cert_);

  return server_cert_;
}

bool SSLClientSocketOpenSSL::DoTransportIO() {
  bool network_moved = false;
  int nsent = BufferSend();
  int nreceived = BufferRecv();
  network_moved = (nsent > 0 || nreceived >= 0);
  return network_moved;
}

int SSLClientSocketOpenSSL::BufferSend(void) {
  if (transport_send_busy_)
    return ERR_IO_PENDING;

  if (!send_buffer_) {
    // Get a fresh send buffer out of the send BIO.
    size_t max_read = BIO_ctrl_pending(transport_bio_);
    if (max_read > 0) {
      send_buffer_ = new DrainableIOBuffer(new IOBuffer(max_read), max_read);
      int read_bytes = BIO_read(transport_bio_, send_buffer_->data(), max_read);
      DCHECK_GT(read_bytes, 0);
      CHECK_EQ(static_cast<int>(max_read), read_bytes);
    }
  }

  int rv = 0;
  while (send_buffer_) {
    rv = transport_->socket()->Write(send_buffer_,
                                     send_buffer_->BytesRemaining(),
                                     &buffer_send_callback_);
    if (rv == ERR_IO_PENDING) {
      transport_send_busy_ = true;
      return rv;
    }
    TransportWriteComplete(rv);
  }
  return rv;
}

void SSLClientSocketOpenSSL::BufferSendComplete(int result) {
  transport_send_busy_ = false;
  TransportWriteComplete(result);
  OnSendComplete(result);
}

void SSLClientSocketOpenSSL::TransportWriteComplete(int result) {
  DCHECK(ERR_IO_PENDING != result);
  if (result < 0) {
    // Got a socket write error; close the BIO to indicate this upward.
    DVLOG(1) << "TransportWriteComplete error " << result;
    (void)BIO_shutdown_wr(transport_bio_);
    send_buffer_ = NULL;
  } else {
    DCHECK(send_buffer_);
    send_buffer_->DidConsume(result);
    DCHECK_GE(send_buffer_->BytesRemaining(), 0);
    if (send_buffer_->BytesRemaining() <= 0)
      send_buffer_ = NULL;
  }
}

int SSLClientSocketOpenSSL::BufferRecv(void) {
  if (transport_recv_busy_)
    return ERR_IO_PENDING;

  size_t max_write = BIO_ctrl_get_write_guarantee(transport_bio_);
  if (max_write > kMaxRecvBufferSize)
    max_write = kMaxRecvBufferSize;

  if (!max_write)
    return ERR_IO_PENDING;

  recv_buffer_ = new IOBuffer(max_write);
  int rv = transport_->socket()->Read(recv_buffer_, max_write,
                                      &buffer_recv_callback_);
  if (rv == ERR_IO_PENDING) {
    transport_recv_busy_ = true;
  } else {
    TransportReadComplete(rv);
  }
  return rv;
}

void SSLClientSocketOpenSSL::BufferRecvComplete(int result) {
  TransportReadComplete(result);
  OnRecvComplete(result);
}

void SSLClientSocketOpenSSL::TransportReadComplete(int result) {
  DCHECK(ERR_IO_PENDING != result);
  if (result <= 0) {
    DVLOG(1) << "TransportReadComplete result " << result;
    // Received 0 (end of file) or an error. Either way, bubble it up to the
    // SSL layer via the BIO. TODO(joth): consider stashing the error code, to
    // relay up to the SSL socket client (i.e. via DoReadCallback).
    BIO_set_mem_eof_return(transport_bio_, 0);
    (void)BIO_shutdown_wr(transport_bio_);
  } else {
    DCHECK(recv_buffer_);
    int ret = BIO_write(transport_bio_, recv_buffer_->data(), result);
    // A write into a memory BIO should always succeed.
    CHECK_EQ(result, ret);
  }
  recv_buffer_ = NULL;
  transport_recv_busy_ = false;
}

void SSLClientSocketOpenSSL::DoConnectCallback(int rv) {
  CompletionCallback* c = user_connect_callback_;
  user_connect_callback_ = NULL;
  c->Run(rv > OK ? OK : rv);
}

void SSLClientSocketOpenSSL::OnHandshakeIOComplete(int result) {
  int rv = DoHandshakeLoop(result);
  if (rv != ERR_IO_PENDING) {
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_SSL_CONNECT, rv);
    DoConnectCallback(rv);
  }
}

void SSLClientSocketOpenSSL::OnSendComplete(int result) {
  if (next_handshake_state_ != STATE_NONE) {
    // In handshake phase.
    OnHandshakeIOComplete(result);
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
}

void SSLClientSocketOpenSSL::OnRecvComplete(int result) {
  if (next_handshake_state_ != STATE_NONE) {
    // In handshake phase.
    OnHandshakeIOComplete(result);
    return;
  }

  // Network layer received some data, check if client requested to read
  // decrypted data.
  if (!user_read_buf_)
    return;

  int rv = DoReadLoop(result);
  if (rv != ERR_IO_PENDING)
    DoReadCallback(rv);
}

bool SSLClientSocketOpenSSL::IsConnected() const {
  bool ret = completed_handshake_ && transport_->socket()->IsConnected();
  return ret;
}

bool SSLClientSocketOpenSSL::IsConnectedAndIdle() const {
  bool ret = completed_handshake_ && transport_->socket()->IsConnectedAndIdle();
  return ret;
}

int SSLClientSocketOpenSSL::GetPeerAddress(AddressList* addressList) const {
  return transport_->socket()->GetPeerAddress(addressList);
}

const BoundNetLog& SSLClientSocketOpenSSL::NetLog() const {
  return net_log_;
}

void SSLClientSocketOpenSSL::SetSubresourceSpeculation() {
  if (transport_.get() && transport_->socket()) {
    transport_->socket()->SetSubresourceSpeculation();
  } else {
    NOTREACHED();
  }
}

void SSLClientSocketOpenSSL::SetOmniboxSpeculation() {
  if (transport_.get() && transport_->socket()) {
    transport_->socket()->SetOmniboxSpeculation();
  } else {
    NOTREACHED();
  }
}

bool SSLClientSocketOpenSSL::WasEverUsed() const {
  if (transport_.get() && transport_->socket())
    return transport_->socket()->WasEverUsed();

  NOTREACHED();
  return false;
}

bool SSLClientSocketOpenSSL::UsingTCPFastOpen() const {
  if (transport_.get() && transport_->socket())
    return transport_->socket()->UsingTCPFastOpen();

  NOTREACHED();
  return false;
}

// Socket methods

int SSLClientSocketOpenSSL::Read(IOBuffer* buf,
                                 int buf_len,
                                 CompletionCallback* callback) {
  user_read_buf_ = buf;
  user_read_buf_len_ = buf_len;

  int rv = DoReadLoop(OK);

  if (rv == ERR_IO_PENDING) {
    user_read_callback_ = callback;
  } else {
    user_read_buf_ = NULL;
    user_read_buf_len_ = 0;
  }

  return rv;
}

int SSLClientSocketOpenSSL::DoReadLoop(int result) {
  if (result < 0)
    return result;

  bool network_moved;
  int rv;
  do {
    rv = DoPayloadRead();
    network_moved = DoTransportIO();
  } while (rv == ERR_IO_PENDING && network_moved);

  return rv;
}

int SSLClientSocketOpenSSL::Write(IOBuffer* buf,
                                  int buf_len,
                                  CompletionCallback* callback) {
  user_write_buf_ = buf;
  user_write_buf_len_ = buf_len;

  int rv = DoWriteLoop(OK);

  if (rv == ERR_IO_PENDING) {
    user_write_callback_ = callback;
  } else {
    user_write_buf_ = NULL;
    user_write_buf_len_ = 0;
  }

  return rv;
}

int SSLClientSocketOpenSSL::DoWriteLoop(int result) {
  if (result < 0)
    return result;

  bool network_moved;
  int rv;
  do {
    rv = DoPayloadWrite();
    network_moved = DoTransportIO();
  } while (rv == ERR_IO_PENDING && network_moved);

  return rv;
}

bool SSLClientSocketOpenSSL::SetReceiveBufferSize(int32 size) {
  return transport_->socket()->SetReceiveBufferSize(size);
}

bool SSLClientSocketOpenSSL::SetSendBufferSize(int32 size) {
  return transport_->socket()->SetSendBufferSize(size);
}

int SSLClientSocketOpenSSL::DoPayloadRead() {
  base::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  int rv = SSL_read(ssl_, user_read_buf_->data(), user_read_buf_len_);
  // We don't need to invalidate the non-client-authenticated SSL session
  // because the server will renegotiate anyway.
  if (client_auth_cert_needed_)
    return ERR_SSL_CLIENT_AUTH_CERT_NEEDED;

  if (rv >= 0)
    return rv;

  int err = SSL_get_error(ssl_, rv);
  return MapOpenSSLError(err, err_tracer);
}

int SSLClientSocketOpenSSL::DoPayloadWrite() {
  base::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  int rv = SSL_write(ssl_, user_write_buf_->data(), user_write_buf_len_);

  if (rv >= 0)
    return rv;

  int err = SSL_get_error(ssl_, rv);
  return MapOpenSSLError(err, err_tracer);
}

}  // namespace net
