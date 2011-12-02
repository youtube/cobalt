// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_certificate.h"

#include <CommonCrypto/CommonDigest.h>
#include <CoreServices/CoreServices.h>
#include <Security/Security.h>
#include <time.h>

#include <vector>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/singleton.h"
#include "base/pickle.h"
#include "base/sha1.h"
#include "base/sys_string_conversions.h"
#include "crypto/cssm_init.h"
#include "crypto/nss_util.h"
#include "crypto/rsa_private_key.h"
#include "net/base/asn1_util.h"
#include "net/base/cert_status_flags.h"
#include "net/base/cert_verify_result.h"
#include "net/base/net_errors.h"
#include "net/base/test_root_certs.h"
#include "net/base/x509_certificate_known_roots_mac.h"
#include "third_party/apple_apsl/cssmapplePriv.h"
#include "third_party/nss/mozilla/security/nss/lib/certdb/cert.h"

using base::mac::ScopedCFTypeRef;
using base::Time;

namespace net {

namespace {

typedef OSStatus (*SecTrustCopyExtendedResultFuncPtr)(SecTrustRef,
                                                      CFDictionaryRef*);

int NetErrorFromOSStatus(OSStatus status) {
  switch (status) {
    case noErr:
      return OK;
    case errSecNotAvailable:
    case errSecNoCertificateModule:
    case errSecNoPolicyModule:
      return ERR_NOT_IMPLEMENTED;
    case errSecAuthFailed:
      return ERR_ACCESS_DENIED;
    default: {
      base::mac::ScopedCFTypeRef<CFStringRef> error_string(
          SecCopyErrorMessageString(status, NULL));
      LOG(ERROR) << "Unknown error " << status
                 << " (" << base::SysCFStringRefToUTF8(error_string) << ")"
                 << " mapped to ERR_FAILED";
      return ERR_FAILED;
    }
  }
}

CertStatus CertStatusFromOSStatus(OSStatus status) {
  switch (status) {
    case noErr:
      return 0;

    case CSSMERR_TP_INVALID_ANCHOR_CERT:
    case CSSMERR_TP_NOT_TRUSTED:
    case CSSMERR_TP_INVALID_CERT_AUTHORITY:
      return CERT_STATUS_AUTHORITY_INVALID;

    case CSSMERR_TP_CERT_EXPIRED:
    case CSSMERR_TP_CERT_NOT_VALID_YET:
      // "Expired" and "not yet valid" collapse into a single status.
      return CERT_STATUS_DATE_INVALID;

    case CSSMERR_TP_CERT_REVOKED:
    case CSSMERR_TP_CERT_SUSPENDED:
      return CERT_STATUS_REVOKED;

    case CSSMERR_APPLETP_HOSTNAME_MISMATCH:
      return CERT_STATUS_COMMON_NAME_INVALID;

    case CSSMERR_APPLETP_CRL_NOT_FOUND:
    case CSSMERR_APPLETP_OCSP_UNAVAILABLE:
    case CSSMERR_APPLETP_INCOMPLETE_REVOCATION_CHECK:
      return CERT_STATUS_NO_REVOCATION_MECHANISM;

    case CSSMERR_APPLETP_CRL_EXPIRED:
    case CSSMERR_APPLETP_CRL_NOT_VALID_YET:
    case CSSMERR_APPLETP_CRL_SERVER_DOWN:
    case CSSMERR_APPLETP_CRL_NOT_TRUSTED:
    case CSSMERR_APPLETP_CRL_INVALID_ANCHOR_CERT:
    case CSSMERR_APPLETP_CRL_POLICY_FAIL:
    case CSSMERR_APPLETP_OCSP_BAD_RESPONSE:
    case CSSMERR_APPLETP_OCSP_BAD_REQUEST:
    case CSSMERR_APPLETP_OCSP_STATUS_UNRECOGNIZED:
    case CSSMERR_APPLETP_NETWORK_FAILURE:
    case CSSMERR_APPLETP_OCSP_NOT_TRUSTED:
    case CSSMERR_APPLETP_OCSP_INVALID_ANCHOR_CERT:
    case CSSMERR_APPLETP_OCSP_SIG_ERROR:
    case CSSMERR_APPLETP_OCSP_NO_SIGNER:
    case CSSMERR_APPLETP_OCSP_RESP_MALFORMED_REQ:
    case CSSMERR_APPLETP_OCSP_RESP_INTERNAL_ERR:
    case CSSMERR_APPLETP_OCSP_RESP_TRY_LATER:
    case CSSMERR_APPLETP_OCSP_RESP_SIG_REQUIRED:
    case CSSMERR_APPLETP_OCSP_RESP_UNAUTHORIZED:
    case CSSMERR_APPLETP_OCSP_NONCE_MISMATCH:
      // We asked for a revocation check, but didn't get it.
      return CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;

    case CSSMERR_APPLETP_CRL_BAD_URI:
    case CSSMERR_APPLETP_IDP_FAIL:
      return CERT_STATUS_INVALID;

    default: {
      // Failure was due to something Chromium doesn't define a
      // specific status for (such as basic constraints violation, or
      // unknown critical extension)
      base::mac::ScopedCFTypeRef<CFStringRef> error_string(
          SecCopyErrorMessageString(status, NULL));
      LOG(WARNING) << "Unknown error " << status
                   << " (" << base::SysCFStringRefToUTF8(error_string) << ")"
                   << " mapped to CERT_STATUS_INVALID";
      return CERT_STATUS_INVALID;
    }
  }
}

// Wrapper for a CSSM_DATA_PTR that was obtained via one of the CSSM field
// accessors (such as CSSM_CL_CertGet[First/Next]Value or
// CSSM_CL_CertGet[First/Next]CachedValue).
class CSSMFieldValue {
 public:
  CSSMFieldValue() : cl_handle_(NULL), oid_(NULL), field_(NULL) {}
  CSSMFieldValue(CSSM_CL_HANDLE cl_handle,
                 const CSSM_OID* oid,
                 CSSM_DATA_PTR field)
      : cl_handle_(cl_handle),
        oid_(const_cast<CSSM_OID_PTR>(oid)),
        field_(field) {
  }

  ~CSSMFieldValue() {
    Reset(NULL, NULL, NULL);
  }

  CSSM_OID_PTR oid() const { return oid_; }
  CSSM_DATA_PTR field() const { return field_; }

  // Returns the field as if it was an arbitrary type - most commonly, by
  // interpreting the field as a specific CSSM/CDSA parsed type, such as
  // CSSM_X509_SUBJECT_PUBLIC_KEY_INFO or CSSM_X509_ALGORITHM_IDENTIFIER.
  // An added check is applied to ensure that the current field is large
  // enough to actually contain the requested type.
  template <typename T> const T* GetAs() const {
    if (!field_ || field_->Length < sizeof(T))
      return NULL;
    return reinterpret_cast<const T*>(field_->Data);
  }

  void Reset(CSSM_CL_HANDLE cl_handle,
             CSSM_OID_PTR oid,
             CSSM_DATA_PTR field) {
    if (cl_handle_ && oid_ && field_)
      CSSM_CL_FreeFieldValue(cl_handle_, oid_, field_);
    cl_handle_ = cl_handle;
    oid_ = oid;
    field_ = field;
  }

 private:
  CSSM_CL_HANDLE cl_handle_;
  CSSM_OID_PTR oid_;
  CSSM_DATA_PTR field_;

