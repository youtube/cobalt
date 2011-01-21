// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_certificate.h"

#include "base/crypto/rsa_private_key.h"
#include "base/crypto/scoped_capi_types.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "net/base/cert_status_flags.h"
#include "net/base/cert_verify_result.h"
#include "net/base/ev_root_ca_metadata.h"
#include "net/base/net_errors.h"
#include "net/base/scoped_cert_chain_context.h"
#include "net/base/test_root_certs.h"

#pragma comment(lib, "crypt32.lib")

using base::Time;

namespace net {

namespace {

typedef base::ScopedCAPIHandle<
    HCERTSTORE,
    base::CAPIDestroyerWithFlags<HCERTSTORE,
                                 CertCloseStore, 0> > ScopedHCERTSTORE;

struct FreeChainEngineFunctor {
  void operator()(HCERTCHAINENGINE engine) const {
    if (engine)
      CertFreeCertificateChainEngine(engine);
  }
};

typedef base::ScopedCAPIHandle<HCERTCHAINENGINE, FreeChainEngineFunctor>
    ScopedHCERTCHAINENGINE;

//-----------------------------------------------------------------------------

// TODO(wtc): This is a copy of the MapSecurityError function in
// ssl_client_socket_win.cc.  Another function that maps Windows error codes
// to our network error codes is WinInetUtil::OSErrorToNetError.  We should
// eliminate the code duplication.
int MapSecurityError(SECURITY_STATUS err) {
  // There are numerous security error codes, but these are the ones we thus
  // far find interesting.
  switch (err) {
    case SEC_E_WRONG_PRINCIPAL:  // Schannel
    case CERT_E_CN_NO_MATCH:  // CryptoAPI
      return ERR_CERT_COMMON_NAME_INVALID;
    case SEC_E_UNTRUSTED_ROOT:  // Schannel
    case CERT_E_UNTRUSTEDROOT:  // CryptoAPI
      return ERR_CERT_AUTHORITY_INVALID;
    case SEC_E_CERT_EXPIRED:  // Schannel
    case CERT_E_EXPIRED:  // CryptoAPI
      return ERR_CERT_DATE_INVALID;
    case CRYPT_E_NO_REVOCATION_CHECK:
      return ERR_CERT_NO_REVOCATION_MECHANISM;
    case CRYPT_E_REVOCATION_OFFLINE:
      return ERR_CERT_UNABLE_TO_CHECK_REVOCATION;
    case CRYPT_E_REVOKED:  // Schannel and CryptoAPI
      return ERR_CERT_REVOKED;
    case SEC_E_CERT_UNKNOWN:
    case CERT_E_ROLE:
      return ERR_CERT_INVALID;
    case CERT_E_WRONG_USAGE:
      // TODO(wtc): Should we add ERR_CERT_WRONG_USAGE?
      return ERR_CERT_INVALID;
    // We received an unexpected_message or illegal_parameter alert message
    // from the server.
    case SEC_E_ILLEGAL_MESSAGE:
      return ERR_SSL_PROTOCOL_ERROR;
    case SEC_E_ALGORITHM_MISMATCH:
      return ERR_SSL_VERSION_OR_CIPHER_MISMATCH;
    case SEC_E_INVALID_HANDLE:
      return ERR_UNEXPECTED;
    case SEC_E_OK:
      return OK;
    default:
      LOG(WARNING) << "Unknown error " << err << " mapped to net::ERR_FAILED";
      return ERR_FAILED;
  }
}

// Map the errors in the chain_context->TrustStatus.dwErrorStatus returned by
// CertGetCertificateChain to our certificate status flags.
int MapCertChainErrorStatusToCertStatus(DWORD error_status) {
  int cert_status = 0;

  // We don't include CERT_TRUST_IS_NOT_TIME_NESTED because it's obsolete and
  // we wouldn't consider it an error anyway
  const DWORD kDateInvalidErrors = CERT_TRUST_IS_NOT_TIME_VALID |
                                   CERT_TRUST_CTL_IS_NOT_TIME_VALID;
  if (error_status & kDateInvalidErrors)
    cert_status |= CERT_STATUS_DATE_INVALID;

  const DWORD kAuthorityInvalidErrors = CERT_TRUST_IS_UNTRUSTED_ROOT |
                                        CERT_TRUST_IS_EXPLICIT_DISTRUST |
                                        CERT_TRUST_IS_PARTIAL_CHAIN;
  if (error_status & kAuthorityInvalidErrors)
    cert_status |= CERT_STATUS_AUTHORITY_INVALID;

  if ((error_status & CERT_TRUST_REVOCATION_STATUS_UNKNOWN) &&
      !(error_status & CERT_TRUST_IS_OFFLINE_REVOCATION))
    cert_status |= CERT_STATUS_NO_REVOCATION_MECHANISM;

  if (error_status & CERT_TRUST_IS_OFFLINE_REVOCATION)
    cert_status |= CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;

  if (error_status & CERT_TRUST_IS_REVOKED)
    cert_status |= CERT_STATUS_REVOKED;

  const DWORD kWrongUsageErrors = CERT_TRUST_IS_NOT_VALID_FOR_USAGE |
                                  CERT_TRUST_CTL_IS_NOT_VALID_FOR_USAGE;
  if (error_status & kWrongUsageErrors) {
    // TODO(wtc): Should we add CERT_STATUS_WRONG_USAGE?
    cert_status |= CERT_STATUS_INVALID;
  }

  // The rest of the errors.
  const DWORD kCertInvalidErrors =
      CERT_TRUST_IS_NOT_SIGNATURE_VALID |
      CERT_TRUST_IS_CYCLIC |
      CERT_TRUST_INVALID_EXTENSION |
      CERT_TRUST_INVALID_POLICY_CONSTRAINTS |
      CERT_TRUST_INVALID_BASIC_CONSTRAINTS |
      CERT_TRUST_INVALID_NAME_CONSTRAINTS |
      CERT_TRUST_CTL_IS_NOT_SIGNATURE_VALID |
      CERT_TRUST_HAS_NOT_SUPPORTED_NAME_CONSTRAINT |
      CERT_TRUST_HAS_NOT_DEFINED_NAME_CONSTRAINT |
      CERT_TRUST_HAS_NOT_PERMITTED_NAME_CONSTRAINT |
      CERT_TRUST_HAS_EXCLUDED_NAME_CONSTRAINT |
      CERT_TRUST_NO_ISSUANCE_CHAIN_POLICY |
      CERT_TRUST_HAS_NOT_SUPPORTED_CRITICAL_EXT;
  if (error_status & kCertInvalidErrors)
    cert_status |= CERT_STATUS_INVALID;

  return cert_status;
}

//-----------------------------------------------------------------------------

// Wrappers of malloc and free for CRYPT_DECODE_PARA, which requires the
// WINAPI calling convention.
void* WINAPI MyCryptAlloc(size_t size) {
  return malloc(size);
}

void WINAPI MyCryptFree(void* p) {
  free(p);
}

// Decodes the cert's subjectAltName extension into a CERT_ALT_NAME_INFO
// structure and stores it in *output.
void GetCertSubjectAltName(PCCERT_CONTEXT cert,
                           scoped_ptr_malloc<CERT_ALT_NAME_INFO>* output) {
  PCERT_EXTENSION extension = CertFindExtension(szOID_SUBJECT_ALT_NAME2,
                                                cert->pCertInfo->cExtension,
                                                cert->pCertInfo->rgExtension);
  if (!extension)
    return;

  CRYPT_DECODE_PARA decode_para;
  decode_para.cbSize = sizeof(decode_para);
  decode_para.pfnAlloc = MyCryptAlloc;
  decode_para.pfnFree = MyCryptFree;
  CERT_ALT_NAME_INFO* alt_name_info = NULL;
  DWORD alt_name_info_size = 0;
  BOOL rv;
  rv = CryptDecodeObjectEx(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                           szOID_SUBJECT_ALT_NAME2,
                           extension->Value.pbData,
                           extension->Value.cbData,
                           CRYPT_DECODE_ALLOC_FLAG | CRYPT_DECODE_NOCOPY_FLAG,
                           &decode_para,
                           &alt_name_info,
                           &alt_name_info_size);
  if (rv)
    output->reset(alt_name_info);
}

// Returns true if any common name in the certificate's Subject field contains
// a NULL character.
bool CertSubjectCommonNameHasNull(PCCERT_CONTEXT cert) {
  CRYPT_DECODE_PARA decode_para;
  decode_para.cbSize = sizeof(decode_para);
  decode_para.pfnAlloc = MyCryptAlloc;
  decode_para.pfnFree = MyCryptFree;
  CERT_NAME_INFO* name_info = NULL;
  DWORD name_info_size = 0;
  BOOL rv;
  rv = CryptDecodeObjectEx(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                           X509_NAME,
                           cert->pCertInfo->Subject.pbData,
                           cert->pCertInfo->Subject.cbData,
                           CRYPT_DECODE_ALLOC_FLAG | CRYPT_DECODE_NOCOPY_FLAG,
                           &decode_para,
                           &name_info,
                           &name_info_size);
  if (rv) {
    scoped_ptr_malloc<CERT_NAME_INFO> scoped_name_info(name_info);

    // The Subject field may have multiple common names.  According to the
    // "PKI Layer Cake" paper, CryptoAPI uses every common name in the
    // Subject field, so we inspect every common name.
    //
    // From RFC 5280:
    // X520CommonName ::= CHOICE {
    //       teletexString     TeletexString   (SIZE (1..ub-common-name)),
    //       printableString   PrintableString (SIZE (1..ub-common-name)),
    //       universalString   UniversalString (SIZE (1..ub-common-name)),
    //       utf8String        UTF8String      (SIZE (1..ub-common-name)),
    //       bmpString         BMPString       (SIZE (1..ub-common-name)) }
    //
    // We also check IA5String and VisibleString.
    for (DWORD i = 0; i < name_info->cRDN; ++i) {
      PCERT_RDN rdn = &name_info->rgRDN[i];
      for (DWORD j = 0; j < rdn->cRDNAttr; ++j) {
        PCERT_RDN_ATTR rdn_attr = &rdn->rgRDNAttr[j];
        if (strcmp(rdn_attr->pszObjId, szOID_COMMON_NAME) == 0) {
          switch (rdn_attr->dwValueType) {
            // After the CryptoAPI ASN.1 security vulnerabilities described in
            // http://www.microsoft.com/technet/security/Bulletin/MS09-056.mspx
            // were patched, we get CERT_RDN_ENCODED_BLOB for a common name
            // that contains a NULL character.
            case CERT_RDN_ENCODED_BLOB:
              break;
            // Array of 8-bit characters.
            case CERT_RDN_PRINTABLE_STRING:
            case CERT_RDN_TELETEX_STRING:
            case CERT_RDN_IA5_STRING:
            case CERT_RDN_VISIBLE_STRING:
              for (DWORD k = 0; k < rdn_attr->Value.cbData; ++k) {
                if (rdn_attr->Value.pbData[k] == '\0')
                  return true;
              }
              break;
            // Array of 16-bit characters.
            case CERT_RDN_BMP_STRING:
            case CERT_RDN_UTF8_STRING: {
              DWORD num_wchars = rdn_attr->Value.cbData / 2;
              wchar_t* common_name =
                  reinterpret_cast<wchar_t*>(rdn_attr->Value.pbData);
              for (DWORD k = 0; k < num_wchars; ++k) {
                if (common_name[k] == L'\0')
                  return true;
              }
              break;
            }
            // Array of ints (32-bit).
            case CERT_RDN_UNIVERSAL_STRING: {
              DWORD num_ints = rdn_attr->Value.cbData / 4;
              int* common_name =
                  reinterpret_cast<int*>(rdn_attr->Value.pbData);
              for (DWORD k = 0; k < num_ints; ++k) {
                if (common_name[k] == 0)
                  return true;
              }
              break;
            }
            default:
              NOTREACHED();
              break;
          }
        }
      }
    }
  }
  return false;
}

// Saves some information about the certificate chain chain_context in
// *verify_result.  The caller MUST initialize *verify_result before calling
// this function.
void GetCertChainInfo(PCCERT_CHAIN_CONTEXT chain_context,
                      CertVerifyResult* verify_result) {
  PCERT_SIMPLE_CHAIN first_chain = chain_context->rgpChain[0];
  int num_elements = first_chain->cElement;
  PCERT_CHAIN_ELEMENT* element = first_chain->rgpElement;

  // Each chain starts with the end entity certificate (i = 0) and ends with
  // the root CA certificate (i = num_elements - 1).  Do not inspect the
  // signature algorithm of the root CA certificate because the signature on
  // the trust anchor is not important.
  for (int i = 0; i < num_elements - 1; ++i) {
    PCCERT_CONTEXT cert = element[i]->pCertContext;
    const char* algorithm = cert->pCertInfo->SignatureAlgorithm.pszObjId;
    if (strcmp(algorithm, szOID_RSA_MD5RSA) == 0) {
      // md5WithRSAEncryption: 1.2.840.113549.1.1.4
      verify_result->has_md5 = true;
      if (i != 0)
        verify_result->has_md5_ca = true;
    } else if (strcmp(algorithm, szOID_RSA_MD2RSA) == 0) {
      // md2WithRSAEncryption: 1.2.840.113549.1.1.2
      verify_result->has_md2 = true;
      if (i != 0)
        verify_result->has_md2_ca = true;
    } else if (strcmp(algorithm, szOID_RSA_MD4RSA) == 0) {
      // md4WithRSAEncryption: 1.2.840.113549.1.1.3
      verify_result->has_md4 = true;
    }
  }
}

// Decodes the cert's certificatePolicies extension into a CERT_POLICIES_INFO
// structure and stores it in *output.
void GetCertPoliciesInfo(PCCERT_CONTEXT cert,
                         scoped_ptr_malloc<CERT_POLICIES_INFO>* output) {
  PCERT_EXTENSION extension = CertFindExtension(szOID_CERT_POLICIES,
                                                cert->pCertInfo->cExtension,
                                                cert->pCertInfo->rgExtension);
  if (!extension)
    return;

  CRYPT_DECODE_PARA decode_para;
  decode_para.cbSize = sizeof(decode_para);
  decode_para.pfnAlloc = MyCryptAlloc;
  decode_para.pfnFree = MyCryptFree;
  CERT_POLICIES_INFO* policies_info = NULL;
  DWORD policies_info_size = 0;
  BOOL rv;
  rv = CryptDecodeObjectEx(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                           szOID_CERT_POLICIES,
                           extension->Value.pbData,
                           extension->Value.cbData,
                           CRYPT_DECODE_ALLOC_FLAG | CRYPT_DECODE_NOCOPY_FLAG,
                           &decode_para,
                           &policies_info,
                           &policies_info_size);
  if (rv)
    output->reset(policies_info);
}

// Helper function to parse a principal from a WinInet description of that
// principal.
void ParsePrincipal(const std::string& description,
                    CertPrincipal* principal) {
  // The description of the principal is a string with each LDAP value on
  // a separate line.
  const std::string kDelimiters("\r\n");

  std::vector<std::string> common_names, locality_names, state_names,
      country_names;

  // TODO(jcampan): add business_category and serial_number.
  const std::string kPrefixes[] = { std::string("CN="),
                                    std::string("L="),
                                    std::string("S="),
                                    std::string("C="),
                                    std::string("STREET="),
                                    std::string("O="),
                                    std::string("OU="),
                                    std::string("DC=") };

  std::vector<std::string>* values[] = {
      &common_names, &locality_names,
      &state_names, &country_names,
      &(principal->street_addresses),
      &(principal->organization_names),
      &(principal->organization_unit_names),
      &(principal->domain_components) };
  DCHECK(arraysize(kPrefixes) == arraysize(values));

  StringTokenizer str_tok(description, kDelimiters);
  while (str_tok.GetNext()) {
    std::string entry = str_tok.token();
    for (int i = 0; i < arraysize(kPrefixes); i++) {
      if (!entry.compare(0, kPrefixes[i].length(), kPrefixes[i])) {
        std::string value = entry.substr(kPrefixes[i].length());
        // Remove enclosing double-quotes if any.
        if (value.size() >= 2 &&
            value[0] == '"' && value[value.size() - 1] == '"')
          value = value.substr(1, value.size() - 2);
        values[i]->push_back(value);
        break;
      }
    }
  }

  // We don't expect to have more than one CN, L, S, and C.
  std::vector<std::string>* single_value_lists[4] = {
      &common_names, &locality_names, &state_names, &country_names };
  std::string* single_values[4] = {
      &principal->common_name, &principal->locality_name,
      &principal->state_or_province_name, &principal->country_name };
  for (int i = 0; i < arraysize(single_value_lists); ++i) {
    int length = static_cast<int>(single_value_lists[i]->size());
    DCHECK(single_value_lists[i]->size() <= 1);
    if (single_value_lists[i]->size() > 0)
      *(single_values[i]) = (*(single_value_lists[i]))[0];
  }
}

void AddCertsFromStore(HCERTSTORE store,
                       X509Certificate::OSCertHandles* results) {
  PCCERT_CONTEXT cert = NULL;

  while ((cert = CertEnumCertificatesInStore(store, cert)) != NULL) {
    PCCERT_CONTEXT to_add = NULL;
    if (CertAddCertificateContextToStore(
        NULL,  // The cert won't be persisted in any cert store. This breaks
               // any association the context currently has to |store|, which
               // allows us, the caller, to safely close |store| without
               // releasing the cert handles.
        cert,
        CERT_STORE_ADD_USE_EXISTING,
        &to_add) && to_add != NULL) {
      // When processing stores generated from PKCS#7/PKCS#12 files, it
      // appears that the order returned is the inverse of the order that it
      // appeared in the file.
      // TODO(rsleevi): Ensure this order is consistent across all Win
      // versions
      results->insert(results->begin(), to_add);
    }
  }
}

X509Certificate::OSCertHandles ParsePKCS7(const char* data, size_t length) {
  X509Certificate::OSCertHandles results;
  CERT_BLOB data_blob;
  data_blob.cbData = length;
  data_blob.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(data));

