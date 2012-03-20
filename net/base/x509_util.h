// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_X509_UTIL_H_
#define NET_BASE_X509_UTIL_H_
#pragma once

#include <string>

#include "base/time.h"
#include "net/base/net_export.h"

namespace crypto {
class ECPrivateKey;
}

namespace net {

namespace x509_util {

// Creates a server bound certificate containing the public key in |key|.
// Domain, serial number and validity period are given as
// parameters. The certificate is signed by the private key in |key|.
// The hashing algorithm for the signature is SHA-1.
//
// See Internet Draft draft-balfanz-tls-obc-00 for more details:
// http://tools.ietf.org/html/draft-balfanz-tls-obc-00
bool NET_EXPORT_PRIVATE CreateDomainBoundCertEC(
    crypto::ECPrivateKey* key,
    const std::string& domain,
    uint32 serial_number,
    base::Time not_valid_before,
    base::Time not_valid_after,
    std::string* der_cert);

} // namespace x509_util

} // namespace net

#endif  // NET_BASE_X509_UTIL_H_
