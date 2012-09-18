// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains functions for iOS to glue NSS and Security.framework
// together.

#ifndef NET_BASE_X509_UTIL_IOS_H_
#define NET_BASE_X509_UTIL_IOS_H_

#include <Security/Security.h>

// Forward declaration; real one in <cert.h>
typedef struct CERTCertificateStr CERTCertificate;

namespace net {
namespace x509_util_ios {

// Converts a Security.framework certificate handle (SecCertificateRef) into
// an NSS certificate handle (CERTCertificate*).
CERTCertificate* CreateNSSCertHandleFromOSHandle(SecCertificateRef cert_handle);

// Converts an NSS certificate handle (CERTCertificate*) into a
// Security.framework handle (SecCertificateRef)
SecCertificateRef CreateOSCertHandleFromNSSHandle(
    CERTCertificate* nss_cert_handle);

// This is a wrapper class around the native NSS certificate handle.
// The constructor copies the certificate data from |cert_handle| and
// uses the NSS library to parse it.
class NSSCertificate {
 public:
  explicit NSSCertificate(SecCertificateRef cert_handle);
  ~NSSCertificate();
  CERTCertificate* cert_handle();
 private:
  CERTCertificate* nss_cert_handle_;
};

}  // namespace x509_util_ios
}  // namespace net

#endif  // NET_BASE_X509_UTIL_IOS_H_
