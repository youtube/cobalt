// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/learning/mojo/public/cpp/learning_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<media_m96::learning::mojom::LabelledExampleDataView,
                  media_m96::learning::LabelledExample>::
    Read(media_m96::learning::mojom::LabelledExampleDataView data,
         media_m96::learning::LabelledExample* out_example) {
  out_example->features.clear();
  if (!data.ReadFeatures(&out_example->features))
    return false;

  if (!data.ReadTargetValue(&out_example->target_value))
    return false;

  return true;
}

// static
bool StructTraits<media_m96::learning::mojom::FeatureValueDataView,
                  media_m96::learning::FeatureValue>::
    Read(media_m96::learning::mojom::FeatureValueDataView data,
         media_m96::learning::FeatureValue* out_feature_value) {
  *out_feature_value = media_m96::learning::FeatureValue(data.value());
  return true;
}

// static
bool StructTraits<media_m96::learning::mojom::TargetValueDataView,
                  media_m96::learning::TargetValue>::
    Read(media_m96::learning::mojom::TargetValueDataView data,
         media_m96::learning::TargetValue* out_target_value) {
  *out_target_value = media_m96::learning::TargetValue(data.value());
  return true;
}

// static
bool StructTraits<media_m96::learning::mojom::ObservationCompletionDataView,
                  media_m96::learning::ObservationCompletion>::
    Read(media_m96::learning::mojom::ObservationCompletionDataView data,
         media_m96::learning::ObservationCompletion* out_observation_completion) {
  if (!data.ReadTargetValue(&out_observation_completion->target_value))
    return false;
  out_observation_completion->weight = data.weight();
  return true;
}

// static
bool StructTraits<media_m96::learning::mojom::TargetHistogramPairDataView,
                  media_m96::learning::TargetHistogramPair>::
    Read(media_m96::learning::mojom::TargetHistogramPairDataView data,
         media_m96::learning::TargetHistogramPair* out_pair) {
  if (!data.ReadTargetValue(&out_pair->target_value))
    return false;
  out_pair->count = data.count();
  return true;
}

// static
bool StructTraits<media_m96::learning::mojom::TargetHistogramDataView,
                  media_m96::learning::TargetHistogram>::
    Read(media_m96::learning::mojom::TargetHistogramDataView data,
         media_m96::learning::TargetHistogram* out_target_histogram) {
  ArrayDataView<media_m96::learning::mojom::TargetHistogramPairDataView> pairs;
  data.GetPairsDataView(&pairs);
  if (pairs.is_null())
    return false;

  for (size_t i = 0; i < pairs.size(); ++i) {
    media_m96::learning::mojom::TargetHistogramPairDataView pair_data;
    pairs.GetDataView(i, &pair_data);
    media_m96::learning::TargetValue value;
    if (!pair_data.ReadTargetValue(&value))
      return false;

    out_target_histogram->counts_.emplace(std::move(value), pair_data.count());
  }

  return true;
}

}  // namespace mojo
