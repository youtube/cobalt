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

#include "starboard/android/shared/experimental_features_internal.h"

#include <memory>

#include "base/no_destructor.h"
#include "base/threading/thread_local.h"
#include "starboard/common/log.h"

namespace starboard::android::shared {

namespace {

using ::starboard::shared::starboard::player::filter::ExperimentalFeatures;

base::ThreadLocalOwnedPointer<ExperimentalFeatures>&
GetExperimentalFeaturesTLS() {
  static base::NoDestructor<base::ThreadLocalOwnedPointer<ExperimentalFeatures>>
      experimental_features_tls;
  return *experimental_features_tls;
}

ExperimentalFeatures* GetOrCreateExperimentalFeatures() {
  auto& tls = GetExperimentalFeaturesTLS();
  ExperimentalFeatures* ptr = tls.Get();
  if (ptr == nullptr) {
    tls.Set(std::make_unique<ExperimentalFeatures>());
    ptr = tls.Get();
  }
  return ptr;
}

}  // namespace

void SetExperimentalFeaturesForCurrentThread(
    const ExperimentalFeatures& experimental_features) {
  *GetOrCreateExperimentalFeatures() = experimental_features;
}

const ExperimentalFeatures& GetExperimentalFeaturesForCurrentThread() {
  ExperimentalFeatures* current = GetExperimentalFeaturesTLS().Get();
  SB_CHECK(current)
      << "ExperimentalFeatures are not set. This method was likely "
         "called on the wrong thread or a race condition occurred.";
  return *current;
}

}  // namespace starboard::android::shared
