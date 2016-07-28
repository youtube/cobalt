// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_cipher_suite_names.h"

#include "base/basictypes.h"
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

TEST(CipherSuiteNamesTest, ParseSSLCipherString) {
  uint16 cipher_suite = 0;
  EXPECT_TRUE(ParseSSLCipherString("0x0004", &cipher_suite));
  EXPECT_EQ(0x00004u, cipher_suite);

  EXPECT_TRUE(ParseSSLCipherString("0xBEEF", &cipher_suite));
  EXPECT_EQ(0xBEEFu, cipher_suite);
}

TEST(CipherSuiteNamesTest, ParseSSLCipherStringFails) {
  const char* const cipher_strings[] = {
    "0004",
    "0x004",
    "0xBEEFY",
  };

  for (size_t i = 0; i < arraysize(cipher_strings); ++i) {
    uint16 cipher_suite = 0;
    EXPECT_FALSE(ParseSSLCipherString(cipher_strings[i], &cipher_suite));
  }
}

}  // anonymous namespace

}  // namespace net
