// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/cached_feature.h"

#include "starboard/shared/starboard/feature_list.h"

namespace starboard::features {

bool CachedFeature::IsEnabled() {
  State cached = state_.load(std::memory_order_relaxed);
  if (cached != State::kUninitialized) {
    return cached == State::kEnabled;
  }
  bool enabled = FeatureList::IsEnabled(feature_);
  state_.store(enabled ? State::kEnabled : State::kDisabled,
               std::memory_order_relaxed);
  return enabled;
}

void CachedFeature::ClearCacheForTesting() {
  state_.store(State::kUninitialized, std::memory_order_relaxed);
}

}  // namespace starboard::features
