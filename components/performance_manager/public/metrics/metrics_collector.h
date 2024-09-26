// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_METRICS_METRICS_COLLECTOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_METRICS_METRICS_COLLECTOR_H_

#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/metrics/background_metrics_reporter.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace performance_manager {

extern const char kTabFromBackgroundedToFirstFaviconUpdatedUMA[];
extern const char kTabFromBackgroundedToFirstTitleUpdatedUMA[];
extern const char
    kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA[];
extern const base::TimeDelta kMetricsReportDelayTimeout;
extern const int kDefaultFrequencyUkmEQTReported;

// The MetricsCollector is a graph observer that reports UMA/UKM.
class MetricsCollector : public FrameNode::ObserverDefaultImpl,
                         public GraphOwned,
                         public PageNode::ObserverDefaultImpl,
                         public ProcessNode::ObserverDefaultImpl {
 public:
  MetricsCollector();

  MetricsCollector(const MetricsCollector&) = delete;
  MetricsCollector& operator=(const MetricsCollector&) = delete;

  ~MetricsCollector() override;

  // FrameNodeObserver implementation:
  void OnNonPersistentNotificationCreated(const FrameNode* frame_node) override;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // PageNodeObserver implementation:
  void OnIsVisibleChanged(const PageNode* page_node) override;
  void OnUkmSourceIdChanged(const PageNode* page_node) override;
  void OnFaviconUpdated(const PageNode* page_node) override;
  void OnTitleUpdated(const PageNode* page_node) override;
  void OnMainFrameDocumentChanged(const PageNode* page_node) override;

  // ProcessNodeObserver implementation:
  void OnProcessLifetimeChange(const ProcessNode* process_node) override;
  void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) override;

 protected:
  friend class MetricsReportRecordHolder;
  friend class UkmCollectionStateHolder;

  struct MetricsReportRecord {
    MetricsReportRecord();
    MetricsReportRecord(const MetricsReportRecord& other);
    void UpdateUkmSourceID(ukm::SourceId ukm_source_id);
    void Reset();
    BackgroundMetricsReporter<
        ukm::builders::TabManager_Background_FirstFaviconUpdated,
        kTabFromBackgroundedToFirstFaviconUpdatedUMA,
        internal::UKMFrameReportType::kMainFrameOnly>
        first_favicon_updated;
    BackgroundMetricsReporter<
        ukm::builders::
            TabManager_Background_FirstNonPersistentNotificationCreated,
        kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA,
        internal::UKMFrameReportType::kMainFrameAndChildFrame>
        first_non_persistent_notification_created;
    BackgroundMetricsReporter<
        ukm::builders::TabManager_Background_FirstTitleUpdated,
        kTabFromBackgroundedToFirstTitleUpdatedUMA,
        internal::UKMFrameReportType::kMainFrameOnly>
        first_title_updated;
    GURL previous_url;
  };

  struct UkmCollectionState {
    ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;
  };

 private:
  static MetricsReportRecord* GetMetricsReportRecord(const PageNode* page_node);
  static UkmCollectionState* GetUkmCollectionState(const PageNode* page_node);

  // (Un)registers the various node observer flavors of this object with the
  // graph. These are invoked by OnPassedToGraph and OnTakenFromGraph, but
  // hoisted to their own functions for testing.
  void RegisterObservers(Graph* graph);
  void UnregisterObservers(Graph* graph);

  bool ShouldReportMetrics(const PageNode* page_node);
  void UpdateUkmSourceIdForPage(const PageNode* page_node,
                                ukm::SourceId ukm_source_id);
  void ResetMetricsReportRecord(const PageNode* page_node);

  void OnProcessDestroyed(const ProcessNode* process_node);

  // The graph to which this object belongs.
  raw_ptr<Graph> graph_ = nullptr;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_METRICS_METRICS_COLLECTOR_H_
