// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cert_database.h"

#include <openssl/x509.h>

#include "base/logging.h"
#include "net/base/net_errors.h"
#include "net/base/openssl_private_key_store.h"
#include "net/base/x509_certificate.h"

namespace net {

CertDatabase::CertDatabase() {
}

int CertDatabase::CheckUserCert(X509Certificate* cert) {
  if (!cert)
    return ERR_CERT_INVALID;
  if (cert->HasExpired())
    return ERR_CERT_DATE_INVALID;

  if (!OpenSSLPrivateKeyStore::GetInstance()->FetchPrivateKey(
      X509_PUBKEY_get(X509_get_X509_PUBKEY(cert->os_cert_handle()))))
    return ERR_NO_PRIVATE_KEY_FOR_CERT;

  return OK;
}

int CertDatabase::AddUserCert(X509Certificate* cert) {
  // TODO(bulach): implement me.
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
}

void CertDatabase::ListCerts(CertificateList* certs) {
  // TODO(bulach): implement me.
  NOTIMPLEMENTED();
}

CryptoModule* CertDatabase::GetDefaultModule() const {
  // TODO(bulach): implement me.
  NOTIMPLEMENTED();
  return NULL;
}

void CertDatabase::ListModules(CryptoModuleList* modules, bool need_rw) const {
  // TODO(bulach): implement me.
  NOTIMPLEMENTED();
  modules->clear();
}

int CertDatabase::ImportFromPKCS12(net::CryptoModule* module,
                                   const std::string& data,
                                   const string16& password) {
  // TODO(bulach): implement me.
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
}

int CertDatabase::ExportToPKCS12(const CertificateList& certs,
                                 const string16& password,
                                 std::string* output) const {
  // TODO(bulach): implement me.
  NOTIMPLEMENTED();
  return 0;
}

bool CertDatabase::DeleteCertAndKey(const X509Certificate* cert) {
  // TODO(bulach): implement me.
  NOTIMPLEMENTED();
  return false;
}

unsigned int CertDatabase::GetCertTrust(const X509Certificate* cert,
                                        CertType type) const {
  // TODO(bulach): implement me.
  NOTIMPLEMENTED();
  return 0;
}

bool CertDatabase::SetCertTrust(const X509Certificate* cert,
                                CertType type,
                                unsigned int trust_bits) {
  // TODO(bulach): implement me.
  NOTIMPLEMENTED();
  return false;
}

}  // namespace net
