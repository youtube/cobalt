// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/singleton.h"
#include "base/pickle.h"
#include "base/sha1.h"
#include "base/sys_string_conversions.h"
#include "crypto/cssm_init.h"
#include "crypto/nss_util.h"
#include "crypto/rsa_private_key.h"
#include "net/base/x509_util_mac.h"
#include "third_party/nss/mozilla/security/nss/lib/certdb/cert.h"

using base::mac::ScopedCFTypeRef;
using base::Time;

namespace net {

namespace {

void GetCertDistinguishedName(
    const x509_util::CSSMCachedCertificate& cached_cert,
    const CSSM_OID* oid,
    CertPrincipal* result) {
  x509_util::CSSMFieldValue distinguished_name;
  OSStatus status = cached_cert.GetField(oid, &distinguished_name);
  if (status || !distinguished_name.field())
    return;
  result->ParseDistinguishedName(distinguished_name.field()->Data,
                                 distinguished_name.field()->Length);
}

void GetCertDateForOID(const x509_util::CSSMCachedCertificate& cached_cert,
                       const CSSM_OID* oid,
                       Time* result) {
  *result = Time::Time();

  x509_util::CSSMFieldValue field;
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

std::string GetCertSerialNumber(
    const x509_util::CSSMCachedCertificate& cached_cert) {
  x509_util::CSSMFieldValue serial_number;
  OSStatus status = cached_cert.GetField(&CSSMOID_X509V1SerialNumber,
                                         &serial_number);
  if (status || !serial_number.field())
    return std::string();

  return std::string(
      reinterpret_cast<const char*>(serial_number.field()->Data),
      serial_number.field()->Length);
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
  OSStatus result = x509_util::CreateSSLClientPolicy(&ssl_policy);
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
    OSSTATUS_DLOG(WARNING, status)
        << "Unable to import items from data of length " << length;
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

}  // namespace

void X509Certificate::Initialize() {
  x509_util::CSSMCachedCertificate cached_cert;
  if (cached_cert.Init(cert_handle_) == CSSM_OK) {
    GetCertDistinguishedName(cached_cert, &CSSMOID_X509V1SubjectNameStd,
                             &subject_);
    GetCertDistinguishedName(cached_cert, &CSSMOID_X509V1IssuerNameStd,
                             &issuer_);
    GetCertDateForOID(cached_cert, &CSSMOID_X509V1ValidityNotBefore,
                      &valid_start_);
    GetCertDateForOID(cached_cert, &CSSMOID_X509V1ValidityNotAfter,
                      &valid_expiry_);
    serial_number_ = GetCertSerialNumber(cached_cert);
  }

  fingerprint_ = CalculateFingerprint(cert_handle_);
  ca_fingerprint_ = CalculateCAFingerprint(intermediate_ca_certs_);
}

// static
X509Certificate* X509Certificate::CreateSelfSigned(
    crypto::RSAPrivateKey* key,
    const std::string& subject,
    uint32 serial_number,
    base::TimeDelta valid_duration) {
  DCHECK(key);
  DCHECK(!subject.empty());

  if (valid_duration.InSeconds() > kuint32max) {
     LOG(ERROR) << "valid_duration too big " << valid_duration.InSeconds();
     valid_duration = base::TimeDelta::FromSeconds(kuint32max);
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
  CSSM_TP_RESULT_SET* resultSet = NULL;
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
  ScopedCFTypeRef<SecCertificateRef> scoped_cert;
  SecCertificateRef certificate_ref = NULL;
  OSStatus os_status =
      SecCertificateCreateFromData(&encCert->CertBlob, encCert->CertType,
                                   encCert->CertEncoding, &certificate_ref);
  if (os_status != 0) {
    OSSTATUS_DLOG(ERROR, os_status) << "SecCertificateCreateFromData failed";
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

  x509_util::CSSMCachedCertificate cached_cert;
  OSStatus status = cached_cert.Init(cert_handle_);
  if (status)
    return;
  x509_util::CSSMFieldValue subject_alt_name;
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
SHA1HashValue X509Certificate::CalculateFingerprint(
    OSCertHandle cert) {
  SHA1HashValue sha1;
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
SHA1HashValue X509Certificate::CalculateCAFingerprint(
    const OSCertHandles& intermediates) {
  SHA1HashValue sha1;
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
  x509_util::CSSMCachedCertificate cached_cert;
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
  x509_util::CSSMFieldValue key_usage;
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
  OSStatus result = CopyCertChain(os_cert_handle(), &cert_chain);
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
    const SHA1HashValue& fingerprint = cert->fingerprint();
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
    if (!valid_issuers.empty() && !cert->IsIssuedBy(valid_issuers))
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
    OSSTATUS_LOG(ERROR, err) << "SecIdentitySearch error";
    return false;
  }
  return true;
}

CFArrayRef X509Certificate::CreateClientCertificateChain() const {
  // Initialize the result array with just the IdentityRef of the receiver:
  SecIdentityRef identity;
  OSStatus result =
      SecIdentityCreateWithCertificate(NULL, cert_handle_, &identity);
  if (result) {
    OSSTATUS_LOG(ERROR, result) << "SecIdentityCreateWithCertificate error";
    return NULL;
  }
  ScopedCFTypeRef<CFMutableArrayRef> chain(
      CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks));
  CFArrayAppendValue(chain, identity);

  CFArrayRef cert_chain = NULL;
  result = CopyCertChain(cert_handle_, &cert_chain);
  ScopedCFTypeRef<CFArrayRef> scoped_cert_chain(cert_chain);
  if (result) {
    OSSTATUS_LOG(ERROR, result) << "CreateIdentityCertificateChain error";
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
X509Certificate::ReadOSCertHandleFromPickle(PickleIterator* pickle_iter) {
  const char* data;
  int length;
  if (!pickle_iter->ReadData(&data, &length))
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

// static
void X509Certificate::GetPublicKeyInfo(OSCertHandle cert_handle,
                                       size_t* size_bits,
                                       PublicKeyType* type) {
  // Since we might fail, set the output parameters to default values first.
  *type = kPublicKeyTypeUnknown;
  *size_bits = 0;

  SecKeyRef key;
  OSStatus status = SecCertificateCopyPublicKey(cert_handle, &key);
  if (status) {
    NOTREACHED() << "SecCertificateCopyPublicKey failed: " << status;
    return;
  }
  ScopedCFTypeRef<SecKeyRef> scoped_key(key);

  const CSSM_KEY* cssm_key;
  status = SecKeyGetCSSMKey(key, &cssm_key);
  if (status) {
    NOTREACHED() << "SecKeyGetCSSMKey failed: " << status;
    return;
  }

  *size_bits = cssm_key->KeyHeader.LogicalKeySizeInBits;

  switch (cssm_key->KeyHeader.AlgorithmId) {
    case CSSM_ALGID_RSA:
      *type = kPublicKeyTypeRSA;
      break;
    case CSSM_ALGID_DSA:
      *type = kPublicKeyTypeDSA;
      break;
    case CSSM_ALGID_ECDSA:
      *type = kPublicKeyTypeECDSA;
      break;
    case CSSM_ALGID_DH:
      *type = kPublicKeyTypeDH;
      break;
    default:
      *type = kPublicKeyTypeUnknown;
      *size_bits = 0;
      break;
  }
}

}  // namespace net
