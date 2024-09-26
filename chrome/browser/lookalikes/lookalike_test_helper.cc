// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/lookalike_test_helper.h"

#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/lookalikes/core/safety_tip_test_utils.h"
#include "components/lookalikes/core/safety_tips_config.h"
#include "components/url_formatter/spoof_checks/idn_spoof_checker.h"
#include "components/url_formatter/spoof_checks/top_domains/test_top500_domains.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace test {
#include "components/url_formatter/spoof_checks/top_domains/browsertest_domains-trie-inc.cc"
}

LookalikeTestHelper::LookalikeTestHelper(ukm::TestUkmRecorder* ukm_recorder)
    : ukm_recorder_(ukm_recorder) {}

// static
void LookalikeTestHelper::SetUpLookalikeTestParams() {
  // Use test top domain lists instead of the actual list.
  url_formatter::IDNSpoofChecker::HuffmanTrieParams trie_params{
      test::kTopDomainsHuffmanTree, sizeof(test::kTopDomainsHuffmanTree),
      test::kTopDomainsTrie, test::kTopDomainsTrieBits,
      test::kTopDomainsRootPosition};
  url_formatter::IDNSpoofChecker::SetTrieParamsForTesting(trie_params);

  // Use test top 500 domain skeletons instead of the actual list.
  lookalikes::Top500DomainsParams top500_params{
      test_top500_domains::kTop500EditDistanceSkeletons,
      test_top500_domains::kNumTop500EditDistanceSkeletons};
  lookalikes::SetTop500DomainsParamsForTesting(top500_params);

  lookalikes::InitializeSafetyTipConfig();
}

// static
void LookalikeTestHelper::TearDownLookalikeTestParams() {
  url_formatter::IDNSpoofChecker::RestoreTrieParamsForTesting();
  lookalikes::ResetTop500DomainsParamsForTesting();
}

void LookalikeTestHelper::CheckSafetyTipUkmCount(
    size_t expected_event_count) const {
  std::vector<const ukm::mojom::UkmEntry*> entries =
      ukm_recorder_->GetEntriesByName(
          ukm::builders::Security_SafetyTip::kEntryName);
  ASSERT_EQ(expected_event_count, entries.size());
}

void LookalikeTestHelper::CheckInterstitialUkmCount(
    size_t expected_event_count) const {
  std::vector<const ukm::mojom::UkmEntry*> entries =
      ukm_recorder_->GetEntriesByName(
          ukm::builders::LookalikeUrl_NavigationSuggestion::kEntryName);
  ASSERT_EQ(expected_event_count, entries.size());
}

void LookalikeTestHelper::CheckNoLookalikeUkm() const {
  CheckSafetyTipUkmCount(0);
  CheckInterstitialUkmCount(0);
}
