// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_client_socket_win.h"

#include <schnlsp.h>
#include <map>

#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/synchronization/lock.h"
#include "base/utf_string_conversions.h"
#include "net/base/cert_verifier.h"
#include "net/base/connection_type_histograms.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/net_log.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_cert_request_info.h"
#include "net/base/ssl_connection_status_flags.h"
#include "net/base/ssl_info.h"
#include "net/socket/client_socket_handle.h"

#pragma comment(lib, "secur32.lib")

namespace net {

//-----------------------------------------------------------------------------

// TODO(wtc): See http://msdn.microsoft.com/en-us/library/aa377188(VS.85).aspx
// for the other error codes we may need to map.
static int MapSecurityError(SECURITY_STATUS err) {
  // There are numerous security error codes, but these are the ones we thus
  // far find interesting.
  switch (err) {
    case SEC_E_WRONG_PRINCIPAL:  // Schannel - server certificate error.
    case CERT_E_CN_NO_MATCH:  // CryptoAPI
      return ERR_CERT_COMMON_NAME_INVALID;
    case SEC_E_UNTRUSTED_ROOT:  // Schannel - server certificate error or
                                // unknown_ca alert.
    case CERT_E_UNTRUSTEDROOT:  // CryptoAPI
      return ERR_CERT_AUTHORITY_INVALID;
    case SEC_E_CERT_EXPIRED:  // Schannel - server certificate error or
                              // certificate_expired alert.
    case CERT_E_EXPIRED:  // CryptoAPI
      return ERR_CERT_DATE_INVALID;
    case CRYPT_E_NO_REVOCATION_CHECK:
      return ERR_CERT_NO_REVOCATION_MECHANISM;
    case CRYPT_E_REVOCATION_OFFLINE:
      return ERR_CERT_UNABLE_TO_CHECK_REVOCATION;
    case CRYPT_E_REVOKED:  // CryptoAPI and Schannel server certificate error,
                           // or certificate_revoked alert.
      return ERR_CERT_REVOKED;

    // We received one of the following alert messages from the server:
    //   bad_certificate
    //   unsupported_certificate
    //   certificate_unknown
    case SEC_E_CERT_UNKNOWN:
    case CERT_E_ROLE:
      return ERR_CERT_INVALID;

    // We received one of the following alert messages from the server:
    //   decode_error
    //   export_restriction
    //   handshake_failure
    //   illegal_parameter
    //   record_overflow
    //   unexpected_message
    // and all other TLS alerts not explicitly specified elsewhere.
    case SEC_E_ILLEGAL_MESSAGE:
    // We received one of the following alert messages from the server:
    //   decrypt_error
    //   decryption_failed
    case SEC_E_DECRYPT_FAILURE:
    // We received one of the following alert messages from the server:
    //   bad_record_mac
    //   decompression_failure
    case SEC_E_MESSAGE_ALTERED:
    // TODO(rsleevi): Add SEC_E_INTERNAL_ERROR, which corresponds to an
    // internal_error alert message being received. However, it is also used
    // by Schannel for authentication errors that happen locally, so it has
    // been omitted to prevent masking them as protocol errors.
      return ERR_SSL_PROTOCOL_ERROR;

    case SEC_E_LOGON_DENIED:  // Received a access_denied alert.
      return ERR_BAD_SSL_CLIENT_AUTH_CERT;

    case SEC_E_UNSUPPORTED_FUNCTION:  // Received a protocol_version alert.
    case SEC_E_ALGORITHM_MISMATCH:  // Received an insufficient_security alert.
      return ERR_SSL_VERSION_OR_CIPHER_MISMATCH;

    case SEC_E_NO_CREDENTIALS:
      return ERR_SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY;
    case SEC_E_INVALID_HANDLE:
    case SEC_E_INVALID_TOKEN:
      LOG(ERROR) << "Unexpected error " << err;
      return ERR_UNEXPECTED;
    case SEC_E_OK:
      return OK;
    default:
      LOG(WARNING) << "Unknown error " << err << " mapped to net::ERR_FAILED";
      return ERR_FAILED;
  }
}

//-----------------------------------------------------------------------------

// A bitmask consisting of these bit flags encodes which versions of the SSL
// protocol (SSL 3.0 and TLS 1.0) are enabled.
enum {
  SSL3 = 1 << 0,
  TLS1 = 1 << 1,
  SSL_VERSION_MASKS = 1 << 2  // The number of SSL version bitmasks.
};

// CredHandleClass simply gives a default constructor and a destructor to
// SSPI's CredHandle type (a C struct).
class CredHandleClass : public CredHandle {
 public:
  CredHandleClass() {
    SecInvalidateHandle(this);
  }

  ~CredHandleClass() {
    if (SecIsValidHandle(this)) {
      SECURITY_STATUS status = FreeCredentialsHandle(this);
      DCHECK(status == SEC_E_OK);
    }
  }
};

// A table of CredHandles.
class CredHandleTable {
 public:
  CredHandleTable() {}

  ~CredHandleTable() {
    STLDeleteContainerPairSecondPointers(client_cert_creds_.begin(),
                                         client_cert_creds_.end());
  }

  int GetHandle(PCCERT_CONTEXT client_cert,
                int ssl_version_mask,
                CredHandle** handle_ptr) {
    DCHECK(0 < ssl_version_mask &&
           ssl_version_mask < arraysize(anonymous_creds_));
    CredHandleClass* handle;
    base::AutoLock lock(lock_);
    if (client_cert) {
      CredHandleMapKey key = std::make_pair(client_cert, ssl_version_mask);
      CredHandleMap::const_iterator it = client_cert_creds_.find(key);
      if (it == client_cert_creds_.end()) {
        handle = new CredHandleClass;
        client_cert_creds_[key] = handle;
      } else {
        handle = it->second;
      }
    } else {
      handle = &anonymous_creds_[ssl_version_mask];
    }
    if (!SecIsValidHandle(handle)) {
      int result = InitializeHandle(handle, client_cert, ssl_version_mask);
      if (result != OK)
        return result;
    }
    *handle_ptr = handle;
    return OK;
  }

 private:
  // CredHandleMapKey is a std::pair consisting of these two components:
  //   PCCERT_CONTEXT client_cert
  //   int ssl_version_mask
  typedef std::pair<PCCERT_CONTEXT, int> CredHandleMapKey;

  typedef std::map<CredHandleMapKey, CredHandleClass*> CredHandleMap;

  // Returns OK on success or a network error code on failure.
  static int InitializeHandle(CredHandle* handle,
                              PCCERT_CONTEXT client_cert,
                              int ssl_version_mask);

  base::Lock lock_;

  // Anonymous (no client certificate) CredHandles for all possible
  // combinations of SSL versions.  Defined as an array for fast lookup.
  CredHandleClass anonymous_creds_[SSL_VERSION_MASKS];

