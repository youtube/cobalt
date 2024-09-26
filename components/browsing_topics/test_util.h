// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_TOPICS_TEST_UTIL_H_
#define COMPONENTS_BROWSING_TOPICS_TEST_UTIL_H_

#include "base/containers/queue.h"

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/browsing_topics/browsing_topics_calculator.h"
#include "components/browsing_topics/browsing_topics_service.h"
#include "components/browsing_topics/mojom/browsing_topics_internals.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/browsing_topics/browsing_topics.mojom.h"

namespace ukm {
class TestAutoSetUkmRecorder;
}  // namespace ukm

namespace browsing_topics {

struct ApiResultUkmMetrics {
  ApiResultUkmMetrics(absl::optional<ApiAccessResult> failure_reason,
                      CandidateTopic topic0,
                      CandidateTopic topic1,
                      CandidateTopic topic2)
      : failure_reason(std::move(failure_reason)),
        topic0(std::move(topic0)),
        topic1(std::move(topic1)),
        topic2(std::move(topic2)) {}

  absl::optional<ApiAccessResult> failure_reason;
  CandidateTopic topic0;
  CandidateTopic topic1;
  CandidateTopic topic2;
};

// Parse the `BrowsingTopics_DocumentBrowsingTopicsApiResult2` metrics.
std::vector<ApiResultUkmMetrics> ReadApiResultUkmMetrics(
    const ukm::TestAutoSetUkmRecorder& ukm_recorder);

// Returns whether the URL entry is eligible in topics calculation.
// Precondition: the history visits contain exactly one matching URL.
bool BrowsingTopicsEligibleForURLVisit(history::HistoryService* history_service,
                                       const GURL& url);

// A tester class that allows mocking the generated random numbers, or directly
// returning a mock result with a delay.
class TesterBrowsingTopicsCalculator : public BrowsingTopicsCalculator {
 public:
  // Initialize a regular `BrowsingTopicsCalculator` with an additional
  // `rand_uint64_queue` member for generating random numbers.
  TesterBrowsingTopicsCalculator(
      privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
      history::HistoryService* history_service,
      content::BrowsingTopicsSiteDataManager* site_data_manager,
      optimization_guide::PageContentAnnotationsService* annotations_service,
      const base::circular_deque<EpochTopics>& epochs,
      CalculateCompletedCallback callback,
      base::queue<uint64_t> rand_uint64_queue);

  // Initialize a mock `BrowsingTopicsCalculator` (with mock result and delay).
  TesterBrowsingTopicsCalculator(
      privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
      history::HistoryService* history_service,
      content::BrowsingTopicsSiteDataManager* site_data_manager,
      optimization_guide::PageContentAnnotationsService* annotations_service,
      CalculateCompletedCallback callback,
      EpochTopics mock_result,
      base::TimeDelta mock_result_delay);

  ~TesterBrowsingTopicsCalculator() override;

  TesterBrowsingTopicsCalculator(const TesterBrowsingTopicsCalculator&) =
      delete;
  TesterBrowsingTopicsCalculator& operator=(
      const TesterBrowsingTopicsCalculator&) = delete;
  TesterBrowsingTopicsCalculator(TesterBrowsingTopicsCalculator&&) = delete;
  TesterBrowsingTopicsCalculator& operator=(TesterBrowsingTopicsCalculator&&) =
      delete;

  // Pop and return the next number in `rand_uint64_queue_`. Precondition:
  // `rand_uint64_queue_` is not empty.
  uint64_t GenerateRandUint64() override;

  // If `use_mock_result_` is true, post a task with `mock_result_delay_` to
  // directly invoke the `finish_callback_` with `mock_result_`; otherwise, use
  // the default handling for `CheckCanCalculate`.
  void CheckCanCalculate() override;

 private:
  void MockDelayReached();

  base::queue<uint64_t> rand_uint64_queue_;

  bool use_mock_result_ = false;
  EpochTopics mock_result_{base::Time()};
  base::TimeDelta mock_result_delay_;
  CalculateCompletedCallback finish_callback_;

  base::WeakPtrFactory<TesterBrowsingTopicsCalculator> weak_ptr_factory_{this};
};

class MockBrowsingTopicsService : public BrowsingTopicsService {
 public:
  MockBrowsingTopicsService();
  ~MockBrowsingTopicsService() override;

  MOCK_METHOD(bool,
              HandleTopicsWebApi,
              (const url::Origin&,
               content::RenderFrameHost*,
               ApiCallerSource,
               bool,
               bool,
               std::vector<blink::mojom::EpochTopicPtr>&),
              (override));
  MOCK_METHOD(void,
              GetBrowsingTopicsStateForWebUi,
              (bool, mojom::PageHandler::GetBrowsingTopicsStateCallback),
              (override));
  MOCK_METHOD(std::vector<privacy_sandbox::CanonicalTopic>,
              GetTopTopicsForDisplay,
              (),
              (const override));
  MOCK_METHOD(void,
              ClearTopic,
              (const privacy_sandbox::CanonicalTopic&),
              (override));
  MOCK_METHOD(void, ClearTopicsDataForOrigin, (const url::Origin&), (override));
  MOCK_METHOD(void, ClearAllTopicsData, (), (override));
};

}  // namespace browsing_topics

#endif  // COMPONENTS_BROWSING_TOPICS_TEST_UTIL_H_
