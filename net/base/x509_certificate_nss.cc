// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_certificate.h"

#include <cert.h>
#include <pk11pub.h>
#include <prerror.h>
#include <prtime.h>
#include <secder.h>
#include <secerr.h>
#include <sechash.h>
#include <sslerr.h>

#include "base/logging.h"
#include "base/pickle.h"
#include "base/time.h"
#include "base/nss_init.h"
#include "net/base/cert_status_flags.h"
#include "net/base/cert_verify_result.h"
#include "net/base/ev_root_ca_metadata.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

class ScopedCERTCertificate {
 public:
  explicit ScopedCERTCertificate(CERTCertificate* cert)
      : cert_(cert) {}

  ~ScopedCERTCertificate() {
    if (cert_)
      CERT_DestroyCertificate(cert_);
  }

 private:
  CERTCertificate* cert_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCERTCertificate);
};

class ScopedCERTCertList {
 public:
  explicit ScopedCERTCertList(CERTCertList* cert_list)
      : cert_list_(cert_list) {}

  ~ScopedCERTCertList() {
    if (cert_list_)
      CERT_DestroyCertList(cert_list_);
  }

 private:
  CERTCertList* cert_list_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCERTCertList);
};

class ScopedCERTCertificatePolicies {
 public:
  explicit ScopedCERTCertificatePolicies(CERTCertificatePolicies* policies)
      : policies_(policies) {}

  ~ScopedCERTCertificatePolicies() {
    if (policies_)
      CERT_DestroyCertificatePoliciesExtension(policies_);
  }

 private:
  CERTCertificatePolicies* policies_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCERTCertificatePolicies);
};

// ScopedCERTValOutParam manages destruction of values in the CERTValOutParam
// array that cvout points to.  cvout must be initialized as passed to
// CERT_PKIXVerifyCert, so that the array must be terminated with
// cert_po_end type.
// When it goes out of scope, it destroys values of cert_po_trustAnchor
// and cert_po_certList types, but doesn't release the array itself.
class ScopedCERTValOutParam {
 public:
  explicit ScopedCERTValOutParam(CERTValOutParam* cvout)
      : cvout_(cvout) {}

  ~ScopedCERTValOutParam() {
    if (cvout_ == NULL)
      return;
    for (CERTValOutParam *p = cvout_; p->type != cert_po_end; p++) {
      switch (p->type) {
        case cert_po_trustAnchor:
          if (p->value.pointer.cert) {
            CERT_DestroyCertificate(p->value.pointer.cert);
            p->value.pointer.cert = NULL;
          }
          break;
        case cert_po_certList:
          if (p->value.pointer.chain) {
            CERT_DestroyCertList(p->value.pointer.chain);
            p->value.pointer.chain = NULL;
          }
          break;
        default:
          break;
      }
    }
  }

 private:
  CERTValOutParam* cvout_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCERTValOutParam);
};

// Map PORT_GetError() return values to our network error codes.
int MapSecurityError(int err) {
  switch (err) {
    case PR_DIRECTORY_LOOKUP_ERROR:  // DNS lookup error.
      return ERR_NAME_NOT_RESOLVED;
    case SEC_ERROR_INVALID_ARGS:
      return ERR_INVALID_ARGUMENT;
    case SEC_ERROR_INVALID_TIME:
    case SEC_ERROR_EXPIRED_CERTIFICATE:
      return ERR_CERT_DATE_INVALID;
    case SEC_ERROR_UNKNOWN_ISSUER:
    case SEC_ERROR_UNTRUSTED_ISSUER:
    case SEC_ERROR_CA_CERT_INVALID:
    case SEC_ERROR_UNTRUSTED_CERT:
      return ERR_CERT_AUTHORITY_INVALID;
    case SEC_ERROR_REVOKED_CERTIFICATE:
      return ERR_CERT_REVOKED;
    case SEC_ERROR_BAD_DER:
    case SEC_ERROR_BAD_SIGNATURE:
    case SEC_ERROR_CERT_NOT_VALID:
    // TODO(port): add an ERR_CERT_WRONG_USAGE error code.
    case SEC_ERROR_CERT_USAGES_INVALID:
      return ERR_CERT_INVALID;
    default:
      LOG(WARNING) << "Unknown error " << err << " mapped to net::ERR_FAILED";
      return ERR_FAILED;
  }
}

