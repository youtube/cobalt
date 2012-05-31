// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/crypto/aes_decryptor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// |kEncryptedData| is encrypted from |kOriginalData| using |kRightKey|, whose
// length is |kKeySize|. Modifying any of these independently would fail the
// test.
static const char kOriginalData[] = "Original data.";
static const int kEncryptedDataSize = 16;
static const unsigned char kEncryptedData[] =
    "\x82\x3A\x76\x92\xEC\x7F\xF8\x85\xEC\x23\x52\xFB\x19\xB1\xB9\x09";
static const int kKeySize = 16;
static const unsigned char kRightKey[] = "A wonderful key!";
static const unsigned char kWrongKey[] = "I'm a wrong key.";
static const int kKeyIdSize = 9;
static const unsigned char kKeyId1[] = "Key ID 1.";
static const unsigned char kKeyId2[] = "Key ID 2.";

class AesDecryptorTest : public testing::Test {
 public:
  AesDecryptorTest() {
    encrypted_data_ = DecoderBuffer::CopyFrom(
        kEncryptedData, kEncryptedDataSize);
  }

 protected:
  void SetKeyIdForEncryptedData(const uint8* key_id, int key_id_size) {
    encrypted_data_->SetDecryptConfig(
        scoped_ptr<DecryptConfig>(new DecryptConfig(key_id, key_id_size)));
  }

  void DecryptAndExpectToSucceed() {
    scoped_refptr<DecoderBuffer> decrypted =
        decryptor_.Decrypt(encrypted_data_);
    ASSERT_TRUE(decrypted);
    int data_length = sizeof(kOriginalData) - 1;
    ASSERT_EQ(data_length, decrypted->GetDataSize());
    EXPECT_EQ(0, memcmp(kOriginalData, decrypted->GetData(), data_length));
  }

  void DecryptAndExpectToFail() {
    scoped_refptr<DecoderBuffer> decrypted =
        decryptor_.Decrypt(encrypted_data_);
    EXPECT_FALSE(decrypted);
  }

  scoped_refptr<DecoderBuffer> encrypted_data_;
  AesDecryptor decryptor_;
};

TEST_F(AesDecryptorTest, NormalDecryption) {
  decryptor_.AddKey(kKeyId1, kKeyIdSize, kRightKey, kKeySize);
  SetKeyIdForEncryptedData(kKeyId1, kKeyIdSize);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToSucceed());
}

TEST_F(AesDecryptorTest, WrongKey) {
  decryptor_.AddKey(kKeyId1, kKeyIdSize, kWrongKey, kKeySize);
  SetKeyIdForEncryptedData(kKeyId1, kKeyIdSize);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToFail());
}

TEST_F(AesDecryptorTest, MultipleKeys) {
  decryptor_.AddKey(kKeyId1, kKeyIdSize, kRightKey, kKeySize);
  decryptor_.AddKey(kKeyId2, kKeyIdSize, kWrongKey, kKeySize);
  SetKeyIdForEncryptedData(kKeyId1, kKeyIdSize);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToSucceed());
}

TEST_F(AesDecryptorTest, KeyReplacement) {
  SetKeyIdForEncryptedData(kKeyId1, kKeyIdSize);
  decryptor_.AddKey(kKeyId1, kKeyIdSize, kWrongKey, kKeySize);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToFail());
  decryptor_.AddKey(kKeyId1, kKeyIdSize, kRightKey, kKeySize);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToSucceed());
}

}  // media
