// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include <hasht.h>
#include <keyhi.h>
#include <nspr.h>
#include <nss.h>
#include <ocsp.h>
#include <pk11pub.h>
#include <secerr.h>
#include <sechash.h>
#include <ssl.h>
#include <sslerr.h>
#include <sslproto.h>

#include <algorithm>
#include <limits>
#include <map>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/build_time.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "crypto/ec_private_key.h"
#include "crypto/rsa_private_key.h"
#include "crypto/scoped_nss_types.h"
#include "net/base/address_list.h"
#include "net/base/asn1_util.h"
#include "net/base/cert_status_flags.h"
#include "net/base/cert_verifier.h"
#include "net/base/connection_type_histograms.h"
#include "net/base/dns_util.h"
#include "net/base/dnssec_chain_verifier.h"
#include "net/base/transport_security_state.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/single_request_cert_verifier.h"
#include "net/base/ssl_cert_request_info.h"
#include "net/base/ssl_connection_status_flags.h"
#include "net/base/ssl_info.h"
#include "net/base/x509_certificate_net_log_param.h"
#include "net/ocsp/nss_ocsp.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/nss_ssl_util.h"
#include "net/socket/ssl_error_params.h"
#include "net/socket/ssl_host_info.h"

#if defined(OS_WIN)
#include <windows.h>
#include <wincrypt.h>
#elif defined(OS_MACOSX)
#include <Security/SecBase.h>
#include <Security/SecCertificate.h>
#include <Security/SecIdentity.h>
#include "base/mac/mac_logging.h"
#elif defined(USE_NSS)
#include <dlfcn.h>
#endif

static const int kRecvBufferSize = 4096;

#if defined(OS_WIN)
// CERT_OCSP_RESPONSE_PROP_ID is only implemented on Vista+, but it can be
// set on Windows XP without error. There is some overhead from the server
// sending the OCSP response if it supports the extension, for the subset of
// XP clients who will request it but be unable to use it, but this is an
// acceptable trade-off for simplicity of implementation.
static bool IsOCSPStaplingSupported() {
  return true;
}
#elif defined(USE_NSS)
typedef SECStatus
(*CacheOCSPResponseFromSideChannelFunction)(
    CERTCertDBHandle *handle, CERTCertificate *cert, PRTime time,
    SECItem *encodedResponse, void *pwArg);

// On Linux, we dynamically link against the system version of libnss3.so. In
// order to continue working on systems without up-to-date versions of NSS we
// lookup CERT_CacheOCSPResponseFromSideChannel with dlsym.

// RuntimeLibNSSFunctionPointers is a singleton which caches the results of any
// runtime symbol resolution that we need.
class RuntimeLibNSSFunctionPointers {
 public:
  CacheOCSPResponseFromSideChannelFunction
  GetCacheOCSPResponseFromSideChannelFunction() {
    return cache_ocsp_response_from_side_channel_;
  }

  static RuntimeLibNSSFunctionPointers* GetInstance() {
    return Singleton<RuntimeLibNSSFunctionPointers>::get();
  }

 private:
  friend struct DefaultSingletonTraits<RuntimeLibNSSFunctionPointers>;

  RuntimeLibNSSFunctionPointers() {
    cache_ocsp_response_from_side_channel_ =
        (CacheOCSPResponseFromSideChannelFunction)
        dlsym(RTLD_DEFAULT, "CERT_CacheOCSPResponseFromSideChannel");
  }

  CacheOCSPResponseFromSideChannelFunction
      cache_ocsp_response_from_side_channel_;
};

static CacheOCSPResponseFromSideChannelFunction
GetCacheOCSPResponseFromSideChannelFunction() {
  return RuntimeLibNSSFunctionPointers::GetInstance()
    ->GetCacheOCSPResponseFromSideChannelFunction();
}

static bool IsOCSPStaplingSupported() {
  return GetCacheOCSPResponseFromSideChannelFunction() != NULL;
}
#else
// TODO(agl): Figure out if we can plumb the OCSP response into Mac's system
// certificate validation functions.
static bool IsOCSPStaplingSupported() {
  return false;
}
#endif

namespace net {

// State machines are easier to debug if you log state transitions.
// Enable these if you want to see what's going on.
#if 1
#define EnterFunction(x)
#define LeaveFunction(x)
#define GotoState(s) next_handshake_state_ = s
#else
#define EnterFunction(x)\
    VLOG(1) << (void *)this << " " << __FUNCTION__ << " enter " << x\
            << "; next_handshake_state " << next_handshake_state_
#define LeaveFunction(x)\
    VLOG(1) << (void *)this << " " << __FUNCTION__ << " leave " << x\
            << "; next_handshake_state " << next_handshake_state_
#define GotoState(s)\
    do {\
      VLOG(1) << (void *)this << " " << __FUNCTION__ << " jump to state " << s;\
      next_handshake_state_ = s;\
    } while (0)
#endif

namespace {

#if defined(OS_WIN)

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
  VLOG(1) << "Calling ClientCertFindCallback from _nss";
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

#endif

// PeerCertificateChain is a helper object which extracts the certificate
// chain, as given by the server, from an NSS socket and performs the needed
// resource management. The first element of the chain is the leaf certificate
// and the other elements are in the order given by the server.
class PeerCertificateChain {
 public:
  explicit PeerCertificateChain(PRFileDesc* nss_fd)
      : num_certs_(0),
        certs_(NULL) {
    SECStatus rv = SSL_PeerCertificateChain(nss_fd, NULL, &num_certs_, 0);
    DCHECK_EQ(rv, SECSuccess);

    certs_ = new CERTCertificate*[num_certs_];
    const unsigned expected_num_certs = num_certs_;
    rv = SSL_PeerCertificateChain(nss_fd, certs_, &num_certs_,
                                  expected_num_certs);
    DCHECK_EQ(rv, SECSuccess);
    DCHECK_EQ(num_certs_, expected_num_certs);
  }

  ~PeerCertificateChain() {
    for (unsigned i = 0; i < num_certs_; i++)
      CERT_DestroyCertificate(certs_[i]);
    delete[] certs_;
  }

  unsigned size() const { return num_certs_; }

  CERTCertificate* operator[](unsigned i) {
    DCHECK_LT(i, num_certs_);
    return certs_[i];
  }

  std::vector<base::StringPiece> AsStringPieceVector() const {
    std::vector<base::StringPiece> v(size());
    for (unsigned i = 0; i < size(); i++) {
      v[i] = base::StringPiece(
          reinterpret_cast<const char*>(certs_[i]->derCert.data),
          certs_[i]->derCert.len);
    }

    return v;
  }

