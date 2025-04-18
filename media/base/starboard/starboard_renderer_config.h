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

#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "media/base/media_export.h"

namespace media {

// Describes the content of a video stream, as described by the media container
// (or otherwise determined by the demuxer).
class MEDIA_EXPORT StarboardRendererConfig {
 public:
  // Constructs an uninitialized object. Clients should call Initialize() with
  // appropriate values before using.
  StarboardRendererConfig();

  // Constructs an initialized object. It is acceptable to pass in NULL for
  // |extra_data|, otherwise the memory is copied.
  StarboardRendererConfig(const base::UnguessableToken& overlay_plane_id,
                          base::TimeDelta audio_write_duration_local,
                          base::TimeDelta audio_write_duration_remote);

  StarboardRendererConfig(const StarboardRendererConfig& other);
  StarboardRendererConfig(StarboardRendererConfig&& other);
  StarboardRendererConfig& operator=(const StarboardRendererConfig& other);
  StarboardRendererConfig& operator=(StarboardRendererConfig&& other);

  ~StarboardRendererConfig();

  // Resets the internal state of this object.
  void Initialize(const base::UnguessableToken& overlay_plane_id,
                  base::TimeDelta audio_write_duration_local,
                  base::TimeDelta video_write_duration_remote);

  const base::UnguessableToken& overlay_plane_id() const {
    return overlay_plane_id_;
  }
  base::TimeDelta audio_write_duration_local() const {
    return audio_write_duration_local_;
  }
  base::TimeDelta audio_write_duration_remote() const {
    return audio_write_duration_remote_;
  }

 private:
  base::UnguessableToken overlay_plane_id_;
  base::TimeDelta audio_write_duration_local_;
  base::TimeDelta audio_write_duration_remote_;

  // Not using DISALLOW_COPY_AND_ASSIGN here intentionally to allow the compiler
  // generated copy constructor and assignment operator. Since the extra data is
  // typically small, the performance impact is minimal.
};

}  // namespace media

#endif  // MEDIA_BASE_STARBOARD_STARBOARD_RENDERER_CONFIG_H_
