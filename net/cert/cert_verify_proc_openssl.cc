// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc_openssl.h"

#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "third_party/boringssl/src/include/openssl/x509_vfy.h"
#include "third_party/boringssl/src/include/openssl/x509v3.h"

#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/sha1.h"
#include "crypto/openssl_util.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"

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

template <typename T, void (*destructor)(T*)>
class ScopedOpenSSL {
 public:
  ScopedOpenSSL() : ptr_(NULL) {}
  explicit ScopedOpenSSL(T* ptr) : ptr_(ptr) {}
  ~ScopedOpenSSL() { reset(NULL); }

  T* get() const { return ptr_; }
  void reset(T* ptr) {
    if (ptr != ptr_) {
      if (ptr_)
        (*destructor)(ptr_);
      ptr_ = ptr;
    }
  }

 private:
  T* ptr_;

  DISALLOW_COPY_AND_ASSIGN(ScopedOpenSSL);
};

// x509_to_buffer returns a |CRYPTO_BUFFER| that contains the serialised
// contents of |x509|.
static bssl::UniquePtr<CRYPTO_BUFFER> x509_to_buffer(X509* x509) {
  uint8_t* buf = NULL;
  int cert_len = i2d_X509(x509, &buf);
  if (cert_len <= 0) {
    return 0;
  }

  bssl::UniquePtr<CRYPTO_BUFFER> buffer(CRYPTO_BUFFER_new(buf, cert_len, NULL));
  OPENSSL_free(buf);

  return buffer;
}

struct DERCache {
  unsigned char* data;
  int data_length;
};

void DERCache_free(void* parent,
                   void* ptr,
                   CRYPTO_EX_DATA* ad,
                   int idx,
                   long argl,
                   void* argp) {
  DERCache* der_cache = static_cast<DERCache*>(ptr);
  if (!der_cache)
    return;
  if (der_cache->data)
    OPENSSL_free(der_cache->data);
  OPENSSL_free(der_cache);
}

class X509InitSingleton {
 public:
  static X509InitSingleton* GetInstance() {
    // We allow the X509 store to leak, because it is used from a non-joinable
    // worker that is not stopped on shutdown, hence may still be using
    // OpenSSL library after the AtExit runner has completed.
    return base::Singleton<X509InitSingleton, base::LeakySingletonTraits<
                                                  X509InitSingleton>>::get();
  }
  int der_cache_ex_index() const { return der_cache_ex_index_; }
  X509_STORE* store() const { return store_.get(); }

  X509InitSingleton() {
    crypto::EnsureOpenSSLInit();
    der_cache_ex_index_ = X509_get_ex_new_index(0, 0, 0, 0, DERCache_free);
    DCHECK_NE(der_cache_ex_index_, -1);
    ResetCertStore();
  }

  void ResetCertStore() {
    store_.reset(X509_STORE_new());
    DCHECK(store_.get());
    // Configure the SSL certs dir. We don't implement getenv() or hardcode
    // the SSL_CERTS_DIR, which are the default methods OpenSSL uses to find
    // the certs path.
    base::FilePath cert_path;
    base::PathService::Get(base::DIR_EXE, &cert_path);
    cert_path = cert_path.Append("ssl").Append("certs");
    X509_STORE_load_locations(store_.get(), NULL, cert_path.value().c_str());
  }

 private:
  int der_cache_ex_index_;
  ScopedOpenSSL<X509_STORE, X509_STORE_free> store_;

  DISALLOW_COPY_AND_ASSIGN(X509InitSingleton);
};

// Takes ownership of |data| (which must have been allocated by OpenSSL).
DERCache* SetDERCache(X509* cert,
                      int x509_der_cache_index,
                      unsigned char* data,
                      int data_length) {
  DERCache* internal_cache =
      static_cast<DERCache*>(OPENSSL_malloc(sizeof(*internal_cache)));
  if (!internal_cache) {
    // We took ownership of |data|, so we must free if we can't add it to
    // |cert|.
    OPENSSL_free(data);
    return NULL;
  }
  internal_cache->data = data;
  internal_cache->data_length = data_length;
  X509_set_ex_data(cert, x509_der_cache_index, internal_cache);
  return internal_cache;
}

// Returns true if |der_cache| points to valid data, false otherwise.
// (note: the DER-encoded data in |der_cache| is owned by |cert|, callers should
// not free it).
bool GetDERAndCacheIfNeeded(X509* cert, DERCache* der_cache) {
  int x509_der_cache_index =
      X509InitSingleton::GetInstance()->der_cache_ex_index();

  // Re-encoding the DER data via i2d_X509 is an expensive operation, but it's
  // necessary for comparing two certificates. We re-encode at most once per
  // certificate and cache the data within the X509 cert using X509_set_ex_data.
  DERCache* internal_cache =
      static_cast<DERCache*>(X509_get_ex_data(cert, x509_der_cache_index));
  if (!internal_cache) {
    unsigned char* data = NULL;
    int data_length = i2d_X509(cert, &data);
    if (data_length <= 0 || !data)
      return false;
    internal_cache = SetDERCache(cert, x509_der_cache_index, data, data_length);
    if (!internal_cache)
      return false;
  }
  *der_cache = *internal_cache;
  return true;
}

// sk_X509_free is a function-style macro, so can't be used as a template
// param directly.
void sk_X509_free_fn(STACK_OF(X509) * st) {
  sk_X509_pop_free(st, X509_free);
}

