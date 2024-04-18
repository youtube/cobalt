// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_util_nss.h"

#include <cert.h>  // Must be included before certdb.h
#include <certdb.h>
#include <cryptohi.h>
#include <dlfcn.h>
#include <nss.h>
#include <pk11pub.h>
#include <prerror.h>
#include <seccomon.h>
#include <secder.h>
#include <sechash.h>
#include <secmod.h>
#include <secport.h>
#include <string.h>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "crypto/nss_util.h"
#include "crypto/scoped_nss_types.h"
#include "net/cert/x509_util.h"
#include "net/third_party/mozilla_security_manager/nsNSSCertificateDB.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace net::x509_util {

namespace {

// Microsoft User Principal Name: 1.3.6.1.4.1.311.20.2.3
const uint8_t kUpnOid[] = {0x2b, 0x6,  0x1,  0x4, 0x1,
                           0x82, 0x37, 0x14, 0x2, 0x3};

std::string DecodeAVAValue(CERTAVA* ava) {
  SECItem* decode_item = CERT_DecodeAVAValue(&ava->value);
  if (!decode_item)
    return std::string();
  std::string value(reinterpret_cast<char*>(decode_item->data),
                    decode_item->len);
  SECITEM_FreeItem(decode_item, PR_TRUE);
  return value;
}

// Generates a unique nickname for |slot|, returning |nickname| if it is
// already unique.
//
// Note: The nickname returned will NOT include the token name, thus the
// token name must be prepended if calling an NSS function that expects
// <token>:<nickname>.
// TODO(gspencer): Internationalize this: it's wrong to hard-code English.
std::string GetUniqueNicknameForSlot(const std::string& nickname,
                                     const SECItem* subject,
                                     PK11SlotInfo* slot) {
  int index = 2;
  std::string new_name = nickname;
  std::string temp_nickname = new_name;
  std::string token_name;

  if (!slot)
    return new_name;

  if (!PK11_IsInternalKeySlot(slot)) {
    token_name.assign(PK11_GetTokenName(slot));
    token_name.append(":");

    temp_nickname = token_name + new_name;
  }

  while (SEC_CertNicknameConflict(temp_nickname.c_str(),
                                  const_cast<SECItem*>(subject),
                                  CERT_GetDefaultCertDB())) {
    base::SStringPrintf(&new_name, "%s #%d", nickname.c_str(), index++);
    temp_nickname = token_name + new_name;
  }

  return new_name;
}

// The default nickname of the certificate, based on the certificate type
// passed in.
std::string GetDefaultNickname(CERTCertificate* nss_cert, CertType type) {
  std::string result;
  if (type == USER_CERT && nss_cert->slot) {
    // Find the private key for this certificate and see if it has a
    // nickname.  If there is a private key, and it has a nickname, then
    // return that nickname.
    SECKEYPrivateKey* private_key = PK11_FindPrivateKeyFromCert(
        nss_cert->slot, nss_cert, nullptr /*wincx*/);
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
      char* nickname = CERT_MakeCANickname(nss_cert);
      result = nickname;
      PORT_Free(nickname);
      break;
    }
    case USER_CERT: {
      std::string subject_name = GetCERTNameDisplayName(&nss_cert->subject);
      if (subject_name.empty()) {
        const char* email = CERT_GetFirstEmailAddress(nss_cert);
        if (email)
          subject_name = email;
      }
      // TODO(gspencer): Internationalize this. It's wrong to assume English
      // here.
      result =
          base::StringPrintf("%s's %s ID", subject_name.c_str(),
                             GetCERTNameDisplayName(&nss_cert->issuer).c_str());
      break;
    }
    case SERVER_CERT: {
      result = GetCERTNameDisplayName(&nss_cert->subject);
      break;
    }
    case OTHER_CERT:
    default:
      break;
  }
  return result;
}

}  // namespace

bool IsSameCertificate(CERTCertificate* a, CERTCertificate* b) {
  DCHECK(a && b);
  if (a == b)
    return true;
  return a->derCert.len == b->derCert.len &&
         memcmp(a->derCert.data, b->derCert.data, a->derCert.len) == 0;
}

