// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_PROCESS_METRICS_DECORATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_PROCESS_METRICS_DECORATOR_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/node_data_describer.h"

namespace memory_instrumentation {
class GlobalMemoryDump;
}

namespace performance_manager {

class SystemNode;

// The ProcessMetricsDecorator is responsible for adorning process nodes with
// performance metrics.
class ProcessMetricsDecorator
    : public GraphOwned,
      public GraphRegisteredImpl<ProcessMetricsDecorator>,
      public NodeDataDescriberDefaultImpl {
 public:
  ProcessMetricsDecorator();

  ProcessMetricsDecorator(const ProcessMetricsDecorator&) = delete;
  ProcessMetricsDecorator& operator=(const ProcessMetricsDecorator&) = delete;

  ~ProcessMetricsDecorator() override;

  // A token used to express an interest for process metrics. Process metrics
  // will only be updated as long as there's at least one token in existence.
  //
  // These objects shouldn't be created directly, they should be acquired by
  // calling RegisterInterestForProcessMetrics.
  class ScopedMetricsInterestToken {
   public:
    ScopedMetricsInterestToken(const ScopedMetricsInterestToken& other) =
        delete;
    ScopedMetricsInterestToken& operator=(const ScopedMetricsInterestToken&) =
        delete;
    virtual ~ScopedMetricsInterestToken() = default;

   protected:
    ScopedMetricsInterestToken() = default;
  };

  // Allows a process to register an interest for process metrics. Metrics are
  // only guaranteed to be refreshed for the lifetime of the returned token.
  static std::unique_ptr<ScopedMetricsInterestToken>
  RegisterInterestForProcessMetrics(Graph* graph);

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // NodeDataDescriber
  base::Value::Dict DescribeSystemNodeData(
      const SystemNode* node) const override;

  void SetGraphForTesting(Graph* graph) { graph_ = graph; }
  bool IsTimerRunningForTesting() const { return refresh_timer_.IsRunning(); }

  base::TimeDelta GetTimerDelayForTesting() const {
    return refresh_timer_.GetCurrentDelay();
  }

  void RefreshMetricsForTesting();

 protected:
  class ScopedMetricsInterestTokenImpl;

  // Starts/Stop the timer responsible for refreshing the process nodes metrics.
  void StartTimer();
  void StopTimer();

  // Asynchronously refreshes the metrics for all the process nodes.
  void RefreshMetrics();

  // Query the MemoryInstrumentation service to get the memory metrics for all
  // processes and run |callback| with the result. Virtual to make a test seam.
  virtual void RequestProcessesMemoryMetrics(
      base::OnceCallback<
          void(bool success,
               std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump)>
          callback);

  // Function that should be used as a callback to
  // MemoryInstrumentation::RequestPrivateMemoryFootprint. |success| will
  // indicate if the data has been retrieved successfully and |process_dumps|
  // will contain the data for all the Chrome processes for which this data was
  // available.
  void DidGetMemoryUsage(
      bool success,
      std::unique_ptr<memory_instrumentation::GlobalMemoryDump> process_dumps);

  // Called whenever a ScopedMetricsInterestToken is created/released.
  void OnMetricsInterestTokenCreated();
  void OnMetricsInterestTokenReleased();

 private:
  // The timer responsible for refreshing the metrics.
  base::OneShotTimer refresh_timer_;

  // The Graph instance owning this decorator.
  raw_ptr<Graph> graph_;

  // The number of clients currently interested by the metrics tracked by this
  // class.
  size_t metrics_interest_token_count_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ProcessMetricsDecorator> weak_factory_{this};
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_PROCESS_METRICS_DECORATOR_H_
