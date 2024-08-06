// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_LEARNING_MOJO_PUBLIC_CPP_LEARNING_MOJOM_TRAITS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_LEARNING_MOJO_PUBLIC_CPP_LEARNING_MOJOM_TRAITS_H_

#include <vector>

#include "base/component_export.h"
#include "third_party/chromium/media/learning/common/learning_task_controller.h"
#include "third_party/chromium/media/learning/common/value.h"
#include "third_party/chromium/media/learning/mojo/public/mojom/learning_types.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MEDIA_LEARNING_SHARED_TYPEMAP_TRAITS)
    StructTraits<media_m96::learning::mojom::LabelledExampleDataView,
                 media_m96::learning::LabelledExample> {
  static const std::vector<media_m96::learning::FeatureValue>& features(
      const media_m96::learning::LabelledExample& e) {
    return e.features;
  }
  static media_m96::learning::TargetValue target_value(
      const media_m96::learning::LabelledExample& e) {
    return e.target_value;
  }

  static bool Read(media_m96::learning::mojom::LabelledExampleDataView data,
                   media_m96::learning::LabelledExample* out_example);
};

template <>
struct COMPONENT_EXPORT(MEDIA_LEARNING_SHARED_TYPEMAP_TRAITS)
    StructTraits<media_m96::learning::mojom::FeatureValueDataView,
                 media_m96::learning::FeatureValue> {
  static double value(const media_m96::learning::FeatureValue& e) {
    return e.value();
  }
  static bool Read(media_m96::learning::mojom::FeatureValueDataView data,
                   media_m96::learning::FeatureValue* out_feature_value);
};

template <>
struct COMPONENT_EXPORT(MEDIA_LEARNING_SHARED_TYPEMAP_TRAITS)
    StructTraits<media_m96::learning::mojom::TargetValueDataView,
                 media_m96::learning::TargetValue> {
  static double value(const media_m96::learning::TargetValue& e) {
    return e.value();
  }
  static bool Read(media_m96::learning::mojom::TargetValueDataView data,
                   media_m96::learning::TargetValue* out_target_value);
};

template <>
struct COMPONENT_EXPORT(MEDIA_LEARNING_SHARED_TYPEMAP_TRAITS)
    StructTraits<media_m96::learning::mojom::ObservationCompletionDataView,
                 media_m96::learning::ObservationCompletion> {
  static media_m96::learning::TargetValue target_value(
      const media_m96::learning::ObservationCompletion& e) {
    return e.target_value;
  }
  static media_m96::learning::WeightType weight(
      const media_m96::learning::ObservationCompletion& e) {
    return e.weight;
  }
  static bool Read(
      media_m96::learning::mojom::ObservationCompletionDataView data,
      media_m96::learning::ObservationCompletion* out_observation_completion);
};

template <>
struct COMPONENT_EXPORT(MEDIA_LEARNING_SHARED_TYPEMAP_TRAITS)
    StructTraits<media_m96::learning::mojom::TargetHistogramPairDataView,
                 media_m96::learning::TargetHistogramPair> {
  static media_m96::learning::TargetValue target_value(
      const media_m96::learning::TargetHistogramPair& e) {
    return e.target_value;
  }
  static double count(const media_m96::learning::TargetHistogramPair& e) {
    return e.count;
  }
  static bool Read(media_m96::learning::mojom::TargetHistogramPairDataView data,
                   media_m96::learning::TargetHistogramPair* out_pair);
};

template <>
struct COMPONENT_EXPORT(MEDIA_LEARNING_SHARED_TYPEMAP_TRAITS)
    StructTraits<media_m96::learning::mojom::TargetHistogramDataView,
                 media_m96::learning::TargetHistogram> {
  static std::vector<media_m96::learning::TargetHistogramPair> pairs(
      const media_m96::learning::TargetHistogram& e) {
    std::vector<media_m96::learning::TargetHistogramPair> pairs;
    for (auto const& entry : e.counts_) {
      pairs.push_back({entry.first, entry.second});
    }

    return pairs;
  }

  static bool Read(media_m96::learning::mojom::TargetHistogramDataView data,
                   media_m96::learning::TargetHistogram* out_target_histogram);
};

}  // namespace mojo

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_LEARNING_MOJO_PUBLIC_CPP_LEARNING_MOJOM_TRAITS_H_
