// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cert_database.h"

#include "net/base/net_errors.h"
#include "net/base/x509_certificate.h"

namespace net {

CertDatabase::CertDatabase() {
}

int CertDatabase::CheckUserCert(X509Certificate* cert) {
  if (!cert)
    return ERR_CERT_INVALID;
  if (cert->HasExpired())
    return ERR_CERT_DATE_INVALID;

  // TODO(bulach): implement me.
  return ERR_NOT_IMPLEMENTED;
}

int CertDatabase::AddUserCert(X509Certificate* cert) {
  // TODO(bulach): implement me.
  return ERR_NOT_IMPLEMENTED;
}

void CertDatabase::ListCerts(CertificateList* certs) {
  // TODO(bulach): implement me.
}

int CertDatabase::ImportFromPKCS12(const std::string& data,
                                   const string16& password) {
  // TODO(bulach): implement me.
  return ERR_NOT_IMPLEMENTED;
}

int CertDatabase::ExportToPKCS12(const CertificateList& certs,
                                 const string16& password,
                                 std::string* output) const {
  // TODO(bulach): implement me.
  return 0;
}

bool CertDatabase::DeleteCertAndKey(const X509Certificate* cert) {
  // TODO(bulach): implement me.
  return false;
}

unsigned int CertDatabase::GetCertTrust(const X509Certificate* cert,
                                        CertType type) const {
  // TODO(bulach): implement me.
  // NOTE: This method is currently only declared for USE_NSS builds.
  return 0;
}

bool CertDatabase::SetCertTrust(const X509Certificate* cert,
                                CertType type,
                                unsigned int trust_bits) {
  // TODO(bulach): implement me.
  // NOTE: This method is currently only declared for USE_NSS builds.
  return false;
}

}  // namespace net