  HCERTSTORE out_store = NULL;

  DWORD expected_types = CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED |
                         CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED |
                         CERT_QUERY_CONTENT_FLAG_PKCS7_UNSIGNED;

  if (!CryptQueryObject(CERT_QUERY_OBJECT_BLOB, &data_blob, expected_types,
                        CERT_QUERY_FORMAT_FLAG_BINARY, 0, NULL, NULL, NULL,
                        &out_store, NULL, NULL) || out_store == NULL) {
    return results;
  }

  AddCertsFromStore(out_store, &results);
  CertCloseStore(out_store, CERT_CLOSE_STORE_CHECK_FLAG);

  return results;
}

}  // namespace

void X509Certificate::Initialize() {
  std::wstring subject_info;
  std::wstring issuer_info;
  DWORD name_size;
  DCHECK(cert_handle_);
  name_size = CertNameToStr(cert_handle_->dwCertEncodingType,
                            &cert_handle_->pCertInfo->Subject,
                            CERT_X500_NAME_STR | CERT_NAME_STR_CRLF_FLAG,
                            NULL, 0);
  name_size = CertNameToStr(cert_handle_->dwCertEncodingType,
                            &cert_handle_->pCertInfo->Subject,
                            CERT_X500_NAME_STR | CERT_NAME_STR_CRLF_FLAG,
                            WriteInto(&subject_info, name_size), name_size);
  name_size = CertNameToStr(cert_handle_->dwCertEncodingType,
                            &cert_handle_->pCertInfo->Issuer,
                            CERT_X500_NAME_STR | CERT_NAME_STR_CRLF_FLAG,
                            NULL, 0);
  name_size = CertNameToStr(cert_handle_->dwCertEncodingType,
                            &cert_handle_->pCertInfo->Issuer,
                            CERT_X500_NAME_STR | CERT_NAME_STR_CRLF_FLAG,
                            WriteInto(&issuer_info, name_size), name_size);
  ParsePrincipal(WideToUTF8(subject_info), &subject_);
  ParsePrincipal(WideToUTF8(issuer_info), &issuer_);

  valid_start_ = Time::FromFileTime(cert_handle_->pCertInfo->NotBefore);
  valid_expiry_ = Time::FromFileTime(cert_handle_->pCertInfo->NotAfter);

  fingerprint_ = CalculateFingerprint(cert_handle_);
}

