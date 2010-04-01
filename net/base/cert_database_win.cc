// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cert_database.h"

#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")

#include "base/logging.h"
#include "base/string_util.h"
#include "net/base/keygen_handler.h"
#include "net/base/net_errors.h"
#include "net/base/x509_certificate.h"

namespace net {

namespace {

// Returns an encoded version of SubjectPublicKeyInfo from |cert| that is
// compatible with KeygenHandler::Cache. If the cert cannot be converted, an
// empty string is returned.
std::string GetSubjectPublicKeyInfo(const X509Certificate* cert) {
  DCHECK(cert);

  std::string result;
  if (!cert->os_cert_handle() || !cert->os_cert_handle()->pCertInfo)
    return result;

  BOOL ok;
  DWORD size = 0;
  PCERT_PUBLIC_KEY_INFO key_info =
      &(cert->os_cert_handle()->pCertInfo->SubjectPublicKeyInfo);
  ok = CryptEncodeObject(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO, key_info,
                         NULL, &size);
  if (!ok)
    return result;

  ok = CryptEncodeObject(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO, key_info,
                         reinterpret_cast<BYTE*>(WriteInto(&result, size + 1)),
                         &size);
  if (!ok) {
    result.clear();
    return result;
  }

  // Per MSDN, the resultant structure may be smaller than the original size
  // supplied, so shrink to the actual size output.
  result.resize(size);

  return result;
}

// Returns true if |cert| was successfully modified to reference |location| to
// obtain the associated private key.
bool LinkCertToPrivateKey(X509Certificate* cert,
                          KeygenHandler::KeyLocation location) {
  DCHECK(cert);

  CRYPT_KEY_PROV_INFO prov_info = { 0 };
  prov_info.pwszContainerName =
      const_cast<LPWSTR>(location.container_name.c_str());
  prov_info.pwszProvName =
      const_cast<LPWSTR>(location.provider_name.c_str());

  // Implicit by it being from KeygenHandler, which only supports RSA keys.
  prov_info.dwProvType = PROV_RSA_FULL;
  prov_info.dwKeySpec = AT_KEYEXCHANGE;

  BOOL ok = CertSetCertificateContextProperty(cert->os_cert_handle(),
                                              CERT_KEY_PROV_INFO_PROP_ID, 0,
                                              &prov_info);
  return ok != FALSE;
}

}  // namespace

CertDatabase::CertDatabase() {
}

int CertDatabase::CheckUserCert(X509Certificate* cert) {
  if (!cert)
    return ERR_CERT_INVALID;
  if (cert->HasExpired())
    return ERR_CERT_DATE_INVALID;

  std::string encoded_info = GetSubjectPublicKeyInfo(cert);
  KeygenHandler::Cache* cache = KeygenHandler::Cache::GetInstance();
  KeygenHandler::KeyLocation location;

  if (encoded_info.empty() || !cache->Find(encoded_info, &location) ||
      !LinkCertToPrivateKey(cert, location))
    return ERR_NO_PRIVATE_KEY_FOR_CERT;

  return OK;
}

int CertDatabase::AddUserCert(X509Certificate* cert) {
  // TODO(rsleevi): Would it be more appropriate to have the CertDatabase take
  // construction parameters (Keychain filepath on Mac OS X, PKCS #11 slot on
  // NSS, and Store Type / Path) here? For now, certs will be stashed into the
  // user's personal store, which will not automatically mark them as trusted,
  // but will allow them to be used for client auth.
  HCERTSTORE cert_db = CertOpenSystemStore(NULL, L"MY");
  if (!cert_db)
    return ERR_ADD_USER_CERT_FAILED;

  BOOL added = CertAddCertificateContextToStore(cert_db,
                                                cert->os_cert_handle(),
                                                CERT_STORE_ADD_USE_EXISTING,
                                                NULL);

  CertCloseStore(cert_db, 0);

  if (!added)
    return ERR_ADD_USER_CERT_FAILED;

  return OK;
}

}  // namespace net
