// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/transformer/recency_filter_transformer.h"

#include <algorithm>
#include <vector>

#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "components/visited_url_ranking/public/features.h"
#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/url_visit.h"

namespace visited_url_ranking {

namespace {

// Returns true if the visit should be discarded from candidates based on
// `fetch_options`.
bool ShouldDiscardVisit(const URLVisitAggregate& visit,
                        base::Time current_time,
                        const FetchOptions& options) {
  URLVisitAggregate::URLTypeSet types = visit.GetURLTypes();
  bool should_discard = true;
  for (URLVisitAggregate::URLType current_url_type : types) {
    auto it = options.result_sources.find(current_url_type);
    if (it == options.result_sources.end()) {
      continue;
    }

    if (current_time - visit.GetLastVisitTime() <= it->second.age_limit) {
      VLOG(2) << "RecencyFilterTransformer: retained candidate "
              << visit.url_key
              << " type: " << static_cast<int>(current_url_type)
              << " since age "
              << (current_time - visit.GetLastVisitTime()).InHours()
              << " within limit " << it->second.age_limit.InHours();
      // Not early-exiting so everything can be logged.
      should_discard = false;
    }
  }
  return should_discard;
}

}  // namespace

RecencyFilterTransformer::RecencyFilterTransformer()
    : aggregate_count_limit_(base::GetFieldTrialParamByFeatureAsInt(
          features::kVisitedURLRankingService,
          features::kURLAggregateCountLimit,
          features::kURLAggregateCountLimitDefaultValue)) {}

RecencyFilterTransformer::~RecencyFilterTransformer() = default;

void RecencyFilterTransformer::Transform(
    std::vector<URLVisitAggregate> aggregates,
    const FetchOptions& options,
    OnTransformCallback callback) {
  base::Time now = base::Time::Now();
  std::erase_if(aggregates, [&](const auto& url_visit_aggregate) {
    return ShouldDiscardVisit(url_visit_aggregate, now, options);
  });

  std::sort(aggregates.begin(), aggregates.end(),
            [](const URLVisitAggregate& a, const URLVisitAggregate& b) {
              return a.GetLastVisitTime() > b.GetLastVisitTime();
            });
  if (aggregates.size() > aggregate_count_limit_) {
    std::vector<URLVisitAggregate> copy;
    for (size_t i = 0; i < aggregate_count_limit_; ++i) {
      copy.emplace_back(std::move(aggregates[i]));
    }
    aggregates.swap(copy);
  }

  std::move(callback).Run(Status::kSuccess, std::move(aggregates));
}

}  // namespace visited_url_ranking