 private:
  unsigned num_certs_;
  CERTCertificate** certs_;
};

void DestroyCertificates(CERTCertificate** certs, unsigned len) {
  for (unsigned i = 0; i < len; i++)
    CERT_DestroyCertificate(certs[i]);
}

// DNSValidationResult enumerates the possible outcomes from processing a
// set of DNS records.
enum DNSValidationResult {
  DNSVR_SUCCESS,   // the cert is immediately acceptable.
  DNSVR_FAILURE,   // the cert is unconditionally rejected.
  DNSVR_CONTINUE,  // perform CA validation as usual.
};

// VerifyCAARecords processes DNSSEC validated RRDATA for a number of DNS CAA
// records and checks them against the given chain.
//    server_cert_nss: the server's leaf certificate.
//    rrdatas: the CAA records for the current domain.
//    port: the TCP port number that we connected to.
DNSValidationResult VerifyCAARecords(
    CERTCertificate* server_cert_nss,
    const std::vector<base::StringPiece>& rrdatas,
    uint16 port) {
  DnsCAARecord::Policy policy;
  const DnsCAARecord::ParseResult r = DnsCAARecord::Parse(rrdatas, &policy);
  if (r == DnsCAARecord::SYNTAX_ERROR || r == DnsCAARecord::UNKNOWN_CRITICAL)
    return DNSVR_FAILURE;
  if (r == DnsCAARecord::DISCARD)
    return DNSVR_CONTINUE;
  DCHECK(r == DnsCAARecord::SUCCESS);

  for (std::vector<DnsCAARecord::Policy::Hash>::const_iterator
       hash = policy.authorized_hashes.begin();
       hash != policy.authorized_hashes.end();
       ++hash) {
    if (hash->target == DnsCAARecord::Policy::SUBJECT_PUBLIC_KEY_INFO &&
        (hash->port == 0 || hash->port == port)) {
      CHECK_LE(hash->data.size(), static_cast<unsigned>(SHA512_LENGTH));
      uint8 calculated_hash[SHA512_LENGTH];  // SHA512 is the largest.
      SECStatus rv = HASH_HashBuf(
          static_cast<HASH_HashType>(hash->algorithm),
          calculated_hash,
          server_cert_nss->derPublicKey.data,
          server_cert_nss->derPublicKey.len);
      DCHECK(rv == SECSuccess);
      const std::string actual_digest(reinterpret_cast<char*>(calculated_hash),
                                      hash->data.size());

      // Note that the parser ensures that hash->data.size() is correct for the
      // given algorithm. An attacker cannot give a zero length hash that
      // always matches.
      if (actual_digest == hash->data) {
        // A DNSSEC secure hash over the public key of the leaf-certificate
        // is sufficient.
        return DNSVR_SUCCESS;
      }
    }
  }

  // If a CAA record was found, but nothing matched, then we reject the
  // certificate.
  return DNSVR_FAILURE;
}

// CheckDNSSECChain tries to validate a DNSSEC chain embedded in
// |server_cert_nss_|. It returns true iff a chain is found that proves the
// value of a CAA record that contains a valid public key fingerprint.
// |port| contains the TCP port number that we connected to as CAA records can
// be specific to a given port.
DNSValidationResult CheckDNSSECChain(
    const std::string& hostname,
    CERTCertificate* server_cert_nss,
    uint16 port) {
  if (!server_cert_nss)
    return DNSVR_CONTINUE;

  // CERT_FindCertExtensionByOID isn't exported so we have to install an OID,
  // get a tag for it and find the extension by using that tag.
  static SECOidTag dnssec_chain_tag;
  static bool dnssec_chain_tag_valid;
  if (!dnssec_chain_tag_valid) {
    // It's harmless if multiple threads enter this block concurrently.
    static const uint8 kDNSSECChainOID[] =
        // 1.3.6.1.4.1.11129.2.1.4
        // (iso.org.dod.internet.private.enterprises.google.googleSecurity.
        //  certificateExtensions.dnssecEmbeddedChain)
        {0x2b, 0x06, 0x01, 0x04, 0x01, 0xd6, 0x79, 0x02, 0x01, 0x04};
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

  if (verifier.rrtype() != kDNS_CAA)
    return DNSVR_CONTINUE;

  DNSValidationResult r = VerifyCAARecords(
      server_cert_nss, verifier.rrdatas(), port);
  SECITEM_FreeItem(&dnssec_embedded_chain, PR_FALSE);

  return r;
}

}  // namespace

SSLClientSocketNSS::SSLClientSocketNSS(ClientSocketHandle* transport_socket,
                                       const HostPortPair& host_and_port,
                                       const SSLConfig& ssl_config,
                                       SSLHostInfo* ssl_host_info,
                                       const SSLClientSocketContext& context)
    : transport_send_busy_(false),
      transport_recv_busy_(false),
      transport_(transport_socket),
      host_and_port_(host_and_port),
      ssl_config_(ssl_config),
      user_read_buf_len_(0),
      user_write_buf_len_(0),
      server_cert_nss_(NULL),
      server_cert_verify_result_(NULL),
      ssl_connection_status_(0),
      client_auth_cert_needed_(false),
      cert_verifier_(context.cert_verifier),
      domain_bound_cert_xtn_negotiated_(false),
      server_bound_cert_service_(context.server_bound_cert_service),
      domain_bound_cert_type_(CLIENT_CERT_INVALID_TYPE),
      domain_bound_cert_request_handle_(NULL),
      handshake_callback_called_(false),
      completed_handshake_(false),
      ssl_session_cache_shard_(context.ssl_session_cache_shard),
      eset_mitm_detected_(false),
      predicted_cert_chain_correct_(false),
      next_handshake_state_(STATE_NONE),
      nss_fd_(NULL),
      nss_bufs_(NULL),
      net_log_(transport_socket->socket()->NetLog()),
      ssl_host_info_(ssl_host_info),
      transport_security_state_(context.transport_security_state),
      next_proto_status_(kNextProtoUnsupported),
      valid_thread_id_(base::kInvalidThreadId) {
  EnterFunction("");
}

SSLClientSocketNSS::~SSLClientSocketNSS() {
  EnterFunction("");
  Disconnect();
  LeaveFunction("");
}

// static
void SSLClientSocket::ClearSessionCache() {
  // SSL_ClearSessionCache can't be called before NSS is initialized.  Don't
  // bother initializing NSS just to clear an empty SSL session cache.
  if (!NSS_IsInitialized())
    return;

  SSL_ClearSessionCache();
}

void SSLClientSocketNSS::GetSSLInfo(SSLInfo* ssl_info) {
  EnterFunction("");
  ssl_info->Reset();
  if (!server_cert_nss_)
    return;

  ssl_info->cert_status = server_cert_verify_result_->cert_status;
  ssl_info->cert = server_cert_verify_result_->verified_cert;
  ssl_info->connection_status = ssl_connection_status_;
  ssl_info->public_key_hashes = server_cert_verify_result_->public_key_hashes;
  for (std::vector<SHA1Fingerprint>::const_iterator
       i = side_pinned_public_keys_.begin();
       i != side_pinned_public_keys_.end(); i++) {
    ssl_info->public_key_hashes.push_back(*i);
  }
  ssl_info->is_issued_by_known_root =
      server_cert_verify_result_->is_issued_by_known_root;
  ssl_info->client_cert_sent = WasDomainBoundCertSent() ||
      (ssl_config_.send_client_cert && ssl_config_.client_cert);

  PRUint16 cipher_suite =
      SSLConnectionStatusToCipherSuite(ssl_connection_status_);
  SSLCipherSuiteInfo cipher_info;
  SECStatus ok = SSL_GetCipherSuiteInfo(cipher_suite,
                                        &cipher_info, sizeof(cipher_info));
  if (ok == SECSuccess) {
    ssl_info->security_bits = cipher_info.effectiveKeyBits;
  } else {
    ssl_info->security_bits = -1;
    LOG(DFATAL) << "SSL_GetCipherSuiteInfo returned " << PR_GetError()
                << " for cipherSuite " << cipher_suite;
  }

  PRBool last_handshake_resumed;
  ok = SSL_HandshakeResumedSession(nss_fd_, &last_handshake_resumed);
  if (ok == SECSuccess) {
    if (last_handshake_resumed) {
      ssl_info->handshake_type = SSLInfo::HANDSHAKE_RESUME;
    } else {
      ssl_info->handshake_type = SSLInfo::HANDSHAKE_FULL;
    }
  }

  LeaveFunction("");
}

void SSLClientSocketNSS::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  EnterFunction("");
  // TODO(rch): switch SSLCertRequestInfo.host_and_port to a HostPortPair
  cert_request_info->host_and_port = host_and_port_.ToString();
  cert_request_info->client_certs = client_certs_;
  LeaveFunction(cert_request_info->client_certs.size());
}

int SSLClientSocketNSS::ExportKeyingMaterial(const base::StringPiece& label,
                                             bool has_context,
                                             const base::StringPiece& context,
                                             unsigned char* out,
                                             unsigned int outlen) {
  if (!IsConnected())
    return ERR_SOCKET_NOT_CONNECTED;
  SECStatus result = SSL_ExportKeyingMaterial(
      nss_fd_, label.data(), label.size(), has_context,
      reinterpret_cast<const unsigned char*>(context.data()),
      context.length(), out, outlen);
  if (result != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_ExportKeyingMaterial", "");
    return MapNSSError(PORT_GetError());
  }
  return OK;
}

SSLClientSocket::NextProtoStatus
SSLClientSocketNSS::GetNextProto(std::string* proto,
                                 std::string* server_protos) {
  *proto = next_proto_;
  *server_protos = server_protos_;
  return next_proto_status_;
}

int SSLClientSocketNSS::Connect(const CompletionCallback& callback) {
  EnterFunction("");
  DCHECK(transport_.get());
  DCHECK(next_handshake_state_ == STATE_NONE);
  DCHECK(user_read_callback_.is_null());
  DCHECK(user_write_callback_.is_null());
  DCHECK(user_connect_callback_.is_null());
  DCHECK(!user_read_buf_);
  DCHECK(!user_write_buf_);

  EnsureThreadIdAssigned();

  net_log_.BeginEvent(NetLog::TYPE_SSL_CONNECT, NULL);

  int rv = Init();
  if (rv != OK) {
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_SSL_CONNECT, rv);
    return rv;
  }

  rv = InitializeSSLOptions();
  if (rv != OK) {
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_SSL_CONNECT, rv);
    return rv;
  }

  rv = InitializeSSLPeerName();
  if (rv != OK) {
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_SSL_CONNECT, rv);
    return rv;
  }

  if (ssl_config_.cached_info_enabled && ssl_host_info_.get()) {
    GotoState(STATE_LOAD_SSL_HOST_INFO);
  } else {
    GotoState(STATE_HANDSHAKE);
  }

  rv = DoHandshakeLoop(OK);
  if (rv == ERR_IO_PENDING) {
    user_connect_callback_ = callback;
  } else {
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_SSL_CONNECT, rv);
  }

  LeaveFunction("");
  return rv > OK ? OK : rv;
}

void SSLClientSocketNSS::Disconnect() {
  EnterFunction("");

  CHECK(CalledOnValidThread());

  // Shut down anything that may call us back.
  verifier_.reset();
  transport_->socket()->Disconnect();

  if (domain_bound_cert_request_handle_ != NULL) {
    server_bound_cert_service_->CancelRequest(
        domain_bound_cert_request_handle_);
    domain_bound_cert_request_handle_ = NULL;
  }

  // TODO(wtc): Send SSL close_notify alert.
  if (nss_fd_ != NULL) {
    PR_Close(nss_fd_);
    nss_fd_ = NULL;
  }

  // Reset object state.
  user_connect_callback_.Reset();
  user_read_callback_.Reset();
  user_write_callback_.Reset();
  transport_send_busy_   = false;
  transport_recv_busy_   = false;
  user_read_buf_         = NULL;
  user_read_buf_len_     = 0;
  user_write_buf_        = NULL;
  user_write_buf_len_    = 0;
  server_cert_           = NULL;
  if (server_cert_nss_) {
    CERT_DestroyCertificate(server_cert_nss_);
    server_cert_nss_     = NULL;
  }
  local_server_cert_verify_result_.Reset();
  server_cert_verify_result_ = NULL;
  ssl_connection_status_ = 0;
  completed_handshake_   = false;
  eset_mitm_detected_    = false;
  start_cert_verification_time_ = base::TimeTicks();
  predicted_cert_chain_correct_ = false;
  nss_bufs_              = NULL;
  client_certs_.clear();
  client_auth_cert_needed_ = false;
  domain_bound_cert_xtn_negotiated_ = false;

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

int SSLClientSocketNSS::GetLocalAddress(IPEndPoint* address) const {
  return transport_->socket()->GetLocalAddress(address);
}

const BoundNetLog& SSLClientSocketNSS::NetLog() const {
  return net_log_;
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

bool SSLClientSocketNSS::UsingTCPFastOpen() const {
  if (transport_.get() && transport_->socket()) {
    return transport_->socket()->UsingTCPFastOpen();
  }
  NOTREACHED();
  return false;
}

int64 SSLClientSocketNSS::NumBytesRead() const {
  if (transport_.get() && transport_->socket()) {
    return transport_->socket()->NumBytesRead();
  }
  NOTREACHED();
  return -1;
}

base::TimeDelta SSLClientSocketNSS::GetConnectTimeMicros() const {
  if (transport_.get() && transport_->socket()) {
    return transport_->socket()->GetConnectTimeMicros();
  }
  NOTREACHED();
  return base::TimeDelta::FromMicroseconds(-1);
}

int SSLClientSocketNSS::Read(IOBuffer* buf, int buf_len,
                             const CompletionCallback& callback) {
  EnterFunction(buf_len);
  DCHECK(completed_handshake_);
  DCHECK(next_handshake_state_ == STATE_NONE);
  DCHECK(user_read_callback_.is_null());
  DCHECK(user_connect_callback_.is_null());
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
                              const CompletionCallback& callback) {
  EnterFunction(buf_len);
  DCHECK(completed_handshake_);
  DCHECK(next_handshake_state_ == STATE_NONE);
  DCHECK(user_write_callback_.is_null());
  DCHECK(user_connect_callback_.is_null());
  DCHECK(!user_write_buf_);
  DCHECK(nss_bufs_);

  user_write_buf_ = buf;
  user_write_buf_len_ = buf_len;

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

int SSLClientSocketNSS::Init() {
  EnterFunction("");
  // Initialize the NSS SSL library in a threadsafe way.  This also
  // initializes the NSS base library.
  EnsureNSSSSLInit();
  if (!NSS_IsInitialized())
    return ERR_UNEXPECTED;
#if !defined(OS_MACOSX) && !defined(OS_WIN)
  if (ssl_config_.cert_io_enabled) {
    // We must call EnsureNSSHttpIOInit() here, on the IO thread, to get the IO
    // loop by MessageLoopForIO::current().
    // X509Certificate::Verify() runs on a worker thread of CertVerifier.
    EnsureNSSHttpIOInit();
  }
#endif

  LeaveFunction("");
  return OK;
}

int SSLClientSocketNSS::InitializeSSLOptions() {
  // Transport connected, now hook it up to nss
  // TODO(port): specify rx and tx buffer sizes separately
  nss_fd_ = memio_CreateIOLayer(kRecvBufferSize);
  if (nss_fd_ == NULL) {
    return ERR_OUT_OF_MEMORY;  // TODO(port): map NSPR error code.
  }

  // Grab pointer to buffers
  nss_bufs_ = memio_GetSecret(nss_fd_);

  /* Create SSL state machine */
  /* Push SSL onto our fake I/O socket */
  nss_fd_ = SSL_ImportFD(NULL, nss_fd_);
  if (nss_fd_ == NULL) {
    LogFailedNSSFunction(net_log_, "SSL_ImportFD", "");
    return ERR_OUT_OF_MEMORY;  // TODO(port): map NSPR/NSS error code.
  }
  // TODO(port): set more ssl options!  Check errors!

  int rv;

  rv = SSL_OptionSet(nss_fd_, SSL_SECURITY, PR_TRUE);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_OptionSet", "SSL_SECURITY");
    return ERR_UNEXPECTED;
  }

  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_SSL2, PR_FALSE);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_OptionSet", "SSL_ENABLE_SSL2");
    return ERR_UNEXPECTED;
  }

  // Don't do V2 compatible hellos because they don't support TLS extensions.
  rv = SSL_OptionSet(nss_fd_, SSL_V2_COMPATIBLE_HELLO, PR_FALSE);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_OptionSet", "SSL_V2_COMPATIBLE_HELLO");
    return ERR_UNEXPECTED;
  }

  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_SSL3, ssl_config_.ssl3_enabled);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_OptionSet", "SSL_ENABLE_SSL3");
    return ERR_UNEXPECTED;
  }

  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_TLS, ssl_config_.tls1_enabled);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_OptionSet", "SSL_ENABLE_TLS");
    return ERR_UNEXPECTED;
  }

  for (std::vector<uint16>::const_iterator it =
           ssl_config_.disabled_cipher_suites.begin();
       it != ssl_config_.disabled_cipher_suites.end(); ++it) {
    // This will fail if the specified cipher is not implemented by NSS, but
    // the failure is harmless.
    SSL_CipherPrefSet(nss_fd_, *it, PR_FALSE);
  }

