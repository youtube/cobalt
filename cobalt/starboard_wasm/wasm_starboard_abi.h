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

// The records a WebAssembly (wasm32) guest exchanges with H5vccWasmPlayer.
//
// Enums cross as Starboard's own values; the static_asserts below make a
// Starboard renumbering a build break instead of an ABI break.
// SbMediaColorMetadata and SbMediaMasteringMetadata cross unchanged.
// Each Starboard struct that holds a pointer has a mirror with Starboard's
// field names and order, where the pointer becomes a byte offset into guest
// memory followed by a `<name>_size` field, unless Starboard already carries
// the size. Strings are not NUL-terminated; a size of 0 means "".
// SbPlayerInfo and SbMediaAudioConfiguration have the same layout on wasm32
// and on every host with 8-byte aligned int64_t and double, but not on 32-bit
// x86, so they have mirrors too.
//
// The host rejects inputs that would crash the process: out-of-bounds guest
// memory, a release-build check or fault in Starboard, or unbounded host
// memory: writes are limited to what Starboard asked for, counted in samples
// (one batch of at most its maximum per kSbPlayerDecoderStateNeedsData), and
// each sample's bytes lie within guest memory.
// It also requires kSbPlayerOutputModePunchOut, the only mode that can show
// video. Other violations of Starboard's API contract are the caller's
// responsibility and may fail DCHECKs in debug builds.

#ifndef COBALT_STARBOARD_WASM_WASM_STARBOARD_ABI_H_
#define COBALT_STARBOARD_WASM_WASM_STARBOARD_ABI_H_

#include <stddef.h>
#include <stdint.h>

#include "build/build_config.h"
#include "starboard/media.h"
#include "starboard/player.h"