  DISALLOW_COPY_AND_ASSIGN(CSSMFieldValue);
};

// CSSMCachedCertificate is a container class that is used to wrap the
// CSSM_CL_CertCache APIs and provide safe and efficient access to
// certificate fields in their CSSM form.
//
// To provide efficient access to certificate/CRL fields, CSSM provides an
// API/SPI to "cache" a certificate/CRL. The exact meaning of a cached
// certificate is not defined by CSSM, but is documented to generally be some
// intermediate or parsed form of the certificate. In the case of Apple's
// CSSM CL implementation, the intermediate form is the parsed certificate
// stored in an internal format (which happens to be NSS). By caching the
// certificate, callers that wish to access multiple fields (such as subject,
// issuer, and validity dates) do not need to repeatedly parse the entire
// certificate, nor are they forced to convert all fields from their NSS types
// to their CSSM equivalents. This latter point is especially helpful when
// running on OS X 10.5, as it will fail to convert some fields that reference
// unsupported algorithms, such as ECC.
class CSSMCachedCertificate {
 public:
  CSSMCachedCertificate() : cl_handle_(NULL), cached_cert_handle_(NULL) {}
  ~CSSMCachedCertificate() {
    if (cl_handle_ && cached_cert_handle_)
      CSSM_CL_CertAbortCache(cl_handle_, cached_cert_handle_);
  }

  // Initializes the CSSMCachedCertificate by caching the specified
  // |os_cert_handle|. On success, returns noErr.
  // Note: Once initialized, the cached certificate should only be accessed
  // from a single thread.
  OSStatus Init(SecCertificateRef os_cert_handle) {
    DCHECK(!cl_handle_ && !cached_cert_handle_);
    DCHECK(os_cert_handle);
    CSSM_DATA cert_data;
    OSStatus status = SecCertificateGetData(os_cert_handle, &cert_data);
    if (status)
      return status;
    status = SecCertificateGetCLHandle(os_cert_handle, &cl_handle_);
    if (status) {
      DCHECK(!cl_handle_);
      return status;
    }

    status = CSSM_CL_CertCache(cl_handle_, &cert_data, &cached_cert_handle_);
    if (status)
      DCHECK(!cached_cert_handle_);
    return status;
  }

  // Fetches the first value for the field associated with |field_oid|.
  // If |field_oid| is a valid OID and is present in the current certificate,
  // returns CSSM_OK and stores the first value in |field|. If additional
  // values are associated with |field_oid|, they are ignored.
  OSStatus GetField(const CSSM_OID* field_oid,
                    CSSMFieldValue* field) const {
    DCHECK(cl_handle_);
    DCHECK(cached_cert_handle_);

    CSSM_OID_PTR oid = const_cast<CSSM_OID_PTR>(field_oid);
    CSSM_DATA_PTR field_ptr = NULL;
    CSSM_HANDLE results_handle = NULL;
    uint32 field_value_count = 0;
    CSSM_RETURN status = CSSM_CL_CertGetFirstCachedFieldValue(
        cl_handle_, cached_cert_handle_, oid, &results_handle,
        &field_value_count, &field_ptr);
    if (status)
      return status;

    // Note: |field_value_count| may be > 1, indicating that more than one
    // value is present. This may happen with extensions, but for current
    // usages, only the first value is returned.
    CSSM_CL_CertAbortQuery(cl_handle_, results_handle);
    field->Reset(cl_handle_, oid, field_ptr);
    return CSSM_OK;
  }

 private:
  CSSM_CL_HANDLE cl_handle_;
  CSSM_HANDLE cached_cert_handle_;
};

void GetCertDateForOID(const CSSMCachedCertificate& cached_cert,
                       const CSSM_OID* oid,
                       Time* result) {
  *result = Time::Time();

  CSSMFieldValue field;
  OSStatus status = cached_cert.GetField(oid, &field);
  if (status)
    return;

  const CSSM_X509_TIME* x509_time = field.GetAs<CSSM_X509_TIME>();
  if (x509_time->timeType != BER_TAG_UTC_TIME &&
      x509_time->timeType != BER_TAG_GENERALIZED_TIME) {
    LOG(ERROR) << "Unsupported date/time format "
               << x509_time->timeType;
    return;
  }

  base::StringPiece time_string(
      reinterpret_cast<const char*>(x509_time->time.Data),
      x509_time->time.Length);
  CertDateFormat format = x509_time->timeType == BER_TAG_UTC_TIME ?
      CERT_DATE_FORMAT_UTC_TIME : CERT_DATE_FORMAT_GENERALIZED_TIME;
  if (!ParseCertificateDate(time_string, format, result))
    LOG(ERROR) << "Invalid certificate date/time " << time_string;
}

std::string GetCertSerialNumber(const CSSMCachedCertificate& cached_cert) {
  CSSMFieldValue serial_number;
  OSStatus status = cached_cert.GetField(&CSSMOID_X509V1SerialNumber,
                                         &serial_number);
  if (status || !serial_number.field())
    return std::string();

  return std::string(
      reinterpret_cast<const char*>(serial_number.field()->Data),
      serial_number.field()->Length);
}

// Creates a SecPolicyRef for the given OID, with optional value.
OSStatus CreatePolicy(const CSSM_OID* policy_oid,
                      void* option_data,
                      size_t option_length,
                      SecPolicyRef* policy) {
  SecPolicySearchRef search;
  OSStatus err = SecPolicySearchCreate(CSSM_CERT_X_509v3, policy_oid, NULL,
                                       &search);
  if (err)
    return err;
  err = SecPolicySearchCopyNext(search, policy);
  CFRelease(search);
  if (err)
    return err;

  if (option_data) {
    CSSM_DATA options_data = {
      option_length,
      reinterpret_cast<uint8_t*>(option_data)
    };
    err = SecPolicySetValue(*policy, &options_data);
    if (err) {
      CFRelease(*policy);
      return err;
    }
  }
  return noErr;
}

// Creates a series of SecPolicyRefs to be added to a SecTrustRef used to
// validate a certificate for an SSL server. |hostname| contains the name of
// the SSL server that the certificate should be verified against. |flags| is
// a bitwise-OR of VerifyFlags that can further alter how trust is validated,
// such as how revocation is checked. If successful, returns noErr, and
// stores the resultant array of SecPolicyRefs in |policies|.
OSStatus CreateTrustPolicies(const std::string& hostname,
                             int flags,
                             ScopedCFTypeRef<CFArrayRef>* policies) {
  ScopedCFTypeRef<CFMutableArrayRef> local_policies(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
  if (!local_policies)
    return memFullErr;

  // Create an SSL server policy. While certificate name validation will be
  // performed by SecTrustEvaluate(), it has the following limitations:
  // - Doesn't support IP addresses in dotted-quad literals (127.0.0.1)
  // - Doesn't support IPv6 addresses
  // - Doesn't support the iPAddress subjectAltName
  // Providing the hostname is necessary in order to locate certain user or
  // system trust preferences, such as those created by Safari. Preferences
  // created by Keychain Access do not share this requirement.
  CSSM_APPLE_TP_SSL_OPTIONS tp_ssl_options;
  memset(&tp_ssl_options, 0, sizeof(tp_ssl_options));
  tp_ssl_options.Version = CSSM_APPLE_TP_SSL_OPTS_VERSION;
  if (!hostname.empty()) {
    tp_ssl_options.ServerName = hostname.data();
    tp_ssl_options.ServerNameLen = hostname.size();
  }

  SecPolicyRef ssl_policy;
  OSStatus status = CreatePolicy(&CSSMOID_APPLE_TP_SSL, &tp_ssl_options,
                                 sizeof(tp_ssl_options), &ssl_policy);
  if (status)
    return status;
  CFArrayAppendValue(local_policies, ssl_policy);
  CFRelease(ssl_policy);

  // Explicitly add revocation policies, in order to override system
  // revocation checking policies and instead respect the application-level
  // revocation preference.
  status = X509Certificate::CreateRevocationPolicies(
      (flags & X509Certificate::VERIFY_REV_CHECKING_ENABLED),
      local_policies);
  if (status)
    return status;

  policies->reset(local_policies.release());
  return noErr;
}

// Saves some information about the certificate chain |cert_chain| in
// |*verify_result|. The caller MUST initialize |*verify_result| before
// calling this function.
void GetCertChainInfo(CFArrayRef cert_chain,
                      CSSM_TP_APPLE_EVIDENCE_INFO* chain_info,
                      CertVerifyResult* verify_result) {
  SecCertificateRef verified_cert = NULL;
  std::vector<SecCertificateRef> verified_chain;
  for (CFIndex i = 0, count = CFArrayGetCount(cert_chain); i < count; ++i) {
    SecCertificateRef chain_cert = reinterpret_cast<SecCertificateRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(cert_chain, i)));
    if (i == 0) {
      verified_cert = chain_cert;
    } else {
      verified_chain.push_back(chain_cert);
    }

    if ((chain_info[i].StatusBits & CSSM_CERT_STATUS_IS_IN_ANCHORS) ||
        (chain_info[i].StatusBits & CSSM_CERT_STATUS_IS_ROOT)) {
      // The current certificate is either in the user's trusted store or is
      // a root (self-signed) certificate. Ignore the signature algorithm for
      // these certificates, as it is meaningless for security. We allow
      // self-signed certificates (i == 0 & IS_ROOT), since we accept that
      // any security assertions by such a cert are inherently meaningless.
      continue;
    }

    CSSMCachedCertificate cached_cert;
    OSStatus status = cached_cert.Init(chain_cert);
    if (status)
      continue;
    CSSMFieldValue signature_field;
    status = cached_cert.GetField(&CSSMOID_X509V1SignatureAlgorithm,
                                  &signature_field);
    if (status || !signature_field.field())
      continue;
    // Match the behaviour of OS X system tools and defensively check that
    // sizes are appropriate. This would indicate a critical failure of the
    // OS X certificate library, but based on history, it is best to play it
    // safe.
    const CSSM_X509_ALGORITHM_IDENTIFIER* sig_algorithm =
        signature_field.GetAs<CSSM_X509_ALGORITHM_IDENTIFIER>();
    if (!sig_algorithm)
      continue;

    const CSSM_OID* alg_oid = &sig_algorithm->algorithm;
    if (CSSMOIDEqual(alg_oid, &CSSMOID_MD2WithRSA)) {
      verify_result->has_md2 = true;
      if (i != 0)
        verify_result->has_md2_ca = true;
    } else if (CSSMOIDEqual(alg_oid, &CSSMOID_MD4WithRSA)) {
      verify_result->has_md4 = true;
    } else if (CSSMOIDEqual(alg_oid, &CSSMOID_MD5WithRSA)) {
      verify_result->has_md5 = true;
      if (i != 0)
        verify_result->has_md5_ca = true;
    }
  }
  if (!verified_cert)
    return;

