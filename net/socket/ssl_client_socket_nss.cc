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

#if defined(OS_LINUX) || defined(USE_SYSTEM_SSL)
#include <dlfcn.h>
#endif

#include <limits>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/nss_util.h"
#include "base/singleton.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "net/base/address_list.h"
#include "net/base/cert_status_flags.h"
#include "net/base/cert_verifier.h"
#include "net/base/connection_type_histograms.h"
#include "net/base/dns_util.h"
#include "net/base/dnsrr_resolver.h"
#include "net/base/dnssec_chain_verifier.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/ssl_cert_request_info.h"
#include "net/base/ssl_connection_status_flags.h"
#include "net/base/ssl_info.h"
#include "net/base/sys_addrinfo.h"
#include "net/ocsp/nss_ocsp.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/dns_cert_provenance_checker.h"
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
#endif

static const int kRecvBufferSize = 4096;

// kCorkTimeoutMs is the number of milliseconds for which we'll wait for a
// Write to an SSL socket which we're False Starting. Since corking stops the
// Finished message from being sent, the server sees an incomplete handshake
// and some will time out such sockets quite aggressively.
static const int kCorkTimeoutMs = 200;

typedef SECStatus
(*CacheOCSPResponseFromSideChannelFunction)(
    CERTCertDBHandle *handle, CERTCertificate *cert, PRTime time,
    SECItem *encodedResponse, void *pwArg);

#if defined(OS_LINUX)
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
#else
// On other platforms we use the system's certificate validation functions.
// Thus we need, in the future, to plumb the OCSP response into those system
// functions. Until then, we act as if we didn't support OCSP stapling.
static CacheOCSPResponseFromSideChannelFunction
GetCacheOCSPResponseFromSideChannelFunction() {
  return NULL;
}
#endif

namespace net {

// State machines are easier to debug if you log state transitions.
// Enable these if you want to see what's going on.
#if 1
#define EnterFunction(x)
#define LeaveFunction(x)
#define GotoState(s) next_handshake_state_ = s
#define LogData(s, len)
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
#define LogData(s, len)\
    VLOG(1) << (void *)this << " " << __FUNCTION__\
            << " data [" << std::string(s, len) << "]"
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
    SECStatus rv = SSL_PeerCertificateChain(nss_fd, NULL, &num_certs_);
    DCHECK_EQ(rv, SECSuccess);

