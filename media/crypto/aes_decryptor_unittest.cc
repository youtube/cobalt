// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/mock_filters.h"
#include "media/crypto/aes_decryptor.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Gt;
using ::testing::NotNull;
using ::testing::SaveArg;
using ::testing::StrNe;

namespace media {

static const char kClearKeySystem[] = "org.w3.clearkey";
static const uint8 kInitData[] = { 0x69, 0x6e, 0x69, 0x74 };
// |kEncryptedData| is encrypted from |kOriginalData| using |kRightKey|.
// Modifying any of these independently would fail the test.
static const uint8 kOriginalData[] = {
  0x4f, 0x72, 0x69, 0x67, 0x69, 0x6e, 0x61, 0x6c,
  0x20, 0x64, 0x61, 0x74, 0x61, 0x2e
};
static const uint8 kEncryptedData[] = {
  0x82, 0x3A, 0x76, 0x92, 0xEC, 0x7F, 0xF8, 0x85,
  0xEC, 0x23, 0x52, 0xFB, 0x19, 0xB1, 0xB9, 0x09
};
static const uint8 kRightKey[] = {
  0x41, 0x20, 0x77, 0x6f, 0x6e, 0x64, 0x65, 0x72,
  0x66, 0x75, 0x6c, 0x20, 0x6b, 0x65, 0x79, 0x21
};
static const uint8 kWrongKey[] = {
  0x49, 0x27, 0x6d, 0x20, 0x61, 0x20, 0x77, 0x72,
  0x6f, 0x6e, 0x67, 0x20, 0x6b, 0x65, 0x79, 0x2e
};
static const uint8 kWrongSizedKey[] = { 0x20, 0x20 };
static const uint8 kKeyId1[] = {
  0x4b, 0x65, 0x79, 0x20, 0x49, 0x44, 0x20, 0x31
};
static const uint8 kKeyId2[] = {
  0x4b, 0x65, 0x79, 0x20, 0x49, 0x44, 0x20, 0x32
};

class AesDecryptorTest : public testing::Test {
 public:
  AesDecryptorTest() : decryptor_(&client_) {
    encrypted_data_ = DecoderBuffer::CopyFrom(kEncryptedData,
                                              arraysize(kEncryptedData));
  }

 protected:
  void GenerateKeyRequest() {
    EXPECT_CALL(client_, KeyMessageMock(kClearKeySystem, StrNe(std::string()),
                                        NotNull(), Gt(0), ""))
        .WillOnce(SaveArg<1>(&session_id_string_));
    decryptor_.GenerateKeyRequest(kClearKeySystem,
                                  kInitData, arraysize(kInitData));
  }

  template <int KeyIdSize, int KeySize>
  void AddKeyAndExpectToSucceed(const uint8 (&key_id)[KeyIdSize],
                                const uint8 (&key)[KeySize]) {
    EXPECT_CALL(client_, KeyAdded(kClearKeySystem, session_id_string_));
    decryptor_.AddKey(kClearKeySystem, key, KeySize, key_id, KeyIdSize,
                      session_id_string_);
  }

  template <int KeyIdSize, int KeySize>
  void AddKeyAndExpectToFail(const uint8 (&key_id)[KeyIdSize],
                             const uint8 (&key)[KeySize]) {
    EXPECT_CALL(client_, KeyError(kClearKeySystem, session_id_string_,
                                  Decryptor::kUnknownError, 0));
    decryptor_.AddKey(kClearKeySystem, key, KeySize, key_id, KeyIdSize,
                      session_id_string_);
  }

  template <int KeyIdSize>
  void SetKeyIdForEncryptedData(const uint8 (&key_id)[KeyIdSize]) {
    encrypted_data_->SetDecryptConfig(
        scoped_ptr<DecryptConfig>(new DecryptConfig(key_id, KeyIdSize)));
  }

  void DecryptAndExpectToSucceed() {
    scoped_refptr<DecoderBuffer> decrypted =
        decryptor_.Decrypt(encrypted_data_);
    ASSERT_TRUE(decrypted);
    int data_length = sizeof(kOriginalData);
    ASSERT_EQ(data_length, decrypted->GetDataSize());
    EXPECT_EQ(0, memcmp(kOriginalData, decrypted->GetData(), data_length));
  }

  void DecryptAndExpectToFail() {
    scoped_refptr<DecoderBuffer> decrypted =
        decryptor_.Decrypt(encrypted_data_);
    EXPECT_FALSE(decrypted);
  }

  scoped_refptr<DecoderBuffer> encrypted_data_;
  MockDecryptorClient client_;
  AesDecryptor decryptor_;
  std::string session_id_string_;
};

TEST_F(AesDecryptorTest, NormalDecryption) {
  GenerateKeyRequest();
  AddKeyAndExpectToSucceed(kKeyId1, kRightKey);
  SetKeyIdForEncryptedData(kKeyId1);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToSucceed());
}

TEST_F(AesDecryptorTest, WrongKey) {
  GenerateKeyRequest();
  AddKeyAndExpectToSucceed(kKeyId1, kWrongKey);
  SetKeyIdForEncryptedData(kKeyId1);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToFail());
}

TEST_F(AesDecryptorTest, MultipleKeys) {
  GenerateKeyRequest();
  AddKeyAndExpectToSucceed(kKeyId1, kRightKey);
  AddKeyAndExpectToSucceed(kKeyId2, kWrongKey);
  SetKeyIdForEncryptedData(kKeyId1);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToSucceed());
}

TEST_F(AesDecryptorTest, KeyReplacement) {
  GenerateKeyRequest();
  SetKeyIdForEncryptedData(kKeyId1);
  AddKeyAndExpectToSucceed(kKeyId1, kWrongKey);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToFail());
  AddKeyAndExpectToSucceed(kKeyId1, kRightKey);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToSucceed());
}

TEST_F(AesDecryptorTest, WrongSizedKey) {
  GenerateKeyRequest();
  AddKeyAndExpectToFail(kKeyId1, kWrongSizedKey);
}

}  // media