// static
X509Certificate* X509Certificate::CreateFromPickle(const Pickle& pickle,
                                                   void** pickle_iter) {
  const char* data;
  int length;
  if (!pickle.ReadData(pickle_iter, &data, &length))
    return NULL;

  OSCertHandle cert_handle = NULL;
  if (!CertAddSerializedElementToStore(
      NULL,  // the cert won't be persisted in any cert store
      reinterpret_cast<const BYTE*>(data), length,
      CERT_STORE_ADD_USE_EXISTING, 0, CERT_STORE_CERTIFICATE_CONTEXT_FLAG,
      NULL, reinterpret_cast<const void **>(&cert_handle)))
    return NULL;

  X509Certificate* cert = CreateFromHandle(cert_handle,
                                           SOURCE_LONE_CERT_IMPORT,
                                           OSCertHandles());
  FreeOSCertHandle(cert_handle);
  return cert;
}

// static
X509Certificate* X509Certificate::CreateSelfSigned(
    base::RSAPrivateKey* key,
    const std::string& subject,
    uint32 serial_number,
    base::TimeDelta valid_duration) {
  // Get the ASN.1 encoding of the certificate subject.
  std::wstring w_subject = ASCIIToWide(subject);
  DWORD encoded_subject_length = 0;
  if (!CertStrToName(
          X509_ASN_ENCODING,
          const_cast<wchar_t*>(w_subject.c_str()),
          CERT_X500_NAME_STR, NULL, NULL, &encoded_subject_length, NULL)) {
    return NULL;
  }

  scoped_array<char> encoded_subject(new char[encoded_subject_length]);
  if (!CertStrToName(
          X509_ASN_ENCODING,
          const_cast<wchar_t*>(w_subject.c_str()),
          CERT_X500_NAME_STR, NULL,
          reinterpret_cast<BYTE*>(encoded_subject.get()),
          &encoded_subject_length, NULL)) {
    return NULL;
  }

  CERT_NAME_BLOB subject_name;
  memset(&subject_name, 0, sizeof(subject_name));
  subject_name.cbData = encoded_subject_length;
  subject_name.pbData = reinterpret_cast<BYTE*>(encoded_subject.get());

  CRYPT_ALGORITHM_IDENTIFIER sign_algo;
  memset(&sign_algo, 0, sizeof(sign_algo));
  sign_algo.pszObjId = szOID_RSA_SHA1RSA;

  base::Time not_valid = base::Time::Now() + valid_duration;
  base::Time::Exploded exploded;
  not_valid.UTCExplode(&exploded);

  // Create the system time struct representing our exploded time.
  SYSTEMTIME system_time;
  system_time.wYear = exploded.year;
  system_time.wMonth = exploded.month;
  system_time.wDayOfWeek = exploded.day_of_week;
  system_time.wDay = exploded.day_of_month;
  system_time.wHour = exploded.hour;
  system_time.wMinute = exploded.minute;
  system_time.wSecond = exploded.second;
  system_time.wMilliseconds = exploded.millisecond;

  PCCERT_CONTEXT cert_handle =
      CertCreateSelfSignCertificate(key->provider(), &subject_name,
                                    CERT_CREATE_SELFSIGN_NO_KEY_INFO,
                                    NULL, &sign_algo, 0, &system_time, 0);
  DCHECK(cert_handle) << "Failed to create self-signed certificate: "
                      << logging::GetLastSystemErrorCode();

  X509Certificate* cert = CreateFromHandle(cert_handle,
                                           SOURCE_LONE_CERT_IMPORT,
                                           OSCertHandles());
  FreeOSCertHandle(cert_handle);
  return cert;
}

