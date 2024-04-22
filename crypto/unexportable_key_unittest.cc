// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/unexportable_key.h"

#include <tuple>

#include "base/logging.h"
#include "base/time/time.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

const crypto::SignatureVerifier::SignatureAlgorithm kAllAlgorithms[] = {
    crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
    crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256,
};

class UnexportableKeySigningTest
    : public testing::TestWithParam<
          std::tuple<crypto::SignatureVerifier::SignatureAlgorithm, bool>> {};

INSTANTIATE_TEST_SUITE_P(All,
                         UnexportableKeySigningTest,
                         testing::Combine(testing::ValuesIn(kAllAlgorithms),
                                          testing::Bool()));

TEST_P(UnexportableKeySigningTest, RoundTrip) {
  const crypto::SignatureVerifier::SignatureAlgorithm algo =
      std::get<0>(GetParam());
  const bool mock_enabled = std::get<1>(GetParam());

  switch (algo) {
    case crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256:
      LOG(INFO) << "ECDSA P-256, mock=" << mock_enabled;
      break;
    case crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256:
      LOG(INFO) << "RSA, mock=" << mock_enabled;
      break;
    default:
      ASSERT_TRUE(false);
  }

  SCOPED_TRACE(static_cast<int>(algo));
  SCOPED_TRACE(mock_enabled);

  absl::optional<crypto::ScopedMockUnexportableKeyProvider> mock;
  if (mock_enabled) {
    mock.emplace();
  }

  const crypto::SignatureVerifier::SignatureAlgorithm algorithms[] = {algo};

  std::unique_ptr<crypto::UnexportableKeyProvider> provider =
      crypto::GetUnexportableKeyProvider();
  if (!provider) {
    LOG(INFO) << "Skipping test because of lack of hardware support.";
    return;
  }

  if (!provider->SelectAlgorithm(algorithms)) {
    LOG(INFO) << "Skipping test because of lack of support for this key type.";
    return;
  }

  const base::TimeTicks generate_start = base::TimeTicks::Now();
  std::unique_ptr<crypto::UnexportableSigningKey> key =
      provider->GenerateSigningKeySlowly(algorithms);
  ASSERT_TRUE(key);
  LOG(INFO) << "Generation took " << (base::TimeTicks::Now() - generate_start);

  ASSERT_EQ(key->Algorithm(), algo);
  const std::vector<uint8_t> wrapped = key->GetWrappedKey();
  const std::vector<uint8_t> spki = key->GetSubjectPublicKeyInfo();
  const uint8_t msg[] = {1, 2, 3, 4};

  const base::TimeTicks sign_start = base::TimeTicks::Now();
  const absl::optional<std::vector<uint8_t>> sig = key->SignSlowly(msg);
  LOG(INFO) << "Signing took " << (base::TimeTicks::Now() - sign_start);
  ASSERT_TRUE(sig);

  crypto::SignatureVerifier verifier;
  ASSERT_TRUE(verifier.VerifyInit(algo, *sig, spki));
  verifier.VerifyUpdate(msg);
  ASSERT_TRUE(verifier.VerifyFinal());

  const base::TimeTicks import2_start = base::TimeTicks::Now();
  std::unique_ptr<crypto::UnexportableSigningKey> key2 =
      provider->FromWrappedSigningKeySlowly(wrapped);
  ASSERT_TRUE(key2);
  LOG(INFO) << "Import took " << (base::TimeTicks::Now() - import2_start);

  const base::TimeTicks sign2_start = base::TimeTicks::Now();
  const absl::optional<std::vector<uint8_t>> sig2 = key->SignSlowly(msg);
  LOG(INFO) << "Signing took " << (base::TimeTicks::Now() - sign2_start);
  ASSERT_TRUE(sig2);

  crypto::SignatureVerifier verifier2;
  ASSERT_TRUE(verifier2.VerifyInit(algo, *sig2, spki));
  verifier2.VerifyUpdate(msg);
  ASSERT_TRUE(verifier2.VerifyFinal());
}
}  // namespace
