// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cert_database.h"

#include <cert.h>
#include <pk11pub.h>
#include <secmod.h>

#include "base/logging.h"
#include "crypto/nss_util.h"
#include "net/base/net_errors.h"
#include "net/base/x509_certificate.h"

namespace net {

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
  if (!slot)
    return ERR_NO_PRIVATE_KEY_FOR_CERT;

  PK11_FreeSlot(slot);

  return OK;
}

int CertDatabase::AddUserCert(X509Certificate* cert_obj) {
  CERTCertificate* cert = cert_obj->os_cert_handle();
  PK11SlotInfo* slot = NULL;

  {
    crypto::AutoNSSWriteLock lock;
    slot = PK11_ImportCertForKey(
        cert,
        cert_obj->GetDefaultNickname(net::USER_CERT).c_str(),
        NULL);
  }

  if (!slot) {
    LOG(ERROR) << "Couldn't import user certificate.";
    return ERR_ADD_USER_CERT_FAILED;
  }
  PK11_FreeSlot(slot);
  NotifyObserversOfCertAdded(cert_obj);
  return OK;
}

}  // namespace net
