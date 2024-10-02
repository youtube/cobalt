// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_BROWSER_TEST_UTIL_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_BROWSER_TEST_UTIL_H_

#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace base {
class HistogramTester;
}  // namespace base

namespace optimization_guide {

// Retries fetching |histogram_name| until it contains at least |count| samples.
// Returns the count of samples.
int RetryForHistogramUntilCountReached(
    const base::HistogramTester* histogram_tester,
    const std::string& histogram_name,
    int count);

// Builds a test models response.
std::unique_ptr<optimization_guide::proto::GetModelsResponse>
BuildGetModelsResponse();

// Helper to receive modelinfo updates.
class ModelFileObserver : public OptimizationTargetModelObserver {
 public:
  using ModelFileReceivedCallback =
      base::OnceCallback<void(proto::OptimizationTarget, const ModelInfo&)>;

  ModelFileObserver();
  ~ModelFileObserver() override;

  void set_model_file_received_callback(ModelFileReceivedCallback callback) {
    file_received_callback_ = std::move(callback);
  }

  absl::optional<proto::OptimizationTarget> optimization_target() const {
    return optimization_target_;
  }

  absl::optional<ModelInfo> model_info() { return model_info_; }

  // OptimizationTargetModelObserver implementation:
  void OnModelUpdated(proto::OptimizationTarget optimization_target,
                      const ModelInfo& model_info) override;

 private:
  ModelFileReceivedCallback file_received_callback_;

  // Holds the optimization target that was received from modelinfo updates.
  absl::optional<proto::OptimizationTarget> optimization_target_;

  // Holds the modelinfo that was received from modelinfo updates.
  absl::optional<ModelInfo> model_info_;
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_BROWSER_TEST_UTIL_H_
