// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_certificate.h"

#include <CommonCrypto/CommonDigest.h>
#include <Security/Security.h>
#include <time.h>

#include <vector>

#include "base/crypto/cssm_init.h"
#include "base/crypto/rsa_private_key.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/nss_util.h"
#include "base/pickle.h"
#include "base/singleton.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/sys_string_conversions.h"
#include "net/base/cert_status_flags.h"
#include "net/base/cert_verify_result.h"
#include "net/base/net_errors.h"
#include "net/base/test_root_certs.h"
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
    default:
      LOG(ERROR) << "Unknown error " << status << " mapped to net::ERR_FAILED";
      return ERR_FAILED;
  }
}

int CertStatusFromOSStatus(OSStatus status) {
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
    case CSSMERR_APPLETP_INCOMPLETE_REVOCATION_CHECK:
    case CSSMERR_APPLETP_OCSP_UNAVAILABLE:
      return CERT_STATUS_NO_REVOCATION_MECHANISM;

    case CSSMERR_APPLETP_CRL_NOT_TRUSTED:
    case CSSMERR_APPLETP_CRL_SERVER_DOWN:
    case CSSMERR_APPLETP_CRL_NOT_VALID_YET:
    case CSSMERR_APPLETP_NETWORK_FAILURE:
    case CSSMERR_APPLETP_OCSP_BAD_RESPONSE:
    case CSSMERR_APPLETP_OCSP_NO_SIGNER:
    case CSSMERR_APPLETP_OCSP_RESP_UNAUTHORIZED:
    case CSSMERR_APPLETP_OCSP_RESP_SIG_REQUIRED:
    case CSSMERR_APPLETP_OCSP_RESP_MALFORMED_REQ:
    case CSSMERR_APPLETP_OCSP_RESP_INTERNAL_ERR:
    case CSSMERR_APPLETP_OCSP_RESP_TRY_LATER:
      // We asked for a revocation check, but didn't get it.
      return CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;

    default:
      // Failure was due to something Chromium doesn't define a
      // specific status for (such as basic constraints violation, or
      // unknown critical extension)
      return CERT_STATUS_INVALID;
  }
}

bool OverrideHostnameMismatch(const std::string& hostname,
                              std::vector<std::string>* dns_names) {
  // SecTrustEvaluate() does not check dotted IP addresses. If
  // hostname is provided as, say, 127.0.0.1, then the error
  // CSSMERR_APPLETP_HOSTNAME_MISMATCH will always be returned,
  // even if the certificate contains 127.0.0.1 as one of its names.
  // We, however, want to allow that behavior. SecTrustEvaluate()
  // only checks for digits and dots when considering whether a
  // hostname is an IP address, so IPv6 and hex addresses go through
  // its normal comparison.
  bool is_dotted_ip = true;
  bool override_hostname_mismatch = false;
  for (std::string::const_iterator c = hostname.begin();
       c != hostname.end() && is_dotted_ip; ++c)
    is_dotted_ip = (*c >= '0' && *c <= '9') || *c == '.';
  if (is_dotted_ip) {
    for (std::vector<std::string>::const_iterator name = dns_names->begin();
         name != dns_names->end() && !override_hostname_mismatch; ++name)
      override_hostname_mismatch = (*name == hostname);
  }
  return override_hostname_mismatch;
}

struct CSSMFields {
  CSSMFields() : cl_handle(NULL), num_of_fields(0), fields(NULL) {}
  ~CSSMFields() {
    if (cl_handle)
      CSSM_CL_FreeFields(cl_handle, num_of_fields, &fields);
  }

  CSSM_CL_HANDLE cl_handle;
  uint32 num_of_fields;
  CSSM_FIELD_PTR fields;
};

OSStatus GetCertFields(X509Certificate::OSCertHandle cert_handle,
                       CSSMFields* fields) {
  DCHECK(cert_handle);
  DCHECK(fields);

  CSSM_DATA cert_data;
  OSStatus status = SecCertificateGetData(cert_handle, &cert_data);
  if (status)
    return status;

  status = SecCertificateGetCLHandle(cert_handle, &fields->cl_handle);
  if (status) {
    DCHECK(!fields->cl_handle);
    return status;
  }

  status = CSSM_CL_CertGetAllFields(fields->cl_handle, &cert_data,
                                    &fields->num_of_fields, &fields->fields);
  return status;
}

