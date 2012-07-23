// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_X509_UTIL_NSS_H_
#define NET_BASE_X509_UTIL_NSS_H_

#include <string>

#include "base/time.h"

typedef struct CERTCertificateStr CERTCertificate;
typedef struct SECKEYPrivateKeyStr SECKEYPrivateKey;
typedef struct SECKEYPublicKeyStr SECKEYPublicKey;


namespace net {

namespace x509_util {

// Creates a self-signed certificate containing |public_key|.  Subject, serial
// number and validity period are given as parameters.  The certificate is
// signed by |private_key|. The hashing algorithm for the signature is SHA-1.
// |subject| is a distinguished name defined in RFC4514.
CERTCertificate* CreateSelfSignedCert(
    SECKEYPublicKey* public_key,
    SECKEYPrivateKey* private_key,
    const std::string& subject,
    uint32 serial_number,
    base::Time not_valid_before,
    base::Time not_valid_after);

} // namespace x509_util

} // namespace net

#endif  // NET_BASE_X509_UTIL_NSS_H_
