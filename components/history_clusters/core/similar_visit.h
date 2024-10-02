// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_SIMILAR_VISIT_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_SIMILAR_VISIT_H_

#include <string>

#include "components/history/core/browser/history_types.h"

namespace history_clusters {

// Represents a visit that can be used for deduping.
struct SimilarVisit {
  SimilarVisit() = default;
  explicit SimilarVisit(const history::ClusterVisit& visit)
      : title(visit.annotated_visit.url_row.title()),
        // TODO(sophiechang): Remove this duplication once the underlying
        // persisted change is rolled out in Stable for at least 90 days
        // (probably around 07/2023).
        url_for_deduping(visit.annotated_visit.content_annotations
                                 .search_normalized_url.is_empty()
                             ? visit.url_for_deduping.spec()
                             : visit.annotated_visit.content_annotations
                                   .search_normalized_url.spec()) {}
  SimilarVisit(const SimilarVisit&) = default;
  ~SimilarVisit() = default;

  std::u16string title;
  std::string url_for_deduping;

  struct Comp {
    bool operator()(const SimilarVisit& lhs, const SimilarVisit& rhs) const {
      if (lhs.title != rhs.title)
        return lhs.title < rhs.title;
      return lhs.url_for_deduping < rhs.url_for_deduping;
    }
  };
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_SIMILAR_VISIT_H_