void GetCertGeneralNamesForOID(X509Certificate::OSCertHandle cert_handle,
                               CSSM_OID oid, CE_GeneralNameType name_type,
                               std::vector<std::string>* result) {
  // For future extension: We only support general names of types
  // GNT_RFC822Name, GNT_DNSName or GNT_URI.
  DCHECK(name_type == GNT_RFC822Name ||
         name_type == GNT_DNSName ||
         name_type == GNT_URI);

  CSSMFields fields;
  OSStatus status = GetCertFields(cert_handle, &fields);
  if (status)
    return;

  for (size_t field = 0; field < fields.num_of_fields; ++field) {
    if (CSSMOIDEqual(&fields.fields[field].FieldOid, &oid)) {
      CSSM_X509_EXTENSION_PTR cssm_ext =
          reinterpret_cast<CSSM_X509_EXTENSION_PTR>(
              fields.fields[field].FieldValue.Data);
      CE_GeneralNames* alt_name =
          reinterpret_cast<CE_GeneralNames*>(cssm_ext->value.parsedValue);

      for (size_t name = 0; name < alt_name->numNames; ++name) {
        const CE_GeneralName& name_struct = alt_name->generalName[name];
        // All of the general name types we support are encoded as
        // IA5String. In general, we should be switching off
        // |name_struct.nameType| and doing type-appropriate conversions. See
        // certextensions.h and the comment immediately preceding
        // CE_GeneralNameType for more information.
        if (name_struct.nameType == name_type) {
          const CSSM_DATA& name_data = name_struct.name;
          std::string value = std::string(
              reinterpret_cast<const char*>(name_data.Data),
              name_data.Length);
          result->push_back(value);
        }
      }
    }
  }
}

void GetCertDateForOID(X509Certificate::OSCertHandle cert_handle,
                       CSSM_OID oid, Time* result) {
  *result = Time::Time();

  CSSMFields fields;
  OSStatus status = GetCertFields(cert_handle, &fields);
  if (status)
    return;

  for (size_t field = 0; field < fields.num_of_fields; ++field) {
    if (CSSMOIDEqual(&fields.fields[field].FieldOid, &oid)) {
      CSSM_X509_TIME* x509_time = reinterpret_cast<CSSM_X509_TIME*>(
          fields.fields[field].FieldValue.Data);
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
      return;
    }
  }
}

// Creates a SecPolicyRef for the given OID, with optional value.
OSStatus CreatePolicy(const CSSM_OID* policy_OID,
                      void* option_data,
                      size_t option_length,
                      SecPolicyRef* policy) {
  SecPolicySearchRef search;
  OSStatus err = SecPolicySearchCreate(CSSM_CERT_X_509v3, policy_OID, NULL,
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
        base::CSSMFree(encCert[i].CertBlob.Data);
      }
    }
    base::CSSMFree(results_->Results);
    base::CSSMFree(results_);
  }

private:
  CSSM_TP_RESULT_SET* results_;
};

}  // namespace