  verify_result->verified_cert =
      X509Certificate::CreateFromHandle(verified_cert, verified_chain);
}

// Gets the issuer for a given cert, starting with the cert itself and
// including the intermediate and finally root certificates (if any).
// This function calls SecTrust but doesn't actually pay attention to the trust
// result: it shouldn't be used to determine trust, just to traverse the chain.
// Caller is responsible for releasing the value stored into *out_cert_chain.
OSStatus CopyCertChain(SecCertificateRef cert_handle,
                       CFArrayRef* out_cert_chain) {
  DCHECK(cert_handle);
  DCHECK(out_cert_chain);
  // Create an SSL policy ref configured for client cert evaluation.
  SecPolicyRef ssl_policy;
  OSStatus result = X509Certificate::CreateSSLClientPolicy(&ssl_policy);
  if (result)
    return result;
  ScopedCFTypeRef<SecPolicyRef> scoped_ssl_policy(ssl_policy);

  // Create a SecTrustRef.
  ScopedCFTypeRef<CFArrayRef> input_certs(CFArrayCreate(
      NULL, const_cast<const void**>(reinterpret_cast<void**>(&cert_handle)),
      1, &kCFTypeArrayCallBacks));
  SecTrustRef trust_ref = NULL;
  result = SecTrustCreateWithCertificates(input_certs, ssl_policy, &trust_ref);
  if (result)
    return result;
  ScopedCFTypeRef<SecTrustRef> trust(trust_ref);

  // Evaluate trust, which creates the cert chain.
  SecTrustResultType status;
  CSSM_TP_APPLE_EVIDENCE_INFO* status_chain;
  result = SecTrustEvaluate(trust, &status);
  if (result)
    return result;
  return SecTrustGetResult(trust, &status, out_cert_chain, &status_chain);
}

// Returns true if |purpose| is listed as allowed in |usage|. This
// function also considers the "Any" purpose. If the attribute is
// present and empty, we return false.
bool ExtendedKeyUsageAllows(const CE_ExtendedKeyUsage* usage,
                            const CSSM_OID* purpose) {
  for (unsigned p = 0; p < usage->numPurposes; ++p) {
    if (CSSMOIDEqual(&usage->purposes[p], purpose))
      return true;
    if (CSSMOIDEqual(&usage->purposes[p], &CSSMOID_ExtendedKeyUsageAny))
      return true;
  }
  return false;
}

// Test that a given |cert_handle| is actually a valid X.509 certificate, and
// return true if it is.
//
// On OS X, SecCertificateCreateFromData() does not return any errors if
// called with invalid data, as long as data is present. The actual decoding
// of the certificate does not happen until an API that requires a CSSM
// handle is called. While SecCertificateGetCLHandle is the most likely
// candidate, as it performs the parsing, it does not check whether the
// parsing was actually successful. Instead, SecCertificateGetSubject is
// used (supported since 10.3), as a means to check that the certificate
// parsed as a valid X.509 certificate.
bool IsValidOSCertHandle(SecCertificateRef cert_handle) {
  const CSSM_X509_NAME* sanity_check = NULL;
  OSStatus status = SecCertificateGetSubject(cert_handle, &sanity_check);
  return status == noErr && sanity_check;
}

// Parses |data| of length |length|, attempting to decode it as the specified
// |format|. If |data| is in the specified format, any certificates contained
// within are stored into |output|.
void AddCertificatesFromBytes(const char* data, size_t length,
                              SecExternalFormat format,
                              X509Certificate::OSCertHandles* output) {
  SecExternalFormat input_format = format;
  ScopedCFTypeRef<CFDataRef> local_data(CFDataCreateWithBytesNoCopy(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(data), length,
      kCFAllocatorNull));

  CFArrayRef items = NULL;
  OSStatus status = SecKeychainItemImport(local_data, NULL, &input_format,
                                          NULL, 0, NULL, NULL, &items);
  if (status) {
    DLOG(WARNING) << status << " Unable to import items from data of length "
                  << length;
    return;
  }

  ScopedCFTypeRef<CFArrayRef> scoped_items(items);
  CFTypeID cert_type_id = SecCertificateGetTypeID();

  for (CFIndex i = 0; i < CFArrayGetCount(items); ++i) {
    SecKeychainItemRef item = reinterpret_cast<SecKeychainItemRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(items, i)));

    // While inputFormat implies only certificates will be imported, if/when
    // other formats (eg: PKCS#12) are supported, this may also include
    // private keys or other items types, so filter appropriately.
    if (CFGetTypeID(item) == cert_type_id) {
      SecCertificateRef cert = reinterpret_cast<SecCertificateRef>(item);
      // OS X ignores |input_format| if it detects that |local_data| is PEM
      // encoded, attempting to decode data based on internal rules for PEM
      // block headers. If a PKCS#7 blob is encoded with a PEM block of
      // CERTIFICATE, OS X 10.5 will return a single, invalid certificate
      // based on the decoded data. If this happens, the certificate should
      // not be included in |output|. Because |output| is empty,
      // CreateCertificateListfromBytes will use PEMTokenizer to decode the
      // data. When called again with the decoded data, OS X will honor
      // |input_format|, causing decode to succeed. On OS X 10.6, the data
      // is properly decoded as a PKCS#7, whether PEM or not, which avoids
      // the need to fallback to internal decoding.
      if (IsValidOSCertHandle(cert)) {
        CFRetain(cert);
        output->push_back(cert);
      }
    }
  }
}

struct CSSMOIDString {
  const CSSM_OID* oid_;
  std::string string_;
};

typedef std::vector<CSSMOIDString> CSSMOIDStringVector;

