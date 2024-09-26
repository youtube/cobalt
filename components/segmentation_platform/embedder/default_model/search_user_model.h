// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_SEARCH_USER_MODEL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_SEARCH_USER_MODEL_H_

#include "components/segmentation_platform/public/model_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform {

struct Config;

// Segmentation search user model provider. Provides a default model and
// metadata for the search user optimization target.
class SearchUserModel : public ModelProvider {
 public:
  SearchUserModel();
  ~SearchUserModel() override = default;

  // Disallow copy/assign.
  SearchUserModel(const SearchUserModel&) = delete;
  SearchUserModel& operator=(const SearchUserModel&) = delete;

  static std::unique_ptr<Config> GetConfig();

  // ModelProvider implementation.
  void InitAndFetchModel(
      const ModelUpdatedCallback& model_updated_callback) override;
  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override;
  bool ModelAvailable() override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_SEARCH_USER_MODEL_H_