  // CredHandles that use a client certificate.
  CredHandleMap client_cert_creds_;
};

static base::LazyInstance<CredHandleTable> g_cred_handle_table(
    base::LINKER_INITIALIZED);

// static
int CredHandleTable::InitializeHandle(CredHandle* handle,
                                      PCCERT_CONTEXT client_cert,
                                      int ssl_version_mask) {
  SCHANNEL_CRED schannel_cred = {0};
  schannel_cred.dwVersion = SCHANNEL_CRED_VERSION;
  if (client_cert) {
    schannel_cred.cCreds = 1;
    schannel_cred.paCred = &client_cert;
    // Schannel will make its own copy of client_cert.
  }

  // The global system registry settings take precedence over the value of
  // schannel_cred.grbitEnabledProtocols.
  schannel_cred.grbitEnabledProtocols = 0;
  if (ssl_version_mask & SSL3)
    schannel_cred.grbitEnabledProtocols |= SP_PROT_SSL3;
  if (ssl_version_mask & TLS1)
    schannel_cred.grbitEnabledProtocols |= SP_PROT_TLS1;

  // The default session lifetime is 36000000 milliseconds (ten hours).  Set
  // schannel_cred.dwSessionLifespan to change the number of milliseconds that
  // Schannel keeps the session in its session cache.

  // We can set the key exchange algorithms (RSA or DH) in
  // schannel_cred.{cSupportedAlgs,palgSupportedAlgs}.

  // Although SCH_CRED_AUTO_CRED_VALIDATION is convenient, we have to use
  // SCH_CRED_MANUAL_CRED_VALIDATION for three reasons.
  // 1. SCH_CRED_AUTO_CRED_VALIDATION doesn't allow us to get the certificate
  //    context if the certificate validation fails.
  // 2. SCH_CRED_AUTO_CRED_VALIDATION returns only one error even if the
  //    certificate has multiple errors.
  // 3. SCH_CRED_AUTO_CRED_VALIDATION doesn't allow us to ignore untrusted CA
  //    and expired certificate errors.  There are only flags to ignore the
  //    name mismatch and unable-to-check-revocation errors.
  //
  // We specify SCH_CRED_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT to cause the TLS
  // certificate status request extension (commonly known as OCSP stapling)
  // to be sent on Vista or later.  This flag matches the
  // CERT_CHAIN_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT flag that we pass to the
  // CertGetCertificateChain calls.  Note: we specify this flag even when
  // revocation checking is disabled to avoid doubling the number of
  // credentials handles we need to acquire.
  //
  // TODO(wtc): Look into undocumented or poorly documented flags:
  //   SCH_CRED_RESTRICTED_ROOTS
  //   SCH_CRED_REVOCATION_CHECK_CACHE_ONLY
  //   SCH_CRED_CACHE_ONLY_URL_RETRIEVAL
  //   SCH_CRED_MEMORY_STORE_CERT
  schannel_cred.dwFlags |= SCH_CRED_NO_DEFAULT_CREDS |
                           SCH_CRED_MANUAL_CRED_VALIDATION |
                           SCH_CRED_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT;
  TimeStamp expiry;
  SECURITY_STATUS status;

  status = AcquireCredentialsHandle(
      NULL,  // Not used
      UNISP_NAME,  // Microsoft Unified Security Protocol Provider
      SECPKG_CRED_OUTBOUND,
      NULL,  // Not used
      &schannel_cred,
      NULL,  // Not used
      NULL,  // Not used
      handle,
      &expiry);  // Optional
  if (status != SEC_E_OK)
    LOG(ERROR) << "AcquireCredentialsHandle failed: " << status;
  return MapSecurityError(status);
}

// For the SSL sockets to share SSL sessions by session resumption handshakes,
// they need to use the same CredHandle.  The GetCredHandle function creates
// and stores a shared CredHandle in *handle_ptr.  On success, GetCredHandle
// returns OK.  On failure, GetCredHandle returns a network error code and
// leaves *handle_ptr unchanged.
//
// The versions of the SSL protocol enabled are a property of the CredHandle.
// So we need a separate CredHandle for each combination of SSL versions.
// Most of the time Chromium will use only one or two combinations of SSL
// versions (for example, SSL3 | TLS1 for normal use, plus SSL3 when visiting
// TLS-intolerant servers).  These CredHandles are initialized only when
// needed.

static int GetCredHandle(PCCERT_CONTEXT client_cert,
                         int ssl_version_mask,
                         CredHandle** handle_ptr) {
  if (ssl_version_mask <= 0 || ssl_version_mask >= SSL_VERSION_MASKS) {
    NOTREACHED();
    return ERR_UNEXPECTED;
  }
  return g_cred_handle_table.Get().GetHandle(client_cert,
                                             ssl_version_mask,
                                             handle_ptr);
}

//-----------------------------------------------------------------------------

// This callback is intended to be used with CertFindChainInStore. In addition
// to filtering by extended/enhanced key usage, we do not show expired
// certificates and require digital signature usage in the key usage
// extension.
//
// This matches our behavior on Mac OS X and that of NSS. It also matches the
// default behavior of IE8. See http://support.microsoft.com/kb/890326 and
// http://blogs.msdn.com/b/askie/archive/2009/06/09/my-expired-client-certificates-no-longer-display-when-connecting-to-my-web-server-using-ie8.aspx
static BOOL WINAPI ClientCertFindCallback(PCCERT_CONTEXT cert_context,
                                          void* find_arg) {
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

  // Verify private key metadata is associated with this certificate.
  DWORD size = 0;
  if (!CertGetCertificateContextProperty(
          cert_context, CERT_KEY_PROV_INFO_PROP_ID, NULL, &size)) {
    return FALSE;
  }

  return TRUE;
}

//-----------------------------------------------------------------------------

// A memory certificate store for client certificates.  This allows us to
// close the "MY" system certificate store when we finish searching for
// client certificates.
class ClientCertStore {
 public:
  ClientCertStore() {
    store_ = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, NULL, 0, NULL);
  }

  ~ClientCertStore() {
    if (store_) {
      BOOL ok = CertCloseStore(store_, CERT_CLOSE_STORE_CHECK_FLAG);
      DCHECK(ok);
    }
  }

  PCCERT_CONTEXT CopyCertContext(PCCERT_CONTEXT client_cert) {
    PCCERT_CONTEXT copy;
    BOOL ok = CertAddCertificateContextToStore(store_, client_cert,
                                               CERT_STORE_ADD_USE_EXISTING,
                                               &copy);
    DCHECK(ok);
    return ok ? copy : NULL;
  }

 private:
  HCERTSTORE store_;
};

static base::LazyInstance<ClientCertStore> g_client_cert_store(
    base::LINKER_INITIALIZED);

//-----------------------------------------------------------------------------

// Size of recv_buffer_
//
// Ciphertext is decrypted one SSL record at a time, so recv_buffer_ needs to
// have room for a full SSL record, with the header and trailer.  Here is the
// breakdown of the size:
//   5: SSL record header
//   16K: SSL record maximum size
//   64: >= SSL record trailer (16 or 20 have been observed)
static const int kRecvBufferSize = (5 + 16*1024 + 64);