bool CERTNameToCSSMOIDVector(CERTName* name, CSSMOIDStringVector* out_values) {
  struct OIDCSSMMap {
    SECOidTag sec_OID_;
    const CSSM_OID* cssm_OID_;
  };

  const OIDCSSMMap kOIDs[] = {
      { SEC_OID_AVA_COMMON_NAME, &CSSMOID_CommonName },
      { SEC_OID_AVA_COUNTRY_NAME, &CSSMOID_CountryName },
      { SEC_OID_AVA_LOCALITY, &CSSMOID_LocalityName },
      { SEC_OID_AVA_STATE_OR_PROVINCE, &CSSMOID_StateProvinceName },
      { SEC_OID_AVA_STREET_ADDRESS, &CSSMOID_StreetAddress },
      { SEC_OID_AVA_ORGANIZATION_NAME, &CSSMOID_OrganizationName },
      { SEC_OID_AVA_ORGANIZATIONAL_UNIT_NAME, &CSSMOID_OrganizationalUnitName },
      { SEC_OID_AVA_DN_QUALIFIER, &CSSMOID_DNQualifier },
      { SEC_OID_RFC1274_UID, &CSSMOID_UniqueIdentifier },
      { SEC_OID_PKCS9_EMAIL_ADDRESS, &CSSMOID_EmailAddress },
  };

  CERTRDN** rdns = name->rdns;
  for (size_t rdn = 0; rdns[rdn]; ++rdn) {
    CERTAVA** avas = rdns[rdn]->avas;
    for (size_t pair = 0; avas[pair] != 0; ++pair) {
      SECOidTag tag = CERT_GetAVATag(avas[pair]);
      if (tag == SEC_OID_UNKNOWN) {
        return false;
      }
      CSSMOIDString oidString;
      bool found_oid = false;
      for (size_t oid = 0; oid < ARRAYSIZE_UNSAFE(kOIDs); ++oid) {
        if (kOIDs[oid].sec_OID_ == tag) {
          SECItem* decode_item = CERT_DecodeAVAValue(&avas[pair]->value);
          if (!decode_item)
            return false;

          // TODO(wtc): Pass decode_item to CERT_RFC1485_EscapeAndQuote.
          std::string value(reinterpret_cast<char*>(decode_item->data),
                            decode_item->len);
          oidString.oid_ = kOIDs[oid].cssm_OID_;
          oidString.string_ = value;
          out_values->push_back(oidString);
          SECITEM_FreeItem(decode_item, PR_TRUE);
          found_oid = true;
          break;
        }
      }
      if (!found_oid) {
        DLOG(ERROR) << "Unrecognized OID: " << tag;
      }
    }
  }
  return true;
}

class ScopedCertName {
 public:
  explicit ScopedCertName(CERTName* name) : name_(name) { }
  ~ScopedCertName() {
    if (name_) CERT_DestroyName(name_);
  }
  operator CERTName*() { return name_; }

 private:
  CERTName* name_;
};

class ScopedEncodedCertResults {
 public:
  explicit ScopedEncodedCertResults(CSSM_TP_RESULT_SET* results)
      : results_(results) { }
  ~ScopedEncodedCertResults() {
    if (results_) {
      CSSM_ENCODED_CERT* encCert =
          reinterpret_cast<CSSM_ENCODED_CERT*>(results_->Results);
      for (uint32 i = 0; i < results_->NumberOfResults; i++) {
        crypto::CSSMFree(encCert[i].CertBlob.Data);
      }
    }
    crypto::CSSMFree(results_->Results);
    crypto::CSSMFree(results_);
  }

 private:
  CSSM_TP_RESULT_SET* results_;
};

void AppendPublicKeyHashes(CFArrayRef chain,
                           std::vector<SHA1Fingerprint>* hashes) {
  const CFIndex n = CFArrayGetCount(chain);
  for (CFIndex i = 0; i < n; i++) {
    SecCertificateRef cert = reinterpret_cast<SecCertificateRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(chain, i)));

    CSSM_DATA cert_data;
    OSStatus err = SecCertificateGetData(cert, &cert_data);
    DCHECK_EQ(err, noErr);
    base::StringPiece der_bytes(reinterpret_cast<const char*>(cert_data.Data),
                               cert_data.Length);
    base::StringPiece spki_bytes;
    if (!asn1::ExtractSPKIFromDERCert(der_bytes, &spki_bytes))
      continue;

    SHA1Fingerprint hash;
    CC_SHA1(spki_bytes.data(), spki_bytes.size(), hash.data);
    hashes->push_back(hash);
  }
}

}  // namespace

void X509Certificate::Initialize() {
  const CSSM_X509_NAME* name;
  OSStatus status = SecCertificateGetSubject(cert_handle_, &name);
  if (!status)
    subject_.Parse(name);

  status = SecCertificateGetIssuer(cert_handle_, &name);
  if (!status)
    issuer_.Parse(name);

  CSSMCachedCertificate cached_cert;
  if (cached_cert.Init(cert_handle_) == CSSM_OK) {
    GetCertDateForOID(cached_cert, &CSSMOID_X509V1ValidityNotBefore,
                      &valid_start_);
    GetCertDateForOID(cached_cert, &CSSMOID_X509V1ValidityNotAfter,
                      &valid_expiry_);
    serial_number_ = GetCertSerialNumber(cached_cert);
  }

  fingerprint_ = CalculateFingerprint(cert_handle_);
  ca_fingerprint_ = CalculateCAFingerprint(intermediate_ca_certs_);
}

// IsIssuedByKnownRoot returns true if the given chain is rooted at a root CA
// that we recognise as a standard root.
// static
bool X509Certificate::IsIssuedByKnownRoot(CFArrayRef chain) {
  int n = CFArrayGetCount(chain);
  if (n < 1)
    return false;
  SecCertificateRef root_ref = reinterpret_cast<SecCertificateRef>(
      const_cast<void*>(CFArrayGetValueAtIndex(chain, n - 1)));
  SHA1Fingerprint hash = X509Certificate::CalculateFingerprint(root_ref);
  return IsSHA1HashInSortedArray(
      hash, &kKnownRootCertSHA1Hashes[0][0], sizeof(kKnownRootCertSHA1Hashes));
}

