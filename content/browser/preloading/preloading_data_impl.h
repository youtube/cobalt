// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRELOADING_DATA_IMPL_H_
#define CONTENT_BROWSER_PRELOADING_PRELOADING_DATA_IMPL_H_

#include <memory>
#include <tuple>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/preloading_data.h"

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace content {

class PreloadingAttemptImpl;
class PreloadingPrediction;
class ExperimentalPreloadingPrediction;
class PrefetchDocumentManager;

// Defines predictors confusion matrix enums used by UMA records. Entries should
// not be renumbered and numeric values should never be reused. Please update
// "PredictorConfusionMatrix" in `tools/metrics/histograms/enums.xml` when new
// enums are added.
enum class PredictorConfusionMatrix {
  // True positive.
  kTruePositive = 0,
  // False positive.
  kFalsePositive = 1,
  // True negative.
  kTrueNegative = 2,
  // False negative.
  kFalseNegative = 3,
  // Required by UMA histogram macro.
  kMaxValue = kFalseNegative
};

// The scope of current preloading logging is only limited to the same
// WebContents navigations. If the predicted URL is opened in a new tab we lose
// the data corresponding to the navigation in different WebContents.
// TODO(crbug.com/1332123): Expand PreloadingData scope to consider multiple
// WebContent navigations.
class CONTENT_EXPORT PreloadingDataImpl
    : public PreloadingData,
      public WebContentsUserData<PreloadingDataImpl>,
      public WebContentsObserver {
 public:
  ~PreloadingDataImpl() override;

  static PreloadingDataImpl* GetOrCreateForWebContents(
      WebContents* web_contents);

  // NoVarySearch is a `/content/browser` feature so is the matcher getter.
  // The matcher first checks if `destination_url` is the same as the
  // prediction; if not, the matcher checks if the `destination_url` matches
  // any NoVarySearch query using `NoVarySearchHelper`.
  static PreloadingURLMatchCallback GetSameURLAndNoVarySearchURLMatcher(
      base::WeakPtr<PrefetchDocumentManager> manager,
      const GURL& destination_url);

  // Disallow copy and assign.
  PreloadingDataImpl(const PreloadingDataImpl& other) = delete;
  PreloadingDataImpl& operator=(const PreloadingDataImpl& other) = delete;

  // PreloadingDataImpl implementation:
  PreloadingAttempt* AddPreloadingAttempt(
      PreloadingPredictor predictor,
      PreloadingType preloading_type,
      PreloadingURLMatchCallback url_match_predicate,
      ukm::SourceId triggering_primary_page_source_id) override;
  void AddPreloadingPrediction(
      PreloadingPredictor predictor,
      int64_t confidence,
      PreloadingURLMatchCallback url_match_predicate) override;
  void SetIsNavigationInDomainCallback(
      PreloadingPredictor predictor,
      PredictorDomainCallback is_navigation_in_domain_callback) override;
  bool CheckNavigationInDomainCallbackForTesting(
      PreloadingPredictor predictor) {
    return is_navigation_in_predictor_domain_callbacks_.count(predictor);
  }

  // The output of many predictors is a score (usually probability or logit),
  // where a higher score indicates a higher confidence in the prediction. To
  // use this score for binary classification, we compare it to a threshold. If
  // the score is above the threshold, we classify the instance as positive;
  // otherwise, we classify it as negative. Threshold choice affects classifier
  // precision and recall. There is a trade-off between precision and recall. If
  // we set the threshold too low, we will have high precision but low recall.
  // If we set the threshold too high, we will have high recall but low
  // precision. To choose the best threshold, we can use ROC curves,
  // precision-recall curves, or precision and recall curves.
  // `ExperimentalPreloadingPrediction` helps us collect the UMA data required
  // to achieve this. This method creates a new
  // ExperimentalPreloadingPrediction. Same as above `url_match_predicate` is
  // passed by the caller to verify that navigated URL is a match. The `score`
  // is the probability/logit score, `min_score`  and `max_score` are the
  // minimum/maximum values that the `score` can have and `buckets` is the
  // number of buckets that will be used for UMA aggregation and should be less
  // than 101.
  void AddExperimentalPreloadingPrediction(
      base::StringPiece name,
      PreloadingURLMatchCallback url_match_predicate,
      float score,
      float min_score,
      float max_score,
      size_t buckets);

  // WebContentsObserver override.
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

 private:
  explicit PreloadingDataImpl(WebContents* web_contents);
  friend class WebContentsUserData<PreloadingDataImpl>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  void RecordMetricsForPreloadingAttempts(
      ukm::SourceId navigated_page_source_id);
  void RecordUKMForPreloadingPredictions(
      ukm::SourceId navigated_page_source_id);
  void SetIsAccurateTriggeringAndPrediction(const GURL& navigated_url);

  void RecordPreloadingAttemptPrecisionToUMA(
      const PreloadingAttemptImpl& attempt);
  void RecordPredictionPrecisionToUMA(const PreloadingPrediction& prediction);

  void UpdatePreloadingAttemptRecallStats(const PreloadingAttemptImpl& attempt);
  void UpdatePredictionRecallStats(const PreloadingPrediction& prediction);

  void ResetRecallStats();
  void RecordRecallStatsToUMA(NavigationHandle* navigation_handle);

  // Stores recall statistics for preloading predictions/attempts to later
  // record them to UMA.
  struct PreloadingPredictorLess {
    bool operator()(const PreloadingPredictor& lhs,
                    const PreloadingPredictor& rhs) const {
      return lhs.ukm_value() < rhs.ukm_value();
    }
  };
  struct PreloadingAttemptLess {
    bool operator()(
        const std::pair<PreloadingPredictor, PreloadingType>& lhs,
        const std::pair<PreloadingPredictor, PreloadingType>& rhs) const {
      return std::forward_as_tuple(lhs.first.ukm_value(), lhs.second) <
             std::forward_as_tuple(rhs.first.ukm_value(), rhs.second);
    }
  };

  base::flat_map<PreloadingPredictor,
                 PredictorDomainCallback,
                 PreloadingPredictorLess>
      is_navigation_in_predictor_domain_callbacks_;
  base::flat_set<PreloadingPredictor, PreloadingPredictorLess>
      predictions_recall_stats_;
  base::flat_set<std::pair<PreloadingPredictor, PreloadingType>,
                 PreloadingAttemptLess>
      preloading_attempt_recall_stats_;

  // Stores all the experimental preloading predictions that are happening for
  // the next navigation until the navigation takes place or the WebContents is
  // destroyed.
  std::vector<std::unique_ptr<ExperimentalPreloadingPrediction>>
      experimental_predictions_;

  // Stores all the preloading attempts that are happening for the next
  // navigation until the navigation takes place.
  std::vector<std::unique_ptr<PreloadingAttemptImpl>> preloading_attempts_;

  // Stores all the preloading predictions that are happening for the next
  // navigation until the navigation takes place.
  std::vector<std::unique_ptr<PreloadingPrediction>> preloading_predictions_;

  // The random seed used to determine if a preloading attempt should be sampled
  // in UKM logs. We use a different random seed for each session and then hash
  // that seed with the UKM source ID so that all attempts for a given source ID
  // are sampled in or out together.
  uint32_t sampling_seed_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRELOADING_DATA_IMPL_H_
