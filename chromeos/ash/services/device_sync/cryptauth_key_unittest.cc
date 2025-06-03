// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_key.h"

#include "chromeos/ash/services/device_sync/value_string_encoding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::device_sync {

namespace {

const char kFakeHandle[] = "fake-handle";
const char kFakeSymmetricKey[] = "fake-symmetric-key";
const char kFakeSymmetricKeySha256HashBase64Url[] =
    "-lh4oqYTenQmzyIY8XJreGDJ95A4Sk41c15BQPKOmCY=";
const char kFakePublicKey[] = "fake-public-key";
const char kFakePublicKeySha256HashBase64Url[] =
    "vj5oRVhZmlDrE4G4RKNV37Etgr_XuNOwEFAzb888_KM=";
const char kFakePrivateKey[] = "fake-private-key";

}  // namespace

TEST(DeviceSyncCryptAuthKeyTest, CreateSymmetricKey) {
  CryptAuthKey key(kFakeSymmetricKey, CryptAuthKey::Status::kActive,
                   cryptauthv2::KeyType::RAW256);

  ASSERT_TRUE(key.IsSymmetricKey());
  ASSERT_FALSE(key.IsAsymmetricKey());
  EXPECT_EQ(key.symmetric_key(), kFakeSymmetricKey);
  EXPECT_EQ(key.status(), CryptAuthKey::Status::kActive);
  EXPECT_EQ(key.type(), cryptauthv2::KeyType::RAW256);
  EXPECT_EQ(key.handle(), kFakeSymmetricKeySha256HashBase64Url);

  CryptAuthKey key_given_handle(kFakeSymmetricKey,
                                CryptAuthKey::Status::kActive,
                                cryptauthv2::KeyType::RAW256, kFakeHandle);
  EXPECT_EQ(key_given_handle.handle(), kFakeHandle);
}

TEST(DeviceSyncCryptAuthKeyTest, CreateAsymmetricKey) {
  CryptAuthKey key(kFakePublicKey, kFakePrivateKey,
                   CryptAuthKey::Status::kActive, cryptauthv2::KeyType::P256);

  ASSERT_FALSE(key.IsSymmetricKey());
  ASSERT_TRUE(key.IsAsymmetricKey());
  EXPECT_EQ(key.public_key(), kFakePublicKey);
  EXPECT_EQ(key.private_key(), kFakePrivateKey);
  EXPECT_EQ(key.status(), CryptAuthKey::Status::kActive);
  EXPECT_EQ(key.type(), cryptauthv2::KeyType::P256);
  EXPECT_EQ(key.handle(), kFakePublicKeySha256HashBase64Url);

  CryptAuthKey key_given_handle(kFakePublicKey, kFakePrivateKey,
                                CryptAuthKey::Status::kActive,
                                cryptauthv2::KeyType::P256, kFakeHandle);
  EXPECT_EQ(key_given_handle.handle(), kFakeHandle);
}

TEST(DeviceSyncCryptAuthKeyTest, SymmetricKeyAsDictionary) {
  CryptAuthKey symmetric_key(kFakeSymmetricKey, CryptAuthKey::Status::kActive,
                             cryptauthv2::KeyType::RAW256, kFakeHandle);

  auto dict =
      base::Value::Dict()
          .Set("handle", kFakeHandle)
          .Set("status", CryptAuthKey::Status::kActive)
          .Set("type", cryptauthv2::KeyType::RAW256)
          .Set("symmetric_key", util::EncodeAsValueString(kFakeSymmetricKey));

  EXPECT_EQ(symmetric_key.AsSymmetricKeyDictionary(), dict);
}

TEST(DeviceSyncCryptAuthKeyTest, AsymmetricKeyAsDictionary) {
  CryptAuthKey asymmetric_key(kFakePublicKey, kFakePrivateKey,
                              CryptAuthKey::Status::kActive,
                              cryptauthv2::KeyType::P256, kFakeHandle);

  auto dict =
      base::Value::Dict()
          .Set("handle", kFakeHandle)
          .Set("status", CryptAuthKey::Status::kActive)
          .Set("type", cryptauthv2::KeyType::P256)
          .Set("public_key", util::EncodeAsValueString(kFakePublicKey))
          .Set("private_key", util::EncodeAsValueString(kFakePrivateKey));

  EXPECT_EQ(asymmetric_key.AsAsymmetricKeyDictionary(), dict);
}

TEST(DeviceSyncCryptAuthKeyTest, SymmetricKeyFromDictionary) {
  auto dict =
      base::Value::Dict()
          .Set("handle", kFakeHandle)
          .Set("status", CryptAuthKey::Status::kActive)
          .Set("type", cryptauthv2::KeyType::RAW256)
          .Set("symmetric_key", util::EncodeAsValueString(kFakeSymmetricKey));

  absl::optional<CryptAuthKey> key =
      CryptAuthKey::FromDictionary(std::move(dict));
  ASSERT_TRUE(key);
  EXPECT_EQ(*key, CryptAuthKey(kFakeSymmetricKey, CryptAuthKey::Status::kActive,
                               cryptauthv2::KeyType::RAW256, kFakeHandle));
}

TEST(DeviceSyncCryptAuthKeyTest, AsymmetricKeyFromDictionary) {
  auto dict =
      base::Value::Dict()
          .Set("handle", kFakeHandle)
          .Set("status", CryptAuthKey::Status::kActive)
          .Set("type", cryptauthv2::KeyType::P256)
          .Set("public_key", util::EncodeAsValueString(kFakePublicKey))
          .Set("private_key", util::EncodeAsValueString(kFakePrivateKey));

  absl::optional<CryptAuthKey> key =
      CryptAuthKey::FromDictionary(std::move(dict));
  ASSERT_TRUE(key);
  EXPECT_EQ(*key, CryptAuthKey(kFakePublicKey, kFakePrivateKey,
                               CryptAuthKey::Status::kActive,
                               cryptauthv2::KeyType::P256, kFakeHandle));
}

