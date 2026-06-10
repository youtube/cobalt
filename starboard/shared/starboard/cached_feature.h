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

#ifndef STARBOARD_SHARED_STARBOARD_CACHED_FEATURE_H_
#define STARBOARD_SHARED_STARBOARD_CACHED_FEATURE_H_

#include <atomic>

#include "starboard/extension/features.h"

namespace starboard::features {

// CachedFeature wraps a SbFeature and caches its enabled status after the first
// query to avoid the runtime overhead of FeatureList::IsEnabled (mutex lock and
// map lookups).
//
// This is safe to use for features that do not change their state after
// initialization, except in tests where ClearCacheForTesting() can be used.
class CachedFeature {
 public:
  enum class State {
    kUninitialized,
    kDisabled,
    kEnabled,
  };

  constexpr CachedFeature(const SbFeature& feature)
      : feature_(feature), state_(State::kUninitialized) {}

  bool IsEnabled();

  void ClearCacheForTesting();

 private:
  const SbFeature& feature_;
  std::atomic<State> state_;
};

}  // namespace starboard::features

#endif  // STARBOARD_SHARED_STARBOARD_CACHED_FEATURE_H_
