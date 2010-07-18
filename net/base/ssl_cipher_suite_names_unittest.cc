// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_cipher_suite_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(CipherSuiteNamesTest, Basic) {
  const char *key_exchange, *cipher, *mac;
  SSLCipherSuiteToStrings(&key_exchange, &cipher, &mac, 0xc001);
  EXPECT_STREQ(key_exchange, "ECDH_ECDSA");
  EXPECT_STREQ(cipher, "NULL");
  EXPECT_STREQ(mac, "SHA1");

  SSLCipherSuiteToStrings(&key_exchange, &cipher, &mac, 0xff31);
  EXPECT_STREQ(key_exchange, "???");
  EXPECT_STREQ(cipher, "???");
  EXPECT_STREQ(mac, "???");
}

}  // anonymous namespace

}  // namespace net