// Map PORT_GetError() return values to our cert status flags.
int MapCertErrorToCertStatus(int err) {
  switch (err) {
    case SSL_ERROR_BAD_CERT_DOMAIN:
      return CERT_STATUS_COMMON_NAME_INVALID;
    case SEC_ERROR_INVALID_TIME:
    case SEC_ERROR_EXPIRED_CERTIFICATE:
      return CERT_STATUS_DATE_INVALID;
    case SEC_ERROR_UNTRUSTED_CERT:
    case SEC_ERROR_UNKNOWN_ISSUER:
    case SEC_ERROR_UNTRUSTED_ISSUER:
    case SEC_ERROR_CA_CERT_INVALID:
      return CERT_STATUS_AUTHORITY_INVALID;
    // TODO(port): map CERT_STATUS_NO_REVOCATION_MECHANISM.
    case SEC_ERROR_OCSP_BAD_HTTP_RESPONSE:
    case SEC_ERROR_OCSP_SERVER_ERROR:
      return CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
    case SEC_ERROR_REVOKED_CERTIFICATE:
      return CERT_STATUS_REVOKED;
    case SEC_ERROR_BAD_DER:
    case SEC_ERROR_BAD_SIGNATURE:
    case SEC_ERROR_CERT_NOT_VALID:
    // TODO(port): add a CERT_STATUS_WRONG_USAGE error code.
    case SEC_ERROR_CERT_USAGES_INVALID:
      return CERT_STATUS_INVALID;
    default:
      return 0;
  }
}

// Saves some information about the certificate chain cert_list in
// *verify_result.  The caller MUST initialize *verify_result before calling
// this function.
// Note that cert_list[0] is the end entity certificate and cert_list doesn't
// contain the root CA certificate.
void GetCertChainInfo(CERTCertList* cert_list,
                      CertVerifyResult* verify_result) {
  // NOTE: Using a NSS library before 3.12.3.1 will crash below.  To see the
  // NSS version currently in use:
  // 1. use ldd on the chrome executable for NSS's location (ie. libnss3.so*)
  // 2. use ident libnss3.so* for the library's version
  DCHECK(cert_list);
  int i = 0;
  for (CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
       !CERT_LIST_END(node, cert_list);
       node = CERT_LIST_NEXT(node), i++) {
    SECAlgorithmID& signature = node->cert->signature;
    SECOidTag oid_tag = SECOID_FindOIDTag(&signature.algorithm);
    switch (oid_tag) {
      case SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION:
        verify_result->has_md5 = true;
        if (i != 0)
          verify_result->has_md5_ca = true;
        break;
      case SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION:
        verify_result->has_md2 = true;
        if (i != 0)
          verify_result->has_md2_ca = true;
        break;
      case SEC_OID_PKCS1_MD4_WITH_RSA_ENCRYPTION:
        verify_result->has_md4 = true;
        break;
      default:
        break;
    }
  }
}

// TODO(port): Implement this more simply, and put it in the right place
base::Time PRTimeToBaseTime(PRTime prtime) {
  PRExplodedTime prxtime;
  PR_ExplodeTime(prtime, PR_GMTParameters, &prxtime);

  base::Time::Exploded exploded;
  exploded.year         = prxtime.tm_year;
  exploded.month        = prxtime.tm_month + 1;
  exploded.day_of_week  = prxtime.tm_wday;
  exploded.day_of_month = prxtime.tm_mday;
  exploded.hour         = prxtime.tm_hour;
  exploded.minute       = prxtime.tm_min;
  exploded.second       = prxtime.tm_sec;
  exploded.millisecond  = prxtime.tm_usec / 1000;

  return base::Time::FromUTCExploded(exploded);
}

typedef char* (*CERTGetNameFunc)(CERTName* name);

