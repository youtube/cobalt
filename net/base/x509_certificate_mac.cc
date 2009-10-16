// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_certificate.h"

#include <CommonCrypto/CommonDigest.h>
#include <time.h>

#include "base/scoped_cftyperef.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "net/base/cert_status_flags.h"
#include "net/base/cert_verify_result.h"
#include "net/base/net_errors.h"

using base::Time;

namespace net {

class MacTrustedCertificates {
 public:
  // Sets the trusted root certificate used by tests. Call with |cert| set
  // to NULL to clear the test certificate.
  void SetTestCertificate(X509Certificate* cert) {
    AutoLock lock(lock_);
    test_certificate_ = cert;
  }

  // Returns an array containing the trusted certificates for use with
  // SecTrustSetAnchorCertificates(). Returns NULL if the system-supplied
  // list of trust anchors is acceptable (that is, there is not test
  // certificate available). Ownership follows the Create Rule (caller
  // is responsible for calling CFRelease on the non-NULL result).
  CFArrayRef CopyTrustedCertificateArray() {
    AutoLock lock(lock_);

    if (!test_certificate_)
      return NULL;

    // Failure to copy the anchor certificates or add the test certificate
    // is non-fatal; SecTrustEvaluate() will use the system anchors instead.
    CFArrayRef anchor_array;
    OSStatus status = SecTrustCopyAnchorCertificates(&anchor_array);
    if (status)
      return NULL;
    scoped_cftyperef<CFArrayRef> scoped_anchor_array(anchor_array);
    CFMutableArrayRef merged_array = CFArrayCreateMutableCopy(
        kCFAllocatorDefault, 0, anchor_array);
    if (!merged_array)
      return NULL;
    CFArrayAppendValue(merged_array, test_certificate_->os_cert_handle());

    return merged_array;
  }
 private:
  friend struct DefaultSingletonTraits<MacTrustedCertificates>;

  // Obtain an instance of MacTrustedCertificates via the singleton
  // interface.
  MacTrustedCertificates() : test_certificate_(NULL) { }

  // An X509Certificate object that may be appended to the list of
  // system trusted anchors.
  scoped_refptr<X509Certificate> test_certificate_;

