// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack.h"

#include <array>

#include "base/containers/span.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_signature.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_package {

namespace {

constexpr std::array<uint8_t, 32> kTestPublicKey1 = {
    0x01, 0x23, 0x43, 0x43, 0x33, 0x42, 0x7A, 0x14, 0x42, 0x14, 0xa2,
    0xb6, 0xc2, 0xd9, 0xf2, 0x02, 0x03, 0x42, 0x18, 0x10, 0x12, 0x26,
    0x62, 0x88, 0xf6, 0xa3, 0xa5, 0x47, 0x14, 0x69, 0x00, 0x73};

constexpr std::array<uint8_t, 32> kTestPublicKey2 = {
    222, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6,
    7,   8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2};

// Corresponds to `kTestPublicKey1`.
constexpr char kEd25519SignedWebBundleId1[] =
    "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";

constexpr std::array<uint8_t, 64> kTestSignature1 = {
    111, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2,
    3,   4, 5, 6, 7, 8, 9, 0, 1, 2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2,
    3,   4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2};
constexpr std::array<uint8_t, 64> kTestSignature2 = {
    222, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2,
    3,   4, 5, 6, 7, 8, 9, 0, 1, 2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2,
    3,   4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2};

TEST(SignedWebBundleSignatureStack,
     CreateFromEmptyVectorOfSignedWebBundleSignatureStackEntry) {
  std::vector<SignedWebBundleSignatureStackEntry> entries;
  auto result = SignedWebBundleSignatureStack::Create(entries);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "The signature stack needs at least one entry.");
}

TEST(SignedWebBundleSignatureStack,
     CreateFromEmptyVectorOfBundleIntegrityBlockSignatureStackEntryPtr) {
  std::vector<mojom::BundleIntegrityBlockSignatureStackEntryPtr> entries;
  auto result = SignedWebBundleSignatureStack::Create(std::move(entries));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "The signature stack needs at least one entry.");
}

TEST(SignedWebBundleSignatureStack,
     CreateFromVectorOfSignedWebBundleSignatureStackEntry) {
  SignedWebBundleSignatureStackEntry entry(
      /*complete_entry_cbor=*/{1, 2, 3}, /*attributes_cbor=*/{4, 5},
      Ed25519PublicKey::Create(base::make_span(kTestPublicKey1)),
      Ed25519Signature::Create(base::make_span(kTestSignature1)));

  std::vector<SignedWebBundleSignatureStackEntry> entries = {entry};
  auto result = SignedWebBundleSignatureStack::Create(entries);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_EQ(result->size(), 1ul);

  EXPECT_EQ(result->entries()[0], entry);
  EXPECT_EQ(result->derived_web_bundle_id(),
            SignedWebBundleId::Create(kEd25519SignedWebBundleId1));
}

TEST(SignedWebBundleSignatureStack,
     CreateFromVectorOfMultipleSignedWebBundleSignatureStackEntry) {
  SignedWebBundleSignatureStackEntry entry1(
      /*complete_entry_cbor=*/{1, 2, 3}, /*attributes_cbor=*/{4, 5},
      Ed25519PublicKey::Create(base::make_span(kTestPublicKey1)),
      Ed25519Signature::Create(base::make_span(kTestSignature1)));
  SignedWebBundleSignatureStackEntry entry2(
      /*complete_entry_cbor=*/{6, 7}, /*attributes_cbor=*/{8, 9, 0},
      Ed25519PublicKey::Create(base::make_span(kTestPublicKey2)),
      Ed25519Signature::Create(base::make_span(kTestSignature2)));

  std::vector<SignedWebBundleSignatureStackEntry> entries = {entry1, entry2};
  auto result = SignedWebBundleSignatureStack::Create(entries);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_EQ(result->size(), 2ul);

  EXPECT_EQ(result->entries()[0], entry1);
  EXPECT_EQ(result->entries()[1], entry2);
}

TEST(SignedWebBundleSignatureStack,
     CreateFromVectorOfBundleIntegrityBlockSignatureStackEntryPtr) {
  auto entry = mojom::BundleIntegrityBlockSignatureStackEntry::New();
  entry->complete_entry_cbor = {1, 2, 3};
  entry->attributes_cbor = {4, 5};
  entry->public_key =
      Ed25519PublicKey::Create(base::make_span(kTestPublicKey1));
  entry->signature = Ed25519Signature::Create(base::make_span(kTestSignature1));

  std::vector<mojom::BundleIntegrityBlockSignatureStackEntryPtr> entries;
  entries.push_back(entry->Clone());
  auto result = SignedWebBundleSignatureStack::Create(std::move(entries));
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_EQ(result->size(), 1ul);

  EXPECT_EQ(result->entries()[0].complete_entry_cbor(),
            entry->complete_entry_cbor);
  EXPECT_EQ(result->entries()[0].attributes_cbor(), entry->attributes_cbor);
  EXPECT_EQ(result->entries()[0].public_key(), entry->public_key);
  EXPECT_EQ(result->entries()[0].signature(), entry->signature);
}

TEST(SignedWebBundleSignatureStack, Comparators) {
  const SignedWebBundleSignatureStackEntry entry1(
      /*complete_entry_cbor=*/{1}, /*attributes_cbor=*/{},
      Ed25519PublicKey::Create(base::make_span(kTestPublicKey1)),
      Ed25519Signature::Create(base::make_span(kTestSignature1)));

  const SignedWebBundleSignatureStackEntry entry2(
      /*complete_entry_cbor=*/{2}, /*attributes_cbor=*/{},
      Ed25519PublicKey::Create(base::make_span(kTestPublicKey1)),
      Ed25519Signature::Create(base::make_span(kTestSignature1)));

  SignedWebBundleSignatureStack stack1a =
      *SignedWebBundleSignatureStack::Create(std::array{entry1});
  SignedWebBundleSignatureStack stack1b =
      *SignedWebBundleSignatureStack::Create(std::array{entry1});

  SignedWebBundleSignatureStack stack2 =
      *SignedWebBundleSignatureStack::Create(std::array{entry2});

  EXPECT_TRUE(stack1a == stack1a);
  EXPECT_TRUE(stack1a == stack1b);
  EXPECT_FALSE(stack1a == stack2);

  EXPECT_FALSE(stack1a != stack1a);
  EXPECT_FALSE(stack1a != stack1b);
  EXPECT_TRUE(stack1a != stack2);
}

}  // namespace

}  // namespace web_package
