// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_util_ios.h"

#include <cert.h>
#include <nss.h>
#include <prtypes.h>

#include "base/mac/scoped_cftyperef.h"
#include "crypto/nss_util.h"
#include "net/base/x509_certificate.h"

using base::mac::ScopedCFTypeRef;

namespace net {
namespace x509_util_ios {

namespace {

// Creates an NSS certificate handle from |data|, which is |length| bytes in
// size.
CERTCertificate* CreateNSSCertHandleFromBytes(const char* data,
                                              int length) {
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
  return CERT_NewTempCertificate(CERT_GetDefaultCertDB(), &der_cert, NULL,
                                 PR_FALSE, PR_TRUE);
}

}  // namespace

CERTCertificate* CreateNSSCertHandleFromOSHandle(
    SecCertificateRef cert_handle) {
  ScopedCFTypeRef<CFDataRef> cert_data(SecCertificateCopyData(cert_handle));
  return CreateNSSCertHandleFromBytes(
      reinterpret_cast<const char*>(CFDataGetBytePtr(cert_data)),
      CFDataGetLength(cert_data));
}

SecCertificateRef CreateOSCertHandleFromNSSHandle(
    CERTCertificate* nss_cert_handle) {
  return X509Certificate::CreateOSCertHandleFromBytes(
      reinterpret_cast<const char*>(nss_cert_handle->derCert.data),
      nss_cert_handle->derCert.len);
}

NSSCertificate::NSSCertificate(SecCertificateRef cert_handle) {
  nss_cert_handle_ = CreateNSSCertHandleFromOSHandle(cert_handle);
  DLOG_IF(INFO, cert_handle && !nss_cert_handle_)
      << "Could not convert SecCertificateRef to CERTCertificate*";
}

NSSCertificate::~NSSCertificate() {
  CERT_DestroyCertificate(nss_cert_handle_);
}

CERTCertificate* NSSCertificate::cert_handle() {
  return nss_cert_handle_;
}

}  // namespace x509_util_ios
}  // namespace net

