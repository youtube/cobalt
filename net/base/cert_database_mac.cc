// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cert_database.h"

#include <Security/Security.h>

#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "crypto/mac_security_services_lock.h"
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

  // Verify the Keychain already has the corresponding private key:
  SecIdentityRef identity = NULL;
  OSStatus err = SecIdentityCreateWithCertificate(NULL, cert->os_cert_handle(),
                                                  &identity);
  if (err == errSecItemNotFound) {
    LOG(ERROR) << "CertDatabase couldn't find private key for user cert";
    return ERR_NO_PRIVATE_KEY_FOR_CERT;
  }
  if (err != noErr || !identity) {
    // TODO(snej): Map the error code more intelligently.
    return ERR_CERT_INVALID;
  }

  CFRelease(identity);
  return OK;
}

int CertDatabase::AddUserCert(X509Certificate* cert) {
  OSStatus err;
  {
    base::AutoLock locked(crypto::GetMacSecurityServicesLock());
    err = SecCertificateAddToKeychain(cert->os_cert_handle(), NULL);
  }
  switch (err) {
    case noErr:
      CertDatabase::NotifyObserversOfUserCertAdded(cert);
      // Fall through.
    case errSecDuplicateItem:
      return OK;
    default:
      LOG(ERROR) << "CertDatabase failed to add cert to keychain: " << err;
      // TODO(snej): Map the error code more intelligently.
      return ERR_ADD_USER_CERT_FAILED;
  }
}

}  // namespace net
