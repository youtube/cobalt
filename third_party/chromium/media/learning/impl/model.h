// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_LEARNING_IMPL_MODEL_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_LEARNING_IMPL_MODEL_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "third_party/chromium/media/learning/common/labelled_example.h"
#include "third_party/chromium/media/learning/common/target_histogram.h"

namespace media_m96 {
namespace learning {

// One trained model, useful for making predictions.
// TODO(liberato): Provide an API for incremental update, for those models that
// can support it.
class COMPONENT_EXPORT(LEARNING_IMPL) Model {
 public:
  // Callback for asynchronous predictions.
  using PredictionCB = base::OnceCallback<void(TargetHistogram predicted)>;

  virtual ~Model() = default;

  virtual TargetHistogram PredictDistribution(
      const FeatureVector& instance) = 0;

  // TODO(liberato): Consider adding an async prediction helper.
};

}  // namespace learning
}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_LEARNING_IMPL_MODEL_H_