SSLClientSocketWin::SSLClientSocketWin(ClientSocketHandle* transport_socket,
                                       const HostPortPair& host_and_port,
                                       const SSLConfig& ssl_config,
                                       CertVerifier* cert_verifier)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
        handshake_io_callback_(this,
                               &SSLClientSocketWin::OnHandshakeIOComplete)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
        read_callback_(this, &SSLClientSocketWin::OnReadComplete)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
        write_callback_(this, &SSLClientSocketWin::OnWriteComplete)),
      transport_(transport_socket),
      host_and_port_(host_and_port),
      ssl_config_(ssl_config),
      user_connect_callback_(NULL),
      user_read_callback_(NULL),
      user_read_buf_len_(0),
      user_write_callback_(NULL),
      user_write_buf_len_(0),
      next_state_(STATE_NONE),
      cert_verifier_(cert_verifier),
      creds_(NULL),
      isc_status_(SEC_E_OK),
      payload_send_buffer_len_(0),
      bytes_sent_(0),
      decrypted_ptr_(NULL),
      bytes_decrypted_(0),
      received_ptr_(NULL),
      bytes_received_(0),
      writing_first_token_(false),
      ignore_ok_result_(false),
      renegotiating_(false),
      need_more_data_(false),
      net_log_(transport_socket->socket()->NetLog()) {
  memset(&stream_sizes_, 0, sizeof(stream_sizes_));
  memset(in_buffers_, 0, sizeof(in_buffers_));
  memset(&send_buffer_, 0, sizeof(send_buffer_));
  memset(&ctxt_, 0, sizeof(ctxt_));
}

SSLClientSocketWin::~SSLClientSocketWin() {
  Disconnect();
}

void SSLClientSocketWin::GetSSLInfo(SSLInfo* ssl_info) {
  ssl_info->Reset();

  if (!server_cert_)
    return;

  ssl_info->cert = server_cert_;
  ssl_info->cert_status = server_cert_verify_result_.cert_status;
  SecPkgContext_ConnectionInfo connection_info;
  SECURITY_STATUS status = QueryContextAttributes(
      &ctxt_, SECPKG_ATTR_CONNECTION_INFO, &connection_info);
  if (status == SEC_E_OK) {
    // TODO(wtc): compute the overall security strength, taking into account
    // dwExchStrength and dwHashStrength.  dwExchStrength needs to be
    // normalized.
    ssl_info->security_bits = connection_info.dwCipherStrength;
  }
  // SecPkgContext_CipherInfo comes from CNG and is available on Vista or
  // later only.  On XP, the next QueryContextAttributes call fails with
  // SEC_E_UNSUPPORTED_FUNCTION (0x80090302), so ssl_info->connection_status
  // won't contain the cipher suite.  If this is a problem, we can build the
  // cipher suite from the aiCipher, aiHash, and aiExch fields of
  // SecPkgContext_ConnectionInfo based on Appendix C of RFC 5246.
  SecPkgContext_CipherInfo cipher_info = { SECPKGCONTEXT_CIPHERINFO_V1 };
  status = QueryContextAttributes(
      &ctxt_, SECPKG_ATTR_CIPHER_INFO, &cipher_info);
  if (status == SEC_E_OK) {
    // TODO(wtc): find out what the cipher_info.dwBaseCipherSuite field is.
    ssl_info->connection_status |=
        (cipher_info.dwCipherSuite & SSL_CONNECTION_CIPHERSUITE_MASK) <<
        SSL_CONNECTION_CIPHERSUITE_SHIFT;
    // SChannel doesn't support TLS compression, so cipher_info doesn't have
    // any field related to the compression method.
  }

  if (ssl_config_.ssl3_fallback)
    ssl_info->connection_status |= SSL_CONNECTION_SSL3_FALLBACK;
}

void SSLClientSocketWin::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  cert_request_info->host_and_port = host_and_port_.ToString();
  cert_request_info->client_certs.clear();

  // Get the certificate_authorities field of the CertificateRequest message.
  // Schannel doesn't return the certificate_types field of the
  // CertificateRequest message to us, so we can't filter the client
  // certificates properly. :-(
  SecPkgContext_IssuerListInfoEx issuer_list;
  SECURITY_STATUS status = QueryContextAttributes(
      &ctxt_, SECPKG_ATTR_ISSUER_LIST_EX, &issuer_list);
  if (status != SEC_E_OK) {
    DLOG(ERROR) << "QueryContextAttributes (issuer list) failed: " << status;
    return;
  }

  // Client certificates of the user are in the "MY" system certificate store.
  HCERTSTORE my_cert_store = CertOpenSystemStore(NULL, L"MY");
  if (!my_cert_store) {
    LOG(ERROR) << "Could not open the \"MY\" system certificate store: "
               << GetLastError();
    FreeContextBuffer(issuer_list.aIssuers);
    return;
  }

  // Enumerate the client certificates.
  CERT_CHAIN_FIND_BY_ISSUER_PARA find_by_issuer_para;
  memset(&find_by_issuer_para, 0, sizeof(find_by_issuer_para));
  find_by_issuer_para.cbSize = sizeof(find_by_issuer_para);
  find_by_issuer_para.pszUsageIdentifier = szOID_PKIX_KP_CLIENT_AUTH;
  find_by_issuer_para.cIssuer = issuer_list.cIssuers;
  find_by_issuer_para.rgIssuer = issuer_list.aIssuers;
  find_by_issuer_para.pfnFindCallback = ClientCertFindCallback;

  PCCERT_CHAIN_CONTEXT chain_context = NULL;

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
    PCCERT_CONTEXT cert_context2 =
        g_client_cert_store.Get().CopyCertContext(cert_context);
    if (!cert_context2) {
      NOTREACHED();
      continue;
    }
    scoped_refptr<X509Certificate> cert = X509Certificate::CreateFromHandle(
        cert_context2, X509Certificate::SOURCE_LONE_CERT_IMPORT,
        X509Certificate::OSCertHandles());
    cert_request_info->client_certs.push_back(cert);
    CertFreeCertificateContext(cert_context2);
  }

  FreeContextBuffer(issuer_list.aIssuers);

  BOOL ok = CertCloseStore(my_cert_store, CERT_CLOSE_STORE_CHECK_FLAG);
  DCHECK(ok);
}

SSLClientSocket::NextProtoStatus
SSLClientSocketWin::GetNextProto(std::string* proto) {
  proto->clear();
  return kNextProtoUnsupported;
}

int SSLClientSocketWin::Connect(CompletionCallback* callback) {
  DCHECK(transport_.get());
  DCHECK(next_state_ == STATE_NONE);
  DCHECK(!user_connect_callback_);

  net_log_.BeginEvent(NetLog::TYPE_SSL_CONNECT, NULL);

  int rv = InitializeSSLContext();
  if (rv != OK) {
    net_log_.EndEvent(NetLog::TYPE_SSL_CONNECT, NULL);
    return rv;
  }

  writing_first_token_ = true;
  next_state_ = STATE_HANDSHAKE_WRITE;
  rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING) {
    user_connect_callback_ = callback;
  } else {
    net_log_.EndEvent(NetLog::TYPE_SSL_CONNECT, NULL);
  }
  return rv;
}

