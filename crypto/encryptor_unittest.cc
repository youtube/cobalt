// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/encryptor.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "crypto/symmetric_key.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(EncryptorTest, EncryptDecrypt) {
  scoped_ptr<crypto::SymmetricKey> key(
      crypto::SymmetricKey::DeriveKeyFromPassword(
          crypto::SymmetricKey::AES, "password", "saltiest", 1000, 256));
  EXPECT_TRUE(NULL != key.get());

  crypto::Encryptor encryptor;
  // The IV must be exactly as long as the cipher block size.
  std::string iv("the iv: 16 bytes");
  EXPECT_EQ(16U, iv.size());
  EXPECT_TRUE(encryptor.Init(key.get(), crypto::Encryptor::CBC, iv));

  std::string plaintext("this is the plaintext");
  std::string ciphertext;
  EXPECT_TRUE(encryptor.Encrypt(plaintext, &ciphertext));

  EXPECT_LT(0U, ciphertext.size());

  std::string decypted;
  EXPECT_TRUE(encryptor.Decrypt(ciphertext, &decypted));

  EXPECT_EQ(plaintext, decypted);
}

// CTR mode encryption is only implemented using NSS.
#if defined(USE_NSS)

TEST(EncryptorTest, EncryptDecryptCTR) {
  scoped_ptr<crypto::SymmetricKey> key(
      crypto::SymmetricKey::GenerateRandomKey(
          crypto::SymmetricKey::AES, 128));

  EXPECT_TRUE(NULL != key.get());
  const std::string kInitialCounter = "0000000000000000";

  crypto::Encryptor encryptor;
  EXPECT_TRUE(encryptor.Init(key.get(), crypto::Encryptor::CTR, ""));
  EXPECT_TRUE(encryptor.SetCounter(kInitialCounter));

  std::string plaintext("normal plaintext of random length");
  std::string ciphertext;
  EXPECT_TRUE(encryptor.Encrypt(plaintext, &ciphertext));
  EXPECT_LT(0U, ciphertext.size());

  std::string decypted;
  EXPECT_TRUE(encryptor.SetCounter(kInitialCounter));
  EXPECT_TRUE(encryptor.Decrypt(ciphertext, &decypted));
  EXPECT_EQ(plaintext, decypted);

  plaintext = "0123456789012345";
  EXPECT_TRUE(encryptor.SetCounter(kInitialCounter));
  EXPECT_TRUE(encryptor.Encrypt(plaintext, &ciphertext));
  EXPECT_LT(0U, ciphertext.size());

  EXPECT_TRUE(encryptor.SetCounter(kInitialCounter));
  EXPECT_TRUE(encryptor.Decrypt(ciphertext, &decypted));
  EXPECT_EQ(plaintext, decypted);
}

