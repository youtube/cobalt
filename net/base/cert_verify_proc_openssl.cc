// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cert_verify_proc_openssl.h"

#include <openssl/x509v3.h>

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/sha1.h"
#include "crypto/openssl_util.h"
#include "crypto/sha2.h"
#include "net/base/asn1_util.h"
#include "net/base/cert_status_flags.h"
#include "net/base/cert_verifier.h"
#include "net/base/cert_verify_result.h"
#include "net/base/net_errors.h"
#include "net/base/x509_certificate.h"

#if defined(OS_ANDROID)
#include "net/android/network_library.h"
#endif

namespace net {

namespace {

// Maps X509_STORE_CTX_get_error() return values to our cert status flags.
CertStatus MapCertErrorToCertStatus(int err) {
  switch (err) {
    case X509_V_ERR_SUBJECT_ISSUER_MISMATCH:
      return CERT_STATUS_COMMON_NAME_INVALID;
    case X509_V_ERR_CERT_NOT_YET_VALID:
    case X509_V_ERR_CERT_HAS_EXPIRED:
    case X509_V_ERR_CRL_NOT_YET_VALID:
    case X509_V_ERR_CRL_HAS_EXPIRED:
    case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
    case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
    case X509_V_ERR_ERROR_IN_CRL_LAST_UPDATE_FIELD:
    case X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD:
      return CERT_STATUS_DATE_INVALID;
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
    case X509_V_ERR_UNABLE_TO_GET_CRL:
    case X509_V_ERR_INVALID_CA:
    case X509_V_ERR_UNABLE_TO_GET_CRL_ISSUER:
    case X509_V_ERR_INVALID_NON_CA:
    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
      return CERT_STATUS_AUTHORITY_INVALID;
#if 0
// TODO(bulach): what should we map to these status?
      return CERT_STATUS_NO_REVOCATION_MECHANISM;
      return CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
#endif
    case X509_V_ERR_CERT_REVOKED:
      return CERT_STATUS_REVOKED;
    // All these status are mapped to CERT_STATUS_INVALID.
    case X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE:
    case X509_V_ERR_UNABLE_TO_DECRYPT_CRL_SIGNATURE:
    case X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY:
    case X509_V_ERR_CERT_SIGNATURE_FAILURE:
    case X509_V_ERR_CRL_SIGNATURE_FAILURE:
    case X509_V_ERR_OUT_OF_MEM:
    case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
    case X509_V_ERR_CERT_CHAIN_TOO_LONG:
    case X509_V_ERR_PATH_LENGTH_EXCEEDED:
    case X509_V_ERR_INVALID_PURPOSE:
    case X509_V_ERR_CERT_UNTRUSTED:
    case X509_V_ERR_CERT_REJECTED:
    case X509_V_ERR_AKID_SKID_MISMATCH:
    case X509_V_ERR_AKID_ISSUER_SERIAL_MISMATCH:
    case X509_V_ERR_UNHANDLED_CRITICAL_EXTENSION:
    case X509_V_ERR_KEYUSAGE_NO_CERTSIGN:
    case X509_V_ERR_KEYUSAGE_NO_CRL_SIGN:
    case X509_V_ERR_UNHANDLED_CRITICAL_CRL_EXTENSION:
    case X509_V_ERR_PROXY_PATH_LENGTH_EXCEEDED:
    case X509_V_ERR_KEYUSAGE_NO_DIGITAL_SIGNATURE:
    case X509_V_ERR_PROXY_CERTIFICATES_NOT_ALLOWED:
    case X509_V_ERR_INVALID_EXTENSION:
    case X509_V_ERR_INVALID_POLICY_EXTENSION:
    case X509_V_ERR_NO_EXPLICIT_POLICY:
    case X509_V_ERR_UNNESTED_RESOURCE:
    case X509_V_ERR_APPLICATION_VERIFICATION:
      return CERT_STATUS_INVALID;
    default:
      NOTREACHED() << "Invalid X509 err " << err;
      return CERT_STATUS_INVALID;
  }
}

// sk_X509_free is a function-style macro, so can't be used as a template
// param directly.
void sk_X509_free_fn(STACK_OF(X509)* st) {
  sk_X509_free(st);
}

void GetCertChainInfo(X509_STORE_CTX* store_ctx,
                      CertVerifyResult* verify_result) {
  STACK_OF(X509)* chain = X509_STORE_CTX_get_chain(store_ctx);
  X509* verified_cert = NULL;
  std::vector<X509*> verified_chain;
  for (int i = 0; i < sk_X509_num(chain); ++i) {
    X509* cert = sk_X509_value(chain, i);
    if (i == 0) {
      verified_cert = cert;
    } else {
      verified_chain.push_back(cert);
    }

    // Only check the algorithm status for certificates that are not in the
    // trust store.
    if (i < store_ctx->last_untrusted) {
      int sig_alg = OBJ_obj2nid(cert->sig_alg->algorithm);
      if (sig_alg == NID_md2WithRSAEncryption) {
        verify_result->has_md2 = true;
        if (i != 0)
          verify_result->has_md2_ca = true;
      } else if (sig_alg == NID_md4WithRSAEncryption) {
        verify_result->has_md4 = true;
      } else if (sig_alg == NID_md5WithRSAEncryption) {
        verify_result->has_md5 = true;
        if (i != 0)
          verify_result->has_md5_ca = true;
      }
    }
  }

  if (verified_cert) {
    verify_result->verified_cert =
        X509Certificate::CreateFromHandle(verified_cert, verified_chain);
  }
}

void AppendPublicKeyHashes(X509_STORE_CTX* store_ctx,
                           HashValueVector* hashes) {
  STACK_OF(X509)* chain = X509_STORE_CTX_get_chain(store_ctx);
  for (int i = 0; i < sk_X509_num(chain); ++i) {
    X509* cert = sk_X509_value(chain, i);

    std::string der_data;
    if (!X509Certificate::GetDEREncoded(cert, &der_data))
      continue;

    base::StringPiece der_bytes(der_data);
    base::StringPiece spki_bytes;
    if (!asn1::ExtractSPKIFromDERCert(der_bytes, &spki_bytes))
      continue;

    HashValue sha1(HASH_VALUE_SHA1);
    base::SHA1HashBytes(reinterpret_cast<const uint8*>(spki_bytes.data()),
                        spki_bytes.size(), sha1.data());
    hashes->push_back(sha1);

    HashValue sha256(HASH_VALUE_SHA256);
    crypto::SHA256HashString(spki_bytes, sha1.data(), crypto::kSHA256Length);
    hashes->push_back(sha256);
  }
}

#if defined(OS_ANDROID)
// Returns true if we have verification result in |verify_result| from Android
// Trust Manager. Otherwise returns false.
bool VerifyFromAndroidTrustManager(const std::vector<std::string>& cert_bytes,
                                   CertVerifyResult* verify_result) {
  // TODO(joth): Fetch the authentication type from SSL rather than hardcode.
  bool verified = true;
  android::VerifyResult result =
      android::VerifyX509CertChain(cert_bytes, "RSA");
  switch (result) {
    case android::VERIFY_OK:
      break;
    case android::VERIFY_NO_TRUSTED_ROOT:
      verify_result->cert_status |= CERT_STATUS_AUTHORITY_INVALID;
      break;
    case android::VERIFY_INVOCATION_ERROR:
      verified = false;
      break;
    default:
      verify_result->cert_status |= CERT_STATUS_INVALID;
      break;
  }
  return verified;
}

void GetChainDEREncodedBytes(X509Certificate* cert,
                             std::vector<std::string>* chain_bytes) {
  X509Certificate::OSCertHandle cert_handle = cert->os_cert_handle();
  X509Certificate::OSCertHandles cert_handles =
      cert->GetIntermediateCertificates();

  // Make sure the peer's own cert is the first in the chain, if it's not
  // already there.
  if (cert_handles.empty() || cert_handles[0] != cert_handle)
    cert_handles.insert(cert_handles.begin(), cert_handle);

  chain_bytes->reserve(cert_handles.size());
  for (X509Certificate::OSCertHandles::const_iterator it =
       cert_handles.begin(); it != cert_handles.end(); ++it) {
    std::string cert_bytes;
    X509Certificate::X509Certificate::GetDEREncoded(*it, &cert_bytes);
    chain_bytes->push_back(cert_bytes);
  }
}
#endif  // defined(OS_ANDROID)

}  // namespace

CertVerifyProcOpenSSL::CertVerifyProcOpenSSL() {}

CertVerifyProcOpenSSL::~CertVerifyProcOpenSSL() {}

int CertVerifyProcOpenSSL::VerifyInternal(X509Certificate* cert,
                                          const std::string& hostname,
                                          int flags,
                                          CRLSet* crl_set,
                                          CertVerifyResult* verify_result) {
  crypto::EnsureOpenSSLInit();

  if (!cert->VerifyNameMatch(hostname))
    verify_result->cert_status |= CERT_STATUS_COMMON_NAME_INVALID;

  bool verify_attempted = false;

#if defined(OS_ANDROID)
  std::vector<std::string> cert_bytes;
  GetChainDEREncodedBytes(cert, &cert_bytes);

  verify_attempted = VerifyFromAndroidTrustManager(cert_bytes, verify_result);
#endif

  if (verify_attempted) {
    if (IsCertStatusError(verify_result->cert_status))
      return MapCertStatusToNetError(verify_result->cert_status);
  } else {
    crypto::ScopedOpenSSL<X509_STORE_CTX, X509_STORE_CTX_free> ctx(
        X509_STORE_CTX_new());

    crypto::ScopedOpenSSL<STACK_OF(X509), sk_X509_free_fn> intermediates(
        sk_X509_new_null());
    if (!intermediates.get())
      return ERR_OUT_OF_MEMORY;

    const X509Certificate::OSCertHandles& os_intermediates =
        cert->GetIntermediateCertificates();
    for (X509Certificate::OSCertHandles::const_iterator it =
         os_intermediates.begin(); it != os_intermediates.end(); ++it) {
      if (!sk_X509_push(intermediates.get(), *it))
        return ERR_OUT_OF_MEMORY;
    }
    int rv = X509_STORE_CTX_init(ctx.get(), X509Certificate::cert_store(),
                                 cert->os_cert_handle(), intermediates.get());
    CHECK_EQ(1, rv);

    if (X509_verify_cert(ctx.get()) != 1) {
      int x509_error = X509_STORE_CTX_get_error(ctx.get());
      CertStatus cert_status = MapCertErrorToCertStatus(x509_error);
      LOG(ERROR) << "X509 Verification error "
          << X509_verify_cert_error_string(x509_error)
          << " : " << x509_error
          << " : " << X509_STORE_CTX_get_error_depth(ctx.get())
          << " : " << cert_status;
      verify_result->cert_status |= cert_status;
    }

    GetCertChainInfo(ctx.get(), verify_result);
    if (IsCertStatusError(verify_result->cert_status))
      return MapCertStatusToNetError(verify_result->cert_status);
    AppendPublicKeyHashes(ctx.get(), &verify_result->public_key_hashes);
  }

  // Currently we only ues OpenSSL's default root CA paths, so treat all
  // correctly verified certs as being from a known root.
  // TODO(joth): if the motivations described in
  // http://src.chromium.org/viewvc/chrome?view=rev&revision=80778 become an
  // issue on OpenSSL builds, we will need to embed a hardcoded list of well
  // known root CAs, as per the _mac and _win versions.
  verify_result->is_issued_by_known_root = true;

  return OK;
}

}  // namespace net