namespace cobalt {
namespace starboard_wasm {

struct WasmSbMediaAudioStreamInfo {
  int32_t codec;
  uint32_t mime;
  uint32_t mime_size;
  uint16_t number_of_channels;
  uint32_t samples_per_second;
  uint16_t bits_per_sample;
  uint16_t audio_specific_config_size;
  uint32_t audio_specific_config;
};

struct WasmSbMediaVideoStreamInfo {
  int32_t codec;
  uint32_t mime;
  uint32_t mime_size;
  uint32_t max_video_capabilities;
  uint32_t max_video_capabilities_size;
  int32_t frame_width;
  int32_t frame_height;
  SbMediaColorMetadata color_metadata;
};

struct WasmSbMediaAudioSampleInfo {
  WasmSbMediaAudioStreamInfo stream_info;
  alignas(8) int64_t discarded_duration_from_front;
  alignas(8) int64_t discarded_duration_from_back;
};

struct WasmSbMediaVideoSampleInfo {
  WasmSbMediaVideoStreamInfo stream_info;
  uint8_t is_key_frame;
};

struct WasmSbPlayerSampleSideData {
  int32_t type;
  uint32_t data;
  uint32_t size;
};

struct WasmSbPlayerSampleInfo {
  int32_t type;
  uint32_t buffer;
  int32_t buffer_size;
  alignas(8) int64_t timestamp;
  uint32_t side_data;
  int32_t side_data_count;
  union {
    WasmSbMediaAudioSampleInfo audio_sample_info;
    WasmSbMediaVideoSampleInfo video_sample_info;
  };
  // Encrypted samples are not supported yet; both must be 0.
  uint32_t drm_info;
  uint32_t drm_info_size;
};

struct WasmSbPlayerCreationParam {
  // DRM is not supported yet; must be 0 (kSbDrmSystemInvalid).
  uint32_t drm_system;
  WasmSbMediaAudioStreamInfo audio_stream_info;
  WasmSbMediaVideoStreamInfo video_stream_info;
  int32_t output_mode;
};

struct WasmSbMediaAudioConfiguration {
  int32_t connector;
  alignas(8) int64_t latency;
  int32_t coding_type;
  int32_t number_of_channels;
};

struct WasmStarboardExperimentalFeatureEntry {
  uint32_t key;
  uint32_t key_size;
  alignas(8) int64_t value;
};

struct WasmSbPlayerInfo {
  alignas(8) int64_t current_media_timestamp;
  alignas(8) int64_t duration;
  alignas(8) int64_t start_date;
  int32_t frame_width;
  int32_t frame_height;
  uint8_t is_paused;
  alignas(8) double volume;
  int32_t total_video_frames;
  int32_t dropped_video_frames;
  int32_t corrupted_video_frames;
  alignas(8) double playback_rate;
};

// One Starboard callback, as queued for H5vccWasmPlayer.pollEvents().
//   kind == kWasmSbEventDecoderStatus: a = SbMediaType,
//       b = SbPlayerDecoderState, c = ticket.
//   kind == kWasmSbEventPlayerStatus: a = SbPlayerState, b = ticket.
//   kind == kWasmSbEventPlayerError: a = SbPlayerError.
//   kind == kWasmSbEventPlayerDestroyed: SbPlayerDestroy() has returned, so
//       the platform resources the player held (such as a video surface) are
//       free. This is the only event reported after destroy().
// Unused fields are 0.
struct WasmSbEvent {
  uint32_t kind;
  uint32_t player;
  int32_t a;
  int32_t b;
  int32_t c;
};

inline constexpr uint32_t kWasmSbEventDecoderStatus = 0;
inline constexpr uint32_t kWasmSbEventPlayerStatus = 1;
inline constexpr uint32_t kWasmSbEventPlayerError = 2;
inline constexpr uint32_t kWasmSbEventPlayerDestroyed = 3;

// The layouts above are wasm32's: 4-byte pointers, natural alignment, 8-byte
// aligned int64_t and double.
static_assert(sizeof(WasmSbMediaAudioStreamInfo) == 28);
static_assert(offsetof(WasmSbMediaAudioStreamInfo, samples_per_second) == 16);
static_assert(offsetof(WasmSbMediaAudioStreamInfo, audio_specific_config) ==
              24);
static_assert(sizeof(WasmSbMediaVideoStreamInfo) == 168);
static_assert(offsetof(WasmSbMediaVideoStreamInfo, color_metadata) == 28);
static_assert(sizeof(WasmSbMediaAudioSampleInfo) == 48);
static_assert(offsetof(WasmSbMediaAudioSampleInfo,
                       discarded_duration_from_front) == 32);
static_assert(sizeof(WasmSbMediaVideoSampleInfo) == 172);
static_assert(offsetof(WasmSbMediaVideoSampleInfo, is_key_frame) == 168);
static_assert(sizeof(WasmSbPlayerSampleSideData) == 12);
static_assert(sizeof(WasmSbPlayerSampleInfo) == 216);
static_assert(offsetof(WasmSbPlayerSampleInfo, timestamp) == 16);
static_assert(offsetof(WasmSbPlayerSampleInfo, side_data) == 24);
static_assert(offsetof(WasmSbPlayerSampleInfo, audio_sample_info) == 32);
static_assert(offsetof(WasmSbPlayerSampleInfo, drm_info) == 208);
static_assert(sizeof(WasmSbPlayerCreationParam) == 204);
static_assert(offsetof(WasmSbPlayerCreationParam, video_stream_info) == 32);
static_assert(offsetof(WasmSbPlayerCreationParam, output_mode) == 200);
static_assert(sizeof(WasmSbPlayerInfo) == 72);
static_assert(offsetof(WasmSbPlayerInfo, frame_width) == 24);
static_assert(offsetof(WasmSbPlayerInfo, is_paused) == 32);
static_assert(offsetof(WasmSbPlayerInfo, volume) == 40);
static_assert(offsetof(WasmSbPlayerInfo, total_video_frames) == 48);
static_assert(offsetof(WasmSbPlayerInfo, playback_rate) == 64);
static_assert(sizeof(WasmSbMediaAudioConfiguration) == 24);
static_assert(offsetof(WasmSbMediaAudioConfiguration, latency) == 8);
static_assert(offsetof(WasmSbMediaAudioConfiguration, coding_type) == 16);
static_assert(sizeof(WasmStarboardExperimentalFeatureEntry) == 16);
static_assert(offsetof(WasmStarboardExperimentalFeatureEntry, value) == 8);
static_assert(sizeof(WasmSbEvent) == 20);
#if !defined(ARCH_CPU_X86)
static_assert(sizeof(SbPlayerInfo) == sizeof(WasmSbPlayerInfo) &&
              offsetof(SbPlayerInfo, is_paused) == 32 &&
              offsetof(SbPlayerInfo, volume) == 40 &&
              offsetof(SbPlayerInfo, playback_rate) == 64);
static_assert(sizeof(SbMediaAudioConfiguration) ==
                  sizeof(WasmSbMediaAudioConfiguration) &&
              offsetof(SbMediaAudioConfiguration, latency) == 8);
#endif

static_assert(sizeof(SbMediaMasteringMetadata) == 40);
static_assert(offsetof(SbMediaMasteringMetadata, luminance_max) == 32);
static_assert(sizeof(SbMediaColorMetadata) == 140);
static_assert(offsetof(SbMediaColorMetadata, mastering_metadata) == 28);
static_assert(offsetof(SbMediaColorMetadata, max_cll) == 68);
static_assert(offsetof(SbMediaColorMetadata, primaries) == 76);
static_assert(offsetof(SbMediaColorMetadata, range) == 88);
static_assert(offsetof(SbMediaColorMetadata, custom_primary_matrix) == 92);
static_assert(sizeof(SbMediaPrimaryId) == 4 && sizeof(SbMediaTransferId) == 4 &&
              sizeof(SbMediaMatrixId) == 4 && sizeof(SbMediaRangeId) == 4);

static_assert(kSbMediaTypeAudio == 0 && kSbMediaTypeVideo == 1);
static_assert(kSbMediaAudioCodecNone == 0 && kSbMediaAudioCodecAac == 1 &&
              kSbMediaAudioCodecAc3 == 2 && kSbMediaAudioCodecEac3 == 3 &&
              kSbMediaAudioCodecOpus == 4 && kSbMediaAudioCodecVorbis == 5 &&
              kSbMediaAudioCodecMp3 == 6 && kSbMediaAudioCodecFlac == 7 &&
              kSbMediaAudioCodecPcm == 8 && kSbMediaAudioCodecIamf == 9);
static_assert(kSbMediaVideoCodecNone == 0 && kSbMediaVideoCodecH264 == 1 &&
              kSbMediaVideoCodecH265 == 2 && kSbMediaVideoCodecMpeg2 == 3 &&
              kSbMediaVideoCodecTheora == 4 && kSbMediaVideoCodecVc1 == 5 &&
              kSbMediaVideoCodecAv1 == 6 && kSbMediaVideoCodecVp8 == 7 &&
              kSbMediaVideoCodecVp9 == 8 && kSbMediaVideoCodecAv2 == 9);
static_assert(kSbMediaSupportTypeNotSupported == 0 &&
              kSbMediaSupportTypeMaybe == 1 &&
              kSbMediaSupportTypeProbably == 2);
static_assert(kSbMediaPrimaryIdBt709 == 1 &&
              kSbMediaPrimaryIdUnspecified == 2 &&
              kSbMediaPrimaryIdBt2020 == 9 &&
              kSbMediaPrimaryIdLastStandardValue == 12 &&
              kSbMediaPrimaryIdUnknown == 1000);
static_assert(kSbMediaTransferIdBt709 == 1 &&
              kSbMediaTransferIdUnspecified == 2 &&
              kSbMediaTransferIdSmpteSt2084 == 16 &&
              kSbMediaTransferIdAribStdB67 == 18 &&
              kSbMediaTransferIdUnknown == 1000);
static_assert(kSbMediaMatrixIdRgb == 0 && kSbMediaMatrixIdBt709 == 1 &&
              kSbMediaMatrixIdUnspecified == 2 &&
              kSbMediaMatrixIdBt2020NonconstantLuminance == 9 &&
              kSbMediaMatrixIdLastStandardValue == 11 &&
              kSbMediaMatrixIdInvalid == 255);
static_assert(kSbMediaRangeIdUnspecified == 0 && kSbMediaRangeIdLimited == 1 &&
              kSbMediaRangeIdFull == 2 && kSbMediaRangeIdDerived == 3);
static_assert(kSbMediaAudioConnectorUnknown == 0 &&
              kSbMediaAudioConnectorAnalog == 1 &&
              kSbMediaAudioConnectorBluetooth == 2 &&
              kSbMediaAudioConnectorBuiltIn == 3 &&
              kSbMediaAudioConnectorHdmi == 4 &&
              kSbMediaAudioConnectorRemoteWired == 5 &&
              kSbMediaAudioConnectorRemoteWireless == 6 &&
              kSbMediaAudioConnectorRemoteOther == 7 &&
              kSbMediaAudioConnectorSpdif == 8 &&
              kSbMediaAudioConnectorUsb == 9);
static_assert(kSbMediaAudioCodingTypeNone == 0 &&
              kSbMediaAudioCodingTypePcm == 10);
static_assert(kSbPlayerDecoderStateNeedsData == 0);
static_assert(kSbPlayerStateInitialized == 0 && kSbPlayerStatePrerolling == 1 &&
              kSbPlayerStatePresenting == 2 && kSbPlayerStateEndOfStream == 3 &&
              kSbPlayerStateDestroyed == 4);
static_assert(kSbPlayerErrorDecode == 0 &&
              kSbPlayerErrorCapabilityChanged == 1);
static_assert(kSbPlayerOutputModeDecodeToTexture == 0 &&
              kSbPlayerOutputModePunchOut == 1 &&
              kSbPlayerOutputModeInvalid == 2);
static_assert(kMatroskaBlockAdditional == 0);
static_assert(SB_PLAYER_INITIAL_TICKET == 0);
static_assert(kSbPlayerWriteDurationLocal == 500000);

}  // namespace starboard_wasm
}  // namespace cobalt

#endif  // COBALT_STARBOARD_WASM_WASM_STARBOARD_ABI_H_
