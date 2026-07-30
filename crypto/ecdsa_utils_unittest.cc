// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/ecdsa_utils.h"

#include <cstdint>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "crypto/keypair.h"
#include "crypto/test_support.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crypto {

namespace {

using ::testing::Optional;

constexpr auto kValidDerSignature = std::to_array<uint8_t>(
    {0x30, 0x45, 0x02, 0x20, 0x74, 0xa0, 0x6f, 0x6b, 0x2b, 0x0e, 0x82, 0x0e,
     0x03, 0x3b, 0x6e, 0x98, 0xfc, 0x89, 0x9c, 0xf3, 0x30, 0xb5, 0x56, 0xd3,
     0x29, 0x89, 0xb5, 0x82, 0x33, 0x5f, 0x9d, 0x97, 0xfb, 0x65, 0x64, 0x90,
     0x02, 0x21, 0x00, 0xbc, 0xb5, 0xee, 0x42, 0xe2, 0x5a, 0x87, 0xae, 0x21,
     0x18, 0xda, 0x7e, 0x68, 0x65, 0x30, 0xbe, 0xe5, 0x69, 0x3d, 0xc5, 0x5f,
     0xd5, 0x62, 0x45, 0x3e, 0x8d, 0x0b, 0x05, 0x1a, 0x33, 0x79, 0x8d});
constexpr auto kInvalidDerSignature = std::to_array<uint8_t>({1, 2, 3});

constexpr auto kValidRawSignature = std::to_array<uint8_t>(
    {0x74, 0xa0, 0x6f, 0x6b, 0x2b, 0x0e, 0x82, 0x0e, 0x03, 0x3b, 0x6e,
     0x98, 0xfc, 0x89, 0x9c, 0xf3, 0x30, 0xb5, 0x56, 0xd3, 0x29, 0x89,
     0xb5, 0x82, 0x33, 0x5f, 0x9d, 0x97, 0xfb, 0x65, 0x64, 0x90, 0xbc,
     0xb5, 0xee, 0x42, 0xe2, 0x5a, 0x87, 0xae, 0x21, 0x18, 0xda, 0x7e,
     0x68, 0x65, 0x30, 0xbe, 0xe5, 0x69, 0x3d, 0xc5, 0x5f, 0xd5, 0x62,
     0x45, 0x3e, 0x8d, 0x0b, 0x05, 0x1a, 0x33, 0x79, 0x8d});

// NOTE: Structured bindings can't be declared `constexpr`, thus we do it
// manually here.
constexpr auto kR = base::span(kValidRawSignature).split_at<32>().first;
constexpr auto kS = base::span(kValidRawSignature).split_at<32>().second;

}  // namespace

TEST(EcdsaUtilsTest, ConvertEcdsaDerSignatureToRawSuccess) {
  std::optional<std::vector<uint8_t>> raw_signature =
      ConvertEcdsaDerSignatureToRaw(test::FixedEcP256PublicKeyForTesting(),
                                    kValidDerSignature);
  ASSERT_TRUE(raw_signature.has_value());
  EXPECT_EQ(base::span(*raw_signature), kValidRawSignature);
}

TEST(EcdsaUtilsTest, ConvertEcdsaDerSignatureToRawInvalidKey) {
  const keypair::PublicKey kNonEcdsaKey =
      test::FixedRsa2048PublicKeyForTesting();
  std::optional<std::vector<uint8_t>> raw_signature =
      ConvertEcdsaDerSignatureToRaw(kNonEcdsaKey, kValidDerSignature);
  EXPECT_FALSE(raw_signature.has_value());
}

TEST(EcdsaUtilsTest, ConvertEcdsaDerSignatureToRawInvalidSignature) {
  std::optional<std::vector<uint8_t>> raw_signature =
      ConvertEcdsaDerSignatureToRaw(test::FixedEcP256PublicKeyForTesting(),
                                    kInvalidDerSignature);
  EXPECT_FALSE(raw_signature.has_value());
}

TEST(EcdsaUtilsTest, ConvertEcdsaRawSignatureToDerSuccess) {
  EXPECT_THAT(ConvertEcdsaRawSignatureToDer(
                  test::FixedEcP256PublicKeyForTesting(), kValidRawSignature),
              Optional(base::ToVector(kValidDerSignature)));
}

TEST(EcdsaUtilsTest, ConvertEcdsaRawComponentsToDerSuccess) {
  EXPECT_THAT(ConvertEcdsaRawComponentsToDer(kR, kS),
              Optional(base::ToVector(kValidDerSignature)));
}

TEST(EcdsaUtilsTest, ConvertEcdsaRawSignatureToDerInvalidSizes) {
  // P-256 signature must be exactly 64 bytes.
  static constexpr auto kInvalidSizeSignature =
      std::to_array<uint8_t>({1, 2, 3});
  EXPECT_EQ(ConvertEcdsaRawSignatureToDer(
                test::FixedEcP256PublicKeyForTesting(), kInvalidSizeSignature),
            std::nullopt);
}

}  // namespace crypto
