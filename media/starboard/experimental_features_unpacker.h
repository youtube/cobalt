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

#ifndef MEDIA_STARBOARD_EXPERIMENTAL_FEATURES_UNPACKER_H_
#define MEDIA_STARBOARD_EXPERIMENTAL_FEATURES_UNPACKER_H_

#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "starboard/extension/experimental_features.h"

namespace media {

class ExperimentalFeaturesUnpacker {
 public:
  explicit ExperimentalFeaturesUnpacker(
      const base::flat_map<std::string, int64_t>& features);

  const StarboardExtensionExperimentalFeatures& GetExtensionFeatures() const {
    return extension_features_;
  }

  int GetMaxSamplesPerWrite() const { return max_samples_per_write_; }

  bool ForceDecodeToTexture() const { return force_decode_to_texture_; }

 private:
  std::optional<bool> use_dual_threads_for_video_;
  std::optional<int> video_decoder_initial_preroll_count_;
  std::optional<int> video_renderer_min_decoded_frames_;
  std::optional<int> video_renderer_min_input_buffers_;

  int max_samples_per_write_;
  bool force_decode_to_texture_;

  StarboardExtensionExperimentalFeatures extension_features_;
};

}  // namespace media

#endif  // MEDIA_STARBOARD_EXPERIMENTAL_FEATURES_UNPACKER_H_