TEST(EncryptorTest, CTRCounter) {
  const int kCounterSize = 16;
  const char kTest1[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  uint8 buf[16];

  // Increment 10 times.
  crypto::Encryptor::Counter counter1(std::string(kTest1, kCounterSize));
  for (int i = 0; i < 10; ++i)
    counter1.Increment();
  counter1.Write(buf);
  EXPECT_EQ(0, memcmp(buf, kTest1, 15));
  EXPECT_TRUE(buf[15] == 10);

  // Check corner cases.
  const char kTest2[] = {0, 0, 0, 0, 0, 0, 0, 0,
                         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  const char kExpect2[] = {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
  crypto::Encryptor::Counter counter2(std::string(kTest2, kCounterSize));
  counter2.Increment();
  counter2.Write(buf);
  EXPECT_EQ(0, memcmp(buf, kExpect2, kCounterSize));

  const char kTest3[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  const char kExpect3[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  crypto::Encryptor::Counter counter3(std::string(kTest3, kCounterSize));
  counter3.Increment();
  counter3.Write(buf);
  EXPECT_EQ(0, memcmp(buf, kExpect3, kCounterSize));
}

#endif

// TODO(wtc): add more known-answer tests.  Test vectors are available from
// http://www.ietf.org/rfc/rfc3602
// http://csrc.nist.gov/publications/nistpubs/800-38a/sp800-38a.pdf
// http://gladman.plushost.co.uk/oldsite/AES/index.php
// http://csrc.nist.gov/groups/STM/cavp/documents/aes/KAT_AES.zip

// NIST SP 800-38A test vector F.2.5 CBC-AES256.Encrypt.
TEST(EncryptorTest, EncryptAES256CBC) {
  // From NIST SP 800-38a test cast F.2.5 CBC-AES256.Encrypt.
  static const unsigned char raw_key[] = {
    0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
    0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
    0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7,
    0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4
  };
  static const unsigned char raw_iv[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
  };
  static const unsigned char raw_plaintext[] = {
    // Block #1
    0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
    0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
    // Block #2
    0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
    0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
    // Block #3
    0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
    0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
    // Block #4
    0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17,
    0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10,
  };
  static const unsigned char raw_ciphertext[] = {
    // Block #1
    0xf5, 0x8c, 0x4c, 0x04, 0xd6, 0xe5, 0xf1, 0xba,
    0x77, 0x9e, 0xab, 0xfb, 0x5f, 0x7b, 0xfb, 0xd6,
    // Block #2
    0x9c, 0xfc, 0x4e, 0x96, 0x7e, 0xdb, 0x80, 0x8d,
    0x67, 0x9f, 0x77, 0x7b, 0xc6, 0x70, 0x2c, 0x7d,
    // Block #3
    0x39, 0xf2, 0x33, 0x69, 0xa9, 0xd9, 0xba, 0xcf,
    0xa5, 0x30, 0xe2, 0x63, 0x04, 0x23, 0x14, 0x61,
    // Block #4
    0xb2, 0xeb, 0x05, 0xe2, 0xc3, 0x9b, 0xe9, 0xfc,
    0xda, 0x6c, 0x19, 0x07, 0x8c, 0x6a, 0x9d, 0x1b,
    // PKCS #5 padding, encrypted.
    0x3f, 0x46, 0x17, 0x96, 0xd6, 0xb0, 0xd6, 0xb2,
    0xe0, 0xc2, 0xa7, 0x2b, 0x4d, 0x80, 0xe6, 0x44
  };

  std::string key(reinterpret_cast<const char*>(raw_key), sizeof(raw_key));
  scoped_ptr<crypto::SymmetricKey> sym_key(crypto::SymmetricKey::Import(
      crypto::SymmetricKey::AES, key));
  ASSERT_TRUE(NULL != sym_key.get());

  crypto::Encryptor encryptor;
  // The IV must be exactly as long a the cipher block size.
  std::string iv(reinterpret_cast<const char*>(raw_iv), sizeof(raw_iv));
  EXPECT_EQ(16U, iv.size());
  EXPECT_TRUE(encryptor.Init(sym_key.get(), crypto::Encryptor::CBC, iv));

  std::string plaintext(reinterpret_cast<const char*>(raw_plaintext),
                        sizeof(raw_plaintext));
  std::string ciphertext;
  EXPECT_TRUE(encryptor.Encrypt(plaintext, &ciphertext));

  EXPECT_EQ(sizeof(raw_ciphertext), ciphertext.size());
  EXPECT_EQ(0, memcmp(ciphertext.data(), raw_ciphertext, ciphertext.size()));

  std::string decypted;
  EXPECT_TRUE(encryptor.Decrypt(ciphertext, &decypted));

  EXPECT_EQ(plaintext, decypted);
}

// Expected output derived from the NSS implementation.
TEST(EncryptorTest, EncryptAES128CBCRegression) {
  std::string key = "128=SixteenBytes";
  std::string iv = "Sweet Sixteen IV";
  std::string plaintext = "Plain text with a g-clef U+1D11E \360\235\204\236";
  std::string expected_ciphertext_hex =
      "D4A67A0BA33C30F207344D81D1E944BBE65587C3D7D9939A"
      "C070C62B9C15A3EA312EA4AD1BC7929F4D3C16B03AD5ADA8";

  scoped_ptr<crypto::SymmetricKey> sym_key(crypto::SymmetricKey::Import(
      crypto::SymmetricKey::AES, key));
  ASSERT_TRUE(NULL != sym_key.get());

  crypto::Encryptor encryptor;
  // The IV must be exactly as long a the cipher block size.
  EXPECT_EQ(16U, iv.size());
  EXPECT_TRUE(encryptor.Init(sym_key.get(), crypto::Encryptor::CBC, iv));

  std::string ciphertext;
  EXPECT_TRUE(encryptor.Encrypt(plaintext, &ciphertext));
  EXPECT_EQ(expected_ciphertext_hex, base::HexEncode(ciphertext.data(),
                                                     ciphertext.size()));

  std::string decypted;
  EXPECT_TRUE(encryptor.Decrypt(ciphertext, &decypted));
  EXPECT_EQ(plaintext, decypted);
}

// Expected output derived from the NSS implementation.
TEST(EncryptorTest, EncryptAES192CBCRegression) {
  std::string key = "192bitsIsTwentyFourByte!";
  std::string iv = "Sweet Sixteen IV";
  std::string plaintext = "Small text";
  std::string expected_ciphertext_hex = "78DE5D7C2714FC5C61346C5416F6C89A";

  scoped_ptr<crypto::SymmetricKey> sym_key(crypto::SymmetricKey::Import(
      crypto::SymmetricKey::AES, key));
  ASSERT_TRUE(NULL != sym_key.get());

  crypto::Encryptor encryptor;
  // The IV must be exactly as long a the cipher block size.
  EXPECT_EQ(16U, iv.size());
  EXPECT_TRUE(encryptor.Init(sym_key.get(), crypto::Encryptor::CBC, iv));

  std::string ciphertext;
  EXPECT_TRUE(encryptor.Encrypt(plaintext, &ciphertext));
  EXPECT_EQ(expected_ciphertext_hex, base::HexEncode(ciphertext.data(),
                                                     ciphertext.size()));

  std::string decypted;
  EXPECT_TRUE(encryptor.Decrypt(ciphertext, &decypted));
  EXPECT_EQ(plaintext, decypted);
}

// Not all platforms allow import/generation of symmetric keys with an
// unsupported size.
#if !defined(OS_WIN) && !defined(USE_NSS)
TEST(EncryptorTest, UnsupportedKeySize) {
  std::string key = "7 = bad";
  std::string iv = "Sweet Sixteen IV";
  scoped_ptr<crypto::SymmetricKey> sym_key(crypto::SymmetricKey::Import(
      crypto::SymmetricKey::AES, key));
  ASSERT_TRUE(NULL != sym_key.get());

  crypto::Encryptor encryptor;
  // The IV must be exactly as long a the cipher block size.
  EXPECT_EQ(16U, iv.size());
  EXPECT_FALSE(encryptor.Init(sym_key.get(), crypto::Encryptor::CBC, iv));
}
#endif  // unsupported platforms.

TEST(EncryptorTest, UnsupportedIV) {
  std::string key = "128=SixteenBytes";
  std::string iv = "OnlyForteen :(";
  scoped_ptr<crypto::SymmetricKey> sym_key(crypto::SymmetricKey::Import(
      crypto::SymmetricKey::AES, key));
  ASSERT_TRUE(NULL != sym_key.get());

  crypto::Encryptor encryptor;
  EXPECT_FALSE(encryptor.Init(sym_key.get(), crypto::Encryptor::CBC, iv));
}

TEST(EncryptorTest, EmptyEncrypt) {
  std::string key = "128=SixteenBytes";
  std::string iv = "Sweet Sixteen IV";
  std::string plaintext;
  std::string expected_ciphertext_hex = "8518B8878D34E7185E300D0FCC426396";

  scoped_ptr<crypto::SymmetricKey> sym_key(crypto::SymmetricKey::Import(
      crypto::SymmetricKey::AES, key));
  ASSERT_TRUE(NULL != sym_key.get());

  crypto::Encryptor encryptor;
  // The IV must be exactly as long a the cipher block size.
  EXPECT_EQ(16U, iv.size());
  EXPECT_TRUE(encryptor.Init(sym_key.get(), crypto::Encryptor::CBC, iv));

  std::string ciphertext;
  EXPECT_TRUE(encryptor.Encrypt(plaintext, &ciphertext));
  EXPECT_EQ(expected_ciphertext_hex, base::HexEncode(ciphertext.data(),
                                                     ciphertext.size()));
}