// static
X509Certificate* X509Certificate::CreateSelfSigned(
    crypto::RSAPrivateKey* key,
    const std::string& subject,
    uint32 serial_number,
    base::TimeDelta valid_duration) {
  DCHECK(key);
  DCHECK(!subject.empty());

  if (valid_duration.InSeconds() > UINT32_MAX) {
     LOG(ERROR) << "valid_duration too big" << valid_duration.InSeconds();
     valid_duration = base::TimeDelta::FromSeconds(UINT32_MAX);
  }

  // There is a comment in
  // http://www.opensource.apple.com/source/security_certtool/security_certtool-31828/src/CertTool.cpp
  // that serial_numbers being passed into CSSM_TP_SubmitCredRequest can't have
  // their high bit set. We will continue though and mask it out below.
  if (serial_number & 0x80000000)
    LOG(ERROR) << "serial_number has high bit set " << serial_number;

  // NSS is used to parse the subject string into a set of
  // CSSM_OID/string pairs. There doesn't appear to be a system routine for
  // parsing Distinguished Name strings.
  crypto::EnsureNSSInit();

  CSSMOIDStringVector subject_name_oids;
  ScopedCertName subject_name(
      CERT_AsciiToName(const_cast<char*>(subject.c_str())));
  if (!CERTNameToCSSMOIDVector(subject_name, &subject_name_oids)) {
    DLOG(ERROR) << "Unable to generate CSSMOIDMap from " << subject;
    return NULL;
  }

  // Convert the map of oid/string pairs into an array of
  // CSSM_APPLE_TP_NAME_OIDs.
  std::vector<CSSM_APPLE_TP_NAME_OID> cssm_subject_names;
  for (CSSMOIDStringVector::iterator iter = subject_name_oids.begin();
      iter != subject_name_oids.end(); ++iter) {
    CSSM_APPLE_TP_NAME_OID cssm_subject_name;
    cssm_subject_name.oid = iter->oid_;
    cssm_subject_name.string = iter->string_.c_str();
    cssm_subject_names.push_back(cssm_subject_name);
  }

  if (cssm_subject_names.empty()) {
    DLOG(ERROR) << "cssm_subject_names.size() == 0. Input: " << subject;
    return NULL;
  }

  // Set up a certificate request.
  CSSM_APPLE_TP_CERT_REQUEST certReq;
  memset(&certReq, 0, sizeof(certReq));
  certReq.cspHand = crypto::GetSharedCSPHandle();
  certReq.clHand = crypto::GetSharedCLHandle();
    // See comment about serial numbers above.
  certReq.serialNumber = serial_number & 0x7fffffff;
  certReq.numSubjectNames = cssm_subject_names.size();
  certReq.subjectNames = &cssm_subject_names[0];
  certReq.numIssuerNames = 0;  // Root.
  certReq.issuerNames = NULL;
  certReq.issuerNameX509 = NULL;
  certReq.certPublicKey = key->public_key();
  certReq.issuerPrivateKey = key->key();
  // These are the Apple defaults.
  certReq.signatureAlg = CSSM_ALGID_SHA1WithRSA;
  certReq.signatureOid = CSSMOID_SHA1WithRSA;
  certReq.notBefore = 0;
  certReq.notAfter = static_cast<uint32>(valid_duration.InSeconds());
  certReq.numExtensions = 0;
  certReq.extensions = NULL;
  certReq.challengeString = NULL;

  CSSM_TP_REQUEST_SET reqSet;
  reqSet.NumberOfRequests = 1;
  reqSet.Requests = &certReq;

  CSSM_FIELD policyId;
  memset(&policyId, 0, sizeof(policyId));
  policyId.FieldOid = CSSMOID_APPLE_TP_LOCAL_CERT_GEN;

  CSSM_TP_CALLERAUTH_CONTEXT callerAuthContext;
  memset(&callerAuthContext, 0, sizeof(callerAuthContext));
  callerAuthContext.Policy.NumberOfPolicyIds = 1;
  callerAuthContext.Policy.PolicyIds = &policyId;

  CSSM_TP_HANDLE tp_handle = crypto::GetSharedTPHandle();
  CSSM_DATA refId;
  memset(&refId, 0, sizeof(refId));
  sint32 estTime;
  CSSM_RETURN crtn = CSSM_TP_SubmitCredRequest(tp_handle, NULL,
      CSSM_TP_AUTHORITY_REQUEST_CERTISSUE, &reqSet, &callerAuthContext,
       &estTime, &refId);
  if (crtn) {
    DLOG(ERROR) << "CSSM_TP_SubmitCredRequest failed " << crtn;
    return NULL;
  }

  CSSM_BOOL confirmRequired;
  CSSM_TP_RESULT_SET *resultSet = NULL;
  crtn = CSSM_TP_RetrieveCredResult(tp_handle, &refId, NULL, &estTime,
                                    &confirmRequired, &resultSet);
  ScopedEncodedCertResults scopedResults(resultSet);
  crypto::CSSMFree(refId.Data);
  if (crtn) {
    DLOG(ERROR) << "CSSM_TP_RetrieveCredResult failed " << crtn;
    return NULL;
  }

  if (confirmRequired) {
    // Potential leak here of resultSet. |confirmRequired| should never be
    // true.
    DLOG(ERROR) << "CSSM_TP_RetrieveCredResult required confirmation";
    return NULL;
  }

  if (resultSet->NumberOfResults != 1) {
     DLOG(ERROR) << "Unexpected number of results: "
                 << resultSet->NumberOfResults;
    return NULL;
  }

  CSSM_ENCODED_CERT* encCert =
      reinterpret_cast<CSSM_ENCODED_CERT*>(resultSet->Results);
  base::mac::ScopedCFTypeRef<SecCertificateRef> scoped_cert;
  SecCertificateRef certificate_ref = NULL;
  OSStatus os_status =
      SecCertificateCreateFromData(&encCert->CertBlob, encCert->CertType,
                                   encCert->CertEncoding, &certificate_ref);
  if (os_status != 0) {
    DLOG(ERROR) << "SecCertificateCreateFromData failed: " << os_status;
    return NULL;
  }
  scoped_cert.reset(certificate_ref);

  return CreateFromHandle(scoped_cert, X509Certificate::OSCertHandles());
}

void X509Certificate::GetSubjectAltName(
    std::vector<std::string>* dns_names,
    std::vector<std::string>* ip_addrs) const {
  if (dns_names)
    dns_names->clear();
  if (ip_addrs)
    ip_addrs->clear();

  CSSMCachedCertificate cached_cert;
  OSStatus status = cached_cert.Init(cert_handle_);
  if (status)
    return;
  CSSMFieldValue subject_alt_name;
  status = cached_cert.GetField(&CSSMOID_SubjectAltName, &subject_alt_name);
  if (status || !subject_alt_name.field())
    return;
  const CSSM_X509_EXTENSION* cssm_ext =
      subject_alt_name.GetAs<CSSM_X509_EXTENSION>();
  if (!cssm_ext || !cssm_ext->value.parsedValue)
    return;
  const CE_GeneralNames* alt_name =
      reinterpret_cast<const CE_GeneralNames*>(cssm_ext->value.parsedValue);

  for (size_t name = 0; name < alt_name->numNames; ++name) {
    const CE_GeneralName& name_struct = alt_name->generalName[name];
    const CSSM_DATA& name_data = name_struct.name;
    // DNSName and IPAddress are encoded as IA5String and OCTET STRINGs
    // respectively, both of which can be byte copied from
    // CSSM_DATA::data into the appropriate output vector.
    if (dns_names && name_struct.nameType == GNT_DNSName) {
      dns_names->push_back(std::string(
          reinterpret_cast<const char*>(name_data.Data),
          name_data.Length));
    } else if (ip_addrs && name_struct.nameType == GNT_IPAddress) {
      ip_addrs->push_back(std::string(
          reinterpret_cast<const char*>(name_data.Data),
          name_data.Length));
    }
  }
}

