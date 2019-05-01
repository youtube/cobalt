/*
 * Copyright 2014 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "securemessage/public_key_proto_util.h"

#include <gtest/gtest.h>

using std::unique_ptr;

namespace securemessage {
namespace {

class PublicKeyProtoUtilTest : public testing::Test {
 public:
  unique_ptr<CryptoOps::PublicKey> public_key_;

  // A byte string representing the zero byte.
  string zero_byte_;

 protected:
  void SetEcP256PublicKey() {
    testing::Test::SetUp();

    unique_ptr<CryptoOps::KeyPair> key_pair =
        CryptoOps::GenerateEcP256KeyPair();
    public_key_ = unique_ptr<CryptoOps::PublicKey>(new CryptoOps::PublicKey(
        key_pair->public_key->data(), key_pair->public_key->algorithm()));
    zero_byte_ = string("", 1);
  }

  void SetRsa2048PublicKey() {
    unique_ptr<CryptoOps::KeyPair> key_pair =
        CryptoOps::GenerateRsa2048KeyPair();
    public_key_ = unique_ptr<CryptoOps::PublicKey>(new CryptoOps::PublicKey(
        key_pair->public_key->data(), key_pair->public_key->algorithm()));
    zero_byte_ = string("", 1);
  }

  void AssertKeyHasSimpleRsaStructure(const unique_ptr<GenericPublicKey>& key) {
    ASSERT_NE(nullptr, key);
    ASSERT_EQ(key->type(), PublicKeyType::RSA2048);
    ASSERT_FALSE(key->rsa2048_public_key().n().empty());
  }
};

}  // namespace
}  // namespace securemessage

namespace securemessage {
namespace unittesting {
namespace {

TEST_F(PublicKeyProtoUtilTest, EncodeAndParseEcP256) {
  SetEcP256PublicKey();

  unique_ptr<GenericPublicKey> generic_key =
      PublicKeyProtoUtil::EncodePublicKey(*public_key_);

  ASSERT_NE(nullptr, generic_key);
  ASSERT_EQ(generic_key->type(), PublicKeyType::EC_P256);

  ASSERT_FALSE(generic_key->ec_p256_public_key().x().empty());
  ASSERT_FALSE(generic_key->ec_p256_public_key().y().empty());

  // Ensure the highest bit is not set.
  char x_highest_byte = generic_key->ec_p256_public_key().x()[0];
  char y_highest_byte = generic_key->ec_p256_public_key().y()[0];
  ASSERT_FALSE(x_highest_byte & 0x80);
  ASSERT_FALSE(y_highest_byte & 0x80);

  unique_ptr<CryptoOps::PublicKey> restored_public_key =
      PublicKeyProtoUtil::ParsePublicKey(*generic_key);

  ASSERT_NE(nullptr, restored_public_key);

  ASSERT_EQ(restored_public_key->algorithm(),
            CryptoOps::KeyAlgorithm::ECDSA_KEY);
  ASSERT_EQ(public_key_->data().String(), restored_public_key->data().String());
}

TEST_F(PublicKeyProtoUtilTest, InvalidEncodingEcP256) {
  SetEcP256PublicKey();

  unique_ptr<GenericPublicKey> invalid_proto;
  unique_ptr<CryptoOps::PublicKey> restored_public_key;

  unique_ptr<GenericPublicKey> valid_proto =
      PublicKeyProtoUtil::EncodePublicKey(*public_key_);
  ASSERT_NE(nullptr, valid_proto);

  // Mess up the X coordinate by repeating it twice
  invalid_proto = unique_ptr<GenericPublicKey>(
      new GenericPublicKey(*valid_proto));
  string new_x = valid_proto->ec_p256_public_key().x();
  new_x += new_x;
  invalid_proto->mutable_ec_p256_public_key()->set_x(new_x);
  restored_public_key =
      PublicKeyProtoUtil::ParsePublicKey(*invalid_proto);
  ASSERT_EQ(nullptr, restored_public_key);

  // Mess up the Y coordinate by erasing it
  invalid_proto = unique_ptr<GenericPublicKey>(
      new GenericPublicKey(*valid_proto));
  invalid_proto->mutable_ec_p256_public_key()->set_y(zero_byte_);
  restored_public_key =
      PublicKeyProtoUtil::ParsePublicKey(*invalid_proto);
  ASSERT_EQ(nullptr, restored_public_key);

  // Pick a point that is likely not on the curve by copying X over Y
  invalid_proto = unique_ptr<GenericPublicKey>(
      new GenericPublicKey(*valid_proto));
  invalid_proto->mutable_ec_p256_public_key()->set_y(
      valid_proto->ec_p256_public_key().x());
  restored_public_key =
      PublicKeyProtoUtil::ParsePublicKey(*invalid_proto);
  ASSERT_EQ(nullptr, restored_public_key);

  // Try the point (0, 0)
  invalid_proto = unique_ptr<GenericPublicKey>(
      new GenericPublicKey(*valid_proto));
  invalid_proto->mutable_ec_p256_public_key()->set_x(zero_byte_);
  invalid_proto->mutable_ec_p256_public_key()->set_y(zero_byte_);
  restored_public_key =
      PublicKeyProtoUtil::ParsePublicKey(*invalid_proto);
  ASSERT_EQ(nullptr, restored_public_key);
}

TEST_F(PublicKeyProtoUtilTest, EncodeAndParseRsa2048) {
  SetRsa2048PublicKey();

  unique_ptr<GenericPublicKey> generic_key =
      PublicKeyProtoUtil::EncodePublicKey(*public_key_);

  AssertKeyHasSimpleRsaStructure(generic_key);

  // Ensure the highest modulus bit is not set.
  char n_highest_byte = generic_key->rsa2048_public_key().n()[0];
  ASSERT_FALSE(n_highest_byte & 0x80);

  unique_ptr<CryptoOps::PublicKey> restored_public_key =
      PublicKeyProtoUtil::ParsePublicKey(*generic_key);

  ASSERT_NE(nullptr, restored_public_key);

  ASSERT_EQ(restored_public_key->algorithm(),
            CryptoOps::KeyAlgorithm::RSA_KEY);
  ASSERT_EQ(public_key_->data().String(), restored_public_key->data().String());
}

TEST_F(PublicKeyProtoUtilTest, ParseInvalidEncodingRsa2048_EmptyModulus) {
  SetRsa2048PublicKey();

  unique_ptr<GenericPublicKey> generic_key =
      PublicKeyProtoUtil::EncodePublicKey(*public_key_);

  AssertKeyHasSimpleRsaStructure(generic_key);

  generic_key->mutable_rsa2048_public_key()->clear_n();

  unique_ptr<CryptoOps::PublicKey> restored_public_key =
      PublicKeyProtoUtil::ParsePublicKey(*generic_key);

  ASSERT_EQ(nullptr, restored_public_key);
}

TEST_F(PublicKeyProtoUtilTest, ParseInvalidEncodingRsa2048_ModulusTooShort) {
  SetRsa2048PublicKey();

  unique_ptr<GenericPublicKey> generic_key =
      PublicKeyProtoUtil::EncodePublicKey(*public_key_);

  AssertKeyHasSimpleRsaStructure(generic_key);

  generic_key->mutable_rsa2048_public_key()->set_n("\x10\x00\x10");

  unique_ptr<CryptoOps::PublicKey> restored_public_key =
      PublicKeyProtoUtil::ParsePublicKey(*generic_key);

  ASSERT_EQ(nullptr, restored_public_key);
}

TEST_F(PublicKeyProtoUtilTest,
       ParseInvalidEncodingRsa2048_ModulusEncodingTooLong) {
  SetRsa2048PublicKey();

  unique_ptr<GenericPublicKey> generic_key =
      PublicKeyProtoUtil::EncodePublicKey(*public_key_);

  AssertKeyHasSimpleRsaStructure(generic_key);

  generic_key->mutable_rsa2048_public_key()->set_n(
      string("\x00\x00\x80", 3).append(string(255, '0')));

  unique_ptr<CryptoOps::PublicKey> restored_public_key =
      PublicKeyProtoUtil::ParsePublicKey(*generic_key);

  ASSERT_EQ(nullptr, restored_public_key);
}

TEST_F(PublicKeyProtoUtilTest, ParseInvalidEncodingRsa2048_ModulusNot2048bit) {
  SetRsa2048PublicKey();

  unique_ptr<GenericPublicKey> generic_key =
      PublicKeyProtoUtil::EncodePublicKey(*public_key_);

  AssertKeyHasSimpleRsaStructure(generic_key);

  generic_key->mutable_rsa2048_public_key()->set_n(
      string("\x7F").append(string(255, '0')));

  unique_ptr<CryptoOps::PublicKey> restored_public_key =
      PublicKeyProtoUtil::ParsePublicKey(*generic_key);

  ASSERT_EQ(nullptr, restored_public_key);
}

TEST_F(PublicKeyProtoUtilTest, ParseInvalidEncodingRsa2048_DoubleModulusSize) {
  SetRsa2048PublicKey();

  unique_ptr<GenericPublicKey> generic_key =
      PublicKeyProtoUtil::EncodePublicKey(*public_key_);

  AssertKeyHasSimpleRsaStructure(generic_key);

  generic_key->mutable_rsa2048_public_key()->mutable_n()->append(
      generic_key->rsa2048_public_key().n());

  unique_ptr<CryptoOps::PublicKey> restored_public_key =
      PublicKeyProtoUtil::ParsePublicKey(*generic_key);

  ASSERT_EQ(nullptr, restored_public_key);
}

TEST_F(PublicKeyProtoUtilTest, ParseInvalidKeyTypeForRsa2048) {
  SetRsa2048PublicKey();

  unique_ptr<GenericPublicKey> generic_key =
      PublicKeyProtoUtil::EncodePublicKey(*public_key_);

  AssertKeyHasSimpleRsaStructure(generic_key);
  generic_key->set_type(PublicKeyType::EC_P256);

  unique_ptr<CryptoOps::PublicKey> restored_public_key =
      PublicKeyProtoUtil::ParsePublicKey(*generic_key);

  ASSERT_EQ(nullptr, restored_public_key);
}

TEST_F(PublicKeyProtoUtilTest, ParseInvalidKeyTypeForEcP256) {
  SetEcP256PublicKey();

  unique_ptr<GenericPublicKey> generic_key =
      PublicKeyProtoUtil::EncodePublicKey(*public_key_);

  ASSERT_NE(nullptr, generic_key);
  ASSERT_EQ(generic_key->type(), PublicKeyType::EC_P256);
  generic_key->set_type(PublicKeyType::RSA2048);

  unique_ptr<CryptoOps::PublicKey> restored_public_key =
      PublicKeyProtoUtil::ParsePublicKey(*generic_key);

  ASSERT_EQ(nullptr, restored_public_key);
}

TEST_F(PublicKeyProtoUtilTest, EncodeInvalidKeyTypeForRsa2048) {
  SetRsa2048PublicKey();

  unique_ptr<GenericPublicKey> generic_key =
      PublicKeyProtoUtil::EncodePublicKey(
          CryptoOps::PublicKey(public_key_->data(),
                               CryptoOps::ECDSA_KEY));

  ASSERT_EQ(nullptr, generic_key);
}

TEST_F(PublicKeyProtoUtilTest, EncodeInvalidKeyTypeForEcP256) {
  SetEcP256PublicKey();

  unique_ptr<GenericPublicKey> generic_key =
      PublicKeyProtoUtil::EncodePublicKey(
          CryptoOps::PublicKey(public_key_->data(),
                               CryptoOps::RSA_KEY));

  ASSERT_EQ(nullptr, generic_key);
}

}  // namespace
}  // namespace unittesting
}  // namespace securemessage
