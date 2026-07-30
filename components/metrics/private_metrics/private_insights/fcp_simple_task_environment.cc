// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/private_insights/fcp_simple_task_environment.h"

namespace private_insights {

namespace {

// Iterator that returns a single string then finishes.
class SingleExampleIterator : public fcp::client::ExampleIterator {
 public:
  explicit SingleExampleIterator(std::string data) : data_(std::move(data)) {}

  absl::StatusOr<std::string> Next() override {  // nocheck
    if (returned_) {
      return absl::OutOfRangeError("End of iterator");
    }
    returned_ = true;
    return data_;
  }

  void Close() override {}

 private:
  std::string data_;
  bool returned_ = false;
};

}  // namespace

FcpSimpleTaskEnvironment::FcpSimpleTaskEnvironment(
    std::string base_dir,
    std::string cache_dir,
    fcp::client::ExampleQueryResult result)
    : base_dir_(std::move(base_dir)),
      cache_dir_(std::move(cache_dir)),
      result_(std::move(result)) {}

FcpSimpleTaskEnvironment::~FcpSimpleTaskEnvironment() = default;

std::string FcpSimpleTaskEnvironment::GetBaseDir() {
  return base_dir_;
}

std::string FcpSimpleTaskEnvironment::GetCacheDir() {
  return cache_dir_;
}

absl::StatusOr<std::unique_ptr<fcp::client::ExampleIterator>>  // nocheck
FcpSimpleTaskEnvironment::CreateExampleIterator(
    const google::internal::federated::plan::ExampleSelector&
        example_selector) {
  return std::make_unique<SingleExampleIterator>(result_.SerializeAsString());
}

bool FcpSimpleTaskEnvironment::TrainingConditionsSatisfied() {
  return true;
}

}  // namespace private_insights
