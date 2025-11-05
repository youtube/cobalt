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

#ifndef MEDIA_BASE_STARBOARD_STARBOARD_RENDERER_CONFIG_H_
#define MEDIA_BASE_STARBOARD_STARBOARD_RENDERER_CONFIG_H_

#include <map>
#include <string>
#include <variant>

#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "media/base/media_export.h"
#include "ui/gfx/geometry/size.h"

namespace media {

using H5vccSettingValue = std::variant<std::string, int64_t>;

// Configs for StarboardRenderer.
struct MEDIA_EXPORT StarboardRendererConfig {
  StarboardRendererConfig();
<<<<<<< HEAD
  StarboardRendererConfig(const base::UnguessableToken& overlay_plane_id,
                          base::TimeDelta audio_write_duration_local,
                          base::TimeDelta audio_write_duration_remote,
                          const std::string& max_video_capabilities,
                          const gfx::Size& viewport_size);
=======
  StarboardRendererConfig(
      const base::UnguessableToken& overlay_plane_id,
      base::TimeDelta audio_write_duration_local,
      base::TimeDelta audio_write_duration_remote,
      const std::string& max_video_capabilities,
      const std::map<std::string, H5vccSettingValue>& h5vcc_settings);
>>>>>>> c5883f44e6 (media: Pass H5vcc settings from GlobalFeatures to StarboardRenderer (#7836))
  StarboardRendererConfig(const StarboardRendererConfig&);
  StarboardRendererConfig& operator=(const StarboardRendererConfig&);

  base::UnguessableToken overlay_plane_id;
  base::TimeDelta audio_write_duration_local;
  base::TimeDelta audio_write_duration_remote;
  std::string max_video_capabilities;
<<<<<<< HEAD
  gfx::Size viewport_size;
=======
  std::map<std::string, H5vccSettingValue> h5vcc_settings;
>>>>>>> c5883f44e6 (media: Pass H5vcc settings from GlobalFeatures to StarboardRenderer (#7836))
};

}  // namespace media

#endif  // MEDIA_BASE_STARBOARD_STARBOARD_RENDERER_CONFIG_H_
