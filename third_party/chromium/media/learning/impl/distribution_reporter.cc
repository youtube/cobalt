// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/distribution_reporter.h"

#include "base/bind.h"
#include "base/cxx17_backports.h"
#include "base/metrics/histogram_functions.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace media {
namespace learning {

// UMA histogram base names.
static const char* kAggregateBase = "Media.Learning.BinaryThreshold.Aggregate.";
static const char* kByTrainingWeightBase =
    "Media.Learning.BinaryThreshold.ByTrainingWeight.";
static const char* kByFeatureBase = "Media.Learning.BinaryThreshold.ByFeature.";

enum /* not class */ Bits {
  // These are meant to be bitwise-or'd together, so both false cases just mean
  // "don't set any bits".
  PredictedFalse = 0x00,
  ObservedFalse = 0x00,
  ObservedTrue = 0x01,
  PredictedTrue = 0x02,
  // Special value to mean that no prediction was made.
  PredictedNothing = 0x04,
};

// Low order bit is "observed", second bit is "predicted", third bit is
// "could not make a prediction".
enum class ConfusionMatrix {
  TrueNegative = Bits::PredictedFalse | Bits::ObservedFalse,
  FalseNegative = Bits::PredictedFalse | Bits::ObservedTrue,
  FalsePositive = Bits::PredictedTrue | Bits::ObservedFalse,
  TruePositive = Bits::PredictedTrue | Bits::ObservedTrue,
  SkippedNegative = Bits::PredictedNothing | Bits::ObservedFalse,
  SkippedPositive = Bits::PredictedNothing | Bits::ObservedTrue,
  kMaxValue = SkippedPositive
};

// TODO(liberato): Currently, this implementation is a hack to collect some
// sanity-checking data for local learning with MediaCapabilities.  We assume
// that the prediction is the "percentage of dropped frames".
class UmaRegressionReporter : public DistributionReporter {
 public:
  UmaRegressionReporter(const LearningTask& task)
      : DistributionReporter(task) {}

  void OnPrediction(const PredictionInfo& info,
                    TargetHistogram predicted) override {
    DCHECK_EQ(task().target_description.ordering,
              LearningTask::Ordering::kNumeric);

    // As a complete hack, record accuracy with a fixed threshold.  The average
    // is the observed / predicted percentage of dropped frames.
    bool observed_smooth = info.observed.value() <= task().smoothness_threshold;

    // See if we made a prediction.
    int prediction_bit_mask = Bits::PredictedNothing;
    if (predicted.total_counts() != 0) {
      bool predicted_smooth =
          predicted.Average() <= task().smoothness_threshold;
      DVLOG(2) << "Learning: " << task().name
               << ": predicted: " << predicted_smooth << " ("
               << predicted.Average() << ") observed: " << observed_smooth
               << " (" << info.observed.value() << ")";
      prediction_bit_mask =
          predicted_smooth ? Bits::PredictedTrue : Bits::PredictedFalse;
    } else {
      DVLOG(2) << "Learning: " << task().name
               << ": predicted: N/A observed: " << observed_smooth << " ("
               << info.observed.value() << ")";
    }

    // Figure out the ConfusionMatrix enum value.
    ConfusionMatrix confusion_matrix_value = static_cast<ConfusionMatrix>(
        (observed_smooth ? Bits::ObservedTrue : Bits::ObservedFalse) |
        prediction_bit_mask);

    // |uma_bucket_number| is the bucket number that we'll fill in with this
    // count.  It ranges from 0 to |max_buckets-1|, inclusive.  Each bucket is
    // is separated from the start of the previous bucket by |uma_bucket_size|.
    int uma_bucket_number = 0;
    constexpr int matrix_size =
        static_cast<int>(ConfusionMatrix::kMaxValue) + 1;

    // The enum.xml entries separate the buckets by 10, to make it easy to see
    // by inspection what bucket number we're in (e.g., x-axis position 23 is
    // bucket 2 * 10 + PredictedTrue|ObservedTrue).  The label in enum.xml for
    // MegaConfusionMatrix also provides the bucket number for easy reading.
    constexpr int uma_bucket_size = 10;
    DCHECK_LE(matrix_size, uma_bucket_size);

    // Maximum number of buckets defined in enums.xml, numbered from 0.
    constexpr int max_buckets = 16;

    // Sparse histograms can technically go past 100 exactly-stored elements,
    // but we limit it anyway.  Note that we don't care about |uma_bucket_size|,
    // since it's a sparse histogram.  Only |matrix_size| elements are used in
    // each bucket.
    DCHECK_LE(max_buckets * matrix_size, 100);

    // If we're splitting by feature, then record it and stop.  The others
    // aren't meaningful to record if we're using random feature subsets.
    if (task().uma_hacky_by_feature_subset_confusion_matrix &&
        feature_indices() && feature_indices()->size() == 1) {
      // The bucket number is just the feature number that was selected.
      uma_bucket_number =
          std::min(*feature_indices()->begin(), max_buckets - 1);

      std::string base(kByFeatureBase);
      base::UmaHistogramSparse(base + task().name,
                               static_cast<int>(confusion_matrix_value) +
                                   uma_bucket_number * uma_bucket_size);

      // Early return since no other measurements are meaningful when we're
      // using feature subsets.
      return;
    }

    // If we're selecting a feature subset that's bigger than one but smaller
    // than all of them, then we don't know how to report that.
    if (feature_indices() &&
        feature_indices()->size() != task().feature_descriptions.size()) {
      return;
    }

    // Do normal reporting.

    // Record the aggregate confusion matrix.
    if (task().uma_hacky_aggregate_confusion_matrix) {
      std::string base(kAggregateBase);
      base::UmaHistogramEnumeration(base + task().name, confusion_matrix_value);
    }

    if (task().uma_hacky_by_training_weight_confusion_matrix) {
      // Adjust |uma_bucket_offset| by the training weight, and store the
      // results in that bucket in the ByTrainingWeight histogram.
      //
      // This will bucket from 0 in even steps, with the last bucket holding
      // |max_reporting_weight+1| and everything above it.

      const int n_buckets = task().num_reporting_weight_buckets;
      DCHECK_LE(n_buckets, max_buckets);

      // If the max reporting weight is zero, then default to splitting the
      // buckets evenly, with the last bucket being "completely full set".
      const int max_reporting_weight = task().max_reporting_weight
                                           ? task().max_reporting_weight
                                           : task().max_data_set_size - 1;

      // We use one fewer buckets, to save one for the overflow.  Buckets are
      // numbered from 0 to |n_buckets-1|, inclusive.  In other words, when the
      // training weight is equal to |max_reporting_weight|, we still want to
      // be in bucket |n_buckets - 2|.  That's why we add one to the max before
      // we divide; only things over the max go into the last bucket.
      uma_bucket_number =
          std::min<int>((n_buckets - 1) * info.total_training_weight /
                            (max_reporting_weight + 1),
                        n_buckets - 1);

      std::string base(kByTrainingWeightBase);
      base::UmaHistogramSparse(base + task().name,
                               static_cast<int>(confusion_matrix_value) +
                                   uma_bucket_number * uma_bucket_size);
    }
  }
};

// Ukm-based reporter.
class UkmRegressionReporter : public DistributionReporter {
 public:
  UkmRegressionReporter(const LearningTask& task)
      : DistributionReporter(task) {}