int X509Certificate::VerifyInternal(const std::string& hostname,
                                    int flags,
                                    CRLSet* crl_set,
                                    CertVerifyResult* verify_result) const {
  ScopedCFTypeRef<CFArrayRef> trust_policies;
  OSStatus status = CreateTrustPolicies(hostname, flags, &trust_policies);
  if (status)
    return NetErrorFromOSStatus(status);

  // Create and configure a SecTrustRef, which takes our certificate(s)
  // and our SSL SecPolicyRef. SecTrustCreateWithCertificates() takes an
  // array of certificates, the first of which is the certificate we're
  // verifying, and the subsequent (optional) certificates are used for
  // chain building.
  ScopedCFTypeRef<CFArrayRef> cert_array(CreateOSCertChainForCert());

  // From here on, only one thread can be active at a time. We have had a number
  // of sporadic crashes in the SecTrustEvaluate call below, way down inside
  // Apple's cert code, which we suspect are caused by a thread-safety issue.
  // So as a speculative fix allow only one thread to use SecTrust on this cert.
  base::AutoLock lock(verification_lock_);

  SecTrustRef trust_ref = NULL;
  status = SecTrustCreateWithCertificates(cert_array, trust_policies,
                                          &trust_ref);
  if (status)
    return NetErrorFromOSStatus(status);
  ScopedCFTypeRef<SecTrustRef> scoped_trust_ref(trust_ref);

  if (TestRootCerts::HasInstance()) {
    status = TestRootCerts::GetInstance()->FixupSecTrustRef(trust_ref);
    if (status)
      return NetErrorFromOSStatus(status);
  }

  CSSM_APPLE_TP_ACTION_DATA tp_action_data;
  memset(&tp_action_data, 0, sizeof(tp_action_data));
  tp_action_data.Version = CSSM_APPLE_TP_ACTION_VERSION;
  // Allow CSSM to download any missing intermediate certificates if an
  // authorityInfoAccess extension or issuerAltName extension is present.
  tp_action_data.ActionFlags = CSSM_TP_ACTION_FETCH_CERT_FROM_NET |
                               CSSM_TP_ACTION_TRUST_SETTINGS;

  if (flags & VERIFY_REV_CHECKING_ENABLED) {
    // Require a positive result from an OCSP responder or a CRL (or both)
    // for every certificate in the chain. The Apple TP automatically
    // excludes the self-signed root from this requirement. If a certificate
    // is missing both a crlDistributionPoints extension and an
    // authorityInfoAccess extension with an OCSP responder URL, then we
    // will get a kSecTrustResultRecoverableTrustFailure back from
    // SecTrustEvaluate(), with a
    // CSSMERR_APPLETP_INCOMPLETE_REVOCATION_CHECK error code. In that case,
    // we'll set our own result to include
    // CERT_STATUS_NO_REVOCATION_MECHANISM. If one or both extensions are
    // present, and a check fails (server unavailable, OCSP retry later,
    // signature mismatch), then we'll set our own result to include
    // CERT_STATUS_UNABLE_TO_CHECK_REVOCATION.
    tp_action_data.ActionFlags |= CSSM_TP_ACTION_REQUIRE_REV_PER_CERT;
    verify_result->cert_status |= CERT_STATUS_REV_CHECKING_ENABLED;
  } else {
    // EV requires revocation checking.
    // Note, under the hood, SecTrustEvaluate() will modify the OCSP options
    // so as to attempt OCSP checking if it believes a certificate may chain
    // to an EV root. However, because network fetches are disabled in
    // CreateTrustPolicies() when revocation checking is disabled, these
    // will only go against the local cache.
    flags &= ~VERIFY_EV_CERT;
  }

  CFDataRef action_data_ref =
      CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
                                  reinterpret_cast<UInt8*>(&tp_action_data),
                                  sizeof(tp_action_data), kCFAllocatorNull);
  if (!action_data_ref)
    return ERR_OUT_OF_MEMORY;
  ScopedCFTypeRef<CFDataRef> scoped_action_data_ref(action_data_ref);
  status = SecTrustSetParameters(trust_ref, CSSM_TP_ACTION_DEFAULT,
                                 action_data_ref);
  if (status)
    return NetErrorFromOSStatus(status);

  // Verify the certificate. A non-zero result from SecTrustGetResult()
  // indicates that some fatal error occurred and the chain couldn't be
  // processed, not that the chain contains no errors. We need to examine the
  // output of SecTrustGetResult() to determine that.
  SecTrustResultType trust_result;
  status = SecTrustEvaluate(trust_ref, &trust_result);
  if (status)
    return NetErrorFromOSStatus(status);
  CFArrayRef completed_chain = NULL;
  CSSM_TP_APPLE_EVIDENCE_INFO* chain_info;
  status = SecTrustGetResult(trust_ref, &trust_result, &completed_chain,
                             &chain_info);
  if (status)
    return NetErrorFromOSStatus(status);
  ScopedCFTypeRef<CFArrayRef> scoped_completed_chain(completed_chain);

  GetCertChainInfo(scoped_completed_chain.get(), chain_info, verify_result);

  // Evaluate the results
  OSStatus cssm_result;
  switch (trust_result) {
    case kSecTrustResultUnspecified:
    case kSecTrustResultProceed:
      // Certificate chain is valid and trusted ("unspecified" indicates that
      // the user has not explicitly set a trust setting)
      break;

    case kSecTrustResultDeny:
    case kSecTrustResultConfirm:
      // Certificate chain is explicitly untrusted. For kSecTrustResultConfirm,
      // we're following what Secure Transport does and treating it as
      // "deny".
      verify_result->cert_status |= CERT_STATUS_AUTHORITY_INVALID;
      break;

    case kSecTrustResultRecoverableTrustFailure:
      // Certificate chain has a failure that can be overridden by the user.
      status = SecTrustGetCssmResultCode(trust_ref, &cssm_result);
      if (status)
        return NetErrorFromOSStatus(status);
      verify_result->cert_status |= CertStatusFromOSStatus(cssm_result);
      // Walk the chain of error codes in the CSSM_TP_APPLE_EVIDENCE_INFO
      // structure which can catch multiple errors from each certificate.
      for (CFIndex index = 0, chain_count = CFArrayGetCount(completed_chain);
           index < chain_count; ++index) {
        if (chain_info[index].StatusBits & CSSM_CERT_STATUS_EXPIRED ||
            chain_info[index].StatusBits & CSSM_CERT_STATUS_NOT_VALID_YET)
          verify_result->cert_status |= CERT_STATUS_DATE_INVALID;
        if (!IsCertStatusError(verify_result->cert_status) &&
            chain_info[index].NumStatusCodes == 0) {
          LOG(WARNING) << "chain_info[" << index << "].NumStatusCodes is 0"
                          ", chain_info[" << index << "].StatusBits is "
                       << chain_info[index].StatusBits;
        }
        for (uint32 status_code_index = 0;
             status_code_index < chain_info[index].NumStatusCodes;
             ++status_code_index) {
          verify_result->cert_status |= CertStatusFromOSStatus(
              chain_info[index].StatusCodes[status_code_index]);
        }
      }
      if (!IsCertStatusError(verify_result->cert_status)) {
        LOG(ERROR) << "cssm_result=" << cssm_result;
        verify_result->cert_status |= CERT_STATUS_INVALID;
        NOTREACHED();
      }
      break;

    default:
      status = SecTrustGetCssmResultCode(trust_ref, &cssm_result);
      if (status)
        return NetErrorFromOSStatus(status);
      verify_result->cert_status |= CertStatusFromOSStatus(cssm_result);
      if (!IsCertStatusError(verify_result->cert_status)) {
        LOG(WARNING) << "trust_result=" << trust_result;
        verify_result->cert_status |= CERT_STATUS_INVALID;
      }
      break;
  }

  // Perform hostname verification independent of SecTrustEvaluate. In order to
  // do so, mask off any reported name errors first.
  verify_result->cert_status &= ~CERT_STATUS_COMMON_NAME_INVALID;
  if (!VerifyNameMatch(hostname))
    verify_result->cert_status |= CERT_STATUS_COMMON_NAME_INVALID;

  // TODO(wtc): Suppress CERT_STATUS_NO_REVOCATION_MECHANISM for now to be
  // compatible with Windows, which in turn implements this behavior to be
  // compatible with WinHTTP, which doesn't report this error (bug 3004).
  verify_result->cert_status &= ~CERT_STATUS_NO_REVOCATION_MECHANISM;

  if (IsCertStatusError(verify_result->cert_status))
    return MapCertStatusToNetError(verify_result->cert_status);

  if (flags & VERIFY_EV_CERT) {
    // Determine the certificate's EV status using SecTrustCopyExtendedResult(),
    // which we need to look up because the function wasn't added until
    // Mac OS X 10.5.7.
    // Note: "ExtendedResult" means extended validation results.
    CFBundleRef bundle =
        CFBundleGetBundleWithIdentifier(CFSTR("com.apple.security"));
    if (bundle) {
      SecTrustCopyExtendedResultFuncPtr copy_extended_result =
          reinterpret_cast<SecTrustCopyExtendedResultFuncPtr>(
              CFBundleGetFunctionPointerForName(bundle,
                  CFSTR("SecTrustCopyExtendedResult")));
      if (copy_extended_result) {
        CFDictionaryRef ev_dict = NULL;
        status = copy_extended_result(trust_ref, &ev_dict);
        if (!status && ev_dict) {
          // The returned dictionary contains the EV organization name from the
          // server certificate, which we don't need at this point (and we
          // have other ways to access, anyway). All we care is that
          // SecTrustCopyExtendedResult() returned noErr and a non-NULL
          // dictionary.
          CFRelease(ev_dict);
          verify_result->cert_status |= CERT_STATUS_IS_EV;
        }
      }
    }
  }

  AppendPublicKeyHashes(completed_chain, &verify_result->public_key_hashes);
  verify_result->is_issued_by_known_root = IsIssuedByKnownRoot(completed_chain);

  return OK;
}