TEST(DeviceSyncCryptAuthKeyTest, KeyFromDictionary_MissingHandle) {
  auto dict = base::Value::Dict()
                  .Set("status", CryptAuthKey::Status::kActive)
                  .Set("type", cryptauthv2::KeyType::RAW256)
                  .Set("symmetric_key", kFakeSymmetricKey);

  EXPECT_FALSE(CryptAuthKey::FromDictionary(std::move(dict)));
}

TEST(DeviceSyncCryptAuthKeyTest, KeyFromDictionary_MissingStatus) {
  auto dict = base::Value::Dict()
                  .Set("handle", kFakeHandle)
                  .Set("type", cryptauthv2::KeyType::RAW256)
                  .Set("symmetric_key", kFakeSymmetricKey);

  EXPECT_FALSE(CryptAuthKey::FromDictionary(std::move(dict)));
}

TEST(DeviceSyncCryptAuthKeyTest, KeyFromDictionary_MissingType) {
  auto dict = base::Value::Dict()
                  .Set("handle", kFakeHandle)
                  .Set("status", CryptAuthKey::Status::kActive)
                  .Set("symmetric_key", kFakeSymmetricKey);

  EXPECT_FALSE(CryptAuthKey::FromDictionary(std::move(dict)));
}

TEST(DeviceSyncCryptAuthKeyTest,
     SymmetricKeyFromDictionary_MissingSymmetricKey) {
  auto dict = base::Value::Dict()
                  .Set("handle", kFakeHandle)
                  .Set("status", CryptAuthKey::Status::kActive)
                  .Set("type", cryptauthv2::KeyType::RAW256);

  EXPECT_FALSE(CryptAuthKey::FromDictionary(std::move(dict)));
}

TEST(DeviceSyncCryptAuthKeyTest, AsymmetricKeyFromDictionary_MissingPublicKey) {
  auto dict = base::Value::Dict()
                  .Set("handle", kFakeHandle)
                  .Set("status", CryptAuthKey::Status::kActive)
                  .Set("type", cryptauthv2::KeyType::P256)
                  .Set("private_key", kFakePrivateKey);

  EXPECT_FALSE(CryptAuthKey::FromDictionary(std::move(dict)));
}

TEST(DeviceSyncCryptAuthKeyTest,
     AsymmetricKeyFromDictionary_MissingPrivateKey) {
  auto dict = base::Value::Dict()
                  .Set("handle", kFakeHandle)
                  .Set("status", CryptAuthKey::Status::kActive)
                  .Set("type", cryptauthv2::KeyType::P256)
                  .Set("public_key", kFakePublicKey);

  EXPECT_FALSE(CryptAuthKey::FromDictionary(std::move(dict)));
}

TEST(DeviceSyncCryptAuthKeyTest, Equality) {
  CryptAuthKey symmetric_key(kFakeSymmetricKey, CryptAuthKey::Status::kActive,
                             cryptauthv2::KeyType::RAW256);
  CryptAuthKey asymmetric_key(kFakePublicKey, kFakePrivateKey,
                              CryptAuthKey::Status::kActive,
                              cryptauthv2::KeyType::P256);

  EXPECT_EQ(symmetric_key,
            CryptAuthKey(kFakeSymmetricKey, CryptAuthKey::Status::kActive,
                         cryptauthv2::KeyType::RAW256));
  EXPECT_EQ(asymmetric_key, CryptAuthKey(kFakePublicKey, kFakePrivateKey,
                                         CryptAuthKey::Status::kActive,
                                         cryptauthv2::KeyType::P256));
}

TEST(DeviceSyncCryptAuthKeyTest, NotEquality) {
  CryptAuthKey symmetric_key(kFakeSymmetricKey, CryptAuthKey::Status::kActive,
                             cryptauthv2::KeyType::RAW256);
  CryptAuthKey asymmetric_key(kFakePublicKey, kFakePrivateKey,
                              CryptAuthKey::Status::kActive,
                              cryptauthv2::KeyType::P256);
  EXPECT_NE(symmetric_key, asymmetric_key);

  EXPECT_NE(symmetric_key,
            CryptAuthKey(kFakeSymmetricKey, CryptAuthKey::Status::kInactive,
                         cryptauthv2::KeyType::RAW256));
  EXPECT_NE(symmetric_key,
            CryptAuthKey(kFakeSymmetricKey, CryptAuthKey::Status::kActive,
                         cryptauthv2::KeyType::RAW128));
  EXPECT_NE(symmetric_key,
            CryptAuthKey("different-sym-key", CryptAuthKey::Status::kActive,
                         cryptauthv2::KeyType::RAW256));

  EXPECT_NE(asymmetric_key, CryptAuthKey(kFakePublicKey, kFakePrivateKey,
                                         CryptAuthKey::Status::kInactive,
                                         cryptauthv2::KeyType::P256));
  EXPECT_NE(asymmetric_key, CryptAuthKey(kFakePublicKey, kFakePrivateKey,
                                         CryptAuthKey::Status::kActive,
                                         cryptauthv2::KeyType::CURVE25519));
  EXPECT_NE(asymmetric_key, CryptAuthKey("different-pub-key", kFakePrivateKey,
                                         CryptAuthKey::Status::kActive,
                                         cryptauthv2::KeyType::P256));
  EXPECT_NE(asymmetric_key, CryptAuthKey(kFakePublicKey, "different-priv-key",
                                         CryptAuthKey::Status::kActive,
                                         cryptauthv2::KeyType::P256));
}

}  // namespace ash::device_sync
