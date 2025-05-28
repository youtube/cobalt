// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/segment_scorer.h"

#include <limits.h>
#include <math.h>

#include <algorithm>
#include <utility>

#include "base/features.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/history/core/browser/features.h"

namespace history {

/******** SegmentScorer::RecencyFactor ********/

// static
std::unique_ptr<SegmentScorer::RecencyFactor>
SegmentScorer::RecencyFactor::CreateFromFeatureFlags() {
  std::string recency_factor_name =
      base::FeatureParam<std::string>(&history::kMostVisitedTilesNewScoring,
                                      kMvtScoringParamRecencyFactor,
                                      kMvtScoringParamRecencyFactor_Default)
          .Get();

  if (recency_factor_name == kMvtScoringParamRecencyFactor_Decay) {
    double decay_per_day = std::clamp(
        base::FeatureParam<double>(&history::kMostVisitedTilesNewScoring,
                                   kMvtScoringParamDecayPerDay, 1.0)
            .Get(),
        1E-10, 1.0);
    return std::make_unique<RecencyFactorDecay>(decay_per_day);
  }

  if (recency_factor_name == kMvtScoringParamRecencyFactor_DecayStaircase) {
    return std::make_unique<RecencyFactorDecayStaircase>();
  }

  if (recency_factor_name != kMvtScoringParamRecencyFactor_Default) {
    DVLOG(1) << "Unknown recency factor name " << recency_factor_name;
  }
  return std::make_unique<RecencyFactorDefault>();
}

SegmentScorer::RecencyFactor::~RecencyFactor() = default;

/******** SegmentScorer::RecencyFactorDefault ********/

SegmentScorer::RecencyFactorDefault::~RecencyFactorDefault() = default;

// Computes a smooth function that boosts today's visits by 3x, week-ago visits
// by 2x, 3-week-ago visits by 1.5x, falling off to 1x asymptotically.
float SegmentScorer::RecencyFactorDefault::Compute(int days_ago) {
  return 1.0f + (2.0f * (1.0f / (1.0f + days_ago / 7.0f)));
}

/******** SegmentScorer::RecencyFactorDecay ********/

SegmentScorer::RecencyFactorDecay::RecencyFactorDecay(double decay_per_day)
    : decay_per_day_(decay_per_day) {
  DCHECK_GT(decay_per_day_, 0);
  DCHECK_LE(decay_per_day_, 1);
}

SegmentScorer::RecencyFactorDecay::~RecencyFactorDecay() = default;

// Computes an exponential decay.
float SegmentScorer::RecencyFactorDecay::Compute(int days_ago) {
  return ::pow(decay_per_day_, days_ago);
}

/******** SegmentScorer::RecencyFactorDecayStaircase ********/

SegmentScorer::RecencyFactorDecayStaircase::~RecencyFactorDecayStaircase() =
    default;

// Computes an exponential decay over the past two weeks. Thereafter, the factor
// is a staircase function decreasing across 3 ranges.
float SegmentScorer::RecencyFactorDecayStaircase::Compute(int days_ago) {
  if (days_ago <= 14) {
    return ::exp(-days_ago / 15.0f) / 1.5f;
  }
  return (days_ago <= 45) ? 0.2f : ((days_ago <= 70) ? 0.1f : 0.05f);
}

/******** SegmentScorer ********/

// static
std::unique_ptr<SegmentScorer> SegmentScorer::CreateFromFeatureFlags() {
  std::unique_ptr<SegmentScorer::RecencyFactor> recency_factor =
      SegmentScorer::RecencyFactor::CreateFromFeatureFlags();

  int daily_visit_count_cap =
      base::FeatureParam<int>(&history::kMostVisitedTilesNewScoring,
                              kMvtScoringParamDailyVisitCountCap, INT_MAX)
          .Get();
  return base::WrapUnique(
      new SegmentScorer(std::move(recency_factor), daily_visit_count_cap));
}

SegmentScorer::SegmentScorer(
    std::unique_ptr<SegmentScorer::RecencyFactor> recency_factor,
    int daily_visit_count_cap)
    : recency_factor_(std::move(recency_factor)),
      daily_visit_count_cap_(daily_visit_count_cap) {}

SegmentScorer::~SegmentScorer() = default;

float SegmentScorer::Compute(const std::vector<base::Time>& time_slots,
                             const std::vector<int>& visit_counts,
                             base::Time now) const {
  size_t n = time_slots.size();
  CHECK_EQ(n, visit_counts.size());
  float score = 0.0f;

  for (size_t i = 0; i < n; ++i) {
    base::Time time_slot = time_slots[i];
    int visit_count = std::min(visit_counts[i], daily_visit_count_cap_);
    // Score for this day in isolation.
    float day_visits_score = (visit_count <= 0.0f)
                                 ? 0.0f
                                 : 1.0f + log(static_cast<float>(visit_count));
    // Recent visits count more than historical ones, so multiply a
    // recency factor related to how long ago this day was.
    int days_ago = (now - time_slot).InDays();
    score += recency_factor_->Compute(days_ago) * day_visits_score;
  }
  return score;
}

}  // namespace history
