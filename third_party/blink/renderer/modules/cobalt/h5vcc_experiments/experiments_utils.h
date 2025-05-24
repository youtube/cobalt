// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_H5VCC_EXPERIMENTS_EXPERIMENTS_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_H5VCC_EXPERIMENTS_EXPERIMENTS_UTILS_H_

#include "base/values.h"
#include "cobalt/browser/constants/cobalt_experiment_names.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_experiment_configuration.h"

namespace blink {

// Tries to parse an ExperimentConfiguration and convert it to
// base::value::Dict.
// Returns an empty optional when featureParams field in the |config| contains
// an unsupported type.
std::optional<base::Value::Dict> ParseConfigToDictionary(
    const ExperimentConfiguration* config);
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_H5VCC_EXPERIMENTS_EXPERIMENTS_UTILS_H_
