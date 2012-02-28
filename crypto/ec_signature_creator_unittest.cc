// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/ec_signature_creator.h"

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "crypto/ec_private_key.h"
#include "crypto/signature_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_OPENSSL)
// Once ECSignatureCreator is implemented for OpenSSL, remove this #if block.
// TODO(rch): When that happens, also add some exported keys from each to
// test interop between NSS and OpenSSL.
TEST(ECSignatureCreatorTest, OpenSSLStub) {
  scoped_ptr<crypto::ECSignatureCreator> signer(
      crypto::ECSignatureCreator::Create(NULL));
  ASSERT_TRUE(signer.get());
  EXPECT_FALSE(signer->Sign(NULL, 0, NULL));
}
#else
TEST(ECSignatureCreatorTest, BasicTest) {
  // Do a verify round trip.
  scoped_ptr<crypto::ECPrivateKey> key_original(
      crypto::ECPrivateKey::Create());
  ASSERT_TRUE(key_original.get());

  std::vector<uint8> key_info;
  ASSERT_TRUE(key_original->ExportEncryptedPrivateKey("", 1000, &key_info));
  std::vector<uint8> pubkey_info;
  ASSERT_TRUE(key_original->ExportPublicKey(&pubkey_info));

  scoped_ptr<crypto::ECPrivateKey> key(
      crypto::ECPrivateKey::CreateFromEncryptedPrivateKeyInfo("", key_info,
                                                              pubkey_info));
  ASSERT_TRUE(key.get());
  ASSERT_TRUE(key->key() != NULL);

  scoped_ptr<crypto::ECSignatureCreator> signer(
      crypto::ECSignatureCreator::Create(key.get()));
  ASSERT_TRUE(signer.get());

  std::string data("Hello, World!");
  std::vector<uint8> signature;
  ASSERT_TRUE(signer->Sign(reinterpret_cast<const uint8*>(data.c_str()),
                           data.size(),
                           &signature));

  std::vector<uint8> public_key_info;
  ASSERT_TRUE(key_original->ExportPublicKey(&public_key_info));

  // This is the algorithm ID for SHA-1 with EC encryption.
  const uint8 kECDSAWithSHA1AlgorithmID[] = {
    0x30, 0x0b,
      0x06, 0x07,
        0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x01,
      0x05, 0x00
  };
  crypto::SignatureVerifier verifier;
  ASSERT_TRUE(verifier.VerifyInit(
      kECDSAWithSHA1AlgorithmID, sizeof(kECDSAWithSHA1AlgorithmID),
      &signature.front(), signature.size(),
      &public_key_info.front(), public_key_info.size()));

  verifier.VerifyUpdate(reinterpret_cast<const uint8*>(data.c_str()),
                        data.size());
  ASSERT_TRUE(verifier.VerifyFinal());
}
#endif  // !defined(USE_OPENSSL)
