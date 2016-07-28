// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/null_decrypter.h"
#include "net/quic/test_tools/quic_test_utils.h"

using base::StringPiece;

namespace net {
namespace test {

TEST(NullDecrypterTest, Decrypt) {
  unsigned char expected[] = {
    // fnv hash
    0x47, 0x11, 0xea, 0x5f,
    0xcf, 0x1d, 0x66, 0x5b,
    0xba, 0xf0, 0xbc, 0xfd,
    0x88, 0x79, 0xca, 0x37,
    // payload
    'g',  'o',  'o',  'd',
    'b',  'y',  'e',  '!',
  };
  NullDecrypter decrypter;
  scoped_ptr<QuicData> decrypted(
      decrypter.Decrypt("hello world!",
                        StringPiece(reinterpret_cast<const char*>(expected),
                                    arraysize(expected))));
  ASSERT_TRUE(decrypted.get());
  EXPECT_EQ("goodbye!", decrypted->AsStringPiece());
}

TEST(NullDecrypterTest, BadHash) {
  unsigned char expected[] = {
    // fnv hash
    0x46, 0x11, 0xea, 0x5f,
    0xcf, 0x1d, 0x66, 0x5b,
    0xba, 0xf0, 0xbc, 0xfd,
    0x88, 0x79, 0xca, 0x37,
    // payload
    'g',  'o',  'o',  'd',
    'b',  'y',  'e',  '!',
  };
  NullDecrypter decrypter;
  scoped_ptr<QuicData> decrypted(
      decrypter.Decrypt("hello world!",
                        StringPiece(reinterpret_cast<const char*>(expected),
                                    arraysize(expected))));
  ASSERT_FALSE(decrypted.get());
}

TEST(NullDecrypterTest, ShortInput) {
  unsigned char expected[] = {
    // fnv hash (truncated)
    0x46, 0x11, 0xea, 0x5f,
    0xcf, 0x1d, 0x66, 0x5b,
    0xba, 0xf0, 0xbc, 0xfd,
    0x88, 0x79, 0xca,
  };
  NullDecrypter decrypter;
  scoped_ptr<QuicData> decrypted(
      decrypter.Decrypt("hello world!",
                        StringPiece(reinterpret_cast<const char*>(expected),
                                    arraysize(expected))));
  ASSERT_FALSE(decrypted.get());
}

}  // namespace test
}  // namespace net