// static
bool X509Certificate::GetDEREncoded(X509Certificate::OSCertHandle cert_handle,
                                    std::string* encoded) {
  CSSM_DATA der_data;
  if (SecCertificateGetData(cert_handle, &der_data) != noErr)
    return false;
  encoded->assign(reinterpret_cast<char*>(der_data.Data),
                  der_data.Length);
  return true;
}

// static
bool X509Certificate::IsSameOSCert(X509Certificate::OSCertHandle a,
                                   X509Certificate::OSCertHandle b) {
  DCHECK(a && b);
  if (a == b)
    return true;
  if (CFEqual(a, b))
    return true;
  CSSM_DATA a_data, b_data;
  return SecCertificateGetData(a, &a_data) == noErr &&
      SecCertificateGetData(b, &b_data) == noErr &&
      a_data.Length == b_data.Length &&
      memcmp(a_data.Data, b_data.Data, a_data.Length) == 0;
}

// static
X509Certificate::OSCertHandle X509Certificate::CreateOSCertHandleFromBytes(
    const char* data, int length) {
  CSSM_DATA cert_data;
  cert_data.Data = const_cast<uint8*>(reinterpret_cast<const uint8*>(data));
  cert_data.Length = length;

  OSCertHandle cert_handle = NULL;
  OSStatus status = SecCertificateCreateFromData(&cert_data,
                                                 CSSM_CERT_X_509v3,
                                                 CSSM_CERT_ENCODING_DER,
                                                 &cert_handle);
  if (status != noErr)
    return NULL;
  if (!IsValidOSCertHandle(cert_handle)) {
    CFRelease(cert_handle);
    return NULL;
  }
  return cert_handle;
}

// static
X509Certificate::OSCertHandles X509Certificate::CreateOSCertHandlesFromBytes(
    const char* data, int length, Format format) {
  OSCertHandles results;

  switch (format) {
    case FORMAT_SINGLE_CERTIFICATE: {
      OSCertHandle handle = CreateOSCertHandleFromBytes(data, length);
      if (handle)
        results.push_back(handle);
      break;
    }
    case FORMAT_PKCS7:
      AddCertificatesFromBytes(data, length, kSecFormatPKCS7, &results);
      break;
    default:
      NOTREACHED() << "Certificate format " << format << " unimplemented";
      break;
  }

  return results;
}

// static
X509Certificate::OSCertHandle X509Certificate::DupOSCertHandle(
    OSCertHandle handle) {
  if (!handle)
    return NULL;
  return reinterpret_cast<OSCertHandle>(const_cast<void*>(CFRetain(handle)));
}

// static
void X509Certificate::FreeOSCertHandle(OSCertHandle cert_handle) {
  CFRelease(cert_handle);
}

// static
SHA1Fingerprint X509Certificate::CalculateFingerprint(
    OSCertHandle cert) {
  SHA1Fingerprint sha1;
  memset(sha1.data, 0, sizeof(sha1.data));

  CSSM_DATA cert_data;
  OSStatus status = SecCertificateGetData(cert, &cert_data);
  if (status)
    return sha1;

  DCHECK(cert_data.Data);
  DCHECK_NE(cert_data.Length, 0U);

  CC_SHA1(cert_data.Data, cert_data.Length, sha1.data);

  return sha1;
}

// static
SHA1Fingerprint X509Certificate::CalculateCAFingerprint(
    const OSCertHandles& intermediates) {
  SHA1Fingerprint sha1;
  memset(sha1.data, 0, sizeof(sha1.data));

  // The CC_SHA(3cc) man page says all CC_SHA1_xxx routines return 1, so
  // we don't check their return values.
  CC_SHA1_CTX sha1_ctx;
  CC_SHA1_Init(&sha1_ctx);
  CSSM_DATA cert_data;
  for (size_t i = 0; i < intermediates.size(); ++i) {
    OSStatus status = SecCertificateGetData(intermediates[i], &cert_data);
    if (status)
      return sha1;
    CC_SHA1_Update(&sha1_ctx, cert_data.Data, cert_data.Length);
  }
  CC_SHA1_Final(sha1.data, &sha1_ctx);

  return sha1;
}

bool X509Certificate::SupportsSSLClientAuth() const {
  CSSMCachedCertificate cached_cert;
  OSStatus status = cached_cert.Init(cert_handle_);
  if (status)
    return false;

  // RFC5280 says to take the intersection of the two extensions.
  //
  // Our underlying crypto libraries don't expose
  // ClientCertificateType, so for now we will not support fixed
  // Diffie-Hellman mechanisms. For rsa_sign, we need the
  // digitalSignature bit.
  //
  // In particular, if a key has the nonRepudiation bit and not the
  // digitalSignature one, we will not offer it to the user.
  CSSMFieldValue key_usage;
  status = cached_cert.GetField(&CSSMOID_KeyUsage, &key_usage);
  if (status == CSSM_OK && key_usage.field()) {
    const CSSM_X509_EXTENSION* ext = key_usage.GetAs<CSSM_X509_EXTENSION>();
    const CE_KeyUsage* key_usage_value =
        reinterpret_cast<const CE_KeyUsage*>(ext->value.parsedValue);
    if (!((*key_usage_value) & CE_KU_DigitalSignature))
      return false;
  }

  status = cached_cert.GetField(&CSSMOID_ExtendedKeyUsage, &key_usage);
  if (status == CSSM_OK && key_usage.field()) {
    const CSSM_X509_EXTENSION* ext = key_usage.GetAs<CSSM_X509_EXTENSION>();
    const CE_ExtendedKeyUsage* ext_key_usage =
        reinterpret_cast<const CE_ExtendedKeyUsage*>(ext->value.parsedValue);
    if (!ExtendedKeyUsageAllows(ext_key_usage, &CSSMOID_ClientAuth))
      return false;
  }
  return true;
}

bool X509Certificate::IsIssuedBy(
    const std::vector<CertPrincipal>& valid_issuers) {
  // Get the cert's issuer chain.
  CFArrayRef cert_chain = NULL;
  OSStatus result;
  result = CopyCertChain(os_cert_handle(), &cert_chain);
  if (result)
    return false;
  ScopedCFTypeRef<CFArrayRef> scoped_cert_chain(cert_chain);

  // Check all the certs in the chain for a match.
  int n = CFArrayGetCount(cert_chain);
  for (int i = 0; i < n; ++i) {
    SecCertificateRef cert_handle = reinterpret_cast<SecCertificateRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(cert_chain, i)));
    scoped_refptr<X509Certificate> cert(X509Certificate::CreateFromHandle(
        cert_handle, X509Certificate::OSCertHandles()));
    for (unsigned j = 0; j < valid_issuers.size(); j++) {
      if (cert->issuer().Matches(valid_issuers[j]))
        return true;
    }
  }
  return false;
}

// static
OSStatus X509Certificate::CreateSSLClientPolicy(SecPolicyRef* policy) {
  CSSM_APPLE_TP_SSL_OPTIONS tp_ssl_options;
  memset(&tp_ssl_options, 0, sizeof(tp_ssl_options));
  tp_ssl_options.Version = CSSM_APPLE_TP_SSL_OPTS_VERSION;
  tp_ssl_options.Flags |= CSSM_APPLE_TP_SSL_CLIENT;

  return CreatePolicy(&CSSMOID_APPLE_TP_SSL, &tp_ssl_options,
                      sizeof(tp_ssl_options), policy);
}

// static
OSStatus X509Certificate::CreateBasicX509Policy(SecPolicyRef* policy) {
  return CreatePolicy(&CSSMOID_APPLE_X509_BASIC, NULL, 0, policy);
}