#ifdef SSL_ENABLE_SESSION_TICKETS
  // Support RFC 5077
  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_SESSION_TICKETS, PR_TRUE);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(
        net_log_, "SSL_OptionSet", "SSL_ENABLE_SESSION_TICKETS");
  }
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
    LogFailedNSSFunction(net_log_, "SSL_OptionSet", "SSL_ENABLE_DEFLATE");
#endif

#ifdef SSL_ENABLE_FALSE_START
  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_FALSE_START,
                     ssl_config_.false_start_enabled);
  if (rv != SECSuccess)
    LogFailedNSSFunction(net_log_, "SSL_OptionSet", "SSL_ENABLE_FALSE_START");
#endif

#ifdef SSL_ENABLE_RENEGOTIATION
  // We allow servers to request renegotiation. Since we're a client,
  // prohibiting this is rather a waste of time. Only servers are in a
  // position to prevent renegotiation attacks.
  // http://extendedsubset.com/?p=8

  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_RENEGOTIATION,
                     SSL_RENEGOTIATE_TRANSITIONAL);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(
        net_log_, "SSL_OptionSet", "SSL_ENABLE_RENEGOTIATION");
  }
#endif  // SSL_ENABLE_RENEGOTIATION

  if (!ssl_config_.next_protos.empty()) {
    rv = SSL_SetNextProtoCallback(
        nss_fd_, SSLClientSocketNSS::NextProtoCallback, this);
    if (rv != SECSuccess)
      LogFailedNSSFunction(net_log_, "SSL_SetNextProtoCallback", "");
  }

#ifdef SSL_CBC_RANDOM_IV
  rv = SSL_OptionSet(nss_fd_, SSL_CBC_RANDOM_IV,
                     ssl_config_.false_start_enabled);
  if (rv != SECSuccess)
    LogFailedNSSFunction(net_log_, "SSL_OptionSet", "SSL_CBC_RANDOM_IV");
#endif

#ifdef SSL_ENABLE_OCSP_STAPLING
  if (IsOCSPStaplingSupported()) {
    rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_OCSP_STAPLING, PR_TRUE);
    if (rv != SECSuccess) {
      LogFailedNSSFunction(net_log_, "SSL_OptionSet",
                           "SSL_ENABLE_OCSP_STAPLING");
    }
  }
#endif

#ifdef SSL_ENABLE_CACHED_INFO
  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_CACHED_INFO,
                     ssl_config_.cached_info_enabled);
  if (rv != SECSuccess)
    LogFailedNSSFunction(net_log_, "SSL_OptionSet", "SSL_ENABLE_CACHED_INFO");
#endif

#ifdef SSL_ENABLE_OB_CERTS
  UMA_HISTOGRAM_BOOLEAN("DBC.Advertised",
                        ssl_config_.domain_bound_certs_enabled);
  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_OB_CERTS,
                     ssl_config_.domain_bound_certs_enabled);
  if (rv != SECSuccess)
    LogFailedNSSFunction(net_log_, "SSL_OptionSet", "SSL_ENABLE_OB_CERTS");
#else
  UMA_HISTOGRAM_BOOLEAN("DBC.Advertised", false);
#endif

#ifdef SSL_ENCRYPT_CLIENT_CERTS
  // For now, enable the encrypted client certificates extension only if
  // server-bound certificates are enabled.
  rv = SSL_OptionSet(nss_fd_, SSL_ENCRYPT_CLIENT_CERTS,
                     ssl_config_.domain_bound_certs_enabled);
  if (rv != SECSuccess)
    LogFailedNSSFunction(net_log_, "SSL_OptionSet", "SSL_ENCRYPT_CLIENT_CERTS");
#endif

  rv = SSL_OptionSet(nss_fd_, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_OptionSet", "SSL_HANDSHAKE_AS_CLIENT");
    return ERR_UNEXPECTED;
  }

  rv = SSL_AuthCertificateHook(nss_fd_, OwnAuthCertHandler, this);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_AuthCertificateHook", "");
    return ERR_UNEXPECTED;
  }

#if defined(NSS_PLATFORM_CLIENT_AUTH)
  rv = SSL_GetPlatformClientAuthDataHook(nss_fd_, PlatformClientAuthHandler,
                                         this);
#else
  rv = SSL_GetClientAuthDataHook(nss_fd_, ClientAuthHandler, this);
#endif
  if (rv != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_GetClientAuthDataHook", "");
    return ERR_UNEXPECTED;
  }

  rv = SSL_HandshakeCallback(nss_fd_, HandshakeCallback, this);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_HandshakeCallback", "");
    return ERR_UNEXPECTED;
  }

  // Tell SSL the hostname we're trying to connect to.
  SSL_SetURL(nss_fd_, host_and_port_.host().c_str());

  // Tell SSL we're a client; needed if not letting NSPR do socket I/O
  SSL_ResetHandshake(nss_fd_, PR_FALSE);

  return OK;
}

int SSLClientSocketNSS::InitializeSSLPeerName() {
  // Tell NSS who we're connected to
  AddressList peer_address;
  int err = transport_->socket()->GetPeerAddress(&peer_address);
  if (err != OK)
    return err;

  SockaddrStorage storage;
  if (!peer_address.front().ToSockAddr(storage.addr, &storage.addr_len))
    return ERR_UNEXPECTED;

  PRNetAddr peername;
  memset(&peername, 0, sizeof(peername));
  DCHECK_LE(static_cast<size_t>(storage.addr_len), sizeof(peername));
  size_t len = std::min(static_cast<size_t>(storage.addr_len),
                        sizeof(peername));
  memcpy(&peername, storage.addr, len);

  // Adjust the address family field for BSD, whose sockaddr
  // structure has a one-byte length and one-byte address family
  // field at the beginning.  PRNetAddr has a two-byte address
  // family field at the beginning.
  peername.raw.family = storage.addr->sa_family;

  memio_SetPeerName(nss_fd_, &peername);

  // Set the peer ID for session reuse.  This is necessary when we create an
  // SSL tunnel through a proxy -- GetPeerName returns the proxy's address
  // rather than the destination server's address in that case.
  std::string peer_id = host_and_port_.ToString();
  // If the ssl_session_cache_shard_ is non-empty, we append it to the peer id.
  // This will cause session cache misses between sockets with different values
  // of ssl_session_cache_shard_ and this is used to partition the session cache
  // for incognito mode.
  if (!ssl_session_cache_shard_.empty()) {
    peer_id += "/" + ssl_session_cache_shard_;
  }
  SECStatus rv = SSL_SetSockPeerID(nss_fd_, const_cast<char*>(peer_id.c_str()));
  if (rv != SECSuccess)
    LogFailedNSSFunction(net_log_, "SSL_SetSockPeerID", peer_id.c_str());

  return OK;
}


// Sets server_cert_ and server_cert_nss_ if not yet set.
void SSLClientSocketNSS::UpdateServerCert() {
  // We set the server_cert_ from HandshakeCallback().
  if (server_cert_ == NULL) {
    server_cert_nss_ = SSL_PeerCertificate(nss_fd_);
    if (server_cert_nss_) {
      PeerCertificateChain certs(nss_fd_);
      // This call may fail when SSL is used inside sandbox. In that
      // case CreateFromDERCertChain() returns NULL.
      server_cert_ = X509Certificate::CreateFromDERCertChain(
          certs.AsStringPieceVector());
      if (server_cert_) {
        net_log_.AddEvent(
            NetLog::TYPE_SSL_CERTIFICATES_RECEIVED,
            make_scoped_refptr(new X509CertificateNetLogParam(server_cert_)));
      }
    }
  }
}

