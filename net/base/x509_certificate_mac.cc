// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_certificate.h"

#include <CommonCrypto/CommonDigest.h>
#include <Security/Security.h>
#include <time.h>

#include "base/scoped_cftyperef.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "base/sys_string_conversions.h"
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
          (CSSM_X509_EXTENSION_PTR)fields.fields[field].FieldValue.Data;
      CE_GeneralNames* alt_name =
          (CE_GeneralNames*) cssm_ext->value.parsedValue;

      for (size_t name = 0; name < alt_name->numNames; ++name) {
        const CE_GeneralName& name_struct = alt_name->generalName[name];
        // All of the general name types we support are encoded as
        // IA5String. In general, we should be switching off
        // |name_struct.nameType| and doing type-appropriate conversions. See
        // certextensions.h and the comment immediately preceding
        // CE_GeneralNameType for more information.
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
  DCHECK(cert_handle && out_cert_chain);
  // Create an SSL policy ref configured for client cert evaluation.
  SecPolicyRef ssl_policy;
  OSStatus result = X509Certificate::CreateSSLClientPolicy(&ssl_policy);
  if (result)
    return result;
  scoped_cftyperef<SecPolicyRef> scoped_ssl_policy(ssl_policy);

  // Create a SecTrustRef.
  scoped_cftyperef<CFArrayRef> input_certs(
      CFArrayCreate(NULL, (const void**)&cert_handle, 1,
                    &kCFTypeArrayCallBacks));
  SecTrustRef trust_ref = NULL;
  result = SecTrustCreateWithCertificates(input_certs, ssl_policy, &trust_ref);
  if (result)
    return result;
  scoped_cftyperef<SecTrustRef> trust(trust_ref);

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

}  // namespace

void X509Certificate::Initialize() {
  const CSSM_X509_NAME* name;
  OSStatus status = SecCertificateGetSubject(cert_handle_, &name);
  if (!status) {
    subject_.Parse(name);
  }
  status = SecCertificateGetIssuer(cert_handle_, &name);
  if (!status) {
    issuer_.Parse(name);
  }

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
  scoped_cftyperef<SecPolicyRef> scoped_ssl_policy(ssl_policy);

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
  for (size_t i = 0; i < intermediate_ca_certs_.size(); ++i)
    CFArrayAppendValue(cert_array, intermediate_ca_certs_[i]);

  // From here on, only one thread can be active at a time. We have had a number
  // of sporadic crashes in the SecTrustEvaluate call below, way down inside
  // Apple's cert code, which we suspect are caused by a thread-safety issue.
  // So as a speculative fix allow only one thread to use SecTrust on this cert.
  AutoLock lock(verification_lock_);

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

  DCHECK(NULL != cert_data.Data);
  DCHECK(0 != cert_data.Length);

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
  if (result != noErr)
    return false;
  scoped_cftyperef<CFArrayRef> scoped_cert_chain(cert_chain);

  // Check all the certs in the chain for a match.
  int n = CFArrayGetCount(cert_chain);
  for (int i = 0; i < n; ++i) {
    SecCertificateRef cert_handle = reinterpret_cast<SecCertificateRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(cert_chain, i)));
    scoped_refptr<X509Certificate> cert = X509Certificate::CreateFromHandle(
        cert_handle,
        X509Certificate::SOURCE_LONE_CERT_IMPORT,
        X509Certificate::OSCertHandles());
    for (unsigned j = 0; j < valid_issuers.size(); j++) {
      if (cert->subject().Matches(valid_issuers[j]))
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
bool X509Certificate::GetSSLClientCertificates (
    const std::string& server_domain,
    const std::vector<CertPrincipal>& valid_issuers,
    std::vector<scoped_refptr<X509Certificate> >* certs) {
  scoped_cftyperef<SecIdentityRef> preferred_identity;
  if (!server_domain.empty()) {
    // See if there's an identity preference for this domain:
    scoped_cftyperef<CFStringRef> domain_str(
        base::SysUTF8ToCFStringRef("https://" + server_domain));
    SecIdentityRef identity = NULL;
    if (SecIdentityCopyPreference(domain_str,
                                  0,
                                  NULL, // validIssuers argument is ignored :(
                                  &identity) == noErr)
      preferred_identity.reset(identity);
  }

  // Now enumerate the identities in the available keychains.
  SecIdentitySearchRef search = nil;
  OSStatus err = SecIdentitySearchCreate(NULL, CSSM_KEYUSE_SIGN, &search);
  scoped_cftyperef<SecIdentitySearchRef> scoped_search(search);
  while (!err) {
    SecIdentityRef identity = NULL;
    err = SecIdentitySearchCopyNext(search, &identity);
    if (err)
      break;
    scoped_cftyperef<SecIdentityRef> scoped_identity(identity);

    SecCertificateRef cert_handle;
    err = SecIdentityCopyCertificate(identity, &cert_handle);
    if (err != noErr)
      continue;
    scoped_cftyperef<SecCertificateRef> scoped_cert_handle(cert_handle);

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
  scoped_cftyperef<CFMutableArrayRef> chain(
      CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks));
  CFArrayAppendValue(chain, identity);

  CFArrayRef cert_chain = NULL;
  result = CopyCertChain(cert_handle_, &cert_chain);
  if (result)
    goto exit;

  // Append the intermediate certs from SecTrust to the result array:
  if (cert_chain) {
    int chain_count = CFArrayGetCount(cert_chain);
    if (chain_count > 1) {
      CFArrayAppendArray(chain,
                         cert_chain,
                         CFRangeMake(1, chain_count - 1));
    }
    CFRelease(cert_chain);
  }
exit:
  if (result)
    LOG(ERROR) << "CreateIdentityCertificateChain error " << result;
  return chain.release();
}

}  // namespace net
