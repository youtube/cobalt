// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_certificate.h"

#include <cert.h>
#include <cryptohi.h>
#include <keyhi.h>
#include <nss.h>
#include <pk11pub.h>
#include <prtime.h>
#include <secder.h>
#include <sechash.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"
#include "base/time.h"
#include "crypto/nss_util.h"
#include "crypto/rsa_private_key.h"
#include "crypto/scoped_nss_types.h"
#include "net/base/x509_util_nss.h"

namespace net {

namespace {

void ParsePrincipal(CERTName* name,
                    CertPrincipal* principal) {
  typedef char* (*CERTGetNameFunc)(CERTName* name);

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
  DCHECK_EQ(SECSuccess, rv);
  *result = crypto::PRTimeToBaseTime(prtime);
}

SECStatus PR_CALLBACK
CollectCertsCallback(void* arg, SECItem** certs, int num_certs) {
  X509Certificate::OSCertHandles* results =
      reinterpret_cast<X509Certificate::OSCertHandles*>(arg);

  for (int i = 0; i < num_certs; ++i) {
    X509Certificate::OSCertHandle handle =
        X509Certificate::CreateOSCertHandleFromBytes(
            reinterpret_cast<char*>(certs[i]->data), certs[i]->len);
    if (handle)
      results->push_back(handle);
  }

  return SECSuccess;
}

}  // namespace

void X509Certificate::Initialize() {
  ParsePrincipal(&cert_handle_->subject, &subject_);
  ParsePrincipal(&cert_handle_->issuer, &issuer_);

  ParseDate(&cert_handle_->validity.notBefore, &valid_start_);
  ParseDate(&cert_handle_->validity.notAfter, &valid_expiry_);

  fingerprint_ = CalculateFingerprint(cert_handle_);
  ca_fingerprint_ = CalculateCAFingerprint(intermediate_ca_certs_);

  serial_number_ = std::string(
      reinterpret_cast<char*>(cert_handle_->serialNumber.data),
      cert_handle_->serialNumber.len);
}

// static
X509Certificate* X509Certificate::CreateFromBytesWithNickname(
    const char* data,
    int length,
    const char* nickname) {
  OSCertHandle cert_handle = CreateOSCertHandleFromBytesWithNickname(data,
                                                                     length,
                                                                     nickname);
  if (!cert_handle)
    return NULL;

  X509Certificate* cert = CreateFromHandle(cert_handle, OSCertHandles());
  FreeOSCertHandle(cert_handle);

  if (nickname)
    cert->default_nickname_ = nickname;

  return cert;
}

std::string X509Certificate::GetDefaultNickname(CertType type) const {
  if (!default_nickname_.empty())
    return default_nickname_;

  std::string result;
  if (type == USER_CERT && cert_handle_->slot) {
    // Find the private key for this certificate and see if it has a
    // nickname.  If there is a private key, and it has a nickname, then
    // we return that nickname.
    SECKEYPrivateKey* private_key = PK11_FindPrivateKeyFromCert(
        cert_handle_->slot,
        cert_handle_,
        NULL);  // wincx
    if (private_key) {
      char* private_key_nickname = PK11_GetPrivateKeyNickname(private_key);
      if (private_key_nickname) {
        result = private_key_nickname;
        PORT_Free(private_key_nickname);
        SECKEY_DestroyPrivateKey(private_key);
        return result;
      }
      SECKEY_DestroyPrivateKey(private_key);
    }
  }

  switch (type) {
    case CA_CERT: {
      char* nickname = CERT_MakeCANickname(cert_handle_);
      result = nickname;
      PORT_Free(nickname);
      break;
    }
    case USER_CERT: {
      // Create a nickname for a user certificate.
      // We use the scheme used by Firefox:
      // --> <subject's common name>'s <issuer's common name> ID.
      // TODO(gspencer): internationalize this: it's wrong to
      // hard code English.

      std::string username, ca_name;
      char* temp_username = CERT_GetCommonName(
          &cert_handle_->subject);
      char* temp_ca_name = CERT_GetCommonName(&cert_handle_->issuer);
      if (temp_username) {
        username = temp_username;
        PORT_Free(temp_username);
      }
      if (temp_ca_name) {
        ca_name = temp_ca_name;
        PORT_Free(temp_ca_name);
      }
      result = username + "'s " + ca_name + " ID";
      break;
    }
    case SERVER_CERT:
      result = subject_.GetDisplayName();
      break;
    case UNKNOWN_CERT:
    default:
      break;
  }
  return result;
}

// static
X509Certificate* X509Certificate::CreateSelfSigned(
    crypto::RSAPrivateKey* key,
    const std::string& subject,
    uint32 serial_number,
    base::TimeDelta valid_duration) {
  DCHECK(key);

  base::Time not_valid_before = base::Time::Now();
  base::Time not_valid_after = not_valid_before + valid_duration;
  CERTCertificate* cert = x509_util::CreateSelfSignedCert(key->public_key(),
                                                          key->key(),
                                                          subject,
                                                          serial_number,
                                                          not_valid_before,
                                                          not_valid_after);

  if (!cert)
    return NULL;

  X509Certificate* x509_cert = X509Certificate::CreateFromHandle(
      cert, X509Certificate::OSCertHandles());
  CERT_DestroyCertificate(cert);
  return x509_cert;
}

void X509Certificate::GetSubjectAltName(
    std::vector<std::string>* dns_names,
    std::vector<std::string>* ip_addrs) const {
  if (dns_names)
    dns_names->clear();
  if (ip_addrs)
    ip_addrs->clear();

  SECItem alt_name;
  SECStatus rv = CERT_FindCertExtension(cert_handle_,
                                        SEC_OID_X509_SUBJECT_ALT_NAME,
                                        &alt_name);
  if (rv != SECSuccess)
    return;

  PLArenaPool* arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  DCHECK(arena != NULL);

  CERTGeneralName* alt_name_list;
  alt_name_list = CERT_DecodeAltNameExtension(arena, &alt_name);
  SECITEM_FreeItem(&alt_name, PR_FALSE);

  CERTGeneralName* name = alt_name_list;
  while (name) {
    // DNSName and IPAddress are encoded as IA5String and OCTET STRINGs
    // respectively, both of which can be byte copied from
    // SECItemType::data into the appropriate output vector.
    if (dns_names && name->type == certDNSName) {
      dns_names->push_back(std::string(
          reinterpret_cast<char*>(name->name.other.data),
          name->name.other.len));
    } else if (ip_addrs && name->type == certIPAddress) {
      ip_addrs->push_back(std::string(
          reinterpret_cast<char*>(name->name.other.data),
          name->name.other.len));
    }
    name = CERT_GetNextGeneralName(name);
    if (name == alt_name_list)
      break;
  }
  PORT_FreeArena(arena, PR_FALSE);
}

bool X509Certificate::VerifyNameMatch(const std::string& hostname) const {
  return CERT_VerifyCertName(cert_handle_, hostname.c_str()) == SECSuccess;
}

// static
bool X509Certificate::GetDEREncoded(X509Certificate::OSCertHandle cert_handle,
                                    std::string* encoded) {
  if (!cert_handle->derCert.len)
    return false;
  encoded->assign(reinterpret_cast<char*>(cert_handle->derCert.data),
                  cert_handle->derCert.len);
  return true;
}

// static
bool X509Certificate::IsSameOSCert(X509Certificate::OSCertHandle a,
                                   X509Certificate::OSCertHandle b) {
  DCHECK(a && b);
  if (a == b)
    return true;
  return a->derCert.len == b->derCert.len &&
      memcmp(a->derCert.data, b->derCert.data, a->derCert.len) == 0;
}

// static
X509Certificate::OSCertHandle X509Certificate::CreateOSCertHandleFromBytes(
    const char* data, int length) {
  return CreateOSCertHandleFromBytesWithNickname(data, length, NULL);
}

// static
X509Certificate::OSCertHandle
X509Certificate::CreateOSCertHandleFromBytesWithNickname(
    const char* data,
    int length,
    const char* nickname) {
  if (length < 0)
    return NULL;

  crypto::EnsureNSSInit();

  if (!NSS_IsInitialized())
    return NULL;

  SECItem der_cert;
  der_cert.data = reinterpret_cast<unsigned char*>(const_cast<char*>(data));
  der_cert.len  = length;
  der_cert.type = siDERCertBuffer;

  // Parse into a certificate structure.
  return CERT_NewTempCertificate(CERT_GetDefaultCertDB(), &der_cert,
                                 const_cast<char*>(nickname),
                                 PR_FALSE, PR_TRUE);
}

// static
X509Certificate::OSCertHandles X509Certificate::CreateOSCertHandlesFromBytes(
    const char* data,
    int length,
    Format format) {
  OSCertHandles results;
  if (length < 0)
    return results;

  crypto::EnsureNSSInit();

  if (!NSS_IsInitialized())
    return results;

  switch (format) {
    case FORMAT_SINGLE_CERTIFICATE: {
      OSCertHandle handle = CreateOSCertHandleFromBytes(data, length);
      if (handle)
        results.push_back(handle);
      break;
    }
    case FORMAT_PKCS7: {
      // Make a copy since CERT_DecodeCertPackage may modify it
      std::vector<char> data_copy(data, data + length);

      SECStatus result = CERT_DecodeCertPackage(&data_copy[0],
          length, CollectCertsCallback, &results);
      if (result != SECSuccess)
        results.clear();
      break;
    }
    default:
      NOTREACHED() << "Certificate format " << format << " unimplemented";
      break;
  }

  return results;
}

// static
X509Certificate::OSCertHandle X509Certificate::DupOSCertHandle(
    OSCertHandle cert_handle) {
  return CERT_DupCertificate(cert_handle);
}

// static
void X509Certificate::FreeOSCertHandle(OSCertHandle cert_handle) {
  CERT_DestroyCertificate(cert_handle);
}

// static
SHA1HashValue X509Certificate::CalculateFingerprint(
    OSCertHandle cert) {
  SHA1HashValue sha1;
  memset(sha1.data, 0, sizeof(sha1.data));

  DCHECK(NULL != cert->derCert.data);
  DCHECK_NE(0U, cert->derCert.len);

  SECStatus rv = HASH_HashBuf(HASH_AlgSHA1, sha1.data,
                              cert->derCert.data, cert->derCert.len);
  DCHECK_EQ(SECSuccess, rv);

  return sha1;
}

// static
SHA1HashValue X509Certificate::CalculateCAFingerprint(
    const OSCertHandles& intermediates) {
  SHA1HashValue sha1;
  memset(sha1.data, 0, sizeof(sha1.data));

  HASHContext* sha1_ctx = HASH_Create(HASH_AlgSHA1);
  if (!sha1_ctx)
    return sha1;
  HASH_Begin(sha1_ctx);
  for (size_t i = 0; i < intermediates.size(); ++i) {
    CERTCertificate* ca_cert = intermediates[i];
    HASH_Update(sha1_ctx, ca_cert->derCert.data, ca_cert->derCert.len);
  }
  unsigned int result_len;
  HASH_End(sha1_ctx, sha1.data, &result_len, HASH_ResultLenContext(sha1_ctx));
  HASH_Destroy(sha1_ctx);

  return sha1;
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
  return pickle->WriteData(
      reinterpret_cast<const char*>(cert_handle->derCert.data),
      cert_handle->derCert.len);
}

// static
void X509Certificate::GetPublicKeyInfo(OSCertHandle cert_handle,
                                       size_t* size_bits,
                                       PublicKeyType* type) {
  // Since we might fail, set the output parameters to default values first.
  *type = kPublicKeyTypeUnknown;
  *size_bits = 0;

  crypto::ScopedSECKEYPublicKey key(CERT_ExtractPublicKey(cert_handle));
  if (!key.get())
    return;

  *size_bits = SECKEY_PublicKeyStrengthInBits(key.get());

  switch (key->keyType) {
    case rsaKey:
      *type = kPublicKeyTypeRSA;
      break;
    case dsaKey:
      *type = kPublicKeyTypeDSA;
      break;
    case dhKey:
      *type = kPublicKeyTypeDH;
      break;
    case ecKey:
      *type = kPublicKeyTypeECDSA;
      break;
    default:
      *type = kPublicKeyTypeUnknown;
      *size_bits = 0;
      break;
  }
}

}  // namespace net