// Sets ssl_connection_status_.
void SSLClientSocketNSS::UpdateConnectionStatus() {
  SSLChannelInfo channel_info;
  SECStatus ok = SSL_GetChannelInfo(nss_fd_,
                                    &channel_info, sizeof(channel_info));
  if (ok == SECSuccess &&
      channel_info.length == sizeof(channel_info) &&
      channel_info.cipherSuite) {
    ssl_connection_status_ |=
        (static_cast<int>(channel_info.cipherSuite) &
         SSL_CONNECTION_CIPHERSUITE_MASK) <<
        SSL_CONNECTION_CIPHERSUITE_SHIFT;

    ssl_connection_status_ |=
        (static_cast<int>(channel_info.compressionMethod) &
         SSL_CONNECTION_COMPRESSION_MASK) <<
        SSL_CONNECTION_COMPRESSION_SHIFT;

    // NSS 3.12.x doesn't have version macros for TLS 1.1 and 1.2 (because NSS
    // doesn't support them yet), so we use 0x0302 and 0x0303 directly.
    int version = SSL_CONNECTION_VERSION_UNKNOWN;
    if (channel_info.protocolVersion < SSL_LIBRARY_VERSION_3_0) {
      // All versions less than SSL_LIBRARY_VERSION_3_0 are treated as SSL
      // version 2.
      version = SSL_CONNECTION_VERSION_SSL2;
    } else if (channel_info.protocolVersion == SSL_LIBRARY_VERSION_3_0) {
      version = SSL_CONNECTION_VERSION_SSL3;
    } else if (channel_info.protocolVersion == SSL_LIBRARY_VERSION_3_1_TLS) {
      version = SSL_CONNECTION_VERSION_TLS1;
    } else if (channel_info.protocolVersion == 0x0302) {
      version = SSL_CONNECTION_VERSION_TLS1_1;
    } else if (channel_info.protocolVersion == 0x0303) {
      version = SSL_CONNECTION_VERSION_TLS1_2;
    }
    ssl_connection_status_ |=
        (version & SSL_CONNECTION_VERSION_MASK) <<
        SSL_CONNECTION_VERSION_SHIFT;
  }

  // SSL_HandshakeNegotiatedExtension was added in NSS 3.12.6.
  // Since SSL_MAX_EXTENSIONS was added at the same time, we can test
  // SSL_MAX_EXTENSIONS for the presence of SSL_HandshakeNegotiatedExtension.
#if defined(SSL_MAX_EXTENSIONS)
  PRBool peer_supports_renego_ext;
  ok = SSL_HandshakeNegotiatedExtension(nss_fd_, ssl_renegotiation_info_xtn,
                                        &peer_supports_renego_ext);
  if (ok == SECSuccess) {
    if (!peer_supports_renego_ext) {
      ssl_connection_status_ |= SSL_CONNECTION_NO_RENEGOTIATION_EXTENSION;
      // Log an informational message if the server does not support secure
      // renegotiation (RFC 5746).
      VLOG(1) << "The server " << host_and_port_.ToString()
              << " does not support the TLS renegotiation_info extension.";
    }
    UMA_HISTOGRAM_ENUMERATION("Net.RenegotiationExtensionSupported",
                              peer_supports_renego_ext, 2);
  }
#endif

  if (ssl_config_.ssl3_fallback)
    ssl_connection_status_ |= SSL_CONNECTION_SSL3_FALLBACK;
}

void SSLClientSocketNSS::DoReadCallback(int rv) {
  EnterFunction(rv);
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(!user_read_callback_.is_null());

  // Since Run may result in Read being called, clear |user_read_callback_|
  // up front.
  CompletionCallback c = user_read_callback_;
  user_read_callback_.Reset();
  user_read_buf_ = NULL;
  user_read_buf_len_ = 0;
  c.Run(rv);
  LeaveFunction("");
}

void SSLClientSocketNSS::DoWriteCallback(int rv) {
  EnterFunction(rv);
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(!user_write_callback_.is_null());

  // Since Run may result in Write being called, clear |user_write_callback_|
  // up front.
  CompletionCallback c = user_write_callback_;
  user_write_callback_.Reset();
  user_write_buf_ = NULL;
  user_write_buf_len_ = 0;
  c.Run(rv);
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
  DCHECK(!user_connect_callback_.is_null());

  CompletionCallback c = user_connect_callback_;
  user_connect_callback_.Reset();
  c.Run(rv > OK ? OK : rv);
  LeaveFunction("");
}

void SSLClientSocketNSS::OnHandshakeIOComplete(int result) {
  EnterFunction(result);
  int rv = DoHandshakeLoop(result);
  if (rv != ERR_IO_PENDING) {
    net_log_.EndEventWithNetErrorCode(net::NetLog::TYPE_SSL_CONNECT, rv);
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

int SSLClientSocketNSS::DoHandshakeLoop(int last_io_result) {
  EnterFunction(last_io_result);
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
      case STATE_LOAD_SSL_HOST_INFO:
        DCHECK(rv == OK || rv == ERR_IO_PENDING);
        rv = DoLoadSSLHostInfo();
        break;
      case STATE_HANDSHAKE:
        rv = DoHandshake();
        break;
      case STATE_GET_DOMAIN_BOUND_CERT_COMPLETE:
        rv = DoGetDBCertComplete(rv);
        break;
      case STATE_VERIFY_DNSSEC:
        rv = DoVerifyDNSSEC(rv);
        break;
      case STATE_VERIFY_CERT:
        DCHECK(rv == OK);
        rv = DoVerifyCert(rv);
        break;
      case STATE_VERIFY_CERT_COMPLETE:
        rv = DoVerifyCertComplete(rv);
        break;
      case STATE_NONE:
      default:
        rv = ERR_UNEXPECTED;
        LOG(DFATAL) << "unexpected state " << state;
        break;
    }

    // Do the actual network I/O
    bool network_moved = DoTransportIO();
    if (network_moved && next_handshake_state_ == STATE_HANDSHAKE) {
      // In general we exit the loop if rv is ERR_IO_PENDING.  In this
      // special case we keep looping even if rv is ERR_IO_PENDING because
      // the transport IO may allow DoHandshake to make progress.
      DCHECK(rv == OK || rv == ERR_IO_PENDING);
      rv = OK;  // This causes us to stay in the loop.
    }
  } while (rv != ERR_IO_PENDING && next_handshake_state_ != STATE_NONE);
  LeaveFunction("");
  return rv;
}

int SSLClientSocketNSS::DoReadLoop(int result) {
  EnterFunction("");
  DCHECK(completed_handshake_);
  DCHECK(next_handshake_state_ == STATE_NONE);

  if (result < 0)
    return result;

  if (!nss_bufs_) {
    LOG(DFATAL) << "!nss_bufs_";
    int rv = ERR_UNEXPECTED;
    net_log_.AddEvent(NetLog::TYPE_SSL_READ_ERROR,
                      make_scoped_refptr(new SSLErrorParams(rv, 0)));
    return rv;
  }

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

  if (!nss_bufs_) {
    LOG(DFATAL) << "!nss_bufs_";
    int rv = ERR_UNEXPECTED;
    net_log_.AddEvent(NetLog::TYPE_SSL_WRITE_ERROR,
                      make_scoped_refptr(new SSLErrorParams(rv, 0)));
    return rv;
  }

  bool network_moved;
  int rv;
  do {
    rv = DoPayloadWrite();
    network_moved = DoTransportIO();
  } while (rv == ERR_IO_PENDING && network_moved);

  LeaveFunction("");
  return rv;
}

bool SSLClientSocketNSS::LoadSSLHostInfo() {
  const SSLHostInfo::State& state(ssl_host_info_->state());

  if (state.certs.empty())
    return true;

  const std::vector<std::string>& certs_in = state.certs;
  scoped_array<CERTCertificate*> certs(new CERTCertificate*[certs_in.size()]);

  for (size_t i = 0; i < certs_in.size(); i++) {
    SECItem derCert;
    derCert.data =
      const_cast<uint8*>(reinterpret_cast<const uint8*>(certs_in[i].data()));
    derCert.len = certs_in[i].size();
    certs[i] = CERT_NewTempCertificate(
        CERT_GetDefaultCertDB(), &derCert, NULL /* no nickname given */,
        PR_FALSE /* not permanent */, PR_TRUE /* copy DER data */);
    if (!certs[i]) {
      DestroyCertificates(&certs[0], i);
      NOTREACHED();
      return false;
    }
  }

  SECStatus rv;
#ifdef SSL_ENABLE_CACHED_INFO
  rv = SSL_SetPredictedPeerCertificates(nss_fd_, certs.get(), certs_in.size());
  DCHECK_EQ(SECSuccess, rv);
#else
  rv = SECFailure;  // Not implemented.
#endif
  DestroyCertificates(&certs[0], certs_in.size());

  return rv == SECSuccess;
}

int SSLClientSocketNSS::DoLoadSSLHostInfo() {
  EnterFunction("");
  int rv = ssl_host_info_->WaitForDataReady(
      base::Bind(&SSLClientSocketNSS::OnHandshakeIOComplete,
                 base::Unretained(this)));
  GotoState(STATE_HANDSHAKE);

  if (rv == OK) {
    if (!LoadSSLHostInfo())
      LOG(WARNING) << "LoadSSLHostInfo failed: " << host_and_port_.ToString();
  } else {
    DCHECK_EQ(ERR_IO_PENDING, rv);
    GotoState(STATE_LOAD_SSL_HOST_INFO);
  }

  LeaveFunction("");
  return rv;
}

