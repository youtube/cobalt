// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/secure_hash.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SecureHashTest, TestUpdate) {
  // Example B.3 from FIPS 180-2: long message.
  std::string input3(500000, 'a');  // 'a' repeated half a million times
  int expected3[] = { 0xcd, 0xc7, 0x6e, 0x5c,
                      0x99, 0x14, 0xfb, 0x92,
                      0x81, 0xa1, 0xc7, 0xe2,
                      0x84, 0xd7, 0x3e, 0x67,
                      0xf1, 0x80, 0x9a, 0x48,
                      0xa4, 0x97, 0x20, 0x0e,
                      0x04, 0x6d, 0x39, 0xcc,
                      0xc7, 0x11, 0x2c, 0xd0 };

  uint8 output3[crypto::SHA256_LENGTH];

  scoped_ptr<crypto::SecureHash> ctx(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  ctx->Update(input3.data(), input3.size());
  ctx->Update(input3.data(), input3.size());

  ctx->Finish(output3, sizeof(output3));
  for (size_t i = 0; i < crypto::SHA256_LENGTH; i++)
    EXPECT_EQ(expected3[i], static_cast<int>(output3[i]));
}
