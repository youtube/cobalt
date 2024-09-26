// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_RESUME_HEAVY_USER_MODEL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_RESUME_HEAVY_USER_MODEL_H_

#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

struct Config;

// Segmentation resume heavy user model provider. Provides a default model and
// metadata for the resume heavy user segment.
// TODO(shaktisahu): Find signals for desktop users.
class ResumeHeavyUserModel : public ModelProvider {
 public:
  ResumeHeavyUserModel();
  ~ResumeHeavyUserModel() override = default;

  // Disallow copy/assign.
  ResumeHeavyUserModel(const ResumeHeavyUserModel&) = delete;
  ResumeHeavyUserModel& operator=(const ResumeHeavyUserModel&) = delete;

  static std::unique_ptr<Config> GetConfig();

  // ModelProvider implementation.
  void InitAndFetchModel(
      const ModelUpdatedCallback& model_updated_callback) override;
  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override;
  bool ModelAvailable() override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_RESUME_HEAVY_USER_MODEL_H_