void X509Certificate::Persist(Pickle* pickle) {
  DCHECK(cert_handle_);
  DWORD length;
  if (!CertSerializeCertificateStoreElement(cert_handle_, 0,
      NULL, &length)) {
    NOTREACHED();
    return;
  }
  BYTE* data = reinterpret_cast<BYTE*>(pickle->BeginWriteData(length));
  if (!CertSerializeCertificateStoreElement(cert_handle_, 0,
      data, &length)) {
    NOTREACHED();
    length = 0;
  }
  pickle->TrimWriteData(length);
}

void X509Certificate::GetDNSNames(std::vector<std::string>* dns_names) const {
  dns_names->clear();
  if (cert_handle_) {
    scoped_ptr_malloc<CERT_ALT_NAME_INFO> alt_name_info;
    GetCertSubjectAltName(cert_handle_, &alt_name_info);
    CERT_ALT_NAME_INFO* alt_name = alt_name_info.get();
    if (alt_name) {
      int num_entries = alt_name->cAltEntry;
      for (int i = 0; i < num_entries; i++) {
        // dNSName is an ASN.1 IA5String representing a string of ASCII
        // characters, so we can use WideToASCII here.
        if (alt_name->rgAltEntry[i].dwAltNameChoice == CERT_ALT_NAME_DNS_NAME)
          dns_names->push_back(
              WideToASCII(alt_name->rgAltEntry[i].pwszDNSName));
      }
    }
  }
  if (dns_names->empty())
    dns_names->push_back(subject_.common_name);
}