void ParsePrincipal(CERTName* name,
                    X509Certificate::Principal* principal) {
  // TODO(jcampan): add business_category and serial_number.
  // TODO(wtc): NSS has the CERT_GetOrgName, CERT_GetOrgUnitName, and
  // CERT_GetDomainComponentName functions, but they return only the most
  // general (the first) RDN.  NSS doesn't have a function for the street
  // address.
  static const SECOidTag kOIDs[] = {
      SEC_OID_AVA_STREET_ADDRESS,
      SEC_OID_AVA_ORGANIZATION_NAME,
      SEC_OID_AVA_ORGANIZATIONAL_UNIT_NAME,
      SEC_OID_AVA_DC };

  std::vector<std::string>* values[] = {
      &principal->street_addresses,
      &principal->organization_names,
      &principal->organization_unit_names,
      &principal->domain_components };
  DCHECK(arraysize(kOIDs) == arraysize(values));

  CERTRDN** rdns = name->rdns;
  for (size_t rdn = 0; rdns[rdn]; ++rdn) {
    CERTAVA** avas = rdns[rdn]->avas;
    for (size_t pair = 0; avas[pair] != 0; ++pair) {
      SECOidTag tag = CERT_GetAVATag(avas[pair]);
      for (size_t oid = 0; oid < arraysize(kOIDs); ++oid) {
        if (kOIDs[oid] == tag) {
          SECItem* decode_item = CERT_DecodeAVAValue(&avas[pair]->value);
          if (!decode_item)
            break;
          // TODO(wtc): Pass decode_item to CERT_RFC1485_EscapeAndQuote.
          std::string value(reinterpret_cast<char*>(decode_item->data),
                            decode_item->len);
          values[oid]->push_back(value);
          SECITEM_FreeItem(decode_item, PR_TRUE);
          break;
        }
      }
    }
  }

  // Get CN, L, S, and C.
  CERTGetNameFunc get_name_funcs[4] = {
      CERT_GetCommonName, CERT_GetLocalityName,
      CERT_GetStateName, CERT_GetCountryName };
  std::string* single_values[4] = {
      &principal->common_name, &principal->locality_name,
      &principal->state_or_province_name, &principal->country_name };
  for (size_t i = 0; i < arraysize(get_name_funcs); ++i) {
    char* value = get_name_funcs[i](name);
    if (value) {
      single_values[i]->assign(value);
      PORT_Free(value);
    }
  }
}

void ParseDate(SECItem* der_date, base::Time* result) {
  PRTime prtime;
  SECStatus rv = DER_DecodeTimeChoice(&prtime, der_date);
  DCHECK(rv == SECSuccess);
  *result = PRTimeToBaseTime(prtime);
}

void GetCertSubjectAltNamesOfType(X509Certificate::OSCertHandle cert_handle,
                                  CERTGeneralNameType name_type,
                                  std::vector<std::string>* result) {
  SECItem alt_name;
  SECStatus rv = CERT_FindCertExtension(cert_handle,
      SEC_OID_X509_SUBJECT_ALT_NAME, &alt_name);
  if (rv != SECSuccess)
    return;

  PRArenaPool* arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  DCHECK(arena != NULL);

  CERTGeneralName* alt_name_list;
  alt_name_list = CERT_DecodeAltNameExtension(arena, &alt_name);
  SECITEM_FreeItem(&alt_name, PR_FALSE);

  CERTGeneralName* name = alt_name_list;
  while (name) {
    // For future extension: We're assuming that these values are of types
    // RFC822Name, DNSName or URI.  See the mac code for notes.
    DCHECK(name->type == certRFC822Name ||
           name->type == certDNSName ||
           name->type == certURI);
    if (name->type == name_type) {
      unsigned char* p = name->name.other.data;
      int len = name->name.other.len;
      std::string value = std::string(reinterpret_cast<char*>(p), len);
      result->push_back(value);
    }
    name = CERT_GetNextGeneralName(name);
    if (name == alt_name_list)
      break;
  }
  PORT_FreeArena(arena, PR_FALSE);
}

