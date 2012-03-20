// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "crypto/ec_private_key.h"
#include "net/base/x509_util.h"
#include "net/base/x509_util_openssl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

// For OpenSSL, x509_util::CreateDomainBoundCertEC() is not yet implemented
// and should return false.  This unit test ensures that a stub implementation
// is present.
TEST(X509UtilOpenSSLTest, CreateDomainBoundCertNotImplemented) {
  std::string domain = "weborigin.com";
  base::Time now = base::Time::Now();
  scoped_ptr<crypto::ECPrivateKey> private_key(
      crypto::ECPrivateKey::Create());
  std::string der_cert;
  EXPECT_FALSE(x509_util::CreateDomainBoundCertEC(
      private_key.get(),
      domain, 1,
      now,
      now + base::TimeDelta::FromDays(1),
      &der_cert));
  EXPECT_TRUE(der_cert.empty());

}

}  // namespace net
