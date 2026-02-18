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

#ifndef MEDIA_BASE_STARBOARD_RENDERER_FACTORY_TRAITS_H_
#define MEDIA_BASE_STARBOARD_RENDERER_FACTORY_TRAITS_H_

#include <optional>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/starboard/starboard_renderer_config.h"
#include "media/base/timestamp_constants.h"
#include "media/starboard/bind_host_receiver_callback.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// Customizations for StarboardRendererClientFactory to
// create StarboardRenderer via mojo services.
struct MEDIA_EXPORT RendererFactoryTraits {
  RendererFactoryTraits() = default;
  ~RendererFactoryTraits() = default;

  base::TimeDelta audio_write_duration_local = kNoTimestamp;
  base::TimeDelta audio_write_duration_remote = kNoTimestamp;
  std::string max_video_capabilities;
  bool enable_flush_during_seek = false;
  bool enable_reset_audio_decoder = false;
  std::optional<int> initial_max_frames_in_decoder;
  std::optional<int> max_pending_input_frames;
  std::optional<int> video_decoder_poll_interval_ms;
  std::optional<int> max_samples_per_write;
  gfx::Size viewport_size;
  BindHostReceiverCallback bind_host_receiver_callback = base::NullCallback();
};

}  // namespace media

#endif  // MEDIA_BASE_STARBOARD_RENDERER_FACTORY_TRAITS_H_