// Call CERT_PKIXVerifyCert for the cert_handle.
// Verification results are stored in an array of CERTValOutParam.
// If policy_oids is not NULL and num_policy_oids is positive, policies
// are also checked.
// Caller must initialize cvout before calling this function.
SECStatus PKIXVerifyCert(X509Certificate::OSCertHandle cert_handle,
                         bool check_revocation,
                         const SECOidTag* policy_oids,
                         int num_policy_oids,
                         CERTValOutParam* cvout) {
  bool use_crl = check_revocation;
  bool use_ocsp = check_revocation;

  PRUint64 revocation_method_flags =
      CERT_REV_M_DO_NOT_TEST_USING_THIS_METHOD |
      CERT_REV_M_ALLOW_NETWORK_FETCHING |
      CERT_REV_M_IGNORE_IMPLICIT_DEFAULT_SOURCE |
      CERT_REV_M_IGNORE_MISSING_FRESH_INFO |
      CERT_REV_M_STOP_TESTING_ON_FRESH_INFO;
  PRUint64 revocation_method_independent_flags =
      CERT_REV_MI_TEST_ALL_LOCAL_INFORMATION_FIRST;
  if (policy_oids && num_policy_oids > 0) {
    // EV verification requires revocation checking.  Consider the certificate
    // revoked if we don't have revocation info.
    // TODO(wtc): Add a bool parameter to expressly specify we're doing EV
    // verification or we want strict revocation flags.
    revocation_method_flags |= CERT_REV_M_REQUIRE_INFO_ON_MISSING_SOURCE;
    revocation_method_independent_flags |=
        CERT_REV_MI_REQUIRE_SOME_FRESH_INFO_AVAILABLE;
  } else {
    revocation_method_flags |= CERT_REV_M_SKIP_TEST_ON_MISSING_SOURCE;
    revocation_method_independent_flags |=
        CERT_REV_MI_NO_OVERALL_INFO_REQUIREMENT;
  }
  PRUint64 method_flags[2];
  method_flags[cert_revocation_method_crl] = revocation_method_flags;
  method_flags[cert_revocation_method_ocsp] = revocation_method_flags;

  if (use_crl) {
    method_flags[cert_revocation_method_crl] |=
        CERT_REV_M_TEST_USING_THIS_METHOD;
  }
  if (use_ocsp) {
    method_flags[cert_revocation_method_ocsp] |=
        CERT_REV_M_TEST_USING_THIS_METHOD;
  }

  CERTRevocationMethodIndex preferred_revocation_methods[1];
  if (use_ocsp) {
    preferred_revocation_methods[0] = cert_revocation_method_ocsp;
  } else {
    preferred_revocation_methods[0] = cert_revocation_method_crl;
  }

  CERTRevocationFlags revocation_flags;
  revocation_flags.leafTests.number_of_defined_methods =
      arraysize(method_flags);
  revocation_flags.leafTests.cert_rev_flags_per_method = method_flags;
  revocation_flags.leafTests.number_of_preferred_methods =
      arraysize(preferred_revocation_methods);
  revocation_flags.leafTests.preferred_methods = preferred_revocation_methods;
  revocation_flags.leafTests.cert_rev_method_independent_flags =
      revocation_method_independent_flags;

  revocation_flags.chainTests.number_of_defined_methods =
      arraysize(method_flags);
  revocation_flags.chainTests.cert_rev_flags_per_method = method_flags;
  revocation_flags.chainTests.number_of_preferred_methods =
      arraysize(preferred_revocation_methods);
  revocation_flags.chainTests.preferred_methods = preferred_revocation_methods;
  revocation_flags.chainTests.cert_rev_method_independent_flags =
      revocation_method_independent_flags;

  CERTValInParam cvin[4];
  int cvin_index = 0;
  // No need to set cert_pi_trustAnchors here.
  cvin[cvin_index].type = cert_pi_revocationFlags;
  cvin[cvin_index].value.pointer.revocation = &revocation_flags;
  cvin_index++;
  std::vector<SECOidTag> policies;
  if (policy_oids && num_policy_oids > 0) {
    cvin[cvin_index].type = cert_pi_policyOID;
    cvin[cvin_index].value.arraySize = num_policy_oids;
    cvin[cvin_index].value.array.oids = policy_oids;
    cvin_index++;
  }
  // Add cert_pi_useAIACertFetch last so we can easily remove it from the
  // cvin array in the workaround below.
  cvin[cvin_index].type = cert_pi_useAIACertFetch;
  cvin[cvin_index].value.scalar.b = PR_TRUE;
  cvin_index++;
  cvin[cvin_index].type = cert_pi_end;

  SECStatus rv = CERT_PKIXVerifyCert(cert_handle, certificateUsageSSLServer,
                                     cvin, cvout, NULL);
  if (rv != SECSuccess) {
    // cert_pi_useAIACertFetch can't handle a CA issuers access location that
    // is an LDAP URL with an empty host name (NSS bug 528741).  If cert fetch
    // fails because of a network error, it also causes CERT_PKIXVerifyCert
    // to report the network error rather than SEC_ERROR_UNKNOWN_ISSUER.  To
    // work around these NSS bugs, we retry without cert_pi_useAIACertFetch.
    int nss_error = PORT_GetError();
    if (nss_error == SEC_ERROR_INVALID_ARGS || !IS_SEC_ERROR(nss_error)) {
      cvin_index--;
      DCHECK_EQ(cvin[cvin_index].type, cert_pi_useAIACertFetch);
      cvin[cvin_index].type = cert_pi_end;
      rv = CERT_PKIXVerifyCert(cert_handle, certificateUsageSSLServer,
                               cvin, cvout, NULL);
    }
  }
  return rv;
}