void X509Certificate::Initialize() {
  const CSSM_X509_NAME* name;
  OSStatus status = SecCertificateGetSubject(cert_handle_, &name);
  if (!status)
    subject_.Parse(name);

  status = SecCertificateGetIssuer(cert_handle_, &name);
  if (!status)
    issuer_.Parse(name);

  GetCertDateForOID(cert_handle_, CSSMOID_X509V1ValidityNotBefore,
                    &valid_start_);
  GetCertDateForOID(cert_handle_, CSSMOID_X509V1ValidityNotAfter,
                    &valid_expiry_);

  fingerprint_ = CalculateFingerprint(cert_handle_);
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
  base::EnsureNSSInit();

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
  for(CSSMOIDStringVector::iterator iter = subject_name_oids.begin();
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
  certReq.cspHand = base::GetSharedCSPHandle();
  certReq.clHand = base::GetSharedCLHandle();
    // See comment about serial numbers above.
  certReq.serialNumber = serial_number & 0x7fffffff;
  certReq.numSubjectNames = cssm_subject_names.size();
  certReq.subjectNames = &cssm_subject_names[0];
  certReq.numIssuerNames = 0; // Root.
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

  CSSM_TP_HANDLE tp_handle = base::GetSharedTPHandle();
  CSSM_DATA refId;
  memset(&refId, 0, sizeof(refId));
  sint32 estTime;
  CSSM_RETURN crtn = CSSM_TP_SubmitCredRequest(tp_handle, NULL,
      CSSM_TP_AUTHORITY_REQUEST_CERTISSUE, &reqSet, &callerAuthContext,
       &estTime, &refId);
  if(crtn) {
    DLOG(ERROR) << "CSSM_TP_SubmitCredRequest failed " << crtn;
    return NULL;
  }

  CSSM_BOOL confirmRequired;
  CSSM_TP_RESULT_SET *resultSet = NULL;
  crtn = CSSM_TP_RetrieveCredResult(tp_handle, &refId, NULL, &estTime,
                                    &confirmRequired, &resultSet);
  ScopedEncodedCertResults scopedResults(resultSet);
  base::CSSMFree(refId.Data);
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

  return CreateFromHandle(
     scoped_cert, X509Certificate::SOURCE_LONE_CERT_IMPORT,
     X509Certificate::OSCertHandles());
}

void X509Certificate::Persist(Pickle* pickle) {
  CSSM_DATA cert_data;
  OSStatus status = SecCertificateGetData(cert_handle_, &cert_data);
  if (status) {
    NOTREACHED();
    return;
  }

  pickle->WriteData(reinterpret_cast<char*>(cert_data.Data), cert_data.Length);
}

void X509Certificate::GetDNSNames(std::vector<std::string>* dns_names) const {
  dns_names->clear();

  GetCertGeneralNamesForOID(cert_handle_, CSSMOID_SubjectAltName, GNT_DNSName,
                            dns_names);

  if (dns_names->empty())
    dns_names->push_back(subject_.common_name);
}

int X509Certificate::Verify(const std::string& hostname, int flags,
                            CertVerifyResult* verify_result) const {
  verify_result->Reset();

  // Create an SSL SecPolicyRef, and configure it to perform hostname
  // validation. The hostname check does 99% of what we want, with the
  // exception of dotted IPv4 addreses, which we handle ourselves below.
  CSSM_APPLE_TP_SSL_OPTIONS tp_ssl_options = {
    CSSM_APPLE_TP_SSL_OPTS_VERSION,
    hostname.size(),
    hostname.data(),
    0
  };
  SecPolicyRef ssl_policy;
  OSStatus status = CreatePolicy(&CSSMOID_APPLE_TP_SSL,
                                 &tp_ssl_options,
                                 sizeof(tp_ssl_options),
                                 &ssl_policy);
  if (status)
    return NetErrorFromOSStatus(status);
  ScopedCFTypeRef<SecPolicyRef> scoped_ssl_policy(ssl_policy);

  // Create and configure a SecTrustRef, which takes our certificate(s)
  // and our SSL SecPolicyRef. SecTrustCreateWithCertificates() takes an
  // array of certificates, the first of which is the certificate we're
  // verifying, and the subsequent (optional) certificates are used for
  // chain building.
  CFMutableArrayRef cert_array = CFArrayCreateMutable(kCFAllocatorDefault, 0,
                                                      &kCFTypeArrayCallBacks);
  if (!cert_array)
    return ERR_OUT_OF_MEMORY;
  ScopedCFTypeRef<CFArrayRef> scoped_cert_array(cert_array);
  CFArrayAppendValue(cert_array, cert_handle_);
  for (size_t i = 0; i < intermediate_ca_certs_.size(); ++i)
    CFArrayAppendValue(cert_array, intermediate_ca_certs_[i]);

  // From here on, only one thread can be active at a time. We have had a number
  // of sporadic crashes in the SecTrustEvaluate call below, way down inside
  // Apple's cert code, which we suspect are caused by a thread-safety issue.
  // So as a speculative fix allow only one thread to use SecTrust on this cert.
  base::AutoLock lock(verification_lock_);

  SecTrustRef trust_ref = NULL;
  status = SecTrustCreateWithCertificates(cert_array, ssl_policy, &trust_ref);
  if (status)
    return NetErrorFromOSStatus(status);
  ScopedCFTypeRef<SecTrustRef> scoped_trust_ref(trust_ref);

  if (TestRootCerts::HasInstance()) {
    status = TestRootCerts::GetInstance()->FixupSecTrustRef(trust_ref);
    if (status)
      return NetErrorFromOSStatus(status);
  }

  if (flags & VERIFY_REV_CHECKING_ENABLED) {
    // When called with VERIFY_REV_CHECKING_ENABLED, we ask SecTrustEvaluate()
    // to apply OCSP and CRL checking, but we're still subject to the global
    // settings, which are configured in the Keychain Access application (in
    // the Certificates tab of the Preferences dialog). If the user has
    // revocation disabled (which is the default), then we will get
    // kSecTrustResultRecoverableTrustFailure back from SecTrustEvaluate()
    // with one of a number of sub error codes indicating that revocation
    // checking did not occur. In that case, we'll set our own result to include
    // CERT_STATUS_UNABLE_TO_CHECK_REVOCATION.
    //
    // NOTE: This does not apply to EV certificates, which always get
    // revocation checks regardless of the global settings.
    verify_result->cert_status |= CERT_STATUS_REV_CHECKING_ENABLED;
    CSSM_APPLE_TP_ACTION_DATA tp_action_data = { CSSM_APPLE_TP_ACTION_VERSION };
    tp_action_data.ActionFlags = CSSM_TP_ACTION_REQUIRE_REV_PER_CERT;
    CFDataRef action_data_ref =
        CFDataCreate(NULL, reinterpret_cast<UInt8*>(&tp_action_data),
                     sizeof(tp_action_data));
    if (!action_data_ref)
      return ERR_OUT_OF_MEMORY;
    ScopedCFTypeRef<CFDataRef> scoped_action_data_ref(action_data_ref);
    status = SecTrustSetParameters(trust_ref, CSSM_TP_ACTION_DEFAULT,
                                   action_data_ref);
    if (status)
      return NetErrorFromOSStatus(status);
  }

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

  // Evaluate the results
  OSStatus cssm_result;
  bool got_certificate_error = false;
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
      switch (cssm_result) {
        case CSSMERR_TP_NOT_TRUSTED:
        case CSSMERR_TP_INVALID_ANCHOR_CERT:
          verify_result->cert_status |= CERT_STATUS_AUTHORITY_INVALID;
          break;
        case CSSMERR_TP_CERT_EXPIRED:
        case CSSMERR_TP_CERT_NOT_VALID_YET:
          verify_result->cert_status |= CERT_STATUS_DATE_INVALID;
          break;
        case CSSMERR_TP_CERT_REVOKED:
        case CSSMERR_TP_CERT_SUSPENDED:
          verify_result->cert_status |= CERT_STATUS_REVOKED;
          break;
        default:
          // Look for specific per-certificate errors below.
          break;
      }
      // Walk the chain of error codes in the CSSM_TP_APPLE_EVIDENCE_INFO
      // structure which can catch multiple errors from each certificate.
      for (CFIndex index = 0, chain_count = CFArrayGetCount(completed_chain);
           index < chain_count; ++index) {
        if (chain_info[index].StatusBits & CSSM_CERT_STATUS_EXPIRED ||
            chain_info[index].StatusBits & CSSM_CERT_STATUS_NOT_VALID_YET)
          verify_result->cert_status |= CERT_STATUS_DATE_INVALID;
        for (uint32 status_code_index = 0;
             status_code_index < chain_info[index].NumStatusCodes;
             ++status_code_index) {
          got_certificate_error = true;
          int cert_status = CertStatusFromOSStatus(
              chain_info[index].StatusCodes[status_code_index]);
          if (cert_status == CERT_STATUS_COMMON_NAME_INVALID) {
            std::vector<std::string> names;
            GetDNSNames(&names);
            if (OverrideHostnameMismatch(hostname, &names))
              cert_status = 0;
          }
          verify_result->cert_status |= cert_status;
        }
      }
      // Be paranoid and ensure that we recorded at least one certificate
      // status on receiving kSecTrustResultRecoverableTrustFailure. The
      // call to SecTrustGetCssmResultCode() should pick up when the chain
      // is not trusted and the loop through CSSM_TP_APPLE_EVIDENCE_INFO
      // should pick up everything else, but let's be safe.
      if (!verify_result->cert_status && !got_certificate_error) {
        verify_result->cert_status |= CERT_STATUS_INVALID;
        NOTREACHED();
      }
      break;

    default:
      status = SecTrustGetCssmResultCode(trust_ref, &cssm_result);
      if (status)
        return NetErrorFromOSStatus(status);
      verify_result->cert_status |= CertStatusFromOSStatus(cssm_result);
      if (!verify_result->cert_status)
        verify_result->cert_status |= CERT_STATUS_INVALID;
      break;
  }

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

  return OK;
}

