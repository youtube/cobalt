// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_QUERY_PROCESSOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_QUERY_PROCESSOR_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"

namespace segmentation_platform::processing {
class FeatureProcessorState;
struct Data;

// Interface that converts aribitrary data to a list of tensor in asynchronous
// callback, which can be fed into machine learning model as training data or
// inference data.
class QueryProcessor {
 public:
  // TODO(ssid): Remove these indirections and replace all uses with the global
  // types.
  using Tensor = segmentation_platform::processing::Tensor;
  using IndexedTensors = segmentation_platform::processing::IndexedTensors;
  using FeatureIndex = segmentation_platform::processing::FeatureIndex;

  // TODO(haileywang): Maybe use a unique_ptr<> here.
  using QueryProcessorCallback =
      base::OnceCallback<void(std::unique_ptr<FeatureProcessorState>,
                              IndexedTensors)>;

  // Processes the data and return the tensor values in |callback|.
  virtual void Process(
      std::unique_ptr<FeatureProcessorState> feature_processor_state,
      QueryProcessorCallback callback) = 0;

  // Disallow copy/assign.
  QueryProcessor(const QueryProcessor&) = delete;
  QueryProcessor& operator=(const QueryProcessor&) = delete;

  virtual ~QueryProcessor() = default;

 protected:
  QueryProcessor() = default;
};

}  // namespace segmentation_platform::processing

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_QUERY_PROCESSOR_H_