int SSLClientSocketNSS::DoHandshake() {
  EnterFunction("");
  int net_error = net::OK;
  SECStatus rv = SSL_ForceHandshake(nss_fd_);

  // TODO(rkn): Handle the case in which server-bound cert generation takes
  // too long and the server has closed the connection. Report some new error
  // code so that the higher level code will attempt to delete the socket and
  // redo the handshake.

  if (client_auth_cert_needed_) {
    if (domain_bound_cert_xtn_negotiated_) {
      GotoState(STATE_GET_DOMAIN_BOUND_CERT_COMPLETE);
      net_error = ERR_IO_PENDING;
    } else {
      net_error = ERR_SSL_CLIENT_AUTH_CERT_NEEDED;
      net_log_.AddEvent(NetLog::TYPE_SSL_HANDSHAKE_ERROR,
                        make_scoped_refptr(new SSLErrorParams(net_error, 0)));
      // If the handshake already succeeded (because the server requests but
      // doesn't require a client cert), we need to invalidate the SSL session
      // so that we won't try to resume the non-client-authenticated session in
      // the next handshake.  This will cause the server to ask for a client
      // cert again.
      if (rv == SECSuccess && SSL_InvalidateSession(nss_fd_) != SECSuccess) {
        LOG(WARNING) << "Couldn't invalidate SSL session: " << PR_GetError();
      }
    }
  } else if (rv == SECSuccess) {
    if (handshake_callback_called_) {
      if (eset_mitm_detected_) {
        net_error = ERR_ESET_ANTI_VIRUS_SSL_INTERCEPTION;
      } else {
        // We need to see if the predicted certificate chain (in
        // |ssl_host_info_->state().certs) matches the actual certificate chain
        // before we call SaveSSLHostInfo, as that will update
        // |ssl_host_info_|.
        if (ssl_host_info_.get() && !ssl_host_info_->state().certs.empty()) {
          PeerCertificateChain certs(nss_fd_);
          const SSLHostInfo::State& state = ssl_host_info_->state();
          predicted_cert_chain_correct_ = certs.size() == state.certs.size();
          if (predicted_cert_chain_correct_) {
            for (unsigned i = 0; i < certs.size(); i++) {
              if (certs[i]->derCert.len != state.certs[i].size() ||
                  memcmp(certs[i]->derCert.data, state.certs[i].data(),
                         certs[i]->derCert.len) != 0) {
                predicted_cert_chain_correct_ = false;
                break;
              }
            }
          }
        }

#if defined(SSL_ENABLE_OCSP_STAPLING)
        // TODO(agl): figure out how to plumb an OCSP response into the Mac
        // system library and update IsOCSPStaplingSupported for Mac.
        if (!predicted_cert_chain_correct_ && IsOCSPStaplingSupported()) {
          unsigned int len = 0;
          SSL_GetStapledOCSPResponse(nss_fd_, NULL, &len);
          if (len) {
            const unsigned int orig_len = len;
            scoped_array<uint8> ocsp_response(new uint8[orig_len]);
            SSL_GetStapledOCSPResponse(nss_fd_, ocsp_response.get(), &len);
            DCHECK_EQ(orig_len, len);

#if defined(OS_WIN)
            CRYPT_DATA_BLOB ocsp_response_blob;
            ocsp_response_blob.cbData = len;
            ocsp_response_blob.pbData = ocsp_response.get();
            BOOL ok = CertSetCertificateContextProperty(
                server_cert_->os_cert_handle(),
                CERT_OCSP_RESPONSE_PROP_ID,
                CERT_SET_PROPERTY_IGNORE_PERSIST_ERROR_FLAG,
                &ocsp_response_blob);
            if (!ok) {
              VLOG(1) << "Failed to set OCSP response property: "
                      << GetLastError();
            }
#elif defined(USE_NSS)
            CacheOCSPResponseFromSideChannelFunction cache_ocsp_response =
                GetCacheOCSPResponseFromSideChannelFunction();
            SECItem ocsp_response_item;
            ocsp_response_item.type = siBuffer;
            ocsp_response_item.data = ocsp_response.get();
            ocsp_response_item.len = len;

            cache_ocsp_response(
                CERT_GetDefaultCertDB(), server_cert_nss_, PR_Now(),
                &ocsp_response_item, NULL);
#endif
          }
        }
#endif

        SaveSSLHostInfo();
        // SSL handshake is completed. Let's verify the certificate.
        GotoState(STATE_VERIFY_DNSSEC);
      }
      // Done!
    } else {
      // Workaround for https://bugzilla.mozilla.org/show_bug.cgi?id=562434 -
      // SSL_ForceHandshake returned SECSuccess prematurely.
      rv = SECFailure;
      net_error = ERR_SSL_PROTOCOL_ERROR;
      net_log_.AddEvent(NetLog::TYPE_SSL_HANDSHAKE_ERROR,
                        make_scoped_refptr(new SSLErrorParams(net_error, 0)));
    }
  } else {
    PRErrorCode prerr = PR_GetError();
    net_error = HandleNSSError(prerr, true);

    // If not done, stay in this state
    if (net_error == ERR_IO_PENDING) {
      GotoState(STATE_HANDSHAKE);
    } else {
      net_log_.AddEvent(
          NetLog::TYPE_SSL_HANDSHAKE_ERROR,
          make_scoped_refptr(new SSLErrorParams(net_error, prerr)));
    }
  }

  LeaveFunction("");
  return net_error;
}

int SSLClientSocketNSS::ImportDBCertAndKey(CERTCertificate** cert,
                                           SECKEYPrivateKey** key) {
  // Set the certificate.
  SECItem cert_item;
  cert_item.data = (unsigned char*) domain_bound_cert_.data();
  cert_item.len = domain_bound_cert_.size();
  *cert = CERT_NewTempCertificate(CERT_GetDefaultCertDB(),
                                  &cert_item,
                                  NULL,
                                  PR_FALSE,
                                  PR_TRUE);
  if (*cert == NULL)
    return MapNSSError(PORT_GetError());

  // Set the private key.
  switch (domain_bound_cert_type_) {
    case CLIENT_CERT_ECDSA_SIGN: {
      SECKEYPublicKey* public_key = NULL;
      if (!crypto::ECPrivateKey::ImportFromEncryptedPrivateKeyInfo(
          ServerBoundCertService::kEPKIPassword,
          reinterpret_cast<const unsigned char*>(
              domain_bound_private_key_.data()),
          domain_bound_private_key_.size(),
          &(*cert)->subjectPublicKeyInfo,
          false,
          false,
          key,
          &public_key)) {
        CERT_DestroyCertificate(*cert);
        *cert = NULL;
        return MapNSSError(PORT_GetError());
      }
      SECKEY_DestroyPublicKey(public_key);
      break;
    }

    default:
      NOTREACHED();
      return ERR_INVALID_ARGUMENT;
  }

  return OK;
}

int SSLClientSocketNSS::DoGetDBCertComplete(int result) {
  net_log_.EndEventWithNetErrorCode(NetLog::TYPE_SSL_GET_DOMAIN_BOUND_CERT,
                                    result);
  client_auth_cert_needed_ = false;
  domain_bound_cert_request_handle_ = NULL;

  if (result != OK)
    return result;

  CERTCertificate* cert;
  SECKEYPrivateKey* key;
  int error = ImportDBCertAndKey(&cert, &key);
  if (error != OK)
    return error;

  CERTCertificateList* cert_chain = CERT_CertChainFromCert(cert,
                                                           certUsageSSLClient,
                                                           PR_FALSE);
  net_log_.AddEvent(NetLog::TYPE_SSL_CLIENT_CERT_PROVIDED,
      make_scoped_refptr(new NetLogIntegerParameter("cert_count",
                                                    cert_chain->len)));
  SECStatus rv;
  rv = SSL_RestartHandshakeAfterCertReq(nss_fd_, cert, key, cert_chain);
  if (rv != SECSuccess)
    return MapNSSError(PORT_GetError());

  GotoState(STATE_HANDSHAKE);
  set_domain_bound_cert_type(domain_bound_cert_type_);
  return OK;
}

int SSLClientSocketNSS::DoVerifyDNSSEC(int result) {
  DNSValidationResult r = CheckDNSSECChain(host_and_port_.host(),
                                           server_cert_nss_,
                                           host_and_port_.port());
  if (r == DNSVR_SUCCESS) {
    local_server_cert_verify_result_.cert_status |= CERT_STATUS_IS_DNSSEC;
    local_server_cert_verify_result_.verified_cert = server_cert_;
    server_cert_verify_result_ = &local_server_cert_verify_result_;
    GotoState(STATE_VERIFY_CERT_COMPLETE);
    return OK;
  }

  GotoState(STATE_VERIFY_CERT);

  return OK;
}

int SSLClientSocketNSS::DoVerifyCert(int result) {
  DCHECK(server_cert_nss_);

  GotoState(STATE_VERIFY_CERT_COMPLETE);

  // If the certificate is expected to be bad we can use the
  // expectation as the cert status. Don't use |server_cert_| here
  // because it can be set to NULL in case we failed to create
  // X509Certificate in UpdateServerCert(). This may happen when this
  // code is used inside sandbox.
  base::StringPiece der_cert(
      reinterpret_cast<char*>(server_cert_nss_->derCert.data),
      server_cert_nss_->derCert.len);
  CertStatus cert_status;
  if (ssl_config_.IsAllowedBadCert(der_cert, &cert_status)) {
    DCHECK(start_cert_verification_time_.is_null());
    VLOG(1) << "Received an expected bad cert with status: " << cert_status;
    server_cert_verify_result_ = &local_server_cert_verify_result_;
    local_server_cert_verify_result_.Reset();
    local_server_cert_verify_result_.cert_status = cert_status;
    local_server_cert_verify_result_.verified_cert = server_cert_;
    return OK;
  }

  // We may have failed to create X509Certificate object if we are
  // running inside sandbox.
  if (!server_cert_) {
    server_cert_verify_result_ = &local_server_cert_verify_result_;
    local_server_cert_verify_result_.Reset();
    local_server_cert_verify_result_.cert_status = CERT_STATUS_INVALID;
    return ERR_CERT_INVALID;
  }

  start_cert_verification_time_ = base::TimeTicks::Now();

  if (ssl_host_info_.get() && !ssl_host_info_->state().certs.empty() &&
      predicted_cert_chain_correct_) {
    // If the SSLHostInfo had a prediction for the certificate chain of this
    // server then it will have optimistically started a verification of that
    // chain. So, if the prediction was correct, we should wait for that
    // verification to finish rather than start our own.
    net_log_.AddEvent(NetLog::TYPE_SSL_VERIFICATION_MERGED, NULL);
    UMA_HISTOGRAM_ENUMERATION("Net.SSLVerificationMerged", 1 /* true */, 2);
    base::TimeTicks end_time = ssl_host_info_->verification_end_time();
    if (end_time.is_null())
      end_time = base::TimeTicks::Now();
    UMA_HISTOGRAM_TIMES("Net.SSLVerificationMergedMsSaved",
                        end_time - ssl_host_info_->verification_start_time());
    server_cert_verify_result_ = &ssl_host_info_->cert_verify_result();
    return ssl_host_info_->WaitForCertVerification(
        base::Bind(&SSLClientSocketNSS::OnHandshakeIOComplete,
                   base::Unretained(this)));
  } else {
    UMA_HISTOGRAM_ENUMERATION("Net.SSLVerificationMerged", 0 /* false */, 2);
  }

  int flags = 0;
  if (ssl_config_.rev_checking_enabled)
    flags |= X509Certificate::VERIFY_REV_CHECKING_ENABLED;
  if (ssl_config_.verify_ev_cert)
    flags |= X509Certificate::VERIFY_EV_CERT;
  if (ssl_config_.cert_io_enabled)
    flags |= X509Certificate::VERIFY_CERT_IO_ENABLED;
  verifier_.reset(new SingleRequestCertVerifier(cert_verifier_));
  server_cert_verify_result_ = &local_server_cert_verify_result_;
  return verifier_->Verify(
      server_cert_, host_and_port_.host(), flags,
      SSLConfigService::GetCRLSet(),
      &local_server_cert_verify_result_,
      base::Bind(&SSLClientSocketNSS::OnHandshakeIOComplete,
                 base::Unretained(this)),
      net_log_);
}