// static
OSStatus X509Certificate::CreateRevocationPolicies(
    bool enable_revocation_checking,
    CFMutableArrayRef policies) {
  // In order to actually disable revocation checking, the SecTrustRef must
  // have at least one revocation policy associated with it. If none are
  // present, the Apple TP will add policies according to the system
  // preferences, which will enable revocation checking even if the caller
  // explicitly disabled it. An OCSP policy is used, rather than a CRL policy,
  // because the Apple TP will force an OCSP policy to be present and enabled
  // if it believes the certificate may chain to an EV root. By explicitly
  // disabling network and OCSP cache access, then even if the Apple TP
  // enables OCSP checking, no revocation checking will actually succeed.
  CSSM_APPLE_TP_OCSP_OPTIONS tp_ocsp_options;
  memset(&tp_ocsp_options, 0, sizeof(tp_ocsp_options));
  tp_ocsp_options.Version = CSSM_APPLE_TP_OCSP_OPTS_VERSION;

  if (enable_revocation_checking) {
    // The default for the OCSP policy is to fetch responses via the network,
    // unlike the CRL policy default. The policy is further modified to
    // prefer OCSP over CRLs, if both are specified on the certificate. This
    // is because an OCSP response is both sufficient and typically
    // significantly smaller than the CRL counterpart.
    tp_ocsp_options.Flags = CSSM_TP_ACTION_OCSP_SUFFICIENT;
  } else {
    // Effectively disable OCSP checking by making it impossible to get an
    // OCSP response. Even if the Apple TP forces OCSP, no checking will
    // be able to succeed. If this happens, the Apple TP will report an error
    // that OCSP was unavailable, but this will be handled and suppressed in
    // X509Certificate::Verify().
    tp_ocsp_options.Flags = CSSM_TP_ACTION_OCSP_DISABLE_NET |
                            CSSM_TP_ACTION_OCSP_CACHE_READ_DISABLE;
  }

  SecPolicyRef ocsp_policy;
  OSStatus status = CreatePolicy(&CSSMOID_APPLE_TP_REVOCATION_OCSP,
                                 &tp_ocsp_options, sizeof(tp_ocsp_options),
                                 &ocsp_policy);
  if (status)
    return status;
  CFArrayAppendValue(policies, ocsp_policy);
  CFRelease(ocsp_policy);

  if (enable_revocation_checking) {
    CSSM_APPLE_TP_CRL_OPTIONS tp_crl_options;
    memset(&tp_crl_options, 0, sizeof(tp_crl_options));
    tp_crl_options.Version = CSSM_APPLE_TP_CRL_OPTS_VERSION;
    tp_crl_options.CrlFlags = CSSM_TP_ACTION_FETCH_CRL_FROM_NET;

    SecPolicyRef crl_policy;
    status = CreatePolicy(&CSSMOID_APPLE_TP_REVOCATION_CRL, &tp_crl_options,
                          sizeof(tp_crl_options), &crl_policy);
    if (status)
      return status;
    CFArrayAppendValue(policies, crl_policy);
    CFRelease(crl_policy);
  }

  return status;
}


// static
bool X509Certificate::GetSSLClientCertificates(
    const std::string& server_domain,
    const std::vector<CertPrincipal>& valid_issuers,
    CertificateList* certs) {
  ScopedCFTypeRef<SecIdentityRef> preferred_identity;
  if (!server_domain.empty()) {
    // See if there's an identity preference for this domain:
    ScopedCFTypeRef<CFStringRef> domain_str(
        base::SysUTF8ToCFStringRef("https://" + server_domain));
    SecIdentityRef identity = NULL;
    // While SecIdentityCopyPreferences appears to take a list of CA issuers
    // to restrict the identity search to, within Security.framework the
    // argument is ignored and filtering unimplemented. See
    // SecIdentity.cpp in libsecurity_keychain, specifically
    // _SecIdentityCopyPreferenceMatchingName().
    if (SecIdentityCopyPreference(domain_str, 0, NULL, &identity) == noErr)
      preferred_identity.reset(identity);
  }

  // Now enumerate the identities in the available keychains.
  SecIdentitySearchRef search = nil;
  OSStatus err = SecIdentitySearchCreate(NULL, CSSM_KEYUSE_SIGN, &search);
  ScopedCFTypeRef<SecIdentitySearchRef> scoped_search(search);
  while (!err) {
    SecIdentityRef identity = NULL;
    err = SecIdentitySearchCopyNext(search, &identity);
    if (err)
      break;
    ScopedCFTypeRef<SecIdentityRef> scoped_identity(identity);

    SecCertificateRef cert_handle;
    err = SecIdentityCopyCertificate(identity, &cert_handle);
    if (err != noErr)
      continue;
    ScopedCFTypeRef<SecCertificateRef> scoped_cert_handle(cert_handle);

    scoped_refptr<X509Certificate> cert(
        CreateFromHandle(cert_handle, OSCertHandles()));
    if (cert->HasExpired() || !cert->SupportsSSLClientAuth())
      continue;

    // Skip duplicates (a cert may be in multiple keychains).
    const SHA1Fingerprint& fingerprint = cert->fingerprint();
    unsigned i;
    for (i = 0; i < certs->size(); ++i) {
      if ((*certs)[i]->fingerprint().Equals(fingerprint))
        break;
    }
    if (i < certs->size())
      continue;

    bool is_preferred = preferred_identity &&
        CFEqual(preferred_identity, identity);

    // Make sure the issuer matches valid_issuers, if given.
    // But an explicit cert preference overrides this.
    if (!is_preferred &&
        !valid_issuers.empty() &&
        !cert->IsIssuedBy(valid_issuers))
      continue;

    // The cert passes, so add it to the vector.
    // If it's the preferred identity, add it at the start (so it'll be
    // selected by default in the UI.)
    if (is_preferred)
      certs->insert(certs->begin(), cert);
    else
      certs->push_back(cert);
  }

  if (err != errSecItemNotFound) {
    LOG(ERROR) << "SecIdentitySearch error " << err;
    return false;
  }
  return true;
}

CFArrayRef X509Certificate::CreateClientCertificateChain() const {
  // Initialize the result array with just the IdentityRef of the receiver:
  OSStatus result;
  SecIdentityRef identity;
  result = SecIdentityCreateWithCertificate(NULL, cert_handle_, &identity);
  if (result) {
    LOG(ERROR) << "SecIdentityCreateWithCertificate error " << result;
    return NULL;
  }
  ScopedCFTypeRef<CFMutableArrayRef> chain(
      CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks));
  CFArrayAppendValue(chain, identity);

  CFArrayRef cert_chain = NULL;
  result = CopyCertChain(cert_handle_, &cert_chain);
  ScopedCFTypeRef<CFArrayRef> scoped_cert_chain(cert_chain);
  if (result) {
    LOG(ERROR) << "CreateIdentityCertificateChain error " << result;
    return chain.release();
  }

  // Append the intermediate certs from SecTrust to the result array:
  if (cert_chain) {
    int chain_count = CFArrayGetCount(cert_chain);
    if (chain_count > 1) {
      CFArrayAppendArray(chain,
                         cert_chain,
                         CFRangeMake(1, chain_count - 1));
    }
  }

  return chain.release();
}

CFArrayRef X509Certificate::CreateOSCertChainForCert() const {
  CFMutableArrayRef cert_list =
      CFArrayCreateMutable(kCFAllocatorDefault, 0,
                           &kCFTypeArrayCallBacks);
  if (!cert_list)
    return NULL;

  CFArrayAppendValue(cert_list, os_cert_handle());
  for (size_t i = 0; i < intermediate_ca_certs_.size(); ++i)
    CFArrayAppendValue(cert_list, intermediate_ca_certs_[i]);

  return cert_list;
}

// static
X509Certificate::OSCertHandle
X509Certificate::ReadOSCertHandleFromPickle(const Pickle& pickle,
                                            void** pickle_iter) {
  const char* data;
  int length;
  if (!pickle.ReadData(pickle_iter, &data, &length))
    return NULL;

  return CreateOSCertHandleFromBytes(data, length);
}

// static
bool X509Certificate::WriteOSCertHandleToPickle(OSCertHandle cert_handle,
                                                Pickle* pickle) {
  CSSM_DATA cert_data;
  OSStatus status = SecCertificateGetData(cert_handle, &cert_data);
  if (status)
    return false;

  return pickle->WriteData(reinterpret_cast<char*>(cert_data.Data),
                           cert_data.Length);
}

}  // namespace net
