// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/ranking/score_normalizing_ranker.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/ash/app_list/search/util/score_normalizer.pb.h"

namespace app_list {
namespace {

// Returns true if results from this provider should not have their result
// scores normalized. This is to prevent creating an unnecessary number of
// normalizers.
bool ShouldIgnoreProvider(ProviderType provider) {
  switch (provider) {
      // Deprecated types:
    case ProviderType::kLauncher:
    case ProviderType::kAnswerCard:
      // Types that only ever create one result:
    case ProviderType::kPlayStoreReinstallApp:
      // Internal types:
    case ProviderType::kUnknown:
    case ProviderType::kInternalPrivacyInfo:
      return true;
    default:
      return false;
  }
}

std::string ProviderToString(ProviderType provider) {
  return base::NumberToString(static_cast<int>(provider));
}

}  // namespace

ScoreNormalizingRanker::ScoreNormalizingRanker(
    ScoreNormalizer::Params params,
    PersistentProto<ScoreNormalizerProto> proto)
    : normalizer_(std::move(proto), params) {}

ScoreNormalizingRanker::~ScoreNormalizingRanker() {}

void ScoreNormalizingRanker::UpdateResultRanks(ResultsMap& results,
                                               ProviderType provider) {
  if (ShouldIgnoreProvider(provider))
    return;

  auto it = results.find(provider);
  DCHECK(it != results.end());

  std::string provider_string = ProviderToString(provider);
  for (auto& result : it->second) {
    normalizer_.Update(provider_string, result->relevance());
  }

  for (auto& result : it->second) {
    result->scoring().set_normalized_relevance(
        normalizer_.Normalize(provider_string, result->relevance()));
  }
}

}  // namespace app_list
