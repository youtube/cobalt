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

}  // namespace

}  // namespace net