bool X509Certificate::GetDEREncoded(std::string* encoded) {
  encoded->clear();
  CSSM_DATA der_data;
  if(SecCertificateGetData(cert_handle_, &der_data) == noErr) {
    encoded->append(reinterpret_cast<char*>(der_data.Data),
                    der_data.Length);
    return true;
  }
  return false;
}

bool X509Certificate::VerifyEV() const {
  // We don't call this private method, but we do need to implement it because
  // it's defined in x509_certificate.h. We perform EV checking in the
  // Verify() above.
  NOTREACHED();
  return false;
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

bool X509Certificate::SupportsSSLClientAuth() const {
  CSSMFields fields;
  if (GetCertFields(cert_handle_, &fields) != noErr)
    return false;

  // Gather the extensions we care about. We do not support
  // CSSMOID_NetscapeCertType on OS X.
  const CE_ExtendedKeyUsage* ext_key_usage = NULL;
  const CE_KeyUsage* key_usage = NULL;
  for (unsigned f = 0; f < fields.num_of_fields; ++f) {
    const CSSM_FIELD& field = fields.fields[f];
    const CSSM_X509_EXTENSION* ext =
        reinterpret_cast<const CSSM_X509_EXTENSION*>(field.FieldValue.Data);
    if (CSSMOIDEqual(&field.FieldOid, &CSSMOID_KeyUsage)) {
      key_usage = reinterpret_cast<const CE_KeyUsage*>(ext->value.parsedValue);
    } else if (CSSMOIDEqual(&field.FieldOid, &CSSMOID_ExtendedKeyUsage)) {
      ext_key_usage =
          reinterpret_cast<const CE_ExtendedKeyUsage*>(ext->value.parsedValue);
    }
  }

  // RFC5280 says to take the intersection of the two extensions.
  //
  // Our underlying crypto libraries don't expose
  // ClientCertificateType, so for now we will not support fixed
  // Diffie-Hellman mechanisms. For rsa_sign, we need the
  // digitalSignature bit.
  //
  // In particular, if a key has the nonRepudiation bit and not the
  // digitalSignature one, we will not offer it to the user.
  if (key_usage && !((*key_usage) & CE_KU_DigitalSignature))
    return false;
  if (ext_key_usage && !ExtendedKeyUsageAllows(ext_key_usage,
                                               &CSSMOID_ClientAuth))
    return false;
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
        cert_handle,
        X509Certificate::SOURCE_LONE_CERT_IMPORT,
        X509Certificate::OSCertHandles()));
    for (unsigned j = 0; j < valid_issuers.size(); j++) {
      if (cert->issuer().Matches(valid_issuers[j]))
        return true;
    }
  }
  return false;
}

// static
OSStatus X509Certificate::CreateSSLClientPolicy(SecPolicyRef* out_policy) {
  CSSM_APPLE_TP_SSL_OPTIONS tp_ssl_options = {
    CSSM_APPLE_TP_SSL_OPTS_VERSION,
    0,
    NULL,
    CSSM_APPLE_TP_SSL_CLIENT
  };
  return CreatePolicy(&CSSMOID_APPLE_TP_SSL,
                      &tp_ssl_options,
                      sizeof(tp_ssl_options),
                      out_policy);
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
        CreateFromHandle(cert_handle, SOURCE_LONE_CERT_IMPORT,
                         OSCertHandles()));
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
        valid_issuers.size() > 0 &&
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

}  // namespace net
