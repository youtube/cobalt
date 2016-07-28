// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_util.h"

#include <algorithm>

#include "base/time.h"
#include "net/base/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace x509_util {

TEST(X509UtilTest, SortClientCertificates) {
  CertificateList certs;

  const base::Time now = base::Time::Now();
  const base::TimeDelta five_days = base::TimeDelta::FromDays(5);

  certs.push_back(scoped_refptr<X509Certificate>(NULL));
  certs.push_back(new X509Certificate(
      "expired", "expired",
      base::Time::UnixEpoch(), base::Time::UnixEpoch()));
  certs.push_back(new X509Certificate(
      "not yet valid", "not yet valid",
      base::Time::Max(), base::Time::Max()));
  certs.push_back(new X509Certificate(
      "older cert", "older cert",
      now - five_days, now + five_days));
  certs.push_back(scoped_refptr<X509Certificate>(NULL));
  certs.push_back(new X509Certificate(
      "newer cert", "newer cert",
      now - base::TimeDelta::FromDays(3), now + five_days));

  std::sort(certs.begin(), certs.end(), ClientCertSorter());

  ASSERT_TRUE(certs[0].get());
  EXPECT_EQ("newer cert", certs[0]->subject().common_name);
  ASSERT_TRUE(certs[1].get());
  EXPECT_EQ("older cert", certs[1]->subject().common_name);
  ASSERT_TRUE(certs[2].get());
  EXPECT_EQ("not yet valid", certs[2]->subject().common_name);
  ASSERT_TRUE(certs[3].get());
  EXPECT_EQ("expired", certs[3]->subject().common_name);
  ASSERT_FALSE(certs[4].get());
  ASSERT_FALSE(certs[5].get());
}

}  // namespace x509_util

}  // namespace net