class GlobalCertStore {
 public:
  HCERTSTORE cert_store() {
    return cert_store_;
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<GlobalCertStore>;

  GlobalCertStore()
      : cert_store_(CertOpenStore(CERT_STORE_PROV_MEMORY, 0, NULL, 0, NULL)) {
  }

  ~GlobalCertStore() {
    CertCloseStore(cert_store_, 0 /* flags */);
  }

  const HCERTSTORE cert_store_;

  DISALLOW_COPY_AND_ASSIGN(GlobalCertStore);
};

static base::LazyInstance<GlobalCertStore> g_cert_store(
    base::LINKER_INITIALIZED);

// static
HCERTSTORE X509Certificate::cert_store() {
  return g_cert_store.Get().cert_store();
}

int X509Certificate::Verify(const std::string& hostname,
                            int flags,
                            CertVerifyResult* verify_result) const {
  verify_result->Reset();
  if (!cert_handle_)
    return ERR_UNEXPECTED;

  // Build and validate certificate chain.

  CERT_CHAIN_PARA chain_para;
  memset(&chain_para, 0, sizeof(chain_para));
  chain_para.cbSize = sizeof(chain_para);
  // ExtendedKeyUsage.
  // We still need to request szOID_SERVER_GATED_CRYPTO and szOID_SGC_NETSCAPE
  // today because some certificate chains need them.  IE also requests these
  // two usages.
  static const LPSTR usage[] = {
    szOID_PKIX_KP_SERVER_AUTH,
    szOID_SERVER_GATED_CRYPTO,
    szOID_SGC_NETSCAPE
  };
  chain_para.RequestedUsage.dwType = USAGE_MATCH_TYPE_OR;
  chain_para.RequestedUsage.Usage.cUsageIdentifier = arraysize(usage);
  chain_para.RequestedUsage.Usage.rgpszUsageIdentifier =
      const_cast<LPSTR*>(usage);
  // We can set CERT_CHAIN_RETURN_LOWER_QUALITY_CONTEXTS to get more chains.
  DWORD chain_flags = CERT_CHAIN_CACHE_END_CERT;
  if (flags & VERIFY_REV_CHECKING_ENABLED) {
    verify_result->cert_status |= CERT_STATUS_REV_CHECKING_ENABLED;
    chain_flags |= CERT_CHAIN_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT;
  } else {
    chain_flags |= CERT_CHAIN_REVOCATION_CHECK_CACHE_ONLY;
    // EV requires revocation checking.
    flags &= ~VERIFY_EV_CERT;
  }

  // Get the certificatePolicies extension of the certificate.
  scoped_ptr_malloc<CERT_POLICIES_INFO> policies_info;
  LPSTR ev_policy_oid = NULL;
  if (flags & VERIFY_EV_CERT) {
    GetCertPoliciesInfo(cert_handle_, &policies_info);
    if (policies_info.get()) {
      EVRootCAMetadata* metadata = EVRootCAMetadata::GetInstance();
      for (DWORD i = 0; i < policies_info->cPolicyInfo; ++i) {
        LPSTR policy_oid = policies_info->rgPolicyInfo[i].pszPolicyIdentifier;
        if (metadata->IsEVPolicyOID(policy_oid)) {
          ev_policy_oid = policy_oid;
          chain_para.RequestedIssuancePolicy.dwType = USAGE_MATCH_TYPE_AND;
          chain_para.RequestedIssuancePolicy.Usage.cUsageIdentifier = 1;
          chain_para.RequestedIssuancePolicy.Usage.rgpszUsageIdentifier =
              &ev_policy_oid;
          break;
        }
      }
    }
  }

  // For non-test scenarios, use the default HCERTCHAINENGINE, NULL, which
  // corresponds to HCCE_CURRENT_USER and is is initialized as needed by
  // crypt32. However, when testing, it is necessary to create a new
  // HCERTCHAINENGINE and use that instead. This is because each
  // HCERTCHAINENGINE maintains a cache of information about certificates
  // encountered, and each test run may modify the trust status of a
  // certificate.
  ScopedHCERTCHAINENGINE chain_engine(NULL);
  if (TestRootCerts::HasInstance())
    chain_engine.reset(TestRootCerts::GetInstance()->GetChainEngine());

  PCCERT_CHAIN_CONTEXT chain_context;
  // IE passes a non-NULL pTime argument that specifies the current system
  // time.  IE passes CERT_CHAIN_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT as the
  // chain_flags argument.
  if (!CertGetCertificateChain(
           chain_engine,
           cert_handle_,
           NULL,  // current system time
           cert_handle_->hCertStore,
           &chain_para,
           chain_flags,
           NULL,  // reserved
           &chain_context)) {
    return MapSecurityError(GetLastError());
  }
  if (chain_context->TrustStatus.dwErrorStatus &
      CERT_TRUST_IS_NOT_VALID_FOR_USAGE) {
    ev_policy_oid = NULL;
    chain_para.RequestedIssuancePolicy.Usage.cUsageIdentifier = 0;
    chain_para.RequestedIssuancePolicy.Usage.rgpszUsageIdentifier = NULL;
    CertFreeCertificateChain(chain_context);
    if (!CertGetCertificateChain(
             chain_engine,
             cert_handle_,
             NULL,  // current system time
             cert_handle_->hCertStore,
             &chain_para,
             chain_flags,
             NULL,  // reserved
             &chain_context)) {
      return MapSecurityError(GetLastError());
    }
  }
  ScopedCertChainContext scoped_chain_context(chain_context);

  GetCertChainInfo(chain_context, verify_result);
  verify_result->cert_status |= MapCertChainErrorStatusToCertStatus(
      chain_context->TrustStatus.dwErrorStatus);

  // Treat certificates signed using broken signature algorithms as invalid.
  if (verify_result->has_md4)
    verify_result->cert_status |= CERT_STATUS_INVALID;

  // Flag certificates signed using weak signature algorithms.
  if (verify_result->has_md2)
    verify_result->cert_status |= CERT_STATUS_WEAK_SIGNATURE_ALGORITHM;

  // Flag certificates that have a Subject common name with a NULL character.
  if (CertSubjectCommonNameHasNull(cert_handle_))
    verify_result->cert_status |= CERT_STATUS_INVALID;

  std::wstring wstr_hostname = ASCIIToWide(hostname);

  SSL_EXTRA_CERT_CHAIN_POLICY_PARA extra_policy_para;
  memset(&extra_policy_para, 0, sizeof(extra_policy_para));
  extra_policy_para.cbSize = sizeof(extra_policy_para);
  extra_policy_para.dwAuthType = AUTHTYPE_SERVER;
  extra_policy_para.fdwChecks = 0;
  extra_policy_para.pwszServerName =
      const_cast<wchar_t*>(wstr_hostname.c_str());

  CERT_CHAIN_POLICY_PARA policy_para;
  memset(&policy_para, 0, sizeof(policy_para));
  policy_para.cbSize = sizeof(policy_para);
  policy_para.dwFlags = 0;
  policy_para.pvExtraPolicyPara = &extra_policy_para;

  CERT_CHAIN_POLICY_STATUS policy_status;
  memset(&policy_status, 0, sizeof(policy_status));
  policy_status.cbSize = sizeof(policy_status);

  if (!CertVerifyCertificateChainPolicy(
           CERT_CHAIN_POLICY_SSL,
           chain_context,
           &policy_para,
           &policy_status)) {
    return MapSecurityError(GetLastError());
  }

  if (policy_status.dwError) {
    verify_result->cert_status |= MapNetErrorToCertStatus(
        MapSecurityError(policy_status.dwError));

    // CertVerifyCertificateChainPolicy reports only one error (in
    // policy_status.dwError) if the certificate has multiple errors.
    // CertGetCertificateChain doesn't report certificate name mismatch, so
    // CertVerifyCertificateChainPolicy is the only function that can report
    // certificate name mismatch.
    //
    // To prevent a potential certificate name mismatch from being hidden by
    // some other certificate error, if we get any other certificate error,
    // we call CertVerifyCertificateChainPolicy again, ignoring all other
    // certificate errors.  Both extra_policy_para.fdwChecks and
    // policy_para.dwFlags allow us to ignore certificate errors, so we set
    // them both.
    if (policy_status.dwError != CERT_E_CN_NO_MATCH) {
      const DWORD extra_ignore_flags =
          0x00000080 |  // SECURITY_FLAG_IGNORE_REVOCATION
          0x00000100 |  // SECURITY_FLAG_IGNORE_UNKNOWN_CA
          0x00002000 |  // SECURITY_FLAG_IGNORE_CERT_DATE_INVALID
          0x00000200;   // SECURITY_FLAG_IGNORE_WRONG_USAGE
      extra_policy_para.fdwChecks = extra_ignore_flags;
      const DWORD ignore_flags =
          CERT_CHAIN_POLICY_IGNORE_ALL_NOT_TIME_VALID_FLAGS |
          CERT_CHAIN_POLICY_IGNORE_INVALID_BASIC_CONSTRAINTS_FLAG |
          CERT_CHAIN_POLICY_ALLOW_UNKNOWN_CA_FLAG |
          CERT_CHAIN_POLICY_IGNORE_WRONG_USAGE_FLAG |
          CERT_CHAIN_POLICY_IGNORE_INVALID_NAME_FLAG |
          CERT_CHAIN_POLICY_IGNORE_INVALID_POLICY_FLAG |
          CERT_CHAIN_POLICY_IGNORE_ALL_REV_UNKNOWN_FLAGS |
          CERT_CHAIN_POLICY_ALLOW_TESTROOT_FLAG |
          CERT_CHAIN_POLICY_TRUST_TESTROOT_FLAG |
          CERT_CHAIN_POLICY_IGNORE_NOT_SUPPORTED_CRITICAL_EXT_FLAG |
          CERT_CHAIN_POLICY_IGNORE_PEER_TRUST_FLAG;
      policy_para.dwFlags = ignore_flags;
      if (!CertVerifyCertificateChainPolicy(
               CERT_CHAIN_POLICY_SSL,
               chain_context,
               &policy_para,
               &policy_status)) {
        return MapSecurityError(GetLastError());
      }
      if (policy_status.dwError) {
        verify_result->cert_status |= MapNetErrorToCertStatus(
            MapSecurityError(policy_status.dwError));
      }
    }
  }

  // TODO(wtc): Suppress CERT_STATUS_NO_REVOCATION_MECHANISM for now to be
  // compatible with WinHTTP, which doesn't report this error (bug 3004).
  verify_result->cert_status &= ~CERT_STATUS_NO_REVOCATION_MECHANISM;

  if (IsCertStatusError(verify_result->cert_status))
    return MapCertStatusToNetError(verify_result->cert_status);

  if (ev_policy_oid && CheckEV(chain_context, ev_policy_oid))
    verify_result->cert_status |= CERT_STATUS_IS_EV;
  return OK;
}

bool X509Certificate::GetDEREncoded(std::string* encoded) {
  if (!cert_handle_->pbCertEncoded || !cert_handle_->cbCertEncoded)
    return false;
  encoded->clear();
  encoded->append(reinterpret_cast<char*>(cert_handle_->pbCertEncoded),
                  cert_handle_->cbCertEncoded);
  return true;
}

// Returns true if the certificate is an extended-validation certificate.
//
// This function checks the certificatePolicies extensions of the
// certificates in the certificate chain according to Section 7 (pp. 11-12)
// of the EV Certificate Guidelines Version 1.0 at
// http://cabforum.org/EV_Certificate_Guidelines.pdf.
bool X509Certificate::CheckEV(PCCERT_CHAIN_CONTEXT chain_context,
                              const char* policy_oid) const {
  DCHECK(chain_context->cChain != 0);
  // If the cert doesn't match any of the policies, the
  // CERT_TRUST_IS_NOT_VALID_FOR_USAGE bit (0x10) in
  // chain_context->TrustStatus.dwErrorStatus is set.
  DWORD error_status = chain_context->TrustStatus.dwErrorStatus;
  DWORD info_status = chain_context->TrustStatus.dwInfoStatus;
  if (!chain_context->cChain || error_status != CERT_TRUST_NO_ERROR)
    return false;

  // Check the end certificate simple chain (chain_context->rgpChain[0]).
  // If the end certificate's certificatePolicies extension contains the
  // EV policy OID of the root CA, return true.
  PCERT_CHAIN_ELEMENT* element = chain_context->rgpChain[0]->rgpElement;
  int num_elements = chain_context->rgpChain[0]->cElement;
  if (num_elements < 2)
    return false;

  // Look up the EV policy OID of the root CA.
  PCCERT_CONTEXT root_cert = element[num_elements - 1]->pCertContext;
  SHA1Fingerprint fingerprint = CalculateFingerprint(root_cert);
  EVRootCAMetadata* metadata = EVRootCAMetadata::GetInstance();
  return metadata->HasEVPolicyOID(fingerprint, policy_oid);
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
  return a->cbCertEncoded == b->cbCertEncoded &&
      memcmp(a->pbCertEncoded, b->pbCertEncoded, a->cbCertEncoded) == 0;
}

// static
X509Certificate::OSCertHandle X509Certificate::CreateOSCertHandleFromBytes(
    const char* data, int length) {
  OSCertHandle cert_handle = NULL;
  if (!CertAddEncodedCertificateToStore(
      NULL,  // the cert won't be persisted in any cert store
      X509_ASN_ENCODING,
      reinterpret_cast<const BYTE*>(data), length,
      CERT_STORE_ADD_USE_EXISTING,
      &cert_handle))
    return NULL;

  return cert_handle;
}

X509Certificate::OSCertHandles X509Certificate::CreateOSCertHandlesFromBytes(
    const char* data, int length, Format format) {
  OSCertHandles results;
  switch (format) {
    case FORMAT_SINGLE_CERTIFICATE: {
      OSCertHandle handle = CreateOSCertHandleFromBytes(data, length);
      if (handle != NULL)
        results.push_back(handle);
      break;
    }
    case FORMAT_PKCS7:
      results = ParsePKCS7(data, length);
      break;
    default:
      NOTREACHED() << "Certificate format " << format << " unimplemented";
      break;
  }

  return results;
}


// static
X509Certificate::OSCertHandle X509Certificate::DupOSCertHandle(
    OSCertHandle cert_handle) {
  return CertDuplicateCertificateContext(cert_handle);
}

// static
void X509Certificate::FreeOSCertHandle(OSCertHandle cert_handle) {
  CertFreeCertificateContext(cert_handle);
}

// static
SHA1Fingerprint X509Certificate::CalculateFingerprint(
    OSCertHandle cert) {
  DCHECK(NULL != cert->pbCertEncoded);
  DCHECK(0 != cert->cbCertEncoded);

  BOOL rv;
  SHA1Fingerprint sha1;
  DWORD sha1_size = sizeof(sha1.data);
  rv = CryptHashCertificate(NULL, CALG_SHA1, 0, cert->pbCertEncoded,
                            cert->cbCertEncoded, sha1.data, &sha1_size);
  DCHECK(rv && sha1_size == sizeof(sha1.data));
  if (!rv)
    memset(sha1.data, 0, sizeof(sha1.data));
  return sha1;
}

}  // namespace net