int SSLClientSocketWin::InitializeSSLContext() {
  int ssl_version_mask = 0;
  if (ssl_config_.ssl3_enabled)
    ssl_version_mask |= SSL3;
  if (ssl_config_.tls1_enabled)
    ssl_version_mask |= TLS1;
  // If we pass 0 to GetCredHandle, we will let Schannel select the protocols,
  // rather than enabling no protocols.  So we have to fail here.
  if (ssl_version_mask == 0)
    return ERR_NO_SSL_VERSIONS_ENABLED;
  PCCERT_CONTEXT cert_context = NULL;
  if (ssl_config_.client_cert)
    cert_context = ssl_config_.client_cert->os_cert_handle();
  int result = GetCredHandle(cert_context, ssl_version_mask, &creds_);
  if (result != OK)
    return result;

  memset(&ctxt_, 0, sizeof(ctxt_));

  SecBufferDesc buffer_desc;
  DWORD out_flags;
  DWORD flags = ISC_REQ_SEQUENCE_DETECT   |
                ISC_REQ_REPLAY_DETECT     |
                ISC_REQ_CONFIDENTIALITY   |
                ISC_RET_EXTENDED_ERROR    |
                ISC_REQ_ALLOCATE_MEMORY   |
                ISC_REQ_STREAM;

  send_buffer_.pvBuffer = NULL;
  send_buffer_.BufferType = SECBUFFER_TOKEN;
  send_buffer_.cbBuffer = 0;

  buffer_desc.cBuffers = 1;
  buffer_desc.pBuffers = &send_buffer_;
  buffer_desc.ulVersion = SECBUFFER_VERSION;

  TimeStamp expiry;
  SECURITY_STATUS status;

  status = InitializeSecurityContext(
      creds_,
      NULL,  // NULL on the first call
      const_cast<wchar_t*>(ASCIIToWide(host_and_port_.host()).c_str()),
      flags,
      0,  // Reserved
      0,  // Not used with Schannel.
      NULL,  // NULL on the first call
      0,  // Reserved
      &ctxt_,  // Receives the new context handle
      &buffer_desc,
      &out_flags,
      &expiry);
  if (status != SEC_I_CONTINUE_NEEDED) {
    LOG(ERROR) << "InitializeSecurityContext failed: " << status;
    if (status == SEC_E_INVALID_HANDLE) {
      // The only handle we passed to this InitializeSecurityContext call is
      // creds_, so print its contents to figure out why it's invalid.
      if (creds_) {
        LOG(ERROR) << "creds_->dwLower = " << creds_->dwLower
                   << "; creds_->dwUpper = " << creds_->dwUpper;
      } else {
        LOG(ERROR) << "creds_ is NULL";
      }
    }
    return MapSecurityError(status);
  }

  return OK;
}


void SSLClientSocketWin::Disconnect() {
  // TODO(wtc): Send SSL close_notify alert.
  next_state_ = STATE_NONE;

  // Shut down anything that may call us back.
  verifier_.reset();
  transport_->socket()->Disconnect();

  if (send_buffer_.pvBuffer)
    FreeSendBuffer();
  if (SecIsValidHandle(&ctxt_)) {
    DeleteSecurityContext(&ctxt_);
    SecInvalidateHandle(&ctxt_);
  }
  if (server_cert_)
    server_cert_ = NULL;

  // TODO(wtc): reset more members?
  bytes_decrypted_ = 0;
  bytes_received_ = 0;
  writing_first_token_ = false;
  renegotiating_ = false;
  need_more_data_ = false;
}

bool SSLClientSocketWin::IsConnected() const {
  // Ideally, we should also check if we have received the close_notify alert
  // message from the server, and return false in that case.  We're not doing
  // that, so this function may return a false positive.  Since the upper
  // layer (HttpNetworkTransaction) needs to handle a persistent connection
  // closed by the server when we send a request anyway, a false positive in
  // exchange for simpler code is a good trade-off.
  return completed_handshake() && transport_->socket()->IsConnected();
}

bool SSLClientSocketWin::IsConnectedAndIdle() const {
  // Unlike IsConnected, this method doesn't return a false positive.
  //
  // Strictly speaking, we should check if we have received the close_notify
  // alert message from the server, and return false in that case.  Although
  // the close_notify alert message means EOF in the SSL layer, it is just
  // bytes to the transport layer below, so
  // transport_->socket()->IsConnectedAndIdle() returns the desired false
  // when we receive close_notify.
  return completed_handshake() && transport_->socket()->IsConnectedAndIdle();
}

int SSLClientSocketWin::GetPeerAddress(AddressList* address) const {
  return transport_->socket()->GetPeerAddress(address);
}

void SSLClientSocketWin::SetSubresourceSpeculation() {
  if (transport_.get() && transport_->socket()) {
    transport_->socket()->SetSubresourceSpeculation();
  } else {
    NOTREACHED();
  }
}

void SSLClientSocketWin::SetOmniboxSpeculation() {
  if (transport_.get() && transport_->socket()) {
    transport_->socket()->SetOmniboxSpeculation();
  } else {
    NOTREACHED();
  }
}

bool SSLClientSocketWin::WasEverUsed() const {
  if (transport_.get() && transport_->socket()) {
    return transport_->socket()->WasEverUsed();
  }
  NOTREACHED();
  return false;
}

bool SSLClientSocketWin::UsingTCPFastOpen() const {
  if (transport_.get() && transport_->socket()) {
    return transport_->socket()->UsingTCPFastOpen();
  }
  NOTREACHED();
  return false;
}

int SSLClientSocketWin::Read(IOBuffer* buf, int buf_len,
                             CompletionCallback* callback) {
  DCHECK(completed_handshake());
  DCHECK(!user_read_callback_);

  // If we have surplus decrypted plaintext, satisfy the Read with it without
  // reading more ciphertext from the transport socket.
  if (bytes_decrypted_ != 0) {
    int len = std::min(buf_len, bytes_decrypted_);
    memcpy(buf->data(), decrypted_ptr_, len);
    decrypted_ptr_ += len;
    bytes_decrypted_ -= len;
    if (bytes_decrypted_ == 0) {
      decrypted_ptr_ = NULL;
      if (bytes_received_ != 0) {
        memmove(recv_buffer_.get(), received_ptr_, bytes_received_);
        received_ptr_ = recv_buffer_.get();
      }
    }
    return len;
  }

  DCHECK(!user_read_buf_);
  // http://crbug.com/16371: We're seeing |buf->data()| return NULL.  See if the
  // user is passing in an IOBuffer with a NULL |data_|.
  CHECK(buf);
  CHECK(buf->data());
  user_read_buf_ = buf;
  user_read_buf_len_ = buf_len;

  int rv = DoPayloadRead();
  if (rv == ERR_IO_PENDING) {
    user_read_callback_ = callback;
  } else {
    user_read_buf_ = NULL;
    user_read_buf_len_ = 0;
  }
  return rv;
}

