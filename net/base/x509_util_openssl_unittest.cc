// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "crypto/rsa_private_key.h"
#include "net/base/x509_util.h"
#include "net/base/x509_util_openssl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

// For OpenSSL, x509_util::CreateOriginBoundCert() is not yet implemented
// and should return false.  This unit test ensures that a stub implementation
// is present.
TEST(X509UtilOpenSSLTest, CreateOriginBoundCertNotImplemented) {
  std::string origin = "http://weborigin.com:443";
  scoped_ptr<crypto::RSAPrivateKey> private_key(
      crypto::RSAPrivateKey::Create(1024));
  std::string der_cert;
  EXPECT_FALSE(x509_util::CreateOriginBoundCert(private_key.get(),
                                                origin, 1,
                                                base::TimeDelta::FromDays(1),
                                                &der_cert));
  EXPECT_TRUE(der_cert.empty());

}

}  // namespace net