// Derived from AuthCertificateCallback() in
// mozilla/source/security/manager/ssl/src/nsNSSCallbacks.cpp.
int SSLClientSocketNSS::DoVerifyCertComplete(int result) {
  verifier_.reset();

  if (!start_cert_verification_time_.is_null()) {
    base::TimeDelta verify_time =
        base::TimeTicks::Now() - start_cert_verification_time_;
    if (result == OK)
        UMA_HISTOGRAM_TIMES("Net.SSLCertVerificationTime", verify_time);
    else
        UMA_HISTOGRAM_TIMES("Net.SSLCertVerificationTimeError", verify_time);
  }

  // We used to remember the intermediate CA certs in the NSS database
  // persistently.  However, NSS opens a connection to the SQLite database
  // during NSS initialization and doesn't close the connection until NSS
  // shuts down.  If the file system where the database resides is gone,
  // the database connection goes bad.  What's worse, the connection won't
  // recover when the file system comes back.  Until this NSS or SQLite bug
  // is fixed, we need to  avoid using the NSS database for non-essential
  // purposes.  See https://bugzilla.mozilla.org/show_bug.cgi?id=508081 and
  // http://crbug.com/15630 for more info.

  // TODO(hclam): Skip logging if server cert was expected to be bad because
  // |server_cert_verify_result_| doesn't contain all the information about
  // the cert.
  if (result == OK)
    LogConnectionTypeMetrics();

  completed_handshake_ = true;

  if (!user_read_callback_.is_null()) {
    int rv = DoReadLoop(OK);
    if (rv != ERR_IO_PENDING)
      DoReadCallback(rv);
  }

#if defined(OFFICIAL_BUILD) && !defined(OS_ANDROID)
  // Take care of any mandates for public key pinning.
  //
  // Pinning is only enabled for official builds to make sure that others don't
  // end up with pins that cannot be easily updated.
  //
  // TODO(agl): we might have an issue here where a request for foo.example.com
  // merges into a SPDY connection to www.example.com, and gets a different
  // certificate.

  const CertStatus cert_status = server_cert_verify_result_->cert_status;
  if ((result == OK || (IsCertificateError(result) &&
                        IsCertStatusMinorError(cert_status))) &&
      server_cert_verify_result_->is_issued_by_known_root &&
      transport_security_state_) {
    bool sni_available = ssl_config_.tls1_enabled || ssl_config_.ssl3_fallback;
    const std::string& host = host_and_port_.host();

    TransportSecurityState::DomainState domain_state;
    if (transport_security_state_->GetDomainState(host, sni_available,
                                                  &domain_state) &&
        domain_state.HasPins()) {
      if (!domain_state.IsChainOfPublicKeysPermitted(
               server_cert_verify_result_->public_key_hashes)) {
        const base::Time build_time = base::GetBuildTime();
        // Pins are not enforced if the build is sufficiently old. Chrome
        // users should get updates every six weeks or so, but it's possible
        // that some users will stop getting updates for some reason. We
        // don't want those users building up as a pool of people with bad
        // pins.
        if ((base::Time::Now() - build_time).InDays() < 70 /* 10 weeks */) {
          result = ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN;
          UMA_HISTOGRAM_BOOLEAN("Net.PublicKeyPinSuccess", false);
          TransportSecurityState::ReportUMAOnPinFailure(host);
        }
      } else {
        UMA_HISTOGRAM_BOOLEAN("Net.PublicKeyPinSuccess", true);
      }
    }
  }
#endif

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
    rv = ERR_SSL_CLIENT_AUTH_CERT_NEEDED;
    net_log_.AddEvent(NetLog::TYPE_SSL_READ_ERROR,
                      make_scoped_refptr(new SSLErrorParams(rv, 0)));
    return rv;
  }
  if (rv >= 0) {
    net_log_.AddByteTransferEvent(NetLog::TYPE_SSL_SOCKET_BYTES_RECEIVED, rv,
                                  user_read_buf_->data());
    LeaveFunction("");
    return rv;
  }
  PRErrorCode prerr = PR_GetError();
  if (prerr == PR_WOULD_BLOCK_ERROR) {
    LeaveFunction("");
    return ERR_IO_PENDING;
  }
  LeaveFunction("");
  rv = HandleNSSError(prerr, false);
  net_log_.AddEvent(NetLog::TYPE_SSL_READ_ERROR,
                    make_scoped_refptr(new SSLErrorParams(rv, prerr)));
  return rv;
}

int SSLClientSocketNSS::DoPayloadWrite() {
  EnterFunction(user_write_buf_len_);
  DCHECK(user_write_buf_);
  int rv = PR_Write(nss_fd_, user_write_buf_->data(), user_write_buf_len_);
  if (rv >= 0) {
    net_log_.AddByteTransferEvent(NetLog::TYPE_SSL_SOCKET_BYTES_SENT, rv,
                                  user_write_buf_->data());
    LeaveFunction("");
    return rv;
  }
  PRErrorCode prerr = PR_GetError();
  if (prerr == PR_WOULD_BLOCK_ERROR) {
    LeaveFunction("");
    return ERR_IO_PENDING;
  }
  LeaveFunction("");
  rv = HandleNSSError(prerr, false);
  net_log_.AddEvent(NetLog::TYPE_SSL_WRITE_ERROR,
                    make_scoped_refptr(new SSLErrorParams(rv, prerr)));
  return rv;
}

void SSLClientSocketNSS::LogConnectionTypeMetrics() const {
  UpdateConnectionTypeHistograms(CONNECTION_SSL);
  if (server_cert_verify_result_->has_md5)
    UpdateConnectionTypeHistograms(CONNECTION_SSL_MD5);
  if (server_cert_verify_result_->has_md2)
    UpdateConnectionTypeHistograms(CONNECTION_SSL_MD2);
  if (server_cert_verify_result_->has_md4)
    UpdateConnectionTypeHistograms(CONNECTION_SSL_MD4);
  if (server_cert_verify_result_->has_md5_ca)
    UpdateConnectionTypeHistograms(CONNECTION_SSL_MD5_CA);
  if (server_cert_verify_result_->has_md2_ca)
    UpdateConnectionTypeHistograms(CONNECTION_SSL_MD2_CA);
  int ssl_version = SSLConnectionStatusToVersion(ssl_connection_status_);
  switch (ssl_version) {
    case SSL_CONNECTION_VERSION_SSL2:
      UpdateConnectionTypeHistograms(CONNECTION_SSL_SSL2);
      break;
    case SSL_CONNECTION_VERSION_SSL3:
      UpdateConnectionTypeHistograms(CONNECTION_SSL_SSL3);
      break;
    case SSL_CONNECTION_VERSION_TLS1:
      UpdateConnectionTypeHistograms(CONNECTION_SSL_TLS1);
      break;
    case SSL_CONNECTION_VERSION_TLS1_1:
      UpdateConnectionTypeHistograms(CONNECTION_SSL_TLS1_1);
      break;
    case SSL_CONNECTION_VERSION_TLS1_2:
      UpdateConnectionTypeHistograms(CONNECTION_SSL_TLS1_2);
      break;
  };
}

// SaveSSLHostInfo saves the certificate chain of the connection so that we can
// start verification faster in the future.
void SSLClientSocketNSS::SaveSSLHostInfo() {
  if (!ssl_host_info_.get())
    return;

  // If the SSLHostInfo hasn't managed to load from disk yet then we can't save
  // anything.
  if (ssl_host_info_->WaitForDataReady(net::CompletionCallback()) != OK)
    return;

  SSLHostInfo::State* state = ssl_host_info_->mutable_state();

  state->certs.clear();
  PeerCertificateChain certs(nss_fd_);
  for (unsigned i = 0; i < certs.size(); i++) {
    if (certs[i]->derCert.len > std::numeric_limits<uint16>::max())
      return;

    state->certs.push_back(std::string(
          reinterpret_cast<char*>(certs[i]->derCert.data),
          certs[i]->derCert.len));
  }

  ssl_host_info_->Persist();
}