  void OnPrediction(const PredictionInfo& info,
                    TargetHistogram predicted) override {
    DCHECK_EQ(task().target_description.ordering,
              LearningTask::Ordering::kNumeric);

    DCHECK_NE(info.source_id, ukm::kInvalidSourceId);

    ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
    if (!ukm_recorder)
      return;

    ukm::builders::Media_Learning_PredictionRecord builder(info.source_id);
    builder.SetLearningTask(task().GetId());
    builder.SetObservedValue(Bucketize(info.observed.value()));
    builder.SetPredictedValue(Bucketize(predicted.Average()));
    builder.SetTrainingDataTotalWeight(info.total_training_weight);
    builder.SetTrainingDataSize(info.total_training_examples);
    // TODO(liberato): we'd add feature subsets here.

    builder.Record(ukm_recorder);
  }

  // Scale and translate |value| from the range specified in the task to 0-100.
  // We scale it so that the buckets have an equal amount of the input range in
  // each of them.
  int Bucketize(double value) {
    const int output_min = 0;
    const int output_max = 100;
    // Scale it so that input_min -> output_min and input_max -> output_max.
    // Note that the input width is |input_max - input_min|, but there are
    // |output_max - output_min + 1| output buckets.  That's why we don't
    // add one to the denominator, but we do add one to the numerator.
    double scaled_value =
        ((output_max - output_min + 1) * (value - task().ukm_min_input_value)) /
            (task().ukm_max_input_value - task().ukm_min_input_value) +
        output_min;
    // Clip to [0, 100] and truncate to an integer.
    return base::clamp(static_cast<int>(scaled_value), output_min, output_max);
  }
};

std::unique_ptr<DistributionReporter> DistributionReporter::Create(
    const LearningTask& task) {
  // We only know how to report regression tasks right now.
  if (task.target_description.ordering != LearningTask::Ordering::kNumeric)
    return nullptr;

  // We can report hacky UMA or non-hacky UKM.  We could report both if we had
  // a DistributionReporter that forwarded predictions to each, but we don't.
  if (task.uma_hacky_aggregate_confusion_matrix ||
      task.uma_hacky_by_training_weight_confusion_matrix ||
      task.uma_hacky_by_feature_subset_confusion_matrix) {
    return std::make_unique<UmaRegressionReporter>(task);
  } else if (task.report_via_ukm) {
    return std::make_unique<UkmRegressionReporter>(task);
  }

  return nullptr;
}

DistributionReporter::DistributionReporter(const LearningTask& task)
    : task_(task) {}

DistributionReporter::~DistributionReporter() = default;

Model::PredictionCB DistributionReporter::GetPredictionCallback(
    const PredictionInfo& info) {
  return base::BindOnce(&DistributionReporter::OnPrediction,
                        weak_factory_.GetWeakPtr(), info);
}

void DistributionReporter::SetFeatureSubset(
    const std::set<int>& feature_indices) {
  feature_indices_ = feature_indices;
}

}  // namespace learning
}  // namespace media
