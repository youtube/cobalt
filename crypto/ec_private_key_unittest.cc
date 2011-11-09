// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/ec_private_key.h"

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_OPENSSL)
// Once ECPrivateKey is implemented for OpenSSL, remove this #if block.
// TODO(mattm): When that happens, also add some exported keys from each to test
// interop between NSS and OpenSSL.
TEST(ECPrivateKeyUnitTest, OpenSSLStub) {
  scoped_ptr<crypto::ECPrivateKey> keypair1(
      crypto::ECPrivateKey::Create());
  ASSERT_FALSE(keypair1.get());
}
#else
// Generate random private keys. Export, then re-import. We should get
// back the same exact public key, and the private key should have the same
// value and elliptic curve params.
TEST(ECPrivateKeyUnitTest, InitRandomTest) {
  const std::string password1 = "";
  const std::string password2 = "test";

  scoped_ptr<crypto::ECPrivateKey> keypair1(
      crypto::ECPrivateKey::Create());
  scoped_ptr<crypto::ECPrivateKey> keypair2(
      crypto::ECPrivateKey::Create());
  ASSERT_TRUE(keypair1.get());
  ASSERT_TRUE(keypair2.get());

  std::vector<uint8> key1value;
  std::vector<uint8> key2value;
  std::vector<uint8> key1params;
  std::vector<uint8> key2params;
  EXPECT_TRUE(keypair1->ExportValue(&key1value));
  EXPECT_TRUE(keypair2->ExportValue(&key2value));
  EXPECT_TRUE(keypair1->ExportECParams(&key1params));
  EXPECT_TRUE(keypair2->ExportECParams(&key2params));

  std::vector<uint8> privkey1;
  std::vector<uint8> privkey2;
  std::vector<uint8> pubkey1;
  std::vector<uint8> pubkey2;
  ASSERT_TRUE(keypair1->ExportEncryptedPrivateKey(
      password1, 1, &privkey1));
  ASSERT_TRUE(keypair2->ExportEncryptedPrivateKey(
      password2, 1, &privkey2));
  EXPECT_TRUE(keypair1->ExportPublicKey(&pubkey1));
  EXPECT_TRUE(keypair2->ExportPublicKey(&pubkey2));

  scoped_ptr<crypto::ECPrivateKey> keypair3(
      crypto::ECPrivateKey::CreateFromEncryptedPrivateKeyInfo(
          password1, privkey1, pubkey1));
  scoped_ptr<crypto::ECPrivateKey> keypair4(
      crypto::ECPrivateKey::CreateFromEncryptedPrivateKeyInfo(
          password2, privkey2, pubkey2));
  ASSERT_TRUE(keypair3.get());
  ASSERT_TRUE(keypair4.get());

  std::vector<uint8> key3value;
  std::vector<uint8> key4value;
  std::vector<uint8> key3params;
  std::vector<uint8> key4params;
  EXPECT_TRUE(keypair3->ExportValue(&key3value));
  EXPECT_TRUE(keypair4->ExportValue(&key4value));
  EXPECT_TRUE(keypair3->ExportECParams(&key3params));
  EXPECT_TRUE(keypair4->ExportECParams(&key4params));

  EXPECT_EQ(key1value, key3value);
  EXPECT_EQ(key2value, key4value);
  EXPECT_EQ(key1params, key3params);
  EXPECT_EQ(key2params, key4params);

  std::vector<uint8> pubkey3;
  std::vector<uint8> pubkey4;
  EXPECT_TRUE(keypair3->ExportPublicKey(&pubkey3));
  EXPECT_TRUE(keypair4->ExportPublicKey(&pubkey4));

  EXPECT_EQ(pubkey1, pubkey3);
  EXPECT_EQ(pubkey2, pubkey4);
}

TEST(ECPrivateKeyUnitTest, BadPasswordTest) {
  const std::string password1 = "";
  const std::string password2 = "test";

  scoped_ptr<crypto::ECPrivateKey> keypair1(
      crypto::ECPrivateKey::Create());
  ASSERT_TRUE(keypair1.get());

  std::vector<uint8> privkey1;
  std::vector<uint8> pubkey1;
  ASSERT_TRUE(keypair1->ExportEncryptedPrivateKey(
      password1, 1, &privkey1));
  ASSERT_TRUE(keypair1->ExportPublicKey(&pubkey1));

  scoped_ptr<crypto::ECPrivateKey> keypair2(
      crypto::ECPrivateKey::CreateFromEncryptedPrivateKeyInfo(
          password2, privkey1, pubkey1));
  ASSERT_FALSE(keypair2.get());
}
#endif  // !defined(USE_OPENSSL)