int SSLClientSocketWin::Write(IOBuffer* buf, int buf_len,
                              CompletionCallback* callback) {
  DCHECK(completed_handshake());
  DCHECK(!user_write_callback_);

  DCHECK(!user_write_buf_);
  user_write_buf_ = buf;
  user_write_buf_len_ = buf_len;

  int rv = DoPayloadEncrypt();
  if (rv != OK)
    return rv;

  rv = DoPayloadWrite();
  if (rv == ERR_IO_PENDING) {
    user_write_callback_ = callback;
  } else {
    user_write_buf_ = NULL;
    user_write_buf_len_ = 0;
  }
  return rv;
}

bool SSLClientSocketWin::SetReceiveBufferSize(int32 size) {
  return transport_->socket()->SetReceiveBufferSize(size);
}

bool SSLClientSocketWin::SetSendBufferSize(int32 size) {
  return transport_->socket()->SetSendBufferSize(size);
}

void SSLClientSocketWin::OnHandshakeIOComplete(int result) {
  int rv = DoLoop(result);

  // The SSL handshake has some round trips.  We need to notify the caller of
  // success or any error, other than waiting for IO.
  if (rv != ERR_IO_PENDING) {
    // If there is no connect callback available to call, we are renegotiating
    // (which occurs because we are in the middle of a Read when the
    // renegotiation process starts).  So we complete the Read here.
    if (!user_connect_callback_) {
      CompletionCallback* c = user_read_callback_;
      user_read_callback_ = NULL;
      user_read_buf_ = NULL;
      user_read_buf_len_ = 0;
      c->Run(rv);
      return;
    }
    net_log_.EndEvent(NetLog::TYPE_SSL_CONNECT, NULL);
    CompletionCallback* c = user_connect_callback_;
    user_connect_callback_ = NULL;
    c->Run(rv);
  }
}

void SSLClientSocketWin::OnReadComplete(int result) {
  DCHECK(completed_handshake());

  result = DoPayloadReadComplete(result);
  if (result > 0)
    result = DoPayloadDecrypt();
  if (result != ERR_IO_PENDING) {
    DCHECK(user_read_callback_);
    CompletionCallback* c = user_read_callback_;
    user_read_callback_ = NULL;
    user_read_buf_ = NULL;
    user_read_buf_len_ = 0;
    c->Run(result);
  }
}

void SSLClientSocketWin::OnWriteComplete(int result) {
  DCHECK(completed_handshake());

  int rv = DoPayloadWriteComplete(result);
  if (rv != ERR_IO_PENDING) {
    DCHECK(user_write_callback_);
    CompletionCallback* c = user_write_callback_;
    user_write_callback_ = NULL;
    user_write_buf_ = NULL;
    user_write_buf_len_ = 0;
    c->Run(rv);
  }
}


int SSLClientSocketWin::DoLoop(int last_io_result) {
  DCHECK(next_state_ != STATE_NONE);
  int rv = last_io_result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_HANDSHAKE_READ:
        rv = DoHandshakeRead();
        break;
      case STATE_HANDSHAKE_READ_COMPLETE:
        rv = DoHandshakeReadComplete(rv);
        break;
      case STATE_HANDSHAKE_WRITE:
        rv = DoHandshakeWrite();
        break;
      case STATE_HANDSHAKE_WRITE_COMPLETE:
        rv = DoHandshakeWriteComplete(rv);
        break;
      case STATE_VERIFY_CERT:
        rv = DoVerifyCert();
        break;
      case STATE_VERIFY_CERT_COMPLETE:
        rv = DoVerifyCertComplete(rv);
        break;
      case STATE_COMPLETED_RENEGOTIATION:
        rv = DoCompletedRenegotiation(rv);
        break;
      case STATE_COMPLETED_HANDSHAKE:
        next_state_ = STATE_COMPLETED_HANDSHAKE;
        // This is the end of our state machine, so return.
        return rv;
      default:
        rv = ERR_UNEXPECTED;
        LOG(DFATAL) << "unexpected state " << state;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);
  return rv;
}

int SSLClientSocketWin::DoHandshakeRead() {
  next_state_ = STATE_HANDSHAKE_READ_COMPLETE;

  if (!recv_buffer_.get())
    recv_buffer_.reset(new char[kRecvBufferSize]);

  int buf_len = kRecvBufferSize - bytes_received_;

  if (buf_len <= 0) {
    LOG(DFATAL) << "Receive buffer is too small!";
    return ERR_UNEXPECTED;
  }

  DCHECK(!transport_read_buf_);
  transport_read_buf_ = new IOBuffer(buf_len);

  return transport_->socket()->Read(transport_read_buf_, buf_len,
                                    &handshake_io_callback_);
}

int SSLClientSocketWin::DoHandshakeReadComplete(int result) {
  if (result < 0) {
    transport_read_buf_ = NULL;
    return result;
  }

  if (transport_read_buf_) {
    // A transition to STATE_HANDSHAKE_READ_COMPLETE is set in multiple places,
    // not only in DoHandshakeRead(), so we may not have a transport_read_buf_.
    DCHECK_LE(result, kRecvBufferSize - bytes_received_);
    char* buf = recv_buffer_.get() + bytes_received_;
    memcpy(buf, transport_read_buf_->data(), result);
    transport_read_buf_ = NULL;
  }

  if (result == 0 && !ignore_ok_result_)
    return ERR_SSL_PROTOCOL_ERROR;  // Incomplete response :(

  ignore_ok_result_ = false;

  bytes_received_ += result;

  // Process the contents of recv_buffer_.
  TimeStamp expiry;
  DWORD out_flags;

  DWORD flags = ISC_REQ_SEQUENCE_DETECT |
                ISC_REQ_REPLAY_DETECT |
                ISC_REQ_CONFIDENTIALITY |
                ISC_RET_EXTENDED_ERROR |
                ISC_REQ_ALLOCATE_MEMORY |
                ISC_REQ_STREAM;

  if (ssl_config_.send_client_cert)
    flags |= ISC_REQ_USE_SUPPLIED_CREDS;

  SecBufferDesc in_buffer_desc, out_buffer_desc;

  in_buffer_desc.cBuffers = 2;
  in_buffer_desc.pBuffers = in_buffers_;
  in_buffer_desc.ulVersion = SECBUFFER_VERSION;

  in_buffers_[0].pvBuffer = recv_buffer_.get();
  in_buffers_[0].cbBuffer = bytes_received_;
  in_buffers_[0].BufferType = SECBUFFER_TOKEN;

  in_buffers_[1].pvBuffer = NULL;
  in_buffers_[1].cbBuffer = 0;
  in_buffers_[1].BufferType = SECBUFFER_EMPTY;

  out_buffer_desc.cBuffers = 1;
  out_buffer_desc.pBuffers = &send_buffer_;
  out_buffer_desc.ulVersion = SECBUFFER_VERSION;

  send_buffer_.pvBuffer = NULL;
  send_buffer_.BufferType = SECBUFFER_TOKEN;
  send_buffer_.cbBuffer = 0;

  isc_status_ = InitializeSecurityContext(
      creds_,
      &ctxt_,
      NULL,
      flags,
      0,
      0,
      &in_buffer_desc,
      0,
      NULL,
      &out_buffer_desc,
      &out_flags,
      &expiry);

  if (isc_status_ == SEC_E_INVALID_TOKEN) {
    // Peer sent us an SSL record type that's invalid during SSL handshake.
    // TODO(wtc): move this to MapSecurityError after sufficient testing.
    LOG(ERROR) << "InitializeSecurityContext failed: " << isc_status_;
    return ERR_SSL_PROTOCOL_ERROR;
  }

  if (send_buffer_.cbBuffer != 0 &&
      (isc_status_ == SEC_E_OK ||
       isc_status_ == SEC_I_CONTINUE_NEEDED ||
      (FAILED(isc_status_) && (out_flags & ISC_RET_EXTENDED_ERROR)))) {
    next_state_ = STATE_HANDSHAKE_WRITE;
    return OK;
  }
  return DidCallInitializeSecurityContext();
}