    certs_ = new CERTCertificate*[num_certs_];
    const unsigned expected_num_certs = num_certs_;
    rv = SSL_PeerCertificateChain(nss_fd, certs_, &num_certs_);
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

// VerifyTXTRecords processes the RRDATA for a number of DNS TXT records and
// checks them against the given certificate.
//   dnssec: if true then the TXT records are DNSSEC validated. In this case,
//       DNSVR_SUCCESS may be returned.
//    server_cert_nss: the certificate to validate
//    rrdatas: the TXT records for the current domain.
DNSValidationResult VerifyTXTRecords(
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
DNSValidationResult CheckDNSSECChain(
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

  if (verifier.rrtype() != kDNS_TXT)
    return DNSVR_CONTINUE;

  DNSValidationResult r = VerifyTXTRecords(
      true /* DNSSEC verified */, server_cert_nss, verifier.rrdatas());
  SECITEM_FreeItem(&dnssec_embedded_chain, PR_FALSE);
  return r;
}

}  // namespace

SSLClientSocketNSS::SSLClientSocketNSS(ClientSocketHandle* transport_socket,
                                       const HostPortPair& host_and_port,
                                       const SSLConfig& ssl_config,
                                       SSLHostInfo* ssl_host_info,
                                       CertVerifier* cert_verifier,
                                       DnsCertProvenanceChecker* dns_ctx)
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
      host_and_port_(host_and_port),
      ssl_config_(ssl_config),
      user_connect_callback_(NULL),
      user_read_callback_(NULL),
      user_write_callback_(NULL),
      user_read_buf_len_(0),
      user_write_buf_len_(0),
      server_cert_nss_(NULL),
      server_cert_verify_result_(NULL),
      ssl_connection_status_(0),
      client_auth_cert_needed_(false),
      cert_verifier_(cert_verifier),
      handshake_callback_called_(false),
      completed_handshake_(false),
      pseudo_connected_(false),
      eset_mitm_detected_(false),
      predicted_cert_chain_correct_(false),
      peername_initialized_(false),
      dnssec_provider_(NULL),
      next_handshake_state_(STATE_NONE),
      nss_fd_(NULL),
      nss_bufs_(NULL),
      net_log_(transport_socket->socket()->NetLog()),
      predicted_npn_status_(kNextProtoUnsupported),
      predicted_npn_proto_used_(false),
      ssl_host_info_(ssl_host_info),
      dns_cert_checker_(dns_ctx) {
  EnterFunction("");
}

SSLClientSocketNSS::~SSLClientSocketNSS() {
  EnterFunction("");
  Disconnect();
  LeaveFunction("");
}

// static
void SSLClientSocketNSS::ClearSessionCache() {
  SSL_ClearSessionCache();
}

void SSLClientSocketNSS::GetSSLInfo(SSLInfo* ssl_info) {
  EnterFunction("");
  ssl_info->Reset();

  if (!server_cert_) {
    LOG(DFATAL) << "!server_cert_";
    return;
  }

  ssl_info->cert_status = server_cert_verify_result_->cert_status;
  DCHECK(server_cert_ != NULL);
  ssl_info->cert = server_cert_;
  ssl_info->connection_status = ssl_connection_status_;

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

SSLClientSocket::NextProtoStatus
SSLClientSocketNSS::GetNextProto(std::string* proto) {
#if defined(SSL_NEXT_PROTO_NEGOTIATED)
  if (!handshake_callback_called_) {
    DCHECK(pseudo_connected_);
    predicted_npn_proto_used_ = true;
    *proto = predicted_npn_proto_;
    return predicted_npn_status_;
  }

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

int SSLClientSocketNSS::Connect(CompletionCallback* callback) {
  EnterFunction("");
  DCHECK(transport_.get());
  DCHECK(next_handshake_state_ == STATE_NONE);
  DCHECK(!user_read_callback_);
  DCHECK(!user_write_callback_);
  DCHECK(!user_connect_callback_);
  DCHECK(!user_read_buf_);
  DCHECK(!user_write_buf_);
  DCHECK(!pseudo_connected_);

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

  // Attempt to initialize the peer name.  In the case of TCP FastOpen,
  // we don't have the peer yet.
  if (!UsingTCPFastOpen()) {
    rv = InitializeSSLPeerName();
    if (rv != OK) {
      net_log_.EndEvent(NetLog::TYPE_SSL_CONNECT, NULL);
      return rv;
    }
  }

  if (ssl_config_.snap_start_enabled && ssl_host_info_.get()) {
    GotoState(STATE_SNAP_START_LOAD_INFO);
  } else {
    GotoState(STATE_HANDSHAKE);
  }

  rv = DoHandshakeLoop(OK);
  if (rv == ERR_IO_PENDING) {
    if (pseudo_connected_) {
      net_log_.EndEvent(NetLog::TYPE_SSL_CONNECT, NULL);
      rv = OK;
    } else {
      user_connect_callback_ = callback;
    }
  } else {
    net_log_.EndEvent(NetLog::TYPE_SSL_CONNECT, NULL);
  }

  LeaveFunction("");
  return rv > OK ? OK : rv;
}

void SSLClientSocketNSS::Disconnect() {
  EnterFunction("");

  // TODO(wtc): Send SSL close_notify alert.
  if (nss_fd_ != NULL) {
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
  local_server_cert_verify_result_.Reset();
  server_cert_verify_result_ = NULL;
  ssl_connection_status_ = 0;
  completed_handshake_   = false;
  pseudo_connected_      = false;
  eset_mitm_detected_    = false;
  start_cert_verification_time_ = base::TimeTicks();
  predicted_cert_chain_correct_ = false;
  peername_initialized_  = false;
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
  bool ret = (pseudo_connected_ || completed_handshake_) &&
             transport_->socket()->IsConnected();
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
  bool ret = (pseudo_connected_ || completed_handshake_) &&
             transport_->socket()->IsConnectedAndIdle();
  LeaveFunction("");
  return ret;
}

int SSLClientSocketNSS::GetPeerAddress(AddressList* address) const {
  return transport_->socket()->GetPeerAddress(address);
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

int SSLClientSocketNSS::Read(IOBuffer* buf, int buf_len,
                             CompletionCallback* callback) {
  EnterFunction(buf_len);
  DCHECK(!user_read_callback_);
  DCHECK(!user_connect_callback_);
  DCHECK(!user_read_buf_);
  DCHECK(nss_bufs_);

  user_read_buf_ = buf;
  user_read_buf_len_ = buf_len;

  if (!completed_handshake_) {
    // In this case we have lied about being connected in order to merge the
    // first Write into a Snap Start handshake. We'll leave the read hanging
    // until the handshake has completed.
    DCHECK(pseudo_connected_);

    user_read_callback_ = callback;
    LeaveFunction(ERR_IO_PENDING);
    return ERR_IO_PENDING;
  }

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
  if (!pseudo_connected_) {
    DCHECK(completed_handshake_);
    DCHECK(next_handshake_state_ == STATE_NONE);
    DCHECK(!user_connect_callback_);
  }
  DCHECK(!user_write_callback_);
  DCHECK(!user_write_buf_);
  DCHECK(nss_bufs_);

  user_write_buf_ = buf;
  user_write_buf_len_ = buf_len;

  if (next_handshake_state_ == STATE_SNAP_START_WAIT_FOR_WRITE) {
    // We lied about being connected and we have been waiting for this write in
    // order to merge it into the Snap Start handshake. We'll leave the write
    // pending until the handshake completes.
    DCHECK(pseudo_connected_);
    int rv = DoHandshakeLoop(OK);
    if (rv == ERR_IO_PENDING) {
      user_write_callback_ = callback;
    } else {
      user_write_buf_ = NULL;
      user_write_buf_len_ = 0;
    }
    if (rv != OK)
      return rv;
  }

  if (corked_) {
    corked_ = false;
    uncork_timer_.Reset();
  }
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
  // We must call EnsureOCSPInit() here, on the IO thread, to get the IO loop
  // by MessageLoopForIO::current().
  // X509Certificate::Verify() runs on a worker thread of CertVerifier.
  EnsureOCSPInit();
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
  rv = SSL_OptionSet(
      nss_fd_, SSL_ENABLE_FALSE_START,
      ssl_config_.false_start_enabled &&
      !SSLConfigService::IsKnownFalseStartIncompatibleServer(
          host_and_port_.host()));
  if (rv != SECSuccess)
    LogFailedNSSFunction(net_log_, "SSL_OptionSet", "SSL_ENABLE_FALSE_START");
#endif

#ifdef SSL_ENABLE_SNAP_START
  // TODO(agl): check that SSL_ENABLE_SNAP_START actually does something in the
  // current NSS code.
  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_SNAP_START,
                     ssl_config_.snap_start_enabled);
  if (rv != SECSuccess)
    VLOG(1) << "SSL_ENABLE_SNAP_START failed.  Old system nss?";
#endif

#ifdef SSL_ENABLE_RENEGOTIATION
  // Deliberately disable this check for now: http://crbug.com/55410
  if (false &&
      SSLConfigService::IsKnownStrictTLSServer(host_and_port_.host()) &&
      !ssl_config_.mitm_proxies_allowed) {
    rv = SSL_OptionSet(nss_fd_, SSL_REQUIRE_SAFE_NEGOTIATION, PR_TRUE);
    if (rv != SECSuccess) {
      LogFailedNSSFunction(
          net_log_, "SSL_OptionSet", "SSL_REQUIRE_SAFE_NEGOTIATION");
    }
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
  if (rv != SECSuccess) {
    LogFailedNSSFunction(
        net_log_, "SSL_OptionSet", "SSL_ENABLE_RENEGOTIATION");
  }
#endif  // SSL_ENABLE_RENEGOTIATION

#ifdef SSL_NEXT_PROTO_NEGOTIATED
  if (!ssl_config_.next_protos.empty()) {
    rv = SSL_SetNextProtoNego(
       nss_fd_,
       reinterpret_cast<const unsigned char *>(ssl_config_.next_protos.data()),
       ssl_config_.next_protos.size());
    if (rv != SECSuccess)
      LogFailedNSSFunction(net_log_, "SSL_SetNextProtoNego", "");
  }
#endif

#ifdef SSL_ENABLE_OCSP_STAPLING
  if (GetCacheOCSPResponseFromSideChannelFunction() &&
      !ssl_config_.snap_start_enabled) {
    rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_OCSP_STAPLING, PR_TRUE);
    if (rv != SECSuccess)
      LogFailedNSSFunction(net_log_, "SSL_OptionSet (OCSP stapling)", "");
  }
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
  SSL_ResetHandshake(nss_fd_, 0);

  return OK;
}

int SSLClientSocketNSS::InitializeSSLPeerName() {
  // Tell NSS who we're connected to
  AddressList peer_address;
  int err = transport_->socket()->GetPeerAddress(&peer_address);
  if (err != OK)
    return err;

  const struct addrinfo* ai = peer_address.head();

  PRNetAddr peername;
  memset(&peername, 0, sizeof(peername));
  DCHECK_LE(ai->ai_addrlen, sizeof(peername));
  size_t len = std::min(static_cast<size_t>(ai->ai_addrlen),
                        sizeof(peername));
  memcpy(&peername, ai->ai_addr, len);

  // Adjust the address family field for BSD, whose sockaddr
  // structure has a one-byte length and one-byte address family
  // field at the beginning.  PRNetAddr has a two-byte address
  // family field at the beginning.
  peername.raw.family = ai->ai_addr->sa_family;

  memio_SetPeerName(nss_fd_, &peername);

  // Set the peer ID for session reuse.  This is necessary when we create an
  // SSL tunnel through a proxy -- GetPeerName returns the proxy's address
  // rather than the destination server's address in that case.
  std::string peer_id = host_and_port_.ToString();
  SECStatus rv = SSL_SetSockPeerID(nss_fd_, const_cast<char*>(peer_id.c_str()));
  if (rv != SECSuccess)
    LogFailedNSSFunction(net_log_, "SSL_SetSockPeerID", peer_id.c_str());

  peername_initialized_ = true;
  return OK;
}


// Sets server_cert_ and server_cert_nss_ if not yet set.
// Returns server_cert_.
X509Certificate *SSLClientSocketNSS::UpdateServerCert() {
  // We set the server_cert_ from HandshakeCallback().
  if (server_cert_ == NULL) {
    server_cert_nss_ = SSL_PeerCertificate(nss_fd_);
    if (server_cert_nss_) {
      PeerCertificateChain certs(nss_fd_);
      server_cert_ = X509Certificate::CreateFromDERCertChain(
          certs.AsStringPieceVector());
    }
  }
  return server_cert_;
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
    // If we pseudo connected for Snap Start, then we won't have a connect
    // callback.
    if (user_connect_callback_)
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
  // decrypted data or if we're waiting for the first write for Snap Start.
  if (!user_read_buf_ || !completed_handshake_) {
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
      case STATE_SNAP_START_LOAD_INFO:
        rv = DoSnapStartLoadInfo();
        break;
      case STATE_SNAP_START_WAIT_FOR_WRITE:
        rv = DoSnapStartWaitForWrite();
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
        LOG(DFATAL) << "unexpected state " << state;
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

int SSLClientSocketNSS::DoSnapStartLoadInfo() {
  EnterFunction("");
  int rv = ssl_host_info_->WaitForDataReady(&handshake_io_callback_);
  GotoState(STATE_HANDSHAKE);

  if (rv == OK) {
    if (ssl_host_info_->WaitForCertVerification(NULL) == OK) {
      if (LoadSnapStartInfo()) {
        pseudo_connected_ = true;
        GotoState(STATE_SNAP_START_WAIT_FOR_WRITE);
        if (user_connect_callback_)
          DoConnectCallback(OK);
      }
    } else if (!ssl_host_info_->state().server_hello.empty()) {
      // A non-empty ServerHello suggests that we would have tried a Snap Start
      // connection.
      base::TimeTicks now = base::TimeTicks::Now();
      const base::TimeDelta duration =
          now - ssl_host_info_->verification_start_time();
      UMA_HISTOGRAM_TIMES("Net.SSLSnapStartNeededVerificationInMs", duration);
      VLOG(1) << "Cannot snap start because verification isn't ready. "
              << "Wanted verification after "
              << duration.InMilliseconds() << "ms";
    }
  } else {
    DCHECK_EQ(ERR_IO_PENDING, rv);
    GotoState(STATE_SNAP_START_LOAD_INFO);
  }

  LeaveFunction("");
  return rv;
}

int SSLClientSocketNSS::DoSnapStartWaitForWrite() {
  EnterFunction("");
  // In this state, we're waiting for the first Write call so that we can merge
  // it into the Snap Start handshake.
  if (!user_write_buf_) {
    // We'll lie and say that we're connected in order that someone will call
    // Write.
    GotoState(STATE_SNAP_START_WAIT_FOR_WRITE);
    DCHECK(!user_connect_callback_);
    LeaveFunction("");
    return ERR_IO_PENDING;
  }

  // This is the largest Snap Start application data payload that we'll try to
  // use. A TCP client can only send three frames of data without an ACK and,
  // at 2048 bytes, this leaves some space for the rest of the ClientHello
  // (including possible session ticket).
  static const int kMaxSnapStartPayloadSize = 2048;

  if (user_write_buf_len_ > kMaxSnapStartPayloadSize) {
    user_write_buf_len_ = kMaxSnapStartPayloadSize;
    // When we complete the handshake and call user_write_callback_ we'll say
    // that we only wrote |kMaxSnapStartPayloadSize| bytes. That way the rest
    // of the payload will be presented to |Write| again and transmitted as
    // normal application data.
  }

  SECStatus rv = SSL_SetSnapStartApplicationData(
      nss_fd_, reinterpret_cast<const unsigned char*>(user_write_buf_->data()),
      user_write_buf_len_);
  DCHECK_EQ(SECSuccess, rv);

  GotoState(STATE_HANDSHAKE);
  LeaveFunction("");
  return OK;
}

int SSLClientSocketNSS::DoHandshake() {
  EnterFunction("");
  int net_error = net::OK;
  SECStatus rv = SSL_ForceHandshake(nss_fd_);

  if (client_auth_cert_needed_) {
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
  } else if (rv == SECSuccess) {
    if (handshake_callback_called_) {
      if (eset_mitm_detected_) {
        net_error = ERR_ESET_ANTI_VIRUS_SSL_INTERCEPTION;
      } else {
        // We need to see if the predicted certificate chain (in
        // |ssl_host_info_->state().certs) matches the actual certificate chain
        // before we call SaveSnapStartInfo, as that will update
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
        const CacheOCSPResponseFromSideChannelFunction cache_ocsp_response =
            GetCacheOCSPResponseFromSideChannelFunction();
        // TODO: we need to be able to plumb an OCSP response into the system
        // libraries. When we do, GetOCSPResponseFromSideChannelFunction
        // needs to be updated for those platforms.
        if (!predicted_cert_chain_correct_ && cache_ocsp_response) {
          unsigned int len = 0;
          SSL_GetStapledOCSPResponse(nss_fd_, NULL, &len);
          if (len) {
            const unsigned int orig_len = len;
            scoped_array<uint8> ocsp_response(new uint8[orig_len]);
            SSL_GetStapledOCSPResponse(nss_fd_, ocsp_response.get(), &len);
            DCHECK_EQ(orig_len, len);

            SECItem ocsp_response_item;
            ocsp_response_item.type = siBuffer;
            ocsp_response_item.data = ocsp_response.get();
            ocsp_response_item.len = len;

            cache_ocsp_response(
                CERT_GetDefaultCertDB(), server_cert_nss_, PR_Now(),
                &ocsp_response_item, NULL);
          }
        }
#endif

        SaveSnapStartInfo();
        // SSL handshake is completed. It's possible that we mispredicted the
        // NPN agreed protocol. In this case, we've just sent a request in the
        // wrong protocol! The higher levels of this network stack aren't
        // prepared for switching the protocol like that so we make up an error
        // and rely on the fact that the request will be retried.
        if (IsNPNProtocolMispredicted()) {
          LOG(WARNING) << "Mispredicted NPN protocol for "
                       << host_and_port_.ToString();
          net_error = ERR_SSL_SNAP_START_NPN_MISPREDICTION;
        } else {
          // Let's verify the certificate.
          GotoState(STATE_VERIFY_DNSSEC);
        }
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
    net_error = MapNSSHandshakeError(prerr);

    // If not done, stay in this state
    if (net_error == ERR_IO_PENDING) {
      GotoState(STATE_HANDSHAKE);
    } else {
      LOG(ERROR) << "handshake failed; NSS error code " << prerr
                 << ", net_error " << net_error;
      net_log_.AddEvent(
          NetLog::TYPE_SSL_HANDSHAKE_ERROR,
          make_scoped_refptr(new SSLErrorParams(net_error, prerr)));
    }
  }

  LeaveFunction("");
  return net_error;
}

int SSLClientSocketNSS::DoVerifyDNSSEC(int result) {
  if (ssl_config_.dns_cert_provenance_checking_enabled &&
      dns_cert_checker_) {
    PeerCertificateChain certs(nss_fd_);
    dns_cert_checker_->DoAsyncVerification(
        host_and_port_.host(), certs.AsStringPieceVector());
  }

  if (ssl_config_.dnssec_enabled) {
    DNSValidationResult r = CheckDNSSECChain(host_and_port_.host(),
                                             server_cert_nss_);
    if (r == DNSVR_SUCCESS) {
      local_server_cert_verify_result_.cert_status |= CERT_STATUS_IS_DNSSEC;
      server_cert_verify_result_ = &local_server_cert_verify_result_;
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
    return OK;
  }

  switch (r) {
    case DNSVR_FAILURE:
      GotoState(STATE_VERIFY_CERT_COMPLETE);
      local_server_cert_verify_result_.cert_status |= CERT_STATUS_NOT_IN_DNS;
      server_cert_verify_result_ = &local_server_cert_verify_result_;
      return ERR_CERT_NOT_IN_DNS;
    case DNSVR_CONTINUE:
      GotoState(STATE_VERIFY_CERT);
      break;
    case DNSVR_SUCCESS:
      local_server_cert_verify_result_.cert_status |= CERT_STATUS_IS_DNSSEC;
      server_cert_verify_result_ = &local_server_cert_verify_result_;
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
    return ssl_host_info_->WaitForCertVerification(&handshake_io_callback_);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Net.SSLVerificationMerged", 0 /* false */, 2);
  }

  int flags = 0;
  if (ssl_config_.rev_checking_enabled)
    flags |= X509Certificate::VERIFY_REV_CHECKING_ENABLED;
  if (ssl_config_.verify_ev_cert)
    flags |= X509Certificate::VERIFY_EV_CERT;
  verifier_.reset(new SingleRequestCertVerifier(cert_verifier_));
  server_cert_verify_result_ = &local_server_cert_verify_result_;
  return verifier_->Verify(server_cert_, host_and_port_.host(), flags,
                           &local_server_cert_verify_result_,
                           &handshake_io_callback_);
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

  if (ssl_host_info_.get())
    ssl_host_info_->set_cert_verification_finished_time();

  // We used to remember the intermediate CA certs in the NSS database
  // persistently.  However, NSS opens a connection to the SQLite database
  // during NSS initialization and doesn't close the connection until NSS
  // shuts down.  If the file system where the database resides is gone,
  // the database connection goes bad.  What's worse, the connection won't
  // recover when the file system comes back.  Until this NSS or SQLite bug
  // is fixed, we need to  avoid using the NSS database for non-essential
  // purposes.  See https://bugzilla.mozilla.org/show_bug.cgi?id=508081 and
  // http://crbug.com/15630 for more info.

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

  if (result == OK)
    LogConnectionTypeMetrics();

  completed_handshake_ = true;

  // If we merged a Write call into the handshake we need to make the
  // callback now.
  if (user_write_callback_) {
    corked_ = false;
    if (result != OK) {
      DoWriteCallback(result);
    } else {
      SSLSnapStartResult snap_start_type;
      SECStatus rv = SSL_GetSnapStartResult(nss_fd_, &snap_start_type);
      DCHECK_EQ(rv, SECSuccess);
      DCHECK_NE(snap_start_type, SSL_SNAP_START_NONE);
      if (snap_start_type == SSL_SNAP_START_RECOVERY ||
          snap_start_type == SSL_SNAP_START_RESUME_RECOVERY) {
        // If we mispredicted the server's handshake then Snap Start will have
        // triggered a recovery mode. The misprediction could have been caused
        // by the server having a different certificate so the application data
        // wasn't resent. Now that we have verified the certificate, we need to
        // resend the application data.
        int bytes_written = DoPayloadWrite();
        if (bytes_written != ERR_IO_PENDING)
          DoWriteCallback(bytes_written);
      } else {
        DoWriteCallback(user_write_buf_len_);
      }
    }
  }

  if (user_read_callback_) {
    int rv = DoReadLoop(OK);
    if (rv != ERR_IO_PENDING)
      DoReadCallback(rv);
  }

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
  rv = MapNSSError(prerr);
  net_log_.AddEvent(NetLog::TYPE_SSL_READ_ERROR,
                    make_scoped_refptr(new SSLErrorParams(rv, prerr)));
  return rv;
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
  rv = MapNSSError(prerr);
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

// SaveSnapStartInfo extracts the information needed to perform a Snap Start
// with this server in the future (if any) and tells |ssl_host_info_| to
// preserve it.
void SSLClientSocketNSS::SaveSnapStartInfo() {
  if (!ssl_host_info_.get())
    return;

  // If the SSLHostInfo hasn't managed to load from disk yet then we can't save
  // anything.
  if (ssl_host_info_->WaitForDataReady(NULL) != OK)
    return;

  SECStatus rv;
  SSLSnapStartResult snap_start_type;
  rv = SSL_GetSnapStartResult(nss_fd_, &snap_start_type);
  if (rv != SECSuccess) {
    NOTREACHED();
    return;
  }
  net_log_.AddEvent(NetLog::TYPE_SSL_SNAP_START,
                    new NetLogIntegerParameter("type", snap_start_type));
  if (snap_start_type == SSL_SNAP_START_FULL ||
      snap_start_type == SSL_SNAP_START_RESUME) {
    // If we did a successful Snap Start then our information was correct and
    // there's no point saving it again.
    return;
  }

  const unsigned char* hello_data;
  unsigned hello_data_len;
  rv = SSL_GetPredictedServerHelloData(nss_fd_, &hello_data, &hello_data_len);
  if (rv != SECSuccess) {
    NOTREACHED();
    return;
  }
  if (hello_data_len > std::numeric_limits<uint16>::max())
    return;
  SSLHostInfo::State* state = ssl_host_info_->mutable_state();

  if (hello_data_len > 0) {
    state->server_hello =
        std::string(reinterpret_cast<const char *>(hello_data), hello_data_len);
    state->npn_valid = true;
    state->npn_status = GetNextProto(&state->npn_protocol);
  } else {
    state->server_hello.clear();
    state->npn_valid = false;
  }

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

// LoadSnapStartInfo parses |info|, which contains data previously serialised
// by |SaveSnapStartInfo|, and sets the predicted certificates and ServerHello
// data on the NSS socket. Returns true on success. If this function returns
// false, the caller should try a normal TLS handshake.
bool SSLClientSocketNSS::LoadSnapStartInfo() {
  const SSLHostInfo::State& state(ssl_host_info_->state());

  if (state.server_hello.empty() ||
      state.certs.empty() ||
      !state.npn_valid) {
    return false;
  }

  SECStatus rv;
  rv = SSL_SetPredictedServerHelloData(
      nss_fd_,
      reinterpret_cast<const uint8*>(state.server_hello.data()),
      state.server_hello.size());
  DCHECK_EQ(SECSuccess, rv);

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

  rv = SSL_SetPredictedPeerCertificates(nss_fd_, certs.get(), certs_in.size());
  DestroyCertificates(&certs[0], certs_in.size());
  DCHECK_EQ(SECSuccess, rv);

  if (state.npn_valid) {
    predicted_npn_status_ = state.npn_status;
    predicted_npn_proto_ = state.npn_protocol;
  }

  return true;
}

bool SSLClientSocketNSS::IsNPNProtocolMispredicted() {
  DCHECK(handshake_callback_called_);
  if (!predicted_npn_proto_used_)
    return false;
  std::string npn_proto;
  GetNextProto(&npn_proto);
  return predicted_npn_proto_ != npn_proto;
}

void SSLClientSocketNSS::UncorkAfterTimeout() {
  corked_ = false;
  int nsent;
  do {
    nsent = BufferSend();
  } while (nsent > 0);
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
    scoped_refptr<IOBuffer> send_buffer(new IOBuffer(len));
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

  // In the case of TCP FastOpen, connect is now finished.
  if (!peername_initialized_ && UsingTCPFastOpen())
    InitializeSSLPeerName();

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
  DCHECK_EQ(SECSuccess, rv);

  if (false_start) {
    SSLClientSocketNSS* that = reinterpret_cast<SSLClientSocketNSS*>(arg);

    // ESET anti-virus is capable of intercepting HTTPS connections on Windows.
    // However, it is False Start intolerant and causes the connections to hang
    // forever. We detect ESET by the issuer of the leaf certificate and set a
    // flag to return a specific error, giving the user instructions for
    // reconfiguring ESET.
    CERTCertificate* cert = SSL_PeerCertificate(that->nss_fd_);
    if (cert) {
      char* common_name = CERT_GetCommonName(&cert->issuer);
      if (common_name) {
        if (strcmp(common_name, "ESET_RootSslCert") == 0)
          that->eset_mitm_detected_ = true;
        if (strcmp(common_name,
                   "ContentWatch Root Certificate Authority") == 0) {
          // This is NetNanny. NetNanny are updating their product so we
          // silently disable False Start for now.
          rv = SSL_OptionSet(socket, SSL_ENABLE_FALSE_START, PR_FALSE);
          DCHECK_EQ(SECSuccess, rv);
          false_start = 0;
        }
        PORT_Free(common_name);
      }
      CERT_DestroyCertificate(cert);
    }

    if (false_start && !that->handshake_callback_called_) {
      that->corked_ = true;
      that->uncork_timer_.Start(
          base::TimeDelta::FromMilliseconds(kCorkTimeoutMs),
          that, &SSLClientSocketNSS::UncorkAfterTimeout);
    }
  }
#endif

  // Tell NSS to not verify the certificate.
  return SECSuccess;
}

#if defined(NSS_PLATFORM_CLIENT_AUTH)
// static
// NSS calls this if a client certificate is needed.
SECStatus SSLClientSocketNSS::PlatformClientAuthHandler(
    void* arg,
    PRFileDesc* socket,
    CERTDistNames* ca_names,
    CERTCertList** result_certs,
    void** result_private_key) {
  SSLClientSocketNSS* that = reinterpret_cast<SSLClientSocketNSS*>(arg);

  that->client_auth_cert_needed_ = !that->ssl_config_.send_client_cert;
#if defined(OS_WIN)
  if (that->ssl_config_.send_client_cert) {
    if (that->ssl_config_.client_cert) {
      PCCERT_CONTEXT cert_context =
          that->ssl_config_.client_cert->os_cert_handle();
      if (VLOG_IS_ON(1)) {
        do {
          DWORD size_needed = 0;
          BOOL got_info = CertGetCertificateContextProperty(
              cert_context, CERT_KEY_PROV_INFO_PROP_ID, NULL, &size_needed);
          if (!got_info) {
            VLOG(1) << "Failed to get key prov info size " << GetLastError();
            break;
          }
          std::vector<BYTE> raw_info(size_needed);
          got_info = CertGetCertificateContextProperty(
              cert_context, CERT_KEY_PROV_INFO_PROP_ID, &raw_info[0],
              &size_needed);
          if (!got_info) {
            VLOG(1) << "Failed to get key prov info " << GetLastError();
            break;
          }
          PCRYPT_KEY_PROV_INFO info =
              reinterpret_cast<PCRYPT_KEY_PROV_INFO>(&raw_info[0]);
          VLOG(1) << "Container Name: " << info->pwszContainerName
                  << "\nProvider Name: " << info->pwszProvName
                  << "\nProvider Type: " << info->dwProvType
                  << "\nFlags: " << info->dwFlags
                  << "\nProvider Param Count: " << info->cProvParam
                  << "\nKey Specifier: " << info->dwKeySpec;
        } while (false);

        do {
          DWORD size_needed = 0;
          BOOL got_identifier = CertGetCertificateContextProperty(
              cert_context, CERT_KEY_IDENTIFIER_PROP_ID, NULL, &size_needed);
          if (!got_identifier) {
            VLOG(1) << "Failed to get key identifier size "
                    << GetLastError();
            break;
          }
          std::vector<BYTE> raw_id(size_needed);
          got_identifier = CertGetCertificateContextProperty(
              cert_context, CERT_KEY_IDENTIFIER_PROP_ID, &raw_id[0],
              &size_needed);
          if (!got_identifier) {
            VLOG(1) << "Failed to get key identifier " << GetLastError();
            break;
          }
          VLOG(1) << "Key Identifier: " << base::HexEncode(&raw_id[0],
                                                           size_needed);
        } while (false);
      }
      HCRYPTPROV provider = NULL;
      DWORD key_spec = AT_KEYEXCHANGE;
      BOOL must_free = FALSE;
      BOOL acquired_key = CryptAcquireCertificatePrivateKey(
          cert_context,
          CRYPT_ACQUIRE_CACHE_FLAG | CRYPT_ACQUIRE_COMPARE_KEY_FLAG,
          NULL, &provider, &key_spec, &must_free);
      if (acquired_key && provider) {
        DCHECK_NE(key_spec, CERT_NCRYPT_KEY_SPEC);

        // The certificate cache may have been updated/used, in which case,
        // duplicate the existing handle, since NSS will free it when no
        // longer in use.
        if (!must_free)
          CryptContextAddRef(provider, NULL, 0);

        SECItem der_cert;
        der_cert.type = siDERCertBuffer;
        der_cert.data = cert_context->pbCertEncoded;
        der_cert.len  = cert_context->cbCertEncoded;

        // TODO(rsleevi): Error checking for NSS allocation errors.
        *result_certs = CERT_NewCertList();
        CERTCertDBHandle* db_handle = CERT_GetDefaultCertDB();
        CERTCertificate* user_cert = CERT_NewTempCertificate(
            db_handle, &der_cert, NULL, PR_FALSE, PR_TRUE);
        CERT_AddCertToListTail(*result_certs, user_cert);

        // Add the intermediates.
        X509Certificate::OSCertHandles intermediates =
            that->ssl_config_.client_cert->GetIntermediateCertificates();
        for (X509Certificate::OSCertHandles::const_iterator it =
            intermediates.begin(); it != intermediates.end(); ++it) {
          der_cert.data = (*it)->pbCertEncoded;
          der_cert.len = (*it)->cbCertEncoded;

          CERTCertificate* intermediate = CERT_NewTempCertificate(
              db_handle, &der_cert, NULL, PR_FALSE, PR_TRUE);
          CERT_AddCertToListTail(*result_certs, intermediate);
        }
        // TODO(wtc): |key_spec| should be passed along with |provider|.
        *result_private_key = reinterpret_cast<void*>(provider);
        return SECSuccess;
      }
      LOG(WARNING) << "Client cert found without private key";
    }
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
    BOOL ok = CertAddCertificateContextToStore(X509Certificate::cert_store(),
                                               cert_context,
                                               CERT_STORE_ADD_USE_EXISTING,
                                               &cert_context2);
    if (!ok) {
      NOTREACHED();
      continue;
    }

    // Copy the rest of the chain to our own store as well. Copying the chain
    // stops gracefully if an error is encountered, with the partial chain
    // being used as the intermediates, rather than failing to consider the
    // client certificate.
    net::X509Certificate::OSCertHandles intermediates;
    for (DWORD i = 1; i < chain_context->rgpChain[0]->cElement; i++) {
      PCCERT_CONTEXT intermediate_copy;
      ok = CertAddCertificateContextToStore(X509Certificate::cert_store(),
          chain_context->rgpChain[0]->rgpElement[i]->pCertContext,
          CERT_STORE_ADD_USE_EXISTING, &intermediate_copy);
      if (!ok) {
        NOTREACHED();
        break;
      }
      intermediates.push_back(intermediate_copy);
    }

    scoped_refptr<X509Certificate> cert = X509Certificate::CreateFromHandle(
        cert_context2, X509Certificate::SOURCE_LONE_CERT_IMPORT,
        intermediates);
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
        *result_private_key = reinterpret_cast<void*>(private_key);

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
          CERT_AddCertToListTail(*result_certs, nss_cert);
        }
      }
      if (os_error == noErr) {
        CFRelease(chain);
        return SECSuccess;
      }
      LOG(WARNING) << "Client cert found, but could not be used: "
                   << os_error;
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

}  // namespace net