// Do as much network I/O as possible between the buffer and the
// transport socket. Return true if some I/O performed, false
// otherwise (error or ERR_IO_PENDING).
bool SSLClientSocketNSS::DoTransportIO() {
  EnterFunction("");
  bool network_moved = false;
  if (nss_bufs_ != NULL) {
    int rv;
    // Read and write as much data as we can. The loop is neccessary
    // because Write() may return synchronously.
    do {
      rv = BufferSend();
      if (rv > 0)
        network_moved = true;
    } while (rv > 0);
    if (BufferRecv() >= 0)
      network_moved = true;
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

  int rv = 0;
  if (len) {
    scoped_refptr<IOBuffer> send_buffer(new IOBuffer(len));
    memcpy(send_buffer->data(), buf1, len1);
    memcpy(send_buffer->data() + len1, buf2, len2);
    rv = transport_->socket()->Write(
        send_buffer, len,
        base::Bind(&SSLClientSocketNSS::BufferSendComplete,
                   base::Unretained(this)));
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

  char* buf;
  int nb = memio_GetReadParams(nss_bufs_, &buf);
  EnterFunction(nb);
  int rv;
  if (!nb) {
    // buffer too full to read into, so no I/O possible at moment
    rv = ERR_IO_PENDING;
  } else {
    recv_buffer_ = new IOBuffer(nb);
    rv = transport_->socket()->Read(
        recv_buffer_, nb,
        base::Bind(&SSLClientSocketNSS::BufferRecvComplete,
                   base::Unretained(this)));
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
    char* buf;
    memio_GetReadParams(nss_bufs_, &buf);
    memcpy(buf, recv_buffer_->data(), result);
  }
  recv_buffer_ = NULL;
  memio_PutReadResult(nss_bufs_, MapErrorToNSS(result));
  transport_recv_busy_ = false;
  OnRecvComplete(result);
  LeaveFunction("");
}

int SSLClientSocketNSS::HandleNSSError(PRErrorCode nss_error,
                                       bool handshake_error) {
  int net_error = handshake_error ? MapNSSHandshakeError(nss_error) :
                                    MapNSSError(nss_error);

#if defined(OS_WIN)
  // On Windows, a handle to the HCRYPTPROV is cached in the X509Certificate
  // os_cert_handle() as an optimization. However, if the certificate
  // private key is stored on a smart card, and the smart card is removed,
  // the cached HCRYPTPROV will not be able to obtain the HCRYPTKEY again,
  // preventing client certificate authentication. Because the
  // X509Certificate may outlive the individual SSLClientSocketNSS, due to
  // caching in X509Certificate, this failure ends up preventing client
  // certificate authentication with the same certificate for all future
  // attempts, even after the smart card has been re-inserted. By setting
  // the CERT_KEY_PROV_HANDLE_PROP_ID to NULL, the cached HCRYPTPROV will
  // typically be freed. This allows a new HCRYPTPROV to be obtained from
  // the certificate on the next attempt, which should succeed if the smart
  // card has been re-inserted, or will typically prompt the user to
  // re-insert the smart card if not.
  if ((net_error == ERR_SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY ||
       net_error == ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED) &&
      ssl_config_.send_client_cert && ssl_config_.client_cert) {
    CertSetCertificateContextProperty(
        ssl_config_.client_cert->os_cert_handle(),
        CERT_KEY_PROV_HANDLE_PROP_ID, 0, NULL);
  }
#endif

  return net_error;
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
  SSLClientSocketNSS* that = reinterpret_cast<SSLClientSocketNSS*>(arg);
  if (!that->server_cert_nss_) {
    // Only need to turn off False Start in the initial handshake. Also, it is
    // unsafe to call SSL_OptionSet in a renegotiation because the "first
    // handshake" lock isn't already held, which will result in an assertion
    // failure in the ssl_Get1stHandshakeLock call in SSL_OptionSet.
    PRBool npn;
    SECStatus rv = SSL_HandshakeNegotiatedExtension(socket,
                                                    ssl_next_proto_nego_xtn,
                                                    &npn);
    if (rv != SECSuccess || !npn) {
      // If the server doesn't support NPN, then we don't do False Start with
      // it.
      SSL_OptionSet(socket, SSL_ENABLE_FALSE_START, PR_FALSE);
    }
  }
#endif

  // Tell NSS to not verify the certificate.
  return SECSuccess;
}

// static
bool SSLClientSocketNSS::DomainBoundCertNegotiated(PRFileDesc* socket) {
  PRBool xtn_negotiated = PR_FALSE;
  SECStatus rv = SSL_HandshakeNegotiatedExtension(
      socket, ssl_ob_cert_xtn, &xtn_negotiated);
  DCHECK_EQ(SECSuccess, rv);

  return xtn_negotiated ? true : false;
}

SECStatus SSLClientSocketNSS::DomainBoundClientAuthHandler(
    const SECItem* cert_types,
    CERTCertificate** result_certificate,
    SECKEYPrivateKey** result_private_key) {
  domain_bound_cert_xtn_negotiated_ = true;

  // We have negotiated the domain-bound certificate extension.
  std::string origin = "https://" + host_and_port_.ToString();
  std::vector<uint8> requested_cert_types(cert_types->data,
                                          cert_types->data + cert_types->len);
  net_log_.BeginEvent(NetLog::TYPE_SSL_GET_DOMAIN_BOUND_CERT, NULL);
  int error = server_bound_cert_service_->GetDomainBoundCert(
      origin,
      requested_cert_types,
      &domain_bound_cert_type_,
      &domain_bound_private_key_,
      &domain_bound_cert_,
      base::Bind(&SSLClientSocketNSS::OnHandshakeIOComplete,
                 base::Unretained(this)),
      &domain_bound_cert_request_handle_);

  if (error == ERR_IO_PENDING) {
    // Asynchronous case.
    client_auth_cert_needed_ = true;
    return SECWouldBlock;
  }
  net_log_.EndEventWithNetErrorCode(NetLog::TYPE_SSL_GET_DOMAIN_BOUND_CERT,
                                    error);

  SECStatus rv = SECSuccess;
  if (error == OK) {
    // Synchronous success.
    int result = ImportDBCertAndKey(result_certificate,
                                    result_private_key);
    if (result == OK) {
      set_domain_bound_cert_type(domain_bound_cert_type_);
    } else {
      rv = SECFailure;
    }
  } else {
    rv = SECFailure;  // Synchronous failure.
  }

  int cert_count = (rv == SECSuccess) ? 1 : 0;
  net_log_.AddEvent(NetLog::TYPE_SSL_CLIENT_CERT_PROVIDED,
      make_scoped_refptr(new NetLogIntegerParameter("cert_count",
                                                    cert_count)));
  return rv;
}

#if defined(NSS_PLATFORM_CLIENT_AUTH)
// static
// NSS calls this if a client certificate is needed.
SECStatus SSLClientSocketNSS::PlatformClientAuthHandler(
    void* arg,
    PRFileDesc* socket,
    CERTDistNames* ca_names,
    CERTCertList** result_certs,
    void** result_private_key,
    CERTCertificate** result_nss_certificate,
    SECKEYPrivateKey** result_nss_private_key) {
  SSLClientSocketNSS* that = reinterpret_cast<SSLClientSocketNSS*>(arg);

  that->net_log_.AddEvent(NetLog::TYPE_SSL_CLIENT_CERT_REQUESTED, NULL);

  const SECItem* cert_types = SSL_GetRequestedClientCertificateTypes(socket);

  // Check if a domain-bound certificate is requested.
  if (DomainBoundCertNegotiated(socket)) {
    return that->DomainBoundClientAuthHandler(
        cert_types, result_nss_certificate, result_nss_private_key);
  }

  that->client_auth_cert_needed_ = !that->ssl_config_.send_client_cert;
#if defined(OS_WIN)
  if (that->ssl_config_.send_client_cert) {
    if (that->ssl_config_.client_cert) {
      PCCERT_CONTEXT cert_context =
          that->ssl_config_.client_cert->os_cert_handle();

      HCRYPTPROV_OR_NCRYPT_KEY_HANDLE crypt_prov = 0;
      DWORD key_spec = 0;
      BOOL must_free = FALSE;
      BOOL acquired_key = CryptAcquireCertificatePrivateKey(
          cert_context, CRYPT_ACQUIRE_CACHE_FLAG, NULL,
          &crypt_prov, &key_spec, &must_free);

      if (acquired_key) {
        // Since we passed CRYPT_ACQUIRE_CACHE_FLAG, |must_free| must be false
        // according to the MSDN documentation.
        CHECK_EQ(must_free, FALSE);
        DCHECK_NE(key_spec, CERT_NCRYPT_KEY_SPEC);

        SECItem der_cert;
        der_cert.type = siDERCertBuffer;
        der_cert.data = cert_context->pbCertEncoded;
        der_cert.len  = cert_context->cbCertEncoded;

        // TODO(rsleevi): Error checking for NSS allocation errors.
        CERTCertDBHandle* db_handle = CERT_GetDefaultCertDB();
        CERTCertificate* user_cert = CERT_NewTempCertificate(
            db_handle, &der_cert, NULL, PR_FALSE, PR_TRUE);
        if (!user_cert) {
          // Importing the certificate can fail for reasons including a serial
          // number collision. See crbug.com/97355.
          that->net_log_.AddEvent(NetLog::TYPE_SSL_CLIENT_CERT_PROVIDED,
              make_scoped_refptr(new NetLogIntegerParameter("cert_count", 0)));
          return SECFailure;
        }
        CERTCertList* cert_chain = CERT_NewCertList();
        CERT_AddCertToListTail(cert_chain, user_cert);

        // Add the intermediates.
        X509Certificate::OSCertHandles intermediates =
            that->ssl_config_.client_cert->GetIntermediateCertificates();
        for (X509Certificate::OSCertHandles::const_iterator it =
            intermediates.begin(); it != intermediates.end(); ++it) {
          der_cert.data = (*it)->pbCertEncoded;
          der_cert.len = (*it)->cbCertEncoded;

          CERTCertificate* intermediate = CERT_NewTempCertificate(
              db_handle, &der_cert, NULL, PR_FALSE, PR_TRUE);
          if (!intermediate) {
            CERT_DestroyCertList(cert_chain);
            that->net_log_.AddEvent(NetLog::TYPE_SSL_CLIENT_CERT_PROVIDED,
                make_scoped_refptr(new NetLogIntegerParameter("cert_count",
                                                              0)));
            return SECFailure;
          }
          CERT_AddCertToListTail(cert_chain, intermediate);
        }
        PCERT_KEY_CONTEXT key_context = reinterpret_cast<PCERT_KEY_CONTEXT>(
            PORT_ZAlloc(sizeof(CERT_KEY_CONTEXT)));
        key_context->cbSize = sizeof(*key_context);
        // NSS will free this context when no longer in use, but the
        // |must_free| result from CryptAcquireCertificatePrivateKey was false
        // so we increment the refcount to negate NSS's future decrement.
        CryptContextAddRef(crypt_prov, NULL, 0);
        key_context->hCryptProv = crypt_prov;
        key_context->dwKeySpec = key_spec;
        *result_private_key = key_context;
        *result_certs = cert_chain;

        int cert_count = 1 + intermediates.size();
        that->net_log_.AddEvent(NetLog::TYPE_SSL_CLIENT_CERT_PROVIDED,
            make_scoped_refptr(new NetLogIntegerParameter("cert_count",
                                                          cert_count)));
        return SECSuccess;
      }
      LOG(WARNING) << "Client cert found without private key";
    }
    // Send no client certificate.
    that->net_log_.AddEvent(NetLog::TYPE_SSL_CLIENT_CERT_PROVIDED,
        make_scoped_refptr(new NetLogIntegerParameter("cert_count", 0)));
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
    that->net_log_.AddEvent(NetLog::TYPE_SSL_CLIENT_CERT_PROVIDED,
        make_scoped_refptr(new NetLogIntegerParameter("cert_count", 0)));
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
  DWORD find_flags = CERT_CHAIN_FIND_BY_ISSUER_CACHE_ONLY_FLAG |
                     CERT_CHAIN_FIND_BY_ISSUER_CACHE_ONLY_URL_FLAG;

  for (;;) {
    // Find a certificate chain.
    chain_context = CertFindChainInStore(my_cert_store,
                                         X509_ASN_ENCODING,
                                         find_flags,
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
    // Create a copy the handle, so that we can close the "MY" certificate store
    // before returning from this function.
    PCCERT_CONTEXT cert_context2;
    BOOL ok = CertAddCertificateContextToStore(NULL, cert_context,
                                               CERT_STORE_ADD_USE_EXISTING,
                                               &cert_context2);
    if (!ok) {
      NOTREACHED();
      continue;
    }

    // Copy the rest of the chain. Copying the chain stops gracefully if an
    // error is encountered, with the partial chain being used as the
    // intermediates, as opposed to failing to consider the client certificate
    // at all.
    net::X509Certificate::OSCertHandles intermediates;
    for (DWORD i = 1; i < chain_context->rgpChain[0]->cElement; i++) {
      PCCERT_CONTEXT intermediate_copy;
      ok = CertAddCertificateContextToStore(
          NULL, chain_context->rgpChain[0]->rgpElement[i]->pCertContext,
          CERT_STORE_ADD_USE_EXISTING, &intermediate_copy);
      if (!ok) {
        NOTREACHED();
        break;
      }
      intermediates.push_back(intermediate_copy);
    }

    scoped_refptr<X509Certificate> cert = X509Certificate::CreateFromHandle(
        cert_context2, intermediates);
    that->client_certs_.push_back(cert);

    X509Certificate::FreeOSCertHandle(cert_context2);
    for (net::X509Certificate::OSCertHandles::iterator it =
        intermediates.begin(); it != intermediates.end(); ++it) {
      net::X509Certificate::FreeOSCertHandle(*it);
    }
  }

  BOOL ok = CertCloseStore(my_cert_store, CERT_CLOSE_STORE_CHECK_FLAG);
  DCHECK(ok);

  // Tell NSS to suspend the client authentication.  We will then abort the
  // handshake by returning ERR_SSL_CLIENT_AUTH_CERT_NEEDED.
  return SECWouldBlock;
#elif defined(OS_MACOSX)
  if (that->ssl_config_.send_client_cert) {
    if (that->ssl_config_.client_cert) {
      OSStatus os_error = noErr;
      SecIdentityRef identity = NULL;
      SecKeyRef private_key = NULL;
      CFArrayRef chain =
          that->ssl_config_.client_cert->CreateClientCertificateChain();
      if (chain) {
        identity = reinterpret_cast<SecIdentityRef>(
            const_cast<void*>(CFArrayGetValueAtIndex(chain, 0)));
      }
      if (identity)
        os_error = SecIdentityCopyPrivateKey(identity, &private_key);

      if (chain && identity && os_error == noErr) {
        // TODO(rsleevi): Error checking for NSS allocation errors.
        *result_certs = CERT_NewCertList();
        *result_private_key = private_key;

        for (CFIndex i = 0; i < CFArrayGetCount(chain); ++i) {
          CSSM_DATA cert_data;
          SecCertificateRef cert_ref;
          if (i == 0) {
            cert_ref = that->ssl_config_.client_cert->os_cert_handle();
          } else {
            cert_ref = reinterpret_cast<SecCertificateRef>(
                const_cast<void*>(CFArrayGetValueAtIndex(chain, i)));
          }
          os_error = SecCertificateGetData(cert_ref, &cert_data);
          if (os_error != noErr)
            break;

          SECItem der_cert;
          der_cert.type = siDERCertBuffer;
          der_cert.data = cert_data.Data;
          der_cert.len = cert_data.Length;
          CERTCertificate* nss_cert = CERT_NewTempCertificate(
              CERT_GetDefaultCertDB(), &der_cert, NULL, PR_FALSE, PR_TRUE);
          if (!nss_cert) {
            // In the event of an NSS error we make up an OS error and reuse
            // the error handling, below.
            os_error = errSecCreateChainFailed;
            break;
          }
          CERT_AddCertToListTail(*result_certs, nss_cert);
        }
      }
      if (os_error == noErr) {
        int cert_count = 0;
        if (chain) {
          cert_count = CFArrayGetCount(chain);
          CFRelease(chain);
        }
        that->net_log_.AddEvent(NetLog::TYPE_SSL_CLIENT_CERT_PROVIDED,
            make_scoped_refptr(new NetLogIntegerParameter("cert_count",
                                                          cert_count)));
        return SECSuccess;
      }
      OSSTATUS_LOG(WARNING, os_error)
          << "Client cert found, but could not be used";
      if (*result_certs) {
        CERT_DestroyCertList(*result_certs);
        *result_certs = NULL;
      }
      if (*result_private_key)
        *result_private_key = NULL;
      if (private_key)
        CFRelease(private_key);
      if (chain)
        CFRelease(chain);
    }
    // Send no client certificate.
    that->net_log_.AddEvent(NetLog::TYPE_SSL_CLIENT_CERT_PROVIDED,
        make_scoped_refptr(new NetLogIntegerParameter("cert_count", 0)));
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
  X509Certificate::GetSSLClientCertificates(that->host_and_port_.host(),
                                            valid_issuers,
                                            &that->client_certs_);

  // Tell NSS to suspend the client authentication.  We will then abort the
  // handshake by returning ERR_SSL_CLIENT_AUTH_CERT_NEEDED.
  return SECWouldBlock;
#else
  return SECFailure;
#endif
}

#else  // NSS_PLATFORM_CLIENT_AUTH

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

  that->net_log_.AddEvent(NetLog::TYPE_SSL_CLIENT_CERT_REQUESTED, NULL);

  const SECItem* cert_types = SSL_GetRequestedClientCertificateTypes(socket);

  // Check if a domain-bound certificate is requested.
  if (DomainBoundCertNegotiated(socket)) {
    return that->DomainBoundClientAuthHandler(
        cert_types, result_certificate, result_private_key);
  }

  // Regular client certificate requested.
  that->client_auth_cert_needed_ = !that->ssl_config_.send_client_cert;
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
        // A cert_count of -1 means the number of certificates is unknown.
        // NSS will construct the certificate chain.
        that->net_log_.AddEvent(NetLog::TYPE_SSL_CLIENT_CERT_PROVIDED,
            make_scoped_refptr(new NetLogIntegerParameter("cert_count", -1)));
        return SECSuccess;
      }
      LOG(WARNING) << "Client cert found without private key";
    }
    // Send no client certificate.
    that->net_log_.AddEvent(NetLog::TYPE_SSL_CLIENT_CERT_PROVIDED,
        make_scoped_refptr(new NetLogIntegerParameter("cert_count", 0)));
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
          node->cert, net::X509Certificate::OSCertHandles());
      that->client_certs_.push_back(x509_cert);
    }
    CERT_DestroyCertList(client_certs);
  }

  // Tell NSS to suspend the client authentication.  We will then abort the
  // handshake by returning ERR_SSL_CLIENT_AUTH_CERT_NEEDED.
  return SECWouldBlock;
}
#endif  // NSS_PLATFORM_CLIENT_AUTH

