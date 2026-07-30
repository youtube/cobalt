// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_FTRL_RANKER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_FTRL_RANKER_H_

#include "chrome/browser/ash/app_list/search/ranking/ranker.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "chrome/browser/ash/app_list/search/util/ftrl_optimizer.h"

namespace app_list {

// A ranker for search results using a Follow the Regularized Leader algorithm.
// This learns weightings for the 'experts' below.
class FtrlRanker : public Ranker {
 public:
  FtrlRanker(FtrlOptimizer::Params params, FtrlOptimizer::Proto proto);
  ~FtrlRanker() override;

  FtrlRanker(const FtrlRanker&) = delete;
  FtrlRanker& operator=(const FtrlRanker&) = delete;

  void AddExpert(std::unique_ptr<Ranker> ranker);

  // Ranker:
  void Start(const std::u16string& query,
             const CategoriesList& categories) override;
  void Train(const LaunchData& launch) override;
  void UpdateResultRanks(ResultsMap& results, ProviderType provider) override;

 private:
  // The Follow the Regularized Leader instance that chooses amongst the expert
  // |rankers_|.
  std::unique_ptr<FtrlOptimizer> ftrl_;

  // The 'experts' in the follow-the-regularized-leader model.
  std::vector<std::unique_ptr<Ranker>> rankers_;
};

// The following classes are 'experts', ie. sub-rankers to be used within the
// FtrlRanker.

// An expert that exposes a score from each result's scoring struct.
class ResultScoringShim : public Ranker {
 public:
  // Correspond to the members of a search result's `Scoring`.
  enum class ScoringMember {
    kNormalizedRelevance,
    kMrfuResultScore,
  };

  explicit ResultScoringShim(ScoringMember member);

  // Ranker:
  std::vector<double> GetResultRanks(const ResultsMap& results,
                                     ProviderType provider) override;

 private:
  ScoringMember member_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_FTRL_RANKER_H_
