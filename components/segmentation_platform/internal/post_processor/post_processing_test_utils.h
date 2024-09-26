// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_POST_PROCESSOR_POST_PROCESSING_TEST_UTILS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_POST_PROCESSOR_POST_PROCESSING_TEST_UTILS_H_

#include "components/segmentation_platform/internal/proto/client_results.pb.h"
#include "components/segmentation_platform/public/config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform::test_utils {

proto::OutputConfig GetTestOutputConfigForBinaryClassifier();
proto::OutputConfig GetTestOutputConfigForBinnedClassifier();
proto::OutputConfig GetTestOutputConfigForMultiClassClassifier(
    int top_k_outputs,
    absl::optional<float> threshold);

std::unique_ptr<Config> CreateTestConfig();
std::unique_ptr<Config> CreateTestConfig(const std::string& client_key,
                                         proto::SegmentId segment_id);
proto::ClientResult CreateClientResult(proto::PredictionResult pred_result);

}  // namespace segmentation_platform::test_utils

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_POST_PROCESSOR_POST_PROCESSING_TEST_UTILS_H_