bool IsSameCertificate(CERTCertificate* a, const X509Certificate* b) {
  return IsSameCertificate(a, b->cert_buffer());
}
bool IsSameCertificate(const X509Certificate* a, CERTCertificate* b) {
  return IsSameCertificate(b, a->cert_buffer());
}

bool IsSameCertificate(CERTCertificate* a, const CRYPTO_BUFFER* b) {
  return a->derCert.len == CRYPTO_BUFFER_len(b) &&
         memcmp(a->derCert.data, CRYPTO_BUFFER_data(b), a->derCert.len) == 0;
}
bool IsSameCertificate(const CRYPTO_BUFFER* a, CERTCertificate* b) {
  return IsSameCertificate(b, a);
}

ScopedCERTCertificate CreateCERTCertificateFromBytes(const uint8_t* data,
                                                     size_t length) {
  crypto::EnsureNSSInit();

  if (!NSS_IsInitialized())
    return nullptr;

  SECItem der_cert;
  der_cert.data = const_cast<uint8_t*>(data);
  der_cert.len = base::checked_cast<unsigned>(length);
  der_cert.type = siDERCertBuffer;

  // Parse into a certificate structure.
  return ScopedCERTCertificate(CERT_NewTempCertificate(
      CERT_GetDefaultCertDB(), &der_cert, nullptr /* nickname */,
      PR_FALSE /* is_perm */, PR_TRUE /* copyDER */));
}

ScopedCERTCertificate CreateCERTCertificateFromX509Certificate(
    const X509Certificate* cert) {
  return CreateCERTCertificateFromBytes(CRYPTO_BUFFER_data(cert->cert_buffer()),
                                        CRYPTO_BUFFER_len(cert->cert_buffer()));
}

ScopedCERTCertificateList CreateCERTCertificateListFromX509Certificate(
    const X509Certificate* cert) {
  return x509_util::CreateCERTCertificateListFromX509Certificate(
      cert, InvalidIntermediateBehavior::kFail);
}

ScopedCERTCertificateList CreateCERTCertificateListFromX509Certificate(
    const X509Certificate* cert,
    InvalidIntermediateBehavior invalid_intermediate_behavior) {
  ScopedCERTCertificateList nss_chain;
  nss_chain.reserve(1 + cert->intermediate_buffers().size());
  ScopedCERTCertificate nss_cert =
      CreateCERTCertificateFromX509Certificate(cert);
  if (!nss_cert)
    return {};
  nss_chain.push_back(std::move(nss_cert));
  for (const auto& intermediate : cert->intermediate_buffers()) {
    ScopedCERTCertificate nss_intermediate =
        CreateCERTCertificateFromBytes(CRYPTO_BUFFER_data(intermediate.get()),
                                       CRYPTO_BUFFER_len(intermediate.get()));
    if (!nss_intermediate) {
      if (invalid_intermediate_behavior == InvalidIntermediateBehavior::kFail)
        return {};
      LOG(WARNING) << "error parsing intermediate";
      continue;
    }
    nss_chain.push_back(std::move(nss_intermediate));
  }
  return nss_chain;
}

ScopedCERTCertificateList CreateCERTCertificateListFromBytes(const char* data,
                                                             size_t length,
                                                             int format) {
  CertificateList certs = X509Certificate::CreateCertificateListFromBytes(
      base::as_bytes(base::make_span(data, length)), format);
  ScopedCERTCertificateList nss_chain;
  nss_chain.reserve(certs.size());
  for (const scoped_refptr<X509Certificate>& cert : certs) {
    ScopedCERTCertificate nss_cert =
        CreateCERTCertificateFromX509Certificate(cert.get());
    if (!nss_cert)
      return {};
    nss_chain.push_back(std::move(nss_cert));
  }
  return nss_chain;
}

ScopedCERTCertificate DupCERTCertificate(CERTCertificate* cert) {
  return ScopedCERTCertificate(CERT_DupCertificate(cert));
}

