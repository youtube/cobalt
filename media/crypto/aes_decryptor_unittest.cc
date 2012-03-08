// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "media/base/data_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/crypto/aes_decryptor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// |kEncryptedDataHex| is encrypted from |kOriginalData| using |kRawKey|, whose
// length is |kKeySize|. Modifying any of these independently would fail the
// test.
static const char kOriginalData[] = "Original data.";
static const int kEncryptedDataSize = 16;
static const unsigned char kEncryptedData[] =
    "\x82\x3A\x76\x92\xEC\x7F\xF8\x85\xEC\x23\x52\xFB\x19\xB1\xB9\x09";
static const int kKeySize = 16;
static const unsigned char kRawKey[] = "A wonderful key!";
static const unsigned char kWrongKey[] = "I'm a wrong key.";

class AesDecryptorTest : public testing::Test {
 public:
  AesDecryptorTest() {
    encrypted_data_ = DataBuffer::CopyFrom(kEncryptedData, kEncryptedDataSize);
  }

 protected:
  void SetKey(const uint8* key, int key_size) {
    encrypted_data_->SetDecryptConfig(
        scoped_ptr<DecryptConfig>(new DecryptConfig(key, key_size)));
  }

  scoped_refptr<DataBuffer> encrypted_data_;
  AesDecryptor decryptor_;
};

TEST_F(AesDecryptorTest, NormalDecryption) {
  SetKey(kRawKey, kKeySize);
  scoped_refptr<Buffer> decrypted_data = decryptor_.Decrypt(encrypted_data_);
  ASSERT_TRUE(decrypted_data.get());
  size_t data_length = sizeof(kOriginalData) - 1;
  ASSERT_EQ(data_length, decrypted_data->GetDataSize());
  ASSERT_EQ(0, memcmp(kOriginalData, decrypted_data->GetData(), data_length));
}

TEST_F(AesDecryptorTest, WrongKey) {
  SetKey(kWrongKey, kKeySize);
  scoped_refptr<Buffer> decrypted_data = decryptor_.Decrypt(encrypted_data_);
  EXPECT_FALSE(decrypted_data.get());
}

}  // media