bool CheckCertPolicies(X509Certificate::OSCertHandle cert_handle,
                       SECOidTag ev_policy_tag) {
  SECItem policy_ext;
  SECStatus rv = CERT_FindCertExtension(
      cert_handle, SEC_OID_X509_CERTIFICATE_POLICIES, &policy_ext);
  if (rv != SECSuccess) {
    LOG(ERROR) << "Cert has no policies extension.";
    return false;
  }
  CERTCertificatePolicies* policies =
      CERT_DecodeCertificatePoliciesExtension(&policy_ext);
  SECITEM_FreeItem(&policy_ext, PR_FALSE);
  if (!policies) {
    LOG(ERROR) << "Failed to decode certificate policy.";
    return false;
  }
  ScopedCERTCertificatePolicies scoped_policies(policies);
  CERTPolicyInfo** policy_infos = policies->policyInfos;
  while (*policy_infos != NULL) {
    CERTPolicyInfo* policy_info = *policy_infos++;
    SECOidTag oid_tag = policy_info->oid;
    if (oid_tag == SEC_OID_UNKNOWN)
      continue;
    if (oid_tag == ev_policy_tag)
      return true;
  }
  LOG(ERROR) << "No EV Policy Tag";
  return false;
}

}  // namespace

void X509Certificate::Initialize() {
  ParsePrincipal(&cert_handle_->subject, &subject_);
  ParsePrincipal(&cert_handle_->issuer, &issuer_);

  ParseDate(&cert_handle_->validity.notBefore, &valid_start_);
  ParseDate(&cert_handle_->validity.notAfter, &valid_expiry_);

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
  pickle->WriteData(reinterpret_cast<const char*>(cert_handle_->derCert.data),
                    cert_handle_->derCert.len);
}

void X509Certificate::GetDNSNames(std::vector<std::string>* dns_names) const {
  dns_names->clear();

  // Compare with CERT_VerifyCertName().
  GetCertSubjectAltNamesOfType(cert_handle_, certDNSName, dns_names);

  // TODO(port): suppress nss's support of the obsolete extension
  //  SEC_OID_NS_CERT_EXT_SSL_SERVER_NAME
  // by providing our own authCertificate callback.

  if (dns_names->empty())
    dns_names->push_back(subject_.common_name);
}

int X509Certificate::Verify(const std::string& hostname,
                            int flags,
                            CertVerifyResult* verify_result) const {
  verify_result->Reset();

  // Make sure that the hostname matches with the common name of the cert.
  SECStatus status = CERT_VerifyCertName(cert_handle_, hostname.c_str());
  if (status != SECSuccess)
    verify_result->cert_status |= CERT_STATUS_COMMON_NAME_INVALID;

  // Make sure that the cert is valid now.
  SECCertTimeValidity validity = CERT_CheckCertValidTimes(
      cert_handle_, PR_Now(), PR_TRUE);
  if (validity != secCertTimeValid)
    verify_result->cert_status |= CERT_STATUS_DATE_INVALID;

  CERTValOutParam cvout[3];
  int cvout_index = 0;
  // We don't need the trust anchor for the first PKIXVerifyCert call.
  cvout[cvout_index].type = cert_po_certList;
  cvout[cvout_index].value.pointer.chain = NULL;
  int cvout_cert_list_index = cvout_index;
  cvout_index++;
  cvout[cvout_index].type = cert_po_end;
  ScopedCERTValOutParam scoped_cvout(cvout);

  bool check_revocation = (flags & VERIFY_REV_CHECKING_ENABLED);
  if (check_revocation) {
    verify_result->cert_status |= CERT_STATUS_REV_CHECKING_ENABLED;
  } else {
    // EV requires revocation checking.
    flags &= ~VERIFY_EV_CERT;
  }
  status = PKIXVerifyCert(cert_handle_, check_revocation, NULL, 0, cvout);
  if (status != SECSuccess) {
    int err = PORT_GetError();
    LOG(ERROR) << "CERT_PKIXVerifyCert for " << hostname
               << " failed err=" << err;
    // CERT_PKIXVerifyCert rerports the wrong error code for
    // expired certificates (NSS bug 491174)
    if (err == SEC_ERROR_CERT_NOT_VALID &&
        (verify_result->cert_status & CERT_STATUS_DATE_INVALID) != 0)
      err = SEC_ERROR_EXPIRED_CERTIFICATE;
    int cert_status = MapCertErrorToCertStatus(err);
    if (cert_status) {
      verify_result->cert_status |= cert_status;
      return MapCertStatusToNetError(verify_result->cert_status);
    }
    // |err| is not a certificate error.
    return MapSecurityError(err);
  }

  GetCertChainInfo(cvout[cvout_cert_list_index].value.pointer.chain,
                   verify_result);
  if (IsCertStatusError(verify_result->cert_status))
    return MapCertStatusToNetError(verify_result->cert_status);

  if ((flags & VERIFY_EV_CERT) && VerifyEV())
    verify_result->cert_status |= CERT_STATUS_IS_EV;
  return OK;
}

