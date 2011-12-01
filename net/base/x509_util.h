// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
class RSAPrivateKey;
}

namespace net {

namespace x509_util {

// Creates an origin bound certificate containing the public key in |key|.
// Web origin, serial number and validity period are given as
// parameters. The certificate is signed by the private key in |key|.
// The hashing algorithm for the signature is SHA-1.
//
// See Internet Draft draft-balfanz-tls-obc-00 for more details:
// http://tools.ietf.org/html/draft-balfanz-tls-obc-00
bool NET_EXPORT_PRIVATE CreateOriginBoundCertRSA(crypto::RSAPrivateKey* key,
                                                 const std::string& origin,
                                                 uint32 serial_number,
                                                 base::TimeDelta valid_duration,
                                                 std::string* der_cert);
bool NET_EXPORT_PRIVATE CreateOriginBoundCertEC(crypto::ECPrivateKey* key,
                                                const std::string& origin,
                                                uint32 serial_number,
                                                base::TimeDelta valid_duration,
                                                std::string* der_cert);

} // namespace x509_util

} // namespace net

#endif  // NET_BASE_X509_UTIL_H_