ScopedCERTCertificateList DupCERTCertificateList(
    const ScopedCERTCertificateList& certs) {
  ScopedCERTCertificateList result;
  result.reserve(certs.size());
  for (const ScopedCERTCertificate& cert : certs)
    result.push_back(DupCERTCertificate(cert.get()));
  return result;
}

scoped_refptr<X509Certificate> CreateX509CertificateFromCERTCertificate(
    CERTCertificate* nss_cert,
    const std::vector<CERTCertificate*>& nss_chain) {
  return CreateX509CertificateFromCERTCertificate(nss_cert, nss_chain, {});
}

scoped_refptr<X509Certificate> CreateX509CertificateFromCERTCertificate(
    CERTCertificate* nss_cert,
    const std::vector<CERTCertificate*>& nss_chain,
    X509Certificate::UnsafeCreateOptions options) {
  if (!nss_cert || !nss_cert->derCert.len) {
    return nullptr;
  }
  bssl::UniquePtr<CRYPTO_BUFFER> cert_handle(x509_util::CreateCryptoBuffer(
      base::make_span(nss_cert->derCert.data, nss_cert->derCert.len)));

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.reserve(nss_chain.size());
  for (const CERTCertificate* nss_intermediate : nss_chain) {
    if (!nss_intermediate || !nss_intermediate->derCert.len) {
      return nullptr;
    }
    intermediates.push_back(x509_util::CreateCryptoBuffer(base::make_span(
        nss_intermediate->derCert.data, nss_intermediate->derCert.len)));
  }

  return X509Certificate::CreateFromBufferUnsafeOptions(
      std::move(cert_handle), std::move(intermediates), options);
}

scoped_refptr<X509Certificate> CreateX509CertificateFromCERTCertificate(
    CERTCertificate* cert) {
  return CreateX509CertificateFromCERTCertificate(
      cert, std::vector<CERTCertificate*>());
}

CertificateList CreateX509CertificateListFromCERTCertificates(
    const ScopedCERTCertificateList& certs) {
  CertificateList result;
  result.reserve(certs.size());
  for (const ScopedCERTCertificate& cert : certs) {
    scoped_refptr<X509Certificate> x509_cert(
        CreateX509CertificateFromCERTCertificate(cert.get()));
    if (!x509_cert)
      return {};
    result.push_back(std::move(x509_cert));
  }
  return result;
}

bool GetDEREncoded(CERTCertificate* cert, std::string* der_encoded) {
  if (!cert || !cert->derCert.len)
    return false;
  der_encoded->assign(reinterpret_cast<char*>(cert->derCert.data),
                      cert->derCert.len);
  return true;
}

bool GetPEMEncoded(CERTCertificate* cert, std::string* pem_encoded) {
  if (!cert || !cert->derCert.len)
    return false;
  std::string der(reinterpret_cast<char*>(cert->derCert.data),
                  cert->derCert.len);
  return X509Certificate::GetPEMEncodedFromDER(der, pem_encoded);
}

void GetRFC822SubjectAltNames(CERTCertificate* cert_handle,
                              std::vector<std::string>* names) {
  crypto::ScopedSECItem alt_name(SECITEM_AllocItem(nullptr, nullptr, 0));
  DCHECK(alt_name.get());

  names->clear();
  SECStatus rv = CERT_FindCertExtension(
      cert_handle, SEC_OID_X509_SUBJECT_ALT_NAME, alt_name.get());
  if (rv != SECSuccess)
    return;

  crypto::ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  DCHECK(arena.get());

  CERTGeneralName* alt_name_list;
  alt_name_list = CERT_DecodeAltNameExtension(arena.get(), alt_name.get());

  CERTGeneralName* name = alt_name_list;
  while (name) {
    if (name->type == certRFC822Name) {
      names->push_back(
          std::string(reinterpret_cast<char*>(name->name.other.data),
                      name->name.other.len));
    }
    name = CERT_GetNextGeneralName(name);
    if (name == alt_name_list)
      break;
  }
}

