// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_UI_QUERY_CLUSTERS_STATE_H_
#define COMPONENTS_HISTORY_CLUSTERS_UI_QUERY_CLUSTERS_STATE_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace history_clusters {

class HistoryClustersService;
class HistoryClustersServiceTask;

using LabelCount = std::pair<std::u16string, size_t>;

// This object encapsulates the results of a query to HistoryClustersService.
// It manages fetching more pages from the clustering backend as the user
// scrolls down.
//
// In the future, it will also manage reusing results for new searches, as well
// as collapsing duplicate clusters across pages.
//
// It's the history_clusters equivalent to history::QueryHistoryState.
class QueryClustersState {
 public:
  // `is_continuation` is true for all 'next-page' responses, but false for the
  // first page.
  using ResultCallback =
      base::OnceCallback<void(const std::string& query,
                              std::vector<history::Cluster> cluster_batch,
                              bool can_load_more,
                              bool is_continuation)>;

  QueryClustersState(base::WeakPtr<HistoryClustersService> service,
                     const std::string& query,
                     bool recluster = false);
  ~QueryClustersState();

  QueryClustersState(const QueryClustersState&) = delete;

  // Returns the current query the state contains.
  const std::string& query() const { return query_; }

  size_t number_clusters_sent_to_page() const {
    return number_clusters_sent_to_page_;
  }

  // Used to request another batch of clusters of the same query.
  void LoadNextBatchOfClusters(ResultCallback callback);

  // The list of raw labels in the same order as the clusters are ordered
  // alongside the number of occurrences so far. The counts can be fetched by
  // inputting the labels into the map as keys - but note, this only counts the
  // number of label instances seen SO FAR, not necessarily in all of History.
  const std::vector<LabelCount>& raw_label_counts_so_far() {
    return raw_label_counts_so_far_;
  }

 private:
  friend class QueryClustersStateTest;

  // Private class containing state that's only accessed on
  // `post_processing_task_runner`.
  class PostProcessor;

  // Callback to `LoadNextBatchOfClusters()`.
  void OnGotRawClusters(
      base::TimeTicks query_start_time,
      ResultCallback callback,
      std::vector<history::Cluster> clusters,
      QueryClustersContinuationParams continuation_params) const;

  // Callback to `OnGotRawClusters()`.
  void OnGotClusters(base::ElapsedTimer post_processing_timer,
                     size_t clusters_from_backend_count,
                     base::TimeTicks query_start_time,
                     ResultCallback callback,
                     QueryClustersContinuationParams continuation_params,
                     std::vector<history::Cluster> clusters);

  // Updates the internal state of raw labels for this next batch of `clusters`.
  void UpdateUniqueRawLabels(const std::vector<history::Cluster>& clusters);

  // Weak pointers to services we may outlive. Never nullptr except in tests.
  const base::WeakPtr<HistoryClustersService> service_;

  // The string query the user entered into the searchbox.
  const std::string query_;

  // The filter params to use for `query_`.
  const QueryClustersFilterParams filter_params_;

  // If true, forces reclustering as if `persist_clusters_in_history_db` were
  // false.
  bool recluster_;

  // The de-duplicated list of raw labels we've seen so far and their number of
  // occurrences, in the same order as the clusters themselves were provided.
  // This is only computed if `query` is empty. For non-empty `query`, this will
  // be an empty list.
  std::vector<LabelCount> raw_label_counts_so_far_;

  // The continuation params used to track where the last query left off and
  // query for the "next page".
  QueryClustersContinuationParams continuation_params_;

  // The number of clusters that have already been sent to the page. This is
  // updated AFTER the callback for each batch.
  size_t number_clusters_sent_to_page_ = 0;

  // Used only to fast-cancel tasks in case we are destroyed.
  std::unique_ptr<HistoryClustersServiceTask> query_clusters_task_;

  // A task runner to run all the post-processing tasks on.
  scoped_refptr<base::SequencedTaskRunner> post_processing_task_runner_;

  // The private state used for post-processing. It's created on the main
  // thread, but used and freed on `post_processing_task_runner`.
  scoped_refptr<PostProcessor> post_processing_state_;

  base::WeakPtrFactory<QueryClustersState> weak_factory_{this};
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_UI_QUERY_CLUSTERS_STATE_H_