void GetCertChainInfo(X509_STORE_CTX* store_ctx,
                      CertVerifyResult* verify_result) {
  STACK_OF(X509)* chain = X509_STORE_CTX_get_chain(store_ctx);
  bssl::UniquePtr<CRYPTO_BUFFER> verified_cert = NULL;
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> verified_chain;
  for (int i = 0; i < sk_X509_num(chain); ++i) {
    X509* cert = sk_X509_value(chain, i);
    auto DER_buffer = x509_to_buffer(cert);
    auto x509_cert =
        x509_util::CreateCryptoBuffer(CRYPTO_BUFFER_data(DER_buffer.get()),
                                      CRYPTO_BUFFER_len(DER_buffer.get()));
    if (i == 0) {
      verified_cert = std::move(x509_cert);
    } else {
      verified_chain.push_back(std::move(x509_cert));
    }

    // Only check the algorithm status for certificates that are not in the
    // trust store.
    if (i < store_ctx->last_untrusted) {
      int sig_alg = OBJ_obj2nid(cert->sig_alg->algorithm);
      if (sig_alg == NID_md2WithRSAEncryption) {
        verify_result->has_md2 = true;
      } else if (sig_alg == NID_md4WithRSAEncryption) {
        verify_result->has_md4 = true;
      } else if (sig_alg == NID_md5WithRSAEncryption) {
        verify_result->has_md5 = true;
      } else if (sig_alg == NID_sha1WithRSAEncryption) {
        verify_result->has_sha1 = true;
      }
    }
  }

  if (verified_cert) {
    verify_result->verified_cert = X509Certificate::CreateFromBuffer(
        std::move(verified_cert), std::move(verified_chain));
  }
}

void AppendPublicKeyHashes(X509_STORE_CTX* store_ctx, HashValueVector* hashes) {
  STACK_OF(X509)* chain = X509_STORE_CTX_get_chain(store_ctx);
  for (int i = 0; i < sk_X509_num(chain); ++i) {
    X509* cert = sk_X509_value(chain, i);

    DERCache der_cache;
    if (!GetDERAndCacheIfNeeded(cert, &der_cache))
      continue;
    std::string der_data;
    der_data.assign(reinterpret_cast<const char*>(der_cache.data),
                    der_cache.data_length);

    base::StringPiece der_bytes(der_data);
    base::StringPiece spki_bytes;
    if (!asn1::ExtractSPKIFromDERCert(der_bytes, &spki_bytes))
      continue;

    HashValue sha256(HASH_VALUE_SHA256);
    crypto::SHA256HashString(spki_bytes, sha256.data(), crypto::kSHA256Length);
    hashes->push_back(sha256);
  }
}

}  // namespace

CertVerifyProcOpenSSL::CertVerifyProcOpenSSL() {}

CertVerifyProcOpenSSL::~CertVerifyProcOpenSSL() {}

int CertVerifyProcOpenSSL::VerifyInternal(
    X509Certificate* cert,
    const std::string& hostname,
    const std::string& /*ocsp_response*/,
    int flags,
    CRLSet* crl_set,
    const CertificateList& /*additional_trust_anchors*/,
    CertVerifyResult* verify_result) {
  crypto::EnsureOpenSSLInit();

  if (!cert->VerifyNameMatch(hostname))
    verify_result->cert_status |= CERT_STATUS_COMMON_NAME_INVALID;

  ScopedOpenSSL<X509_STORE_CTX, X509_STORE_CTX_free> ctx(X509_STORE_CTX_new());

  ScopedOpenSSL<STACK_OF(X509), sk_X509_free_fn> intermediates(
      sk_X509_new_null());
  if (!intermediates.get()) {
    return ERR_OUT_OF_MEMORY;
  }

  const auto& cert_current_intermediates = cert->intermediate_buffers();
  for (auto it = cert_current_intermediates.begin();
       it != cert_current_intermediates.end(); ++it) {
    X509* x509_intermediate = X509_parse_from_buffer(it->get());
    if (!sk_X509_push(intermediates.get(), x509_intermediate)) {
      return ERR_OUT_OF_MEMORY;
    }
  }

  ScopedOpenSSL<X509, X509_free> cert_in_x509;
  cert_in_x509.reset(X509_parse_from_buffer(cert->cert_buffer()));
  if (X509_STORE_CTX_init(ctx.get(), X509InitSingleton::GetInstance()->store(),
                          cert_in_x509.get(), intermediates.get()) != 1) {
    NOTREACHED();
    return ERR_FAILED;
  }

  if (X509_verify_cert(ctx.get()) != 1) {
    int x509_error = X509_STORE_CTX_get_error(ctx.get());
    // TODO[johnx]: replace this with net's map function.
    CertStatus cert_status = MapCertErrorToCertStatus(x509_error);
    LOG(ERROR) << "X509 Verification error "
               << X509_verify_cert_error_string(x509_error) << " : "
               << x509_error << " : "
               << X509_STORE_CTX_get_error_depth(ctx.get()) << " : "
               << cert_status;
    verify_result->cert_status |= cert_status;
  }

  GetCertChainInfo(ctx.get(), verify_result);
  AppendPublicKeyHashes(ctx.get(), &verify_result->public_key_hashes);

  if (IsCertStatusError(verify_result->cert_status)) {
    return MapCertStatusToNetError(verify_result->cert_status);
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