// static
// NSS calls this when handshake is completed.
// After the SSL handshake is finished, use CertVerifier to verify
// the saved server certificate.
void SSLClientSocketNSS::HandshakeCallback(PRFileDesc* socket,
                                           void* arg) {
  SSLClientSocketNSS* that = reinterpret_cast<SSLClientSocketNSS*>(arg);

  that->handshake_callback_called_ = true;

  that->UpdateServerCert();
  that->UpdateConnectionStatus();
}

// NextProtoCallback is called by NSS during the handshake, if the server
// supports NPN, to select a protocol from the list that the server offered.
// See the comment in net/third_party/nss/ssl/ssl.h for the meanings of the
// arguments.
// static
SECStatus
SSLClientSocketNSS::NextProtoCallback(void* arg,
                                      PRFileDesc* nss_fd,
                                      const unsigned char* protos,
                                      unsigned int protos_len,
                                      unsigned char* proto_out,
                                      unsigned int* proto_out_len,
                                      unsigned int proto_max_len) {
  SSLClientSocketNSS* that = reinterpret_cast<SSLClientSocketNSS*>(arg);

  // For each protocol in server preference, see if we support it.
  for (unsigned int i = 0; i < protos_len; ) {
    const size_t len = protos[i];
    for (std::vector<std::string>::const_iterator
         j = that->ssl_config_.next_protos.begin();
         j != that->ssl_config_.next_protos.end(); j++) {
      // Having very long elements in the |next_protos| vector isn't a disaster
      // because they'll never be selected, but it does indicate an error
      // somewhere.
      DCHECK_LT(j->size(), 256u);

      if (j->size() == len &&
          memcmp(&protos[i + 1], j->data(), len) == 0) {
        that->next_proto_status_ = kNextProtoNegotiated;
        that->next_proto_ = *j;
        break;
      }
    }

    if (that->next_proto_status_ == kNextProtoNegotiated)
      break;

    // NSS checks that the data in |protos| is well formed, so we know that
    // this doesn't cause us to jump off the end of the buffer.
    i += len + 1;
  }

  that->server_protos_.assign(
      reinterpret_cast<const char*>(protos), protos_len);

  // If we didn't find a protocol, we select the first one from our list.
  if (that->next_proto_status_ != kNextProtoNegotiated) {
    that->next_proto_status_ = kNextProtoNoOverlap;
    that->next_proto_ = that->ssl_config_.next_protos[0];
  }

  if (that->next_proto_.size() > proto_max_len) {
    PORT_SetError(SEC_ERROR_OUTPUT_LEN);
    return SECFailure;
  }
  memcpy(proto_out, that->next_proto_.data(), that->next_proto_.size());
  *proto_out_len = that->next_proto_.size();
  return SECSuccess;
}

void SSLClientSocketNSS::EnsureThreadIdAssigned() const {
  base::AutoLock auto_lock(lock_);
  if (valid_thread_id_ != base::kInvalidThreadId)
    return;
  valid_thread_id_ = base::PlatformThread::CurrentId();
}

bool SSLClientSocketNSS::CalledOnValidThread() const {
  EnsureThreadIdAssigned();
  base::AutoLock auto_lock(lock_);
  return valid_thread_id_ == base::PlatformThread::CurrentId();
}

ServerBoundCertService* SSLClientSocketNSS::GetServerBoundCertService() const {
  return server_bound_cert_service_;
}

}  // namespace net
