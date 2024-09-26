// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_FREQUENT_FEATURE_USER_MODEL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_FREQUENT_FEATURE_USER_MODEL_H_

#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

// Provides a default model and metadata for the frequent feature user segment.
class FrequentFeatureUserModel : public ModelProvider {
 public:
  FrequentFeatureUserModel();
  ~FrequentFeatureUserModel() override = default;

  // Disallow copy/assign.
  FrequentFeatureUserModel(const FrequentFeatureUserModel&) = delete;
  FrequentFeatureUserModel& operator=(const FrequentFeatureUserModel&) = delete;

  static std::unique_ptr<Config> GetConfig();

  // ModelProvider implementation.
  void InitAndFetchModel(
      const ModelUpdatedCallback& model_updated_callback) override;
  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override;
  bool ModelAvailable() override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_FREQUENT_FEATURE_USER_MODEL_H_