void GetUPNSubjectAltNames(CERTCertificate* cert_handle,
                           std::vector<std::string>* names) {
  crypto::ScopedSECItem alt_name(SECITEM_AllocItem(nullptr, nullptr, 0));
  DCHECK(alt_name.get());

  names->clear();
  SECStatus rv = CERT_FindCertExtension(
      cert_handle, SEC_OID_X509_SUBJECT_ALT_NAME, alt_name.get());
  if (rv != SECSuccess)
    return;

  crypto::ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  DCHECK(arena.get());

  CERTGeneralName* alt_name_list;
  alt_name_list = CERT_DecodeAltNameExtension(arena.get(), alt_name.get());

  CERTGeneralName* name = alt_name_list;
  while (name) {
    if (name->type == certOtherName) {
      OtherName* on = &name->name.OthName;
      if (on->oid.len == sizeof(kUpnOid) &&
          memcmp(on->oid.data, kUpnOid, sizeof(kUpnOid)) == 0) {
        SECItem decoded;
        if (SEC_QuickDERDecodeItem(arena.get(), &decoded,
                                   SEC_ASN1_GET(SEC_UTF8StringTemplate),
                                   &name->name.OthName.name) == SECSuccess) {
          names->push_back(
              std::string(reinterpret_cast<char*>(decoded.data), decoded.len));
        }
      }
    }
    name = CERT_GetNextGeneralName(name);
    if (name == alt_name_list)
      break;
  }
}

std::string GetDefaultUniqueNickname(CERTCertificate* nss_cert,
                                     CertType type,
                                     PK11SlotInfo* slot) {
  return GetUniqueNicknameForSlot(GetDefaultNickname(nss_cert, type),
                                  &nss_cert->derSubject, slot);
}

std::string GetCERTNameDisplayName(CERTName* name) {
  // Search for attributes in the Name, in this order: CN, O and OU.
  CERTAVA* ou_ava = nullptr;
  CERTAVA* o_ava = nullptr;
  CERTRDN** rdns = name->rdns;
  for (size_t rdn = 0; rdns[rdn]; ++rdn) {
    CERTAVA** avas = rdns[rdn]->avas;
    for (size_t pair = 0; avas[pair] != nullptr; ++pair) {
      SECOidTag tag = CERT_GetAVATag(avas[pair]);
      if (tag == SEC_OID_AVA_COMMON_NAME) {
        // If CN is found, return immediately.
        return DecodeAVAValue(avas[pair]);
      }
      // If O or OU is found, save the first one of each so that it can be
      // returned later if no CN attribute is found.
      if (tag == SEC_OID_AVA_ORGANIZATION_NAME && !o_ava)
        o_ava = avas[pair];
      if (tag == SEC_OID_AVA_ORGANIZATIONAL_UNIT_NAME && !ou_ava)
        ou_ava = avas[pair];
    }
  }
  if (o_ava)
    return DecodeAVAValue(o_ava);
  if (ou_ava)
    return DecodeAVAValue(ou_ava);
  return std::string();
}

bool GetValidityTimes(CERTCertificate* cert,
                      base::Time* not_before,
                      base::Time* not_after) {
  PRTime pr_not_before, pr_not_after;
  if (CERT_GetCertTimes(cert, &pr_not_before, &pr_not_after) == SECSuccess) {
    if (not_before)
      *not_before = crypto::PRTimeToBaseTime(pr_not_before);
    if (not_after)
      *not_after = crypto::PRTimeToBaseTime(pr_not_after);
    return true;
  }
  return false;
}

SHA256HashValue CalculateFingerprint256(CERTCertificate* cert) {
  SHA256HashValue sha256;
  memset(sha256.data, 0, sizeof(sha256.data));

  DCHECK(cert->derCert.data);
  DCHECK_NE(0U, cert->derCert.len);

  SECStatus rv = HASH_HashBuf(HASH_AlgSHA256, sha256.data, cert->derCert.data,
                              cert->derCert.len);
  DCHECK_EQ(SECSuccess, rv);

  return sha256;
}

int ImportUserCert(CERTCertificate* cert) {
  return mozilla_security_manager::ImportUserCert(cert);
}

}  // namespace net::x509_util