// Studied Mozilla's code (esp. security/manager/ssl/src/nsIdentityChecking.cpp
// and nsNSSCertHelper.cpp) to learn how to verify EV certificate.
// TODO(wtc): A possible optimization is that we get the trust anchor from
// the first PKIXVerifyCert call.  We look up the EV policy for the trust
// anchor.  If the trust anchor has no EV policy, we know the cert isn't EV.
// Otherwise, we pass just that EV policy (as opposed to all the EV policies)
// to the second PKIXVerifyCert call.
bool X509Certificate::VerifyEV() const {
  net::EVRootCAMetadata* metadata = net::EVRootCAMetadata::GetInstance();

  CERTValOutParam cvout[3];
  int cvout_index = 0;
  cvout[cvout_index].type = cert_po_trustAnchor;
  cvout[cvout_index].value.pointer.cert = NULL;
  int cvout_trust_anchor_index = cvout_index;
  cvout_index++;
  cvout[cvout_index].type = cert_po_end;
  ScopedCERTValOutParam scoped_cvout(cvout);

  SECStatus status = PKIXVerifyCert(cert_handle_,
                                    true,
                                    metadata->GetPolicyOIDs(),
                                    metadata->NumPolicyOIDs(),
                                    cvout);
  if (status != SECSuccess)
    return false;

  CERTCertificate* root_ca =
      cvout[cvout_trust_anchor_index].value.pointer.cert;
  if (root_ca == NULL)
    return false;
  X509Certificate::Fingerprint fingerprint =
      X509Certificate::CalculateFingerprint(root_ca);
  SECOidTag ev_policy_tag = SEC_OID_UNKNOWN;
  if (!metadata->GetPolicyOID(fingerprint, &ev_policy_tag))
    return false;

  if (!CheckCertPolicies(cert_handle_, ev_policy_tag))
    return false;

  return true;
}

// static
X509Certificate::OSCertHandle X509Certificate::CreateOSCertHandleFromBytes(
    const char* data, int length) {
  base::EnsureNSSInit();

  SECItem der_cert;
  der_cert.data = reinterpret_cast<unsigned char*>(const_cast<char*>(data));
  der_cert.len = length;
  return CERT_NewTempCertificate(CERT_GetDefaultCertDB(), &der_cert,
                                 NULL, PR_FALSE, PR_TRUE);
}

// static
void X509Certificate::FreeOSCertHandle(OSCertHandle cert_handle) {
  CERT_DestroyCertificate(cert_handle);
}

// static
X509Certificate::Fingerprint X509Certificate::CalculateFingerprint(
    OSCertHandle cert) {
  Fingerprint sha1;
  memset(sha1.data, 0, sizeof(sha1.data));

  DCHECK(NULL != cert->derCert.data);
  DCHECK(0 != cert->derCert.len);

  SECStatus rv = HASH_HashBuf(HASH_AlgSHA1, sha1.data,
                              cert->derCert.data, cert->derCert.len);
  DCHECK(rv == SECSuccess);

  return sha1;
}

}  // namespace net
