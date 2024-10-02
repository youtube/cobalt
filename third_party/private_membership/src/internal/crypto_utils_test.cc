// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "third_party/private_membership/src/internal/crypto_utils.h"

#include <string>

#include "third_party/private_membership/src/internal/aes_ctr_256_with_fixed_iv.h"
#include "third_party/private_membership/src/private_membership.pb.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "third_party/shell-encryption/src/testing/status_matchers.h"
#include "third_party/shell-encryption/src/testing/status_testing.h"

namespace private_membership {
namespace {

constexpr int kCurveId = NID_X9_62_prime256v1;

using ::rlwe::testing::StatusIs;
using ::testing::HasSubstr;

class CryptoUtilsTest : public ::testing::Test {
 protected:
  private_join_and_compute::Context ctx_;
};

TEST_F(CryptoUtilsTest, PadMaxByteLengthSmallerThanInputByteLength) {
  EXPECT_THAT(
      Pad("input", 4),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          HasSubstr("max_byte_length smaller than the input bytes length.")));
}

TEST_F(CryptoUtilsTest, PadSuccess) {
  std::string expected_output("\x05\x00\x00\x00input\x00\x00", 11);
  ASSERT_OK_AND_ASSIGN(auto x, Pad("input", 7));
  EXPECT_EQ(x, expected_output);
}

TEST_F(CryptoUtilsTest, UnpadByteLengthSmallerThan4) {
  EXPECT_THAT(Unpad("abc"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid bytes does not encode length.")));
}

TEST_F(CryptoUtilsTest, UnpadIncorrectByteLength) {
  std::string input("\x06\x00\x00\x00input", 9);
  EXPECT_THAT(Unpad(input), StatusIs(absl::StatusCode::kInvalidArgument,
                                     HasSubstr("Incorrect bytes length.")));
}

TEST_F(CryptoUtilsTest, UnpadSuccess) {
  std::string input("\x05\x00\x00\x00input\x00\x00", 11);
  ASSERT_OK_AND_ASSIGN(auto x, Unpad(input));
  EXPECT_EQ(x, "input");
}

TEST_F(CryptoUtilsTest, HashEncryptedId) {
  std::string id = "id";
  auto hash1 = HashEncryptedId(id, &ctx_);
  auto hash2 = HashEncryptedId(id, &ctx_);
  EXPECT_EQ(hash1, hash2);
  EXPECT_NE(hash1, id);
}

TEST_F(CryptoUtilsTest, EncryptValue) {
  std::string encrypted_id = "encrypt_value_id";
  std::string value = "value";
  uint32_t max_value_byte_length = 32;
  ASSERT_OK_AND_ASSIGN(
      std::string ciphertext,
      EncryptValue(encrypted_id, value, max_value_byte_length, &ctx_));
  EXPECT_NE(ciphertext, value);

  ASSERT_OK_AND_ASSIGN(auto value_encryption_key,
                       GetValueEncryptionKey(encrypted_id, &ctx_));
  ASSERT_OK_AND_ASSIGN(auto aes_ctr_256,
                       AesCtr256WithFixedIV::Create(value_encryption_key));

  ASSERT_OK_AND_ASSIGN(auto x, DecryptValue(encrypted_id, ciphertext, &ctx_));
  EXPECT_EQ(x, value);
}

TEST_F(CryptoUtilsTest, EncryptValueCiphertextLength) {
  std::string encrypted_id = "encrypt_value_id_length";
  uint32_t max_value_byte_length = 16;
  ASSERT_OK_AND_ASSIGN(
      std::string ciphertext_empty_string,
      EncryptValue(encrypted_id, /*value=*/"", max_value_byte_length, &ctx_));
  int ciphertext_length = ciphertext_empty_string.length();
  for (int i = 1; i <= max_value_byte_length; ++i) {
    std::string value(i, 0);
    ASSERT_OK_AND_ASSIGN(
        std::string ciphertext,
        EncryptValue(encrypted_id, value, max_value_byte_length, &ctx_));
    EXPECT_EQ(ciphertext.length(), ciphertext_length);
  }
}

TEST_F(CryptoUtilsTest, EncryptInvalidTooLongValue) {
  std::string encrypted_id = "encrypt_invalid_value_id";
  uint32_t max_value_byte_length = 8;
  std::string value(max_value_byte_length + 1, 'a');
  EXPECT_THAT(EncryptValue(encrypted_id, value, max_value_byte_length, &ctx_),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("larger than maximum value byte length")));
}

TEST_F(CryptoUtilsTest, DecryptInvalidCiphertextWithoutLengthEncoding) {
  std::string encrypted_id = "no_length_encoding";
  std::string invalid_plaintext = "abc";
  ASSERT_OK_AND_ASSIGN(auto value_encryption_key,
                       GetValueEncryptionKey(encrypted_id, &ctx_));
  ASSERT_OK_AND_ASSIGN(auto aes_ctr,
                       AesCtr256WithFixedIV::Create(value_encryption_key));
  ASSERT_OK_AND_ASSIGN(auto encrypted_value,
                       aes_ctr->Encrypt(invalid_plaintext));
  EXPECT_THAT(DecryptValue(encrypted_id, encrypted_value, &ctx_),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("does not encode length")));
}

TEST_F(CryptoUtilsTest, DecryptInvalidCiphertextWithIncorrectLength) {
  std::string encrypted_id = "incorrect_length";
  std::string invalid_plaintext = "aaaa";
  ASSERT_OK_AND_ASSIGN(auto value_encryption_key,
                       GetValueEncryptionKey(encrypted_id, &ctx_));
  ASSERT_OK_AND_ASSIGN(auto aes_ctr,
                       AesCtr256WithFixedIV::Create(value_encryption_key));
  ASSERT_OK_AND_ASSIGN(auto encrypted_value,
                       aes_ctr->Encrypt(invalid_plaintext));
  EXPECT_THAT(DecryptValue(encrypted_id, encrypted_value, &ctx_),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Incorrect bytes length")));
}

TEST_F(CryptoUtilsTest, HashEncryptedIdRegression) {
  private_join_and_compute::Context ctx;
  std::string id = "id";
  std::string hash = HashEncryptedId(id, &ctx);

  constexpr unsigned char expected_hash[] = {
      0xd0, 0xd5, 0x50, 0xc1, 0x88, 0xa8, 0xcd, 0x8a, 0x4,  0x97, 0x26,
      0x86, 0x90, 0x4,  0xe1, 0xbc, 0xbd, 0x9d, 0xba, 0x19, 0x3f, 0x2d,
      0x36, 0xa8, 0xb5, 0x87, 0x95, 0xe8, 0xe2, 0x57, 0x1,  0x76,
  };
  EXPECT_EQ(hash, std::string(reinterpret_cast<const char*>(expected_hash),
                              hash.length()));
}

TEST_F(CryptoUtilsTest, GetValueEncryptionKeyRegression) {
  private_join_and_compute::Context ctx;
  std::string id = "id";
  ASSERT_OK_AND_ASSIGN(std::string hash, GetValueEncryptionKey(id, &ctx));

  constexpr unsigned char expected_hash[] = {
      0x2a, 0x3b, 0xe5, 0xe4, 0x33, 0xd0, 0x85, 0x73, 0xdd, 0x76, 0xad,
      0xd1, 0x86, 0x33, 0x30, 0xac, 0x5b, 0x7f, 0x81, 0xbd, 0x4e, 0xdd,
      0xdb, 0x80, 0xc3, 0x51, 0xb5, 0xad, 0x0,  0x38, 0xca, 0xbd,
  };
  EXPECT_EQ(hash, std::string(reinterpret_cast<const char*>(expected_hash),
                              hash.length()));
}

}  // namespace
}  // namespace private_membership
