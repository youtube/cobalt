// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cert_database.h"

#include <pk11pub.h>
#include <secmod.h>
#include <ssl.h>
#include <nssb64.h>    // NSSBase64_EncodeItem()
#include <secder.h>    // DER_Encode()
#include <cryptohi.h>  // SEC_DerSignData()
#include <keyhi.h>     // SECKEY_CreateSubjectPublicKeyInfo()

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/nss_util.h"
#include "net/base/net_errors.h"
#include "net/base/x509_certificate.h"

namespace net {

CertDatabase::CertDatabase() {
  base::EnsureNSSInit();
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

}  // namespace net