  // The trusted cache may be accessed from multiple threads.
  mutable Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(MacTrustedCertificates);
};

void SetMacTestCertificate(X509Certificate* cert) {
  Singleton<MacTrustedCertificates>::get()->SetTestCertificate(cert);
}

namespace {

typedef OSStatus (*SecTrustCopyExtendedResultFuncPtr)(SecTrustRef,
                                                      CFDictionaryRef*);

inline bool CSSMOIDEqual(const CSSM_OID* oid1, const CSSM_OID* oid2) {
  return oid1->Length == oid2->Length &&
      (memcmp(oid1->Data, oid2->Data, oid1->Length) == 0);
}

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

void ParsePrincipal(const CSSM_X509_NAME* name,
                    X509Certificate::Principal* principal) {
  std::vector<std::string> common_names, locality_names, state_names,
      country_names;

  // TODO(jcampan): add business_category and serial_number.
  const CSSM_OID* kOIDs[] = { &CSSMOID_CommonName,
                              &CSSMOID_LocalityName,
                              &CSSMOID_StateProvinceName,
                              &CSSMOID_CountryName,
                              &CSSMOID_StreetAddress,
                              &CSSMOID_OrganizationName,
                              &CSSMOID_OrganizationalUnitName,
                              &CSSMOID_DNQualifier };  // This should be "DC"
                                                       // but is undoubtedly
                                                       // wrong. TODO(avi):
                                                       // Find the right OID.

  std::vector<std::string>* values[] = {
      &common_names, &locality_names,
      &state_names, &country_names,
      &(principal->street_addresses),
      &(principal->organization_names),
      &(principal->organization_unit_names),
      &(principal->domain_components) };
  DCHECK(arraysize(kOIDs) == arraysize(values));

  for (size_t rdn = 0; rdn < name->numberOfRDNs; ++rdn) {
    CSSM_X509_RDN rdn_struct = name->RelativeDistinguishedName[rdn];
    for (size_t pair = 0; pair < rdn_struct.numberOfPairs; ++pair) {
      CSSM_X509_TYPE_VALUE_PAIR pair_struct =
          rdn_struct.AttributeTypeAndValue[pair];
      for (size_t oid = 0; oid < arraysize(kOIDs); ++oid) {
        if (CSSMOIDEqual(&pair_struct.type, kOIDs[oid])) {
          std::string value =
              std::string(reinterpret_cast<std::string::value_type*>
                              (pair_struct.value.Data),
                          pair_struct.value.Length);
          values[oid]->push_back(value);
          break;
        }
      }
    }
  }

  // We don't expect to have more than one CN, L, S, and C.
  std::vector<std::string>* single_value_lists[4] = {
      &common_names, &locality_names, &state_names, &country_names };
  std::string* single_values[4] = {
      &principal->common_name, &principal->locality_name,
      &principal->state_or_province_name, &principal->country_name };
  for (size_t i = 0; i < arraysize(single_value_lists); ++i) {
    DCHECK(single_value_lists[i]->size() <= 1);
    if (single_value_lists[i]->size() > 0)
      *(single_values[i]) = (*(single_value_lists[i]))[0];
  }
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
  CSSMFields fields;
  OSStatus status = GetCertFields(cert_handle, &fields);
  if (status)
    return;

  for (size_t field = 0; field < fields.num_of_fields; ++field) {
    if (CSSMOIDEqual(&fields.fields[field].FieldOid, &oid)) {
      CSSM_X509_EXTENSION_PTR cssm_ext =
          (CSSM_X509_EXTENSION_PTR)fields.fields[field].FieldValue.Data;
      CE_GeneralNames* alt_name =
          (CE_GeneralNames*) cssm_ext->value.parsedValue;

      for (size_t name = 0; name < alt_name->numNames; ++name) {
        const CE_GeneralName& name_struct = alt_name->generalName[name];
        // For future extension: We're assuming that these values are of types
        // GNT_RFC822Name, GNT_DNSName or GNT_URI, all of which are encoded as
        // IA5String. In general, we should be switching off
        // |name_struct.nameType| and doing type-appropriate conversions. See
        // certextensions.h and the comment immediately preceding
        // CE_GeneralNameType for more information.
        DCHECK(name_struct.nameType == GNT_RFC822Name ||
               name_struct.nameType == GNT_DNSName ||
               name_struct.nameType == GNT_URI);
        if (name_struct.nameType == name_type) {
          const CSSM_DATA& name_data = name_struct.name;
          std::string value =
          std::string(reinterpret_cast<std::string::value_type*>
                      (name_data.Data),
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
      CSSM_X509_TIME* x509_time =
          reinterpret_cast<CSSM_X509_TIME *>
            (fields.fields[field].FieldValue.Data);
      std::string time_string =
          std::string(reinterpret_cast<std::string::value_type*>
                      (x509_time->time.Data),
                      x509_time->time.Length);

      DCHECK(x509_time->timeType == BER_TAG_UTC_TIME ||
             x509_time->timeType == BER_TAG_GENERALIZED_TIME);

      struct tm time;
      const char* parse_string;
      if (x509_time->timeType == BER_TAG_UTC_TIME)
        parse_string = "%y%m%d%H%M%SZ";
      else if (x509_time->timeType == BER_TAG_GENERALIZED_TIME)
        parse_string = "%y%m%d%H%M%SZ";
      else {
        // Those are the only two BER tags for time; if neither are used then
        // this is a rather broken cert.
        return;
      }

      strptime(time_string.c_str(), parse_string, &time);

      Time::Exploded exploded;
      exploded.year         = time.tm_year + 1900;
      exploded.month        = time.tm_mon + 1;
      exploded.day_of_week  = time.tm_wday;
      exploded.day_of_month = time.tm_mday;
      exploded.hour         = time.tm_hour;
      exploded.minute       = time.tm_min;
      exploded.second       = time.tm_sec;
      exploded.millisecond  = 0;

      *result = Time::FromUTCExploded(exploded);
      break;
    }
  }
}

}  // namespace

void X509Certificate::Initialize() {
  const CSSM_X509_NAME* name;
  OSStatus status = SecCertificateGetSubject(cert_handle_, &name);
  if (!status) {
    ParsePrincipal(name, &subject_);
  }
  status = SecCertificateGetIssuer(cert_handle_, &name);
  if (!status) {
    ParsePrincipal(name, &issuer_);
  }

  GetCertDateForOID(cert_handle_, CSSMOID_X509V1ValidityNotBefore,
                    &valid_start_);
  GetCertDateForOID(cert_handle_, CSSMOID_X509V1ValidityNotAfter,
                    &valid_expiry_);

  fingerprint_ = CalculateFingerprint(cert_handle_);

  // Store the certificate in the cache in case we need it later.
  X509Certificate::Cache::GetInstance()->Insert(this);
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
  SecPolicySearchRef ssl_policy_search_ref = NULL;
  OSStatus status = SecPolicySearchCreate(CSSM_CERT_X_509v3,
                                          &CSSMOID_APPLE_TP_SSL,
                                          NULL,
                                          &ssl_policy_search_ref);
  if (status)
    return NetErrorFromOSStatus(status);
  scoped_cftyperef<SecPolicySearchRef>
      scoped_ssl_policy_search_ref(ssl_policy_search_ref);
  SecPolicyRef ssl_policy = NULL;
  status = SecPolicySearchCopyNext(ssl_policy_search_ref, &ssl_policy);
  if (status)
    return NetErrorFromOSStatus(status);
  scoped_cftyperef<SecPolicyRef> scoped_ssl_policy(ssl_policy);
  CSSM_APPLE_TP_SSL_OPTIONS tp_ssl_options = { CSSM_APPLE_TP_SSL_OPTS_VERSION };
  tp_ssl_options.ServerName = hostname.data();
  tp_ssl_options.ServerNameLen = hostname.size();
  CSSM_DATA tp_ssl_options_data_value;
  tp_ssl_options_data_value.Data = reinterpret_cast<uint8*>(&tp_ssl_options);
  tp_ssl_options_data_value.Length = sizeof(tp_ssl_options);
  status = SecPolicySetValue(ssl_policy, &tp_ssl_options_data_value);
  if (status)
    return NetErrorFromOSStatus(status);

  // Create and configure a SecTrustRef, which takes our certificate(s)
  // and our SSL SecPolicyRef. SecTrustCreateWithCertificates() takes an
  // array of certificates, the first of which is the certificate we're
  // verifying, and the subsequent (optional) certificates are used for
  // chain building.
  CFMutableArrayRef cert_array = CFArrayCreateMutable(kCFAllocatorDefault, 0,
                                                      &kCFTypeArrayCallBacks);
  if (!cert_array)
    return ERR_OUT_OF_MEMORY;
  scoped_cftyperef<CFArrayRef> scoped_cert_array(cert_array);
  CFArrayAppendValue(cert_array, cert_handle_);
  if (intermediate_ca_certs_) {
    CFIndex intermediate_count = CFArrayGetCount(intermediate_ca_certs_);
    for (CFIndex i = 0; i < intermediate_count; ++i) {
      SecCertificateRef intermediate_cert = static_cast<SecCertificateRef>(
          const_cast<void*>(CFArrayGetValueAtIndex(intermediate_ca_certs_, i)));
      CFArrayAppendValue(cert_array, intermediate_cert);
    }
  }

  SecTrustRef trust_ref = NULL;
  status = SecTrustCreateWithCertificates(cert_array, ssl_policy, &trust_ref);
  if (status)
    return NetErrorFromOSStatus(status);
  scoped_cftyperef<SecTrustRef> scoped_trust_ref(trust_ref);

  // Set the trusted anchor certificates for the SecTrustRef by merging the
  // system trust anchors and the test root certificate.
  CFArrayRef anchor_array =
      Singleton<MacTrustedCertificates>::get()->CopyTrustedCertificateArray();
  scoped_cftyperef<CFArrayRef> scoped_anchor_array(anchor_array);
  if (anchor_array) {
    status = SecTrustSetAnchorCertificates(trust_ref, anchor_array);
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
    scoped_cftyperef<CFDataRef> scoped_action_data_ref(action_data_ref);
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
  scoped_cftyperef<CFArrayRef> scoped_completed_chain(completed_chain);

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
            if (OverrideHostnameMismatch(hostname, &names)) {
              cert_status = 0;
            }
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
      if (!verify_result->cert_status) {
        verify_result->cert_status |= CERT_STATUS_INVALID;
      }
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

bool X509Certificate::VerifyEV() const {
  // We don't call this private method, but we do need to implement it because
  // it's defined in x509_certificate.h. We perform EV checking in the
  // Verify() above.
  NOTREACHED();
  return false;
}

void X509Certificate::AddIntermediateCertificate(SecCertificateRef cert) {
  if (cert) {
    if (!intermediate_ca_certs_) {
      intermediate_ca_certs_ = CFArrayCreateMutable(kCFAllocatorDefault, 0,
                                                    &kCFTypeArrayCallBacks);
    }
    if (intermediate_ca_certs_) {
      CFArrayAppendValue(intermediate_ca_certs_, cert);
    }
  }
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
                                                 CSSM_CERT_ENCODING_BER,
                                                 &cert_handle);
  if (status)
    return NULL;

  return cert_handle;
}

// static
void X509Certificate::FreeOSCertHandle(OSCertHandle cert_handle) {
  CFRelease(cert_handle);
}

// static
X509Certificate::Fingerprint X509Certificate::CalculateFingerprint(
    OSCertHandle cert) {
  Fingerprint sha1;
  memset(sha1.data, 0, sizeof(sha1.data));

  CSSM_DATA cert_data;
  OSStatus status = SecCertificateGetData(cert, &cert_data);
  if (status)
    return sha1;

  DCHECK(NULL != cert_data.Data);
  DCHECK(0 != cert_data.Length);

  CC_SHA1(cert_data.Data, cert_data.Length, sha1.data);

  return sha1;
}

}  // namespace net
