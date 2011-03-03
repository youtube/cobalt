// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_certificate.h"

#include <openssl/asn1.h>
#include <openssl/crypto.h>
#include <openssl/obj_mac.h>
#include <openssl/pem.h>
#include <openssl/pkcs7.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include "base/openssl_util.h"
#include "base/pickle.h"
#include "base/singleton.h"
#include "base/string_number_conversions.h"
#include "net/base/cert_status_flags.h"
#include "net/base/cert_verify_result.h"
#include "net/base/net_errors.h"
#include "net/base/x509_openssl_util.h"

namespace net {

namespace nxou = net::x509_openssl_util;

namespace {

void CreateOSCertHandlesFromPKCS7Bytes(
    const char* data, int length,
    X509Certificate::OSCertHandles* handles) {
  base::EnsureOpenSSLInit();
  const unsigned char* der_data = reinterpret_cast<const unsigned char*>(data);
  base::ScopedOpenSSL<PKCS7, PKCS7_free> pkcs7_cert(
      d2i_PKCS7(NULL, &der_data, length));
  if (!pkcs7_cert.get())
    return;

  STACK_OF(X509)* certs = NULL;
  int nid = OBJ_obj2nid(pkcs7_cert.get()->type);
  if (nid == NID_pkcs7_signed) {
    certs = pkcs7_cert.get()->d.sign->cert;
  } else if (nid == NID_pkcs7_signedAndEnveloped) {
    certs = pkcs7_cert.get()->d.signed_and_enveloped->cert;
  }

  if (certs) {
    for (int i = 0; i < sk_X509_num(certs); ++i) {
      X509* x509_cert =
          X509Certificate::DupOSCertHandle(sk_X509_value(certs, i));
      handles->push_back(x509_cert);
    }
  }
}

void ParsePrincipalValues(X509_NAME* name,
                          int nid,
                          std::vector<std::string>* fields) {
  for (int index = -1;
       (index = X509_NAME_get_index_by_NID(name, nid, index)) != -1;) {
    std::string field;
    if (!nxou::ParsePrincipalValueByIndex(name, index, &field))
      break;
    fields->push_back(field);
  }
}

void ParsePrincipal(X509Certificate::OSCertHandle cert,
                    X509_NAME* x509_name,
                    CertPrincipal* principal) {
  if (!x509_name)
    return;

  ParsePrincipalValues(x509_name, NID_streetAddress,
                       &principal->street_addresses);
  ParsePrincipalValues(x509_name, NID_organizationName,
                       &principal->organization_names);
  ParsePrincipalValues(x509_name, NID_organizationalUnitName,
                       &principal->organization_unit_names);
  ParsePrincipalValues(x509_name, NID_domainComponent,
                       &principal->domain_components);

  nxou::ParsePrincipalValueByNID(x509_name, NID_commonName,
                                 &principal->common_name);
  nxou::ParsePrincipalValueByNID(x509_name, NID_localityName,
                                 &principal->locality_name);
  nxou::ParsePrincipalValueByNID(x509_name, NID_stateOrProvinceName,
                                 &principal->state_or_province_name);
  nxou::ParsePrincipalValueByNID(x509_name, NID_countryName,
                                 &principal->country_name);
}

void ParseSubjectAltNames(X509Certificate::OSCertHandle cert,
                          std::vector<std::string>* dns_names) {
  int index = X509_get_ext_by_NID(cert, NID_subject_alt_name, -1);
  X509_EXTENSION* alt_name_ext = X509_get_ext(cert, index);
  if (!alt_name_ext)
    return;

  base::ScopedOpenSSL<GENERAL_NAMES, GENERAL_NAMES_free> alt_names(
      reinterpret_cast<GENERAL_NAMES*>(X509V3_EXT_d2i(alt_name_ext)));
  if (!alt_names.get())
    return;

  for (int i = 0; i < sk_GENERAL_NAME_num(alt_names.get()); ++i) {
    const GENERAL_NAME* name = sk_GENERAL_NAME_value(alt_names.get(), i);
    if (name->type == GEN_DNS) {
      unsigned char* dns_name = ASN1_STRING_data(name->d.dNSName);
      if (!dns_name)
        continue;
      int dns_name_len = ASN1_STRING_length(name->d.dNSName);
      dns_names->push_back(
          std::string(reinterpret_cast<char*>(dns_name), dns_name_len));
    }
  }
}

// Maps X509_STORE_CTX_get_error() return values to our cert status flags.
int MapCertErrorToCertStatus(int err) {
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
      return CERT_STATUS_NOT_IN_DNS;
#endif
    case X509_V_ERR_CERT_REVOKED:
      return CERT_STATUS_REVOKED;
    case X509_V_ERR_KEYUSAGE_NO_CERTSIGN:
      return CERT_STATUS_WEAK_SIGNATURE_ALGORITHM;
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

struct DERCache {
  unsigned char* data;
  int data_length;
};

void DERCache_free(void* parent, void* ptr, CRYPTO_EX_DATA* ad, int idx,
                   long argl, void* argp) {
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
    return Singleton<X509InitSingleton,
                     LeakySingletonTraits<X509InitSingleton> >::get();
  }
  int der_cache_ex_index() const { return der_cache_ex_index_; }
  X509_STORE* store() const { return store_.get(); }

  void ResetCertStore() {
    store_.reset(X509_STORE_new());
    DCHECK(store_.get());
    X509_STORE_set_default_paths(store_.get());
    // TODO(joth): Enable CRL (see X509_STORE_set_flags(X509_V_FLAG_CRL_CHECK)).
  }

 private:
  friend struct DefaultSingletonTraits<X509InitSingleton>;
  X509InitSingleton() {
    base::EnsureOpenSSLInit();
    der_cache_ex_index_ = X509_get_ex_new_index(0, 0, 0, 0, DERCache_free);
    DCHECK_NE(der_cache_ex_index_, -1);
    ResetCertStore();
  }

  int der_cache_ex_index_;
  base::ScopedOpenSSL<X509_STORE, X509_STORE_free> store_;

  DISALLOW_COPY_AND_ASSIGN(X509InitSingleton);
};

// Takes ownership of |data| (which must have been allocated by OpenSSL).
DERCache* SetDERCache(X509Certificate::OSCertHandle cert,
                      int x509_der_cache_index,
                      unsigned char* data,
                      int data_length) {
  DERCache* internal_cache = static_cast<DERCache*>(
      OPENSSL_malloc(sizeof(*internal_cache)));
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
bool GetDERAndCacheIfNeeded(X509Certificate::OSCertHandle cert,
                            DERCache* der_cache) {
  int x509_der_cache_index =
      X509InitSingleton::GetInstance()->der_cache_ex_index();

  // Re-encoding the DER data via i2d_X509 is an expensive operation, but it's
  // necessary for comparing two certificates. We re-encode at most once per
  // certificate and cache the data within the X509 cert using X509_set_ex_data.
  DERCache* internal_cache = static_cast<DERCache*>(
      X509_get_ex_data(cert, x509_der_cache_index));
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

}  // namespace

// static
X509Certificate::OSCertHandle X509Certificate::DupOSCertHandle(
    OSCertHandle cert_handle) {
  DCHECK(cert_handle);
  // Using X509_dup causes the entire certificate to be reparsed. This
  // conversion, besides being non-trivial, drops any associated
  // application-specific data set by X509_set_ex_data. Using CRYPTO_add
  // just bumps up the ref-count for the cert, without causing any allocations
  // or deallocations.
  CRYPTO_add(&cert_handle->references, 1, CRYPTO_LOCK_X509);
  return cert_handle;
}

// static
void X509Certificate::FreeOSCertHandle(OSCertHandle cert_handle) {
  // Decrement the ref-count for the cert and, if all references are gone,
  // free the memory and any application-specific data associated with the
  // certificate.
  X509_free(cert_handle);
}

void X509Certificate::Initialize() {
  base::EnsureOpenSSLInit();
  fingerprint_ = CalculateFingerprint(cert_handle_);
  ParsePrincipal(cert_handle_, X509_get_subject_name(cert_handle_), &subject_);
  ParsePrincipal(cert_handle_, X509_get_issuer_name(cert_handle_), &issuer_);
  nxou::ParseDate(X509_get_notBefore(cert_handle_), &valid_start_);
  nxou::ParseDate(X509_get_notAfter(cert_handle_), &valid_expiry_);
}

// static
void X509Certificate::ResetCertStore() {
  X509InitSingleton::GetInstance()->ResetCertStore();
}

SHA1Fingerprint X509Certificate::CalculateFingerprint(OSCertHandle cert) {
  SHA1Fingerprint sha1;
  unsigned int sha1_size = static_cast<unsigned int>(sizeof(sha1.data));
  int ret = X509_digest(cert, EVP_sha1(), sha1.data, &sha1_size);
  CHECK(ret);
  CHECK_EQ(sha1_size, sizeof(sha1.data));
  return sha1;
}

// static
X509Certificate::OSCertHandle X509Certificate::CreateOSCertHandleFromBytes(
    const char* data, int length) {
  if (length < 0)
    return NULL;
  base::EnsureOpenSSLInit();
  const unsigned char* d2i_data =
      reinterpret_cast<const unsigned char*>(data);
  // Don't cache this data via SetDERCache as this wire format may be not be
  // identical from the i2d_X509 roundtrip.
  X509* cert = d2i_X509(NULL, &d2i_data, length);
  return cert;
}

// static
X509Certificate::OSCertHandles X509Certificate::CreateOSCertHandlesFromBytes(
    const char* data, int length, Format format) {
  OSCertHandles results;
  if (length < 0)
    return results;

  switch (format) {
    case FORMAT_SINGLE_CERTIFICATE: {
      OSCertHandle handle = CreateOSCertHandleFromBytes(data, length);
      if (handle)
        results.push_back(handle);
      break;
    }
    case FORMAT_PKCS7: {
      CreateOSCertHandlesFromPKCS7Bytes(data, length, &results);
      break;
    }
    default: {
      NOTREACHED() << "Certificate format " << format << " unimplemented";
      break;
    }
  }

  return results;
}

// static
X509Certificate* X509Certificate::CreateFromPickle(const Pickle& pickle,
                                                   void** pickle_iter) {
  const char* data;
  int length;
  if (!pickle.ReadData(pickle_iter, &data, &length))
    return NULL;

  return CreateFromBytes(data, length);
}

// static
X509Certificate* X509Certificate::CreateSelfSigned(
    base::RSAPrivateKey* key,
    const std::string& subject,
    uint32 serial_number,
    base::TimeDelta valid_duration) {
  // TODO(port): Implement.
  return NULL;
}

void X509Certificate::Persist(Pickle* pickle) {
  DERCache der_cache;
  if (!GetDERAndCacheIfNeeded(cert_handle_, &der_cache))
      return;

  pickle->WriteData(reinterpret_cast<const char*>(der_cache.data),
                    der_cache.data_length);
}

void X509Certificate::GetDNSNames(std::vector<std::string>* dns_names) const {
  dns_names->clear();

  ParseSubjectAltNames(cert_handle_, dns_names);

  if (dns_names->empty())
    dns_names->push_back(subject_.common_name);
}

// static
X509_STORE* X509Certificate::cert_store() {
  return X509InitSingleton::GetInstance()->store();
}

int X509Certificate::Verify(const std::string& hostname,
                            int flags,
                            CertVerifyResult* verify_result) const {
  verify_result->Reset();

  // TODO(joth): We should fetch the subjectAltNames directly rather than via
  // GetDNSNames, so we can apply special handling for IP addresses vs DNS
  // names, etc. See http://crbug.com/62973.
  std::vector<std::string> cert_names;
  GetDNSNames(&cert_names);
  if (!VerifyHostname(hostname, cert_names))
    verify_result->cert_status |= CERT_STATUS_COMMON_NAME_INVALID;

  base::ScopedOpenSSL<X509_STORE_CTX, X509_STORE_CTX_free> ctx(
      X509_STORE_CTX_new());

  base::ScopedOpenSSL<STACK_OF(X509), sk_X509_free_fn> intermediates(
      sk_X509_new_null());
  if (!intermediates.get())
    return ERR_OUT_OF_MEMORY;

  for (OSCertHandles::const_iterator it = intermediate_ca_certs_.begin();
       it != intermediate_ca_certs_.end(); ++it) {
    if (!sk_X509_push(intermediates.get(), *it))
      return ERR_OUT_OF_MEMORY;
  }
  int rv = X509_STORE_CTX_init(ctx.get(), cert_store(),
                               cert_handle_, intermediates.get());
  CHECK_EQ(1, rv);

  if (X509_verify_cert(ctx.get()) != 1) {
    int x509_error = X509_STORE_CTX_get_error(ctx.get());
    int cert_status = MapCertErrorToCertStatus(x509_error);
    LOG(ERROR) << "X509 Verification error "
        << X509_verify_cert_error_string(x509_error)
        << " : " << x509_error
        << " : " << X509_STORE_CTX_get_error_depth(ctx.get())
        << " : " << cert_status;
    verify_result->cert_status |= cert_status;
  }

  if (IsCertStatusError(verify_result->cert_status))
    return MapCertStatusToNetError(verify_result->cert_status);

  return OK;
}

bool X509Certificate::GetDEREncoded(std::string* encoded) {
  // TODO(port): Implement.
  return false;
}

// static
bool X509Certificate::IsSameOSCert(X509Certificate::OSCertHandle a,
                                   X509Certificate::OSCertHandle b) {
  DCHECK(a && b);
  if (a == b)
    return true;

  // X509_cmp only checks the fingerprint, but we want to compare the whole
  // DER data. Encoding it from OSCertHandle is an expensive operation, so we
  // cache the DER (if not already cached via X509_set_ex_data).
  DERCache der_cache_a, der_cache_b;

  return GetDERAndCacheIfNeeded(a, &der_cache_a) &&
      GetDERAndCacheIfNeeded(b, &der_cache_b) &&
      der_cache_a.data_length == der_cache_b.data_length &&
      memcmp(der_cache_a.data, der_cache_b.data, der_cache_a.data_length) == 0;
}

} // namespace net