int SSLClientSocketWin::DidCallInitializeSecurityContext() {
  if (isc_status_ == SEC_E_INCOMPLETE_MESSAGE) {
    next_state_ = STATE_HANDSHAKE_READ;
    return OK;
  }

  if (isc_status_ == SEC_E_OK) {
    if (in_buffers_[1].BufferType == SECBUFFER_EXTRA) {
      // Save this data for later.
      memmove(recv_buffer_.get(),
              recv_buffer_.get() + (bytes_received_ - in_buffers_[1].cbBuffer),
              in_buffers_[1].cbBuffer);
      bytes_received_ = in_buffers_[1].cbBuffer;
    } else {
      bytes_received_ = 0;
    }
    return DidCompleteHandshake();
  }

  if (FAILED(isc_status_)) {
    LOG(ERROR) << "InitializeSecurityContext failed: " << isc_status_;
    if (isc_status_ == SEC_E_INTERNAL_ERROR) {
      // "The Local Security Authority cannot be contacted".  This happens
      // when the user denies access to the private key for SSL client auth.
      return ERR_SSL_CLIENT_AUTH_PRIVATE_KEY_ACCESS_DENIED;
    }
    int result = MapSecurityError(isc_status_);
    // We told Schannel to not verify the server certificate
    // (SCH_CRED_MANUAL_CRED_VALIDATION), so any certificate error returned by
    // InitializeSecurityContext must be referring to the bad or missing
    // client certificate.
    if (IsCertificateError(result)) {
      // TODO(wtc): Add fine-grained error codes for client certificate errors
      // reported by the server using the following SSL/TLS alert messages:
      //   access_denied
      //   bad_certificate
      //   unsupported_certificate
      //   certificate_expired
      //   certificate_revoked
      //   certificate_unknown
      //   unknown_ca
      return ERR_BAD_SSL_CLIENT_AUTH_CERT;
    }
    return result;
  }

  if (isc_status_ == SEC_I_INCOMPLETE_CREDENTIALS)
    return ERR_SSL_CLIENT_AUTH_CERT_NEEDED;

  if (isc_status_ == SEC_I_NO_RENEGOTIATION) {
    // Received a no_renegotiation alert message.  Although this is just a
    // warning, SChannel doesn't seem to allow us to continue after this
    // point, so we have to return an error.  See http://crbug.com/36835.
    return ERR_SSL_NO_RENEGOTIATION;
  }

  DCHECK(isc_status_ == SEC_I_CONTINUE_NEEDED);
  if (in_buffers_[1].BufferType == SECBUFFER_EXTRA) {
    memmove(recv_buffer_.get(),
            recv_buffer_.get() + (bytes_received_ - in_buffers_[1].cbBuffer),
            in_buffers_[1].cbBuffer);
    bytes_received_ = in_buffers_[1].cbBuffer;
    next_state_ = STATE_HANDSHAKE_READ_COMPLETE;
    ignore_ok_result_ = true;  // OK doesn't mean EOF.
    return OK;
  }

  bytes_received_ = 0;
  next_state_ = STATE_HANDSHAKE_READ;
  return OK;
}

int SSLClientSocketWin::DoHandshakeWrite() {
  next_state_ = STATE_HANDSHAKE_WRITE_COMPLETE;

  // We should have something to send.
  DCHECK(send_buffer_.pvBuffer);
  DCHECK(send_buffer_.cbBuffer > 0);
  DCHECK(!transport_write_buf_);

  const char* buf = static_cast<char*>(send_buffer_.pvBuffer) + bytes_sent_;
  int buf_len = send_buffer_.cbBuffer - bytes_sent_;
  transport_write_buf_ = new IOBuffer(buf_len);
  memcpy(transport_write_buf_->data(), buf, buf_len);

  return transport_->socket()->Write(transport_write_buf_, buf_len,
                                     &handshake_io_callback_);
}

int SSLClientSocketWin::DoHandshakeWriteComplete(int result) {
  DCHECK(transport_write_buf_);
  transport_write_buf_ = NULL;
  if (result < 0)
    return result;

  DCHECK(result != 0);

  bytes_sent_ += result;
  DCHECK(bytes_sent_ <= static_cast<int>(send_buffer_.cbBuffer));

  if (bytes_sent_ >= static_cast<int>(send_buffer_.cbBuffer)) {
    bool overflow = (bytes_sent_ > static_cast<int>(send_buffer_.cbBuffer));
    FreeSendBuffer();
    bytes_sent_ = 0;
    if (overflow) {  // Bug!
      LOG(DFATAL) << "overflow";
      return ERR_UNEXPECTED;
    }
    if (writing_first_token_) {
      writing_first_token_ = false;
      DCHECK(bytes_received_ == 0);
      next_state_ = STATE_HANDSHAKE_READ;
      return OK;
    }
    return DidCallInitializeSecurityContext();
  }

  // Send the remaining bytes.
  next_state_ = STATE_HANDSHAKE_WRITE;
  return OK;
}

