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

// Records and enums passed to H5vccWasmPlayer and H5vccWasmSurface. Layouts
// are fixed (see the static_asserts); records start with `struct_size`.

#ifndef COBALT_STARBOARD_WASM_PLAYER_PLAYER_ABI_H_
#define COBALT_STARBOARD_WASM_PLAYER_PLAYER_ABI_H_

#include <stdint.h>

#include "cobalt/starboard_wasm/core/abi.h"

namespace cobalt {
namespace starboard_wasm {

enum me_track {
  ME_TRACK_AUDIO = 0,
  ME_TRACK_VIDEO = 1,
  ME_TRACK_COUNT = 2,
};

enum me_audio_codec {
  ME_AUDIO_NONE = 0,
  ME_AUDIO_AAC = 1,
  ME_AUDIO_OPUS = 2,
  ME_AUDIO_AC3 = 3,
  ME_AUDIO_EAC3 = 4,
  ME_AUDIO_FLAC = 5,
  ME_AUDIO_VORBIS = 6,
  ME_AUDIO_MP3 = 7,
  ME_AUDIO_PCM = 8,
};

enum me_video_codec {
  ME_VIDEO_NONE = 0,
  ME_VIDEO_H264 = 1,
  ME_VIDEO_H265 = 2,
  ME_VIDEO_VP9 = 3,
  ME_VIDEO_AV1 = 4,
  ME_VIDEO_VP8 = 5,
};

enum me_player_state {
  ME_PLAYER_NONE = 0,
  ME_PLAYER_INITIALIZED = 1,
  ME_PLAYER_PREROLLING = 2,
  ME_PLAYER_PRESENTING = 3,
  ME_PLAYER_END_OF_STREAM = 4,
  ME_PLAYER_DESTROYED = 5,
};

enum me_host_event {
  ME_EV_NEEDS_DATA = 1,    // a = me_track, b = seek ticket
  ME_EV_PLAYER_STATE = 2,  // a = me_player_state, b = seek ticket
  ME_EV_PLAYER_ERROR = 4,  // a = me_player_error, b = 0
  // Sent with handle 0 after a create() that failed for lack of a free video
  // surface, once one is free: create() returned 0, or the player reported
  // ME_PLAYER_ERROR_CREATE_FAILED. The guest may then create again.
  ME_EV_SURFACE_READY = 5,
};

enum me_player_error {
  // The player can't continue; it must be destroyed.
  ME_PLAYER_ERROR_DECODE = 0,
  // The platform's playback capabilities changed; a new player, possibly with
  // other codecs, may succeed.
  ME_PLAYER_ERROR_CAPABILITY_CHANGED = 1,
  // The platform failed to create the player after create() returned its
  // handle. The guest still destroys the handle. Retrying is worthwhile after
  // ME_EV_SURFACE_READY; otherwise the stream is likely unsupported.
  ME_PLAYER_ERROR_CREATE_FAILED = 2,
};

enum me_sample_flags {
  ME_SAMPLE_KEY_FRAME = 1u << 0,
  ME_SAMPLE_ENCRYPTED = 1u << 1,
};

#define ME_MIME_MAX 256u
#define ME_CSD_MAX 256u
#define ME_MAX_VIDEO_CAPABILITIES_MAX 64u

struct me_audio_config {
  uint32_t struct_size;
  uint32_t codec;
  uint32_t sample_rate_hz;
  uint32_t channels;
  uint32_t bits_per_sample;
  uint32_t csd_len;
  uint8_t csd[ME_CSD_MAX];
  char mime[ME_MIME_MAX];
};

struct me_video_config {
  uint32_t struct_size;
  uint32_t codec;
  uint32_t coded_width;
  uint32_t coded_height;
  uint32_t frame_rate_milli;
  uint32_t profile;
  uint32_t level;
  uint32_t bit_depth;
  uint32_t color_primaries;
  uint32_t transfer_characteristics;
  uint32_t matrix_coefficients;
  uint32_t color_range;
  // SMPTE ST 2086 primaries in 0.00002 units; luminance in 0.0001 nit units.
  uint32_t mastering_primaries_x[3];
  uint32_t mastering_primaries_y[3];
  uint32_t mastering_white_x;
  uint32_t mastering_white_y;
  uint32_t mastering_luminance_max;
  uint32_t mastering_luminance_min;
  uint32_t max_cll;
  uint32_t max_fall;
  uint32_t csd_len;
  uint32_t reserved0;
  uint8_t csd[ME_CSD_MAX];
  char mime[ME_MIME_MAX];
  char max_video_capabilities[ME_MAX_VIDEO_CAPABILITIES_MAX];
};

struct me_sample_info {
  uint32_t offset;
  uint32_t size;
  int64_t pts_us;
  int64_t dts_us;
  int64_t duration_us;
  uint32_t flags;
  uint32_t drm_offset;
};

struct me_player_info {
  uint32_t struct_size;
  uint32_t player_state;
  int64_t media_time_us;
  uint32_t total_video_frames;
  uint32_t dropped_video_frames;
  uint32_t corrupted_video_frames;
  uint32_t frame_width;
  uint32_t frame_height;
  int32_t playback_rate_milli;
  int32_t volume_milli;
  uint32_t is_paused;
  uint32_t reserved0;
  uint32_t reserved1;
};

struct me_media_caps {
  uint32_t struct_size;
  uint32_t can_play;
  uint32_t can_change_type;
  uint32_t max_players;
  uint32_t punch_out;
  // For a video MIME that Starboard probably plays, the largest of the common
  // resolutions from 8K down to 240p that it plays at, and the highest frame
  // rate at that resolution; 0 if unknown.
  uint32_t max_width;
  uint32_t max_height;
  uint32_t max_frame_rate_milli;
  uint32_t max_samples_per_write;
  uint32_t reserved0;
  uint32_t reserved1;
  uint32_t reserved2;
};

enum sb_surface_flags {
  SB_SURFACE_PUNCH_OUT = 1u << 0,
};

static_assert(sizeof(me_audio_config) == 536, "me_audio_config layout");
static_assert(sizeof(me_video_config) == 680, "me_video_config layout");
static_assert(sizeof(me_sample_info) == 40, "me_sample_info layout");
static_assert(sizeof(me_player_info) == 56, "me_player_info layout");
static_assert(sizeof(me_media_caps) == 48, "me_media_caps layout");

}  // namespace starboard_wasm
}  // namespace cobalt

#endif  // COBALT_STARBOARD_WASM_PLAYER_PLAYER_ABI_H_
