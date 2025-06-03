// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_QUERY_TILES_MODEL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_QUERY_TILES_MODEL_H_

#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

struct Config;

// Segmentation query tiles model provider. Provides a default model and
// metadata for the query tiles optimization target.
class QueryTilesModel : public DefaultModelProvider {
 public:
  QueryTilesModel();
  ~QueryTilesModel() override = default;

  // Disallow copy/assign.
  QueryTilesModel(const QueryTilesModel&) = delete;
  QueryTilesModel& operator=(const QueryTilesModel&) = delete;

  static std::unique_ptr<Config> GetConfig();

  // ModelProvider implementation.
  std::unique_ptr<ModelConfig> GetModelConfig() override;

  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_QUERY_TILES_MODEL_H_
