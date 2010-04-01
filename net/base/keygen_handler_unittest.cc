// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/keygen_handler.h"

#include <string>

#include "base/base64.h"
#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

KeygenHandler::KeyLocation ValidKeyLocation() {
  KeygenHandler::KeyLocation result;
#if defined(OS_WIN)
  result.container_name = L"Unit tests";
  result.provider_name = L"Test Provider";
#elif defined(OS_MACOSX)
  result.keychain_path = "/Users/tests/test.chain";
#elif defined(USE_NSS)
  result.slot_name = "Sample slot";
#endif

  return result;
}

TEST(KeygenHandlerTest, FLAKY_SmokeTest) {
  KeygenHandler handler(2048, "some challenge");
  handler.set_stores_key(false);  // Don't leave the key-pair behind
  std::string result = handler.GenKeyAndSignChallenge();
  LOG(INFO) << "KeygenHandler produced: " << result;
  ASSERT_GT(result.length(), 0U);

  // Verify it's valid base64:
  std::string spkac;
  ASSERT_TRUE(base::Base64Decode(result, &spkac));
  // In lieu of actually parsing and validating the DER data,
  // just check that it exists and has a reasonable length.
  // (It's almost always 590 bytes, but the DER encoding of the random key
  // and signature could sometimes be a few bytes different.)
  ASSERT_GE(spkac.length(), 580U);
  ASSERT_LE(spkac.length(), 600U);

  // NOTE:
  // The value of |result| can be validated by prefixing 'SPKAC=' to it
  // and piping it through
  //   openssl spkac -verify
  // whose output should look like:
  //   Netscape SPKI:
  //     Public Key Algorithm: rsaEncryption
  //     RSA Public Key: (2048 bit)
  //     Modulus (2048 bit):
  //         00:b6:cc:14:c9:43:b5:2d:51:65:7e:11:8b:80:9e: .....
  //     Exponent: 65537 (0x10001)
  //     Challenge String: some challenge
  //     Signature Algorithm: md5WithRSAEncryption
  //         92:f3:cc:ff:0b:d3:d0:4a:3a:4c:ba:ff:d6:38:7f:a5:4b:b5: .....
  //   Signature OK
  //
  // The value of |spkac| can be ASN.1-parsed with:
  //    openssl asn1parse -inform DER
}

TEST(KeygenHandlerTest, Cache) {
  KeygenHandler::Cache* cache = KeygenHandler::Cache::GetInstance();
  KeygenHandler::KeyLocation location1;
  KeygenHandler::KeyLocation location2;

  std::string key1("abcd");
  cache->Insert(key1, location1);

  // The cache should have stored location1 at key1
  EXPECT_TRUE(cache->Find(key1, &location2));

  // The cache should have retrieved it into location2, and their equality
  // should be reflexive
  EXPECT_TRUE(location1.Equals(location2));
  EXPECT_TRUE(location2.Equals(location1));

  location2 = ValidKeyLocation();
  KeygenHandler::KeyLocation location3 = ValidKeyLocation();
  EXPECT_FALSE(location1.Equals(location2));

  // The cache should miss for an unregistered key
  std::string key2("def");
  EXPECT_FALSE(cache->Find(key2, &location2));

  // A cache miss should leave the original location unmolested
  EXPECT_TRUE(location2.Equals(location3));
}

}  // namespace

}  // namespace net
