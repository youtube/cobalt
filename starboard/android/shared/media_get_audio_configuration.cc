// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

// clang-format off
#include "starboard/media.h"
// clang-format on

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/media_capabilities_cache.h"
#include "starboard/common/media.h"

// TODO(b/284140486): Refine the implementation so it works when the audio
// outputs are changed during the query.
bool SbMediaGetAudioConfiguration(
    int output_index,
    SbMediaAudioConfiguration* out_configuration) {
  using starboard::GetMediaAudioConnectorName;
  using starboard::android::shared::JniEnvExt;
  using starboard::android::shared::MediaCapabilitiesCache;
  using starboard::android::shared::ScopedLocalJavaRef;

  if (output_index < 0) {
    SB_LOG(WARNING) << "output_index is " << output_index
                    << ", which cannot be negative.";
    return false;
  }

  if (out_configuration == nullptr) {
    SB_LOG(WARNING) << "out_configuration cannot be nullptr.";
    return false;
  }

  if (MediaCapabilitiesCache::GetInstance()->GetAudioConfiguration(
          output_index, out_configuration)) {
    SB_LOG(INFO) << "Audio connector type for index " << output_index << " is "
                 << GetMediaAudioConnectorName(out_configuration->connector)
                 << " and it has " << out_configuration->number_of_channels
                 << " channels.";
    return true;
  }

  SB_LOG(INFO) << "Failed to find audio connector type for index "
               << output_index;
  return false;
}