// Set server_cert_status_ and return OK or a network error.
int SSLClientSocketWin::DoVerifyCert() {
  next_state_ = STATE_VERIFY_CERT_COMPLETE;

  DCHECK(server_cert_);

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

int SSLClientSocketWin::DoVerifyCertComplete(int result) {
  DCHECK(verifier_.get());
  verifier_.reset();

  // If we have been explicitly told to accept this certificate, override the
  // result of verifier_.Verify.
  // Eventually, we should cache the cert verification results so that we don't
  // need to call verifier_.Verify repeatedly. But for now we need to do this.
  // Alternatively, we could use the cert's status that we stored along with
  // the cert in the allowed_bad_certs vector.
  if (IsCertificateError(result) &&
      ssl_config_.IsAllowedBadCert(server_cert_))
    result = OK;

  if (result == OK)
    LogConnectionTypeMetrics();
  if (renegotiating_) {
    DidCompleteRenegotiation();
    return result;
  }

  // The initial handshake has completed.
  next_state_ = STATE_COMPLETED_HANDSHAKE;
  return result;
}

int SSLClientSocketWin::DoPayloadRead() {
  DCHECK(recv_buffer_.get());

  int buf_len = kRecvBufferSize - bytes_received_;

  if (buf_len <= 0) {
    NOTREACHED() << "Receive buffer is too small!";
    return ERR_FAILED;
  }

  int rv;
  // If bytes_received_, we have some data from a previous read still ready
  // for decoding.  Otherwise, we need to issue a real read.
  if (!bytes_received_ || need_more_data_) {
    DCHECK(!transport_read_buf_);
    transport_read_buf_ = new IOBuffer(buf_len);

    rv = transport_->socket()->Read(transport_read_buf_, buf_len,
                                    &read_callback_);
    if (rv != ERR_IO_PENDING)
      rv = DoPayloadReadComplete(rv);
    if (rv <= 0)
      return rv;
  }

  // Decode what we've read.  If there is not enough data to decode yet,
  // this may return ERR_IO_PENDING still.
  return DoPayloadDecrypt();
}

// result is the number of bytes that have been read; it should not be
// less than zero; a value of zero means that no additional bytes have
// been read.
int SSLClientSocketWin::DoPayloadReadComplete(int result) {
  DCHECK(completed_handshake());

  // If IO Pending, there is nothing to do here.
  if (result == ERR_IO_PENDING)
    return result;

  // We completed a Read, so reset the need_more_data_ flag.
  need_more_data_ = false;

  // Check for error
  if (result <= 0) {
    transport_read_buf_ = NULL;
    if (result == 0 && bytes_received_ != 0) {
      // TODO(wtc): Unless we have received the close_notify alert, we need
      // to return an error code indicating that the SSL connection ended
      // uncleanly, a potential truncation attack.  See
      // http://crbug.com/18586.
      return ERR_SSL_PROTOCOL_ERROR;
    }
    return result;
  }

  // Transfer the data from transport_read_buf_ to recv_buffer_.
  if (transport_read_buf_) {
    DCHECK_LE(result, kRecvBufferSize - bytes_received_);
    char* buf = recv_buffer_.get() + bytes_received_;
    memcpy(buf, transport_read_buf_->data(), result);
    transport_read_buf_ = NULL;
  }

  bytes_received_ += result;

  return result;
}

int SSLClientSocketWin::DoPayloadDecrypt() {
  // Process the contents of recv_buffer_.
  int len = 0;  // the number of bytes we've copied to the user buffer.
  while (bytes_received_) {
    SecBuffer buffers[4];
    buffers[0].pvBuffer = recv_buffer_.get();
    buffers[0].cbBuffer = bytes_received_;
    buffers[0].BufferType = SECBUFFER_DATA;

    buffers[1].BufferType = SECBUFFER_EMPTY;
    buffers[2].BufferType = SECBUFFER_EMPTY;
    buffers[3].BufferType = SECBUFFER_EMPTY;

    SecBufferDesc buffer_desc;
    buffer_desc.cBuffers = 4;
    buffer_desc.pBuffers = buffers;
    buffer_desc.ulVersion = SECBUFFER_VERSION;

    SECURITY_STATUS status;
    status = DecryptMessage(&ctxt_, &buffer_desc, 0, NULL);

    if (status == SEC_E_INCOMPLETE_MESSAGE) {
      need_more_data_ = true;
      return DoPayloadRead();
    }

    if (status == SEC_I_CONTEXT_EXPIRED) {
      // Received the close_notify alert.
      bytes_received_ = 0;
      return OK;
    }

    if (status != SEC_E_OK && status != SEC_I_RENEGOTIATE) {
      DCHECK(status != SEC_E_MESSAGE_ALTERED);
      LOG(ERROR) << "DecryptMessage failed: " << status;
      return MapSecurityError(status);
    }

    // The received ciphertext was decrypted in place in recv_buffer_.  Remember
    // the location and length of the decrypted plaintext and any unused
    // ciphertext.
    decrypted_ptr_ = NULL;
    bytes_decrypted_ = 0;
    received_ptr_ = NULL;
    bytes_received_ = 0;
    for (int i = 1; i < 4; i++) {
      switch (buffers[i].BufferType) {
        case SECBUFFER_DATA:
          DCHECK(!decrypted_ptr_ && bytes_decrypted_ == 0);
          decrypted_ptr_ = static_cast<char*>(buffers[i].pvBuffer);
          bytes_decrypted_ = buffers[i].cbBuffer;
          break;
        case SECBUFFER_EXTRA:
          DCHECK(!received_ptr_ && bytes_received_ == 0);
          received_ptr_ = static_cast<char*>(buffers[i].pvBuffer);
          bytes_received_ = buffers[i].cbBuffer;
          break;
        default:
          break;
      }
    }

    DCHECK(len == 0);
    if (bytes_decrypted_ != 0) {
      len = std::min(user_read_buf_len_, bytes_decrypted_);
      memcpy(user_read_buf_->data(), decrypted_ptr_, len);
      decrypted_ptr_ += len;
      bytes_decrypted_ -= len;
    }
    if (bytes_decrypted_ == 0) {
      decrypted_ptr_ = NULL;
      if (bytes_received_ != 0) {
        memmove(recv_buffer_.get(), received_ptr_, bytes_received_);
        received_ptr_ = recv_buffer_.get();
      }
    }

    if (status == SEC_I_RENEGOTIATE) {
      if (bytes_received_ != 0) {
        // The server requested renegotiation, but there are some data yet to
        // be decrypted.  The Platform SDK WebClient.c sample doesn't handle
        // this, so we don't know how to handle this.  Assume this cannot
        // happen.
        LOG(ERROR) << "DecryptMessage returned SEC_I_RENEGOTIATE with a buffer "
                   << "of type SECBUFFER_EXTRA.";
        return ERR_SSL_RENEGOTIATION_REQUESTED;
      }
      if (len != 0) {
        // The server requested renegotiation, but there are some decrypted
        // data.  We can't start renegotiation until we have returned all
        // decrypted data to the caller.
        //
        // This hasn't happened during testing.  Assume this cannot happen even
        // though we know how to handle this.
        LOG(ERROR) << "DecryptMessage returned SEC_I_RENEGOTIATE with a buffer "
                   << "of type SECBUFFER_DATA.";
        return ERR_SSL_RENEGOTIATION_REQUESTED;
      }
      // Jump to the handshake sequence.  Will come back when the rehandshake is
      // done.
      renegotiating_ = true;
      ignore_ok_result_ = true;  // OK doesn't mean EOF.
      // If renegotiation handshake occurred, we need to go back into the
      // handshake state machine.
      next_state_ = STATE_HANDSHAKE_READ_COMPLETE;
      return DoLoop(OK);
    }

    // We've already copied data into the user buffer, so quit now.
    // TODO(mbelshe): We really should keep decoding as long as we can.  This
    // break out is causing us to return pretty small chunks of data up to the
    // application, even though more is already buffered and ready to be
    // decoded.
    if (len)
      break;
  }

  // If we decrypted 0 bytes, don't report 0 bytes read, which would be
  // mistaken for EOF.  Continue decrypting or read more.
  if (len == 0)
    return DoPayloadRead();
  return len;
}

int SSLClientSocketWin::DoPayloadEncrypt() {
  DCHECK(completed_handshake());
  DCHECK(user_write_buf_);
  DCHECK(user_write_buf_len_ > 0);

  ULONG message_len = std::min(
      stream_sizes_.cbMaximumMessage, static_cast<ULONG>(user_write_buf_len_));
  ULONG alloc_len =
      message_len + stream_sizes_.cbHeader + stream_sizes_.cbTrailer;
  user_write_buf_len_ = message_len;

  payload_send_buffer_.reset(new char[alloc_len]);
  memcpy(&payload_send_buffer_[stream_sizes_.cbHeader],
         user_write_buf_->data(), message_len);

  SecBuffer buffers[4];
  buffers[0].pvBuffer = payload_send_buffer_.get();
  buffers[0].cbBuffer = stream_sizes_.cbHeader;
  buffers[0].BufferType = SECBUFFER_STREAM_HEADER;

  buffers[1].pvBuffer = &payload_send_buffer_[stream_sizes_.cbHeader];
  buffers[1].cbBuffer = message_len;
  buffers[1].BufferType = SECBUFFER_DATA;

  buffers[2].pvBuffer = &payload_send_buffer_[stream_sizes_.cbHeader +
                                              message_len];
  buffers[2].cbBuffer = stream_sizes_.cbTrailer;
  buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;

  buffers[3].BufferType = SECBUFFER_EMPTY;

  SecBufferDesc buffer_desc;
  buffer_desc.cBuffers = 4;
  buffer_desc.pBuffers = buffers;
  buffer_desc.ulVersion = SECBUFFER_VERSION;

  SECURITY_STATUS status = EncryptMessage(&ctxt_, 0, &buffer_desc, 0);

  if (FAILED(status)) {
    LOG(ERROR) << "EncryptMessage failed: " << status;
    return MapSecurityError(status);
  }

  payload_send_buffer_len_ = buffers[0].cbBuffer +
                             buffers[1].cbBuffer +
                             buffers[2].cbBuffer;
  DCHECK(bytes_sent_ == 0);
  return OK;
}

int SSLClientSocketWin::DoPayloadWrite() {
  DCHECK(completed_handshake());

  // We should have something to send.
  DCHECK(payload_send_buffer_.get());
  DCHECK(payload_send_buffer_len_ > 0);
  DCHECK(!transport_write_buf_);

  const char* buf = payload_send_buffer_.get() + bytes_sent_;
  int buf_len = payload_send_buffer_len_ - bytes_sent_;
  transport_write_buf_ = new IOBuffer(buf_len);
  memcpy(transport_write_buf_->data(), buf, buf_len);

  int rv = transport_->socket()->Write(transport_write_buf_, buf_len,
                                       &write_callback_);
  if (rv != ERR_IO_PENDING)
    rv = DoPayloadWriteComplete(rv);
  return rv;
}

int SSLClientSocketWin::DoPayloadWriteComplete(int result) {
  DCHECK(transport_write_buf_);
  transport_write_buf_ = NULL;
  if (result < 0)
    return result;

  DCHECK(result != 0);

  bytes_sent_ += result;
  DCHECK(bytes_sent_ <= payload_send_buffer_len_);

  if (bytes_sent_ >= payload_send_buffer_len_) {
    bool overflow = (bytes_sent_ > payload_send_buffer_len_);
    payload_send_buffer_.reset();
    payload_send_buffer_len_ = 0;
    bytes_sent_ = 0;
    if (overflow) {  // Bug!
      LOG(DFATAL) << "overflow";
      return ERR_UNEXPECTED;
    }
    // Done
    return user_write_buf_len_;
  }

  // Send the remaining bytes.
  return DoPayloadWrite();
}

int SSLClientSocketWin::DoCompletedRenegotiation(int result) {
  // The user had a read in progress, which was usurped by the renegotiation.
  // Restart the read sequence.
  next_state_ = STATE_COMPLETED_HANDSHAKE;
  if (result != OK)
    return result;
  return DoPayloadRead();
}

int SSLClientSocketWin::DidCompleteHandshake() {
  SECURITY_STATUS status = QueryContextAttributes(
      &ctxt_, SECPKG_ATTR_STREAM_SIZES, &stream_sizes_);
  if (status != SEC_E_OK) {
    LOG(ERROR) << "QueryContextAttributes (stream sizes) failed: " << status;
    return MapSecurityError(status);
  }
  DCHECK(!server_cert_ || renegotiating_);
  PCCERT_CONTEXT server_cert_handle = NULL;
  status = QueryContextAttributes(
      &ctxt_, SECPKG_ATTR_REMOTE_CERT_CONTEXT, &server_cert_handle);
  if (status != SEC_E_OK) {
    LOG(ERROR) << "QueryContextAttributes (remote cert) failed: " << status;
    return MapSecurityError(status);
  }
  if (renegotiating_ &&
      X509Certificate::IsSameOSCert(server_cert_->os_cert_handle(),
                                    server_cert_handle)) {
    // We already verified the server certificate.  Either it is good or the
    // user has accepted the certificate error.
    DidCompleteRenegotiation();
  } else {
    server_cert_ = X509Certificate::CreateFromHandle(
        server_cert_handle, X509Certificate::SOURCE_FROM_NETWORK,
        X509Certificate::OSCertHandles());

    next_state_ = STATE_VERIFY_CERT;
  }
  CertFreeCertificateContext(server_cert_handle);
  return OK;
}

// Called when a renegotiation is completed.  |result| is the verification
// result of the server certificate received during renegotiation.
void SSLClientSocketWin::DidCompleteRenegotiation() {
  DCHECK(!user_connect_callback_);
  DCHECK(user_read_callback_);
  renegotiating_ = false;
  next_state_ = STATE_COMPLETED_RENEGOTIATION;
}

void SSLClientSocketWin::LogConnectionTypeMetrics() const {
  UpdateConnectionTypeHistograms(CONNECTION_SSL);
  if (server_cert_verify_result_.has_md5)
    UpdateConnectionTypeHistograms(CONNECTION_SSL_MD5);
  if (server_cert_verify_result_.has_md2)
    UpdateConnectionTypeHistograms(CONNECTION_SSL_MD2);
  if (server_cert_verify_result_.has_md4)
    UpdateConnectionTypeHistograms(CONNECTION_SSL_MD4);
  if (server_cert_verify_result_.has_md5_ca)
    UpdateConnectionTypeHistograms(CONNECTION_SSL_MD5_CA);
  if (server_cert_verify_result_.has_md2_ca)
    UpdateConnectionTypeHistograms(CONNECTION_SSL_MD2_CA);
}

void SSLClientSocketWin::FreeSendBuffer() {
  SECURITY_STATUS status = FreeContextBuffer(send_buffer_.pvBuffer);
  DCHECK(status == SEC_E_OK);
  memset(&send_buffer_, 0, sizeof(send_buffer_));
}

}  // namespace net
