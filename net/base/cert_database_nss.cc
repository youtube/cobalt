// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cert_database.h"

#include <cert.h>
#include <certdb.h>
#include <keyhi.h>
#include <pk11pub.h>
#include <secmod.h>

#include "base/logging.h"
#include "base/nss_util.h"
#include "base/scoped_ptr.h"
#include "net/base/net_errors.h"
#include "net/base/x509_certificate.h"
#include "net/third_party/mozilla_security_manager/nsNSSCertificateDB.h"
#include "net/third_party/mozilla_security_manager/nsPKCS12Blob.h"

// PSM = Mozilla's Personal Security Manager.
namespace psm = mozilla_security_manager;

namespace net {

CertDatabase::CertDatabase() {
  base::EnsureNSSInit();
  psm::EnsurePKCS12Init();
}

int CertDatabase::CheckUserCert(X509Certificate* cert_obj) {
  if (!cert_obj)
    return ERR_CERT_INVALID;
  if (cert_obj->HasExpired())
    return ERR_CERT_DATE_INVALID;

  // Check if the private key corresponding to the certificate exist
  // We shouldn't accept any random client certificate sent by a CA.

  // Note: The NSS source documentation wrongly suggests that this
  // also imports the certificate if the private key exists. This
  // doesn't seem to be the case.

  CERTCertificate* cert = cert_obj->os_cert_handle();
  PK11SlotInfo* slot = PK11_KeyForCertExists(cert, NULL, NULL);
  if (!slot) {
    LOG(ERROR) << "No corresponding private key in store";
    return ERR_NO_PRIVATE_KEY_FOR_CERT;
  }
  PK11_FreeSlot(slot);

  return OK;
}

int CertDatabase::AddUserCert(X509Certificate* cert_obj) {
  CERTCertificate* cert = cert_obj->os_cert_handle();
  PK11SlotInfo* slot = NULL;
  std::string nickname;

  // Create a nickname for this certificate.
  // We use the scheme used by Firefox:
  // --> <subject's common name>'s <issuer's common name> ID.

  std::string username, ca_name;
  char* temp_username = CERT_GetCommonName(&cert->subject);
  char* temp_ca_name = CERT_GetCommonName(&cert->issuer);
  if (temp_username) {
    username = temp_username;
    PORT_Free(temp_username);
  }
  if (temp_ca_name) {
    ca_name = temp_ca_name;
    PORT_Free(temp_ca_name);
  }
  nickname = username + "'s " + ca_name + " ID";

  {
    base::AutoNSSWriteLock lock;
    slot = PK11_ImportCertForKey(cert,
                                 const_cast<char*>(nickname.c_str()),
                                 NULL);
  }

  if (!slot) {
    LOG(ERROR) << "Couldn't import user certificate.";
    return ERR_ADD_USER_CERT_FAILED;
  }
  PK11_FreeSlot(slot);
  return OK;
}

void CertDatabase::ListCerts(CertificateList* certs) {
  certs->clear();

  CERTCertList* cert_list = PK11_ListCerts(PK11CertListUnique, NULL);
  CERTCertListNode* node;
  for (node = CERT_LIST_HEAD(cert_list);
       !CERT_LIST_END(node, cert_list);
       node = CERT_LIST_NEXT(node)) {
    certs->push_back(X509Certificate::CreateFromHandle(
        node->cert,
        X509Certificate::SOURCE_LONE_CERT_IMPORT,
        X509Certificate::OSCertHandles()));
  }
  CERT_DestroyCertList(cert_list);
}

int CertDatabase::ImportFromPKCS12(
    const std::string& data, const string16& password) {
  return psm::nsPKCS12Blob_Import(data.data(), data.size(), password);
}

int CertDatabase::ExportToPKCS12(
    const CertificateList& certs,
    const string16& password,
    std::string* output) {
  return psm::nsPKCS12Blob_Export(output, certs, password);
}

X509Certificate* CertDatabase::FindRootInList(
    const CertificateList& certificates) {
  DCHECK_GT(certificates.size(), 0U);

  if (certificates.size() == 1)
    return certificates[0].get();

  X509Certificate* cert0 = certificates[0];
  X509Certificate* cert1 = certificates[1];
  X509Certificate* certn_2 = certificates[certificates.size() - 2];
  X509Certificate* certn_1 = certificates[certificates.size() - 1];

  if (CERT_CompareName(&cert1->os_cert_handle()->issuer,
                       &cert0->os_cert_handle()->subject) == SECEqual)
    return cert0;
  if (CERT_CompareName(&certn_2->os_cert_handle()->issuer,
                       &certn_1->os_cert_handle()->subject) == SECEqual)
    return certn_1;

  LOG(INFO) << "certificate list is not a hierarchy";
  return cert0;
}

bool CertDatabase::ImportCACerts(const CertificateList& certificates,
                                 unsigned int trust_bits,
                                 ImportCertFailureList* not_imported) {
  X509Certificate* root = FindRootInList(certificates);
  return psm::ImportCACerts(certificates, root, trust_bits, not_imported);
}

bool CertDatabase::SetCertTrust(const X509Certificate* cert,
                                CertType type,
                                unsigned int trusted) {
  return psm::SetCertTrust(cert, type, trusted);
}

bool CertDatabase::DeleteCertAndKey(const X509Certificate* cert) {
  // For some reason, PK11_DeleteTokenCertAndKey only calls
  // SEC_DeletePermCertificate if the private key is found.  So, we check
  // whether a private key exists before deciding which function to call to
  // delete the cert.
  SECKEYPrivateKey *privKey = PK11_FindKeyByAnyCert(cert->os_cert_handle(),
                                                    NULL);
  if (privKey) {
    SECKEY_DestroyPrivateKey(privKey);
    if (PK11_DeleteTokenCertAndKey(cert->os_cert_handle(), NULL)) {
      LOG(ERROR) << "PK11_DeleteTokenCertAndKey failed: " << PORT_GetError();
      return false;
    }
  } else {
    if (SEC_DeletePermCertificate(cert->os_cert_handle())) {
      LOG(ERROR) << "SEC_DeletePermCertificate failed: " << PORT_GetError();
      return false;
    }
  }
  return true;
}

}  // namespace net
