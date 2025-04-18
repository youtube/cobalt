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

#include "media/base/starboard/starboard_renderer_config.h"

namespace media {

StarboardRendererConfig::StarboardRendererConfig() = default;

StarboardRendererConfig::StarboardRendererConfig(
    const base::UnguessableToken& overlay_plane_id,
    base::TimeDelta audio_write_duration_local,
    base::TimeDelta audio_write_duration_remote)
    : overlay_plane_id_(overlay_plane_id),
      audio_write_duration_local_(audio_write_duration_local),
      audio_write_duration_remote_(audio_write_duration_remote) {}

StarboardRendererConfig::StarboardRendererConfig(
    const StarboardRendererConfig& other) = default;

StarboardRendererConfig::StarboardRendererConfig(
    StarboardRendererConfig&& other) = default;

StarboardRendererConfig& StarboardRendererConfig::operator=(
    const StarboardRendererConfig& other) = default;

StarboardRendererConfig& StarboardRendererConfig::operator=(
    StarboardRendererConfig&& other) = default;

StarboardRendererConfig::~StarboardRendererConfig() = default;

void StarboardRendererConfig::Initialize(
    const base::UnguessableToken& overlay_plane_id,
    base::TimeDelta audio_write_duration_local,
    base::TimeDelta audio_write_duration_remote) {
  overlay_plane_id_ = overlay_plane_id;
  audio_write_duration_local_ = audio_write_duration_local;
  audio_write_duration_remote_ = audio_write_duration_remote;
}

}  // namespace media
