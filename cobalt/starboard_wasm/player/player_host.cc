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

#include "cobalt/starboard_wasm/player/player_host.h"

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <chrono>
#include <limits>
#include <map>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "cobalt/starboard_wasm/player/player_settings.h"
#include "starboard/decode_target.h"
#include "starboard/drm.h"
#include "starboard/extension/experimental/experimental_features.h"
#include "starboard/system.h"
#include "starboard/window.h"

#if BUILDFLAG(IS_ANDROID)
#include "starboard/android/shared/video_window.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace cobalt {
namespace starboard_wasm {

namespace {

constexpr int64_t kMediaTimeCheckIntervalUs = 100'000;
constexpr int64_t kVideoWriteDurationUs = 1'500'000;

#if BUILDFLAG(IS_ANDROID)
// Maximum time an accepted Android create() waits on the player thread for the
// video surface held by a player whose teardown is in flight.
constexpr int64_t kSurfaceWaitTimeoutUs = 1'000'000;

// MediaCodecVideoDecoder's punch-out teardown requires a non-null decode-target
// provider and invokes ClearNativeWindow() through it. ClearNativeWindow()
// manages its own temporary EGL context, so running it inline on the calling
// Starboard thread is safe.
void RunInline(SbDecodeTargetGraphicsContextProvider*,
               SbDecodeTargetGlesContextRunnerTarget target,
               void* target_context) {
  target(target_context);
}

SbDecodeTargetGraphicsContextProvider* DummyDecodeTargetProvider() {
  static SbDecodeTargetGraphicsContextProvider provider = {
      /*egl_display=*/nullptr, /*egl_context=*/nullptr, &RunInline,
      /*gles_context_runner_context=*/nullptr};
  return &provider;
}
#endif  // BUILDFLAG(IS_ANDROID)

SbMediaAudioCodec MapAudioCodec(uint32_t codec) {
  switch (codec) {
    case ME_AUDIO_AAC:
      return kSbMediaAudioCodecAac;
    case ME_AUDIO_OPUS:
      return kSbMediaAudioCodecOpus;
    case ME_AUDIO_AC3:
      return kSbMediaAudioCodecAc3;
    case ME_AUDIO_EAC3:
      return kSbMediaAudioCodecEac3;
    case ME_AUDIO_VORBIS:
      return kSbMediaAudioCodecVorbis;
    case ME_AUDIO_MP3:
      return kSbMediaAudioCodecMp3;
    case ME_AUDIO_FLAC:
      return kSbMediaAudioCodecFlac;
    case ME_AUDIO_PCM:
      return kSbMediaAudioCodecPcm;
    default:
      return kSbMediaAudioCodecNone;
  }
}

SbMediaVideoCodec MapVideoCodec(uint32_t codec) {
  switch (codec) {
    case ME_VIDEO_H264:
      return kSbMediaVideoCodecH264;
    case ME_VIDEO_H265:
      return kSbMediaVideoCodecH265;
    case ME_VIDEO_VP9:
      return kSbMediaVideoCodecVp9;
    case ME_VIDEO_AV1:
      return kSbMediaVideoCodecAv1;
    case ME_VIDEO_VP8:
      return kSbMediaVideoCodecVp8;
    default:
      return kSbMediaVideoCodecNone;
  }
}

uint32_t MapPlayerState(SbPlayerState state) {
  switch (state) {
    case kSbPlayerStateInitialized:
      return ME_PLAYER_INITIALIZED;
    case kSbPlayerStatePrerolling:
      return ME_PLAYER_PREROLLING;
    case kSbPlayerStatePresenting:
      return ME_PLAYER_PRESENTING;
    case kSbPlayerStateEndOfStream:
      return ME_PLAYER_END_OF_STREAM;
    case kSbPlayerStateDestroyed:
      return ME_PLAYER_DESTROYED;
    default:
      return ME_PLAYER_NONE;
  }
}

size_t BoundedStrLen(const char* s, size_t max_len) {
  size_t n = 0;
  while (n < max_len && s[n] != '\0') {
    ++n;
  }
  return n;
}

struct Resolution {
  int width;
  int height;
};
constexpr Resolution kProbeResolutions[] = {
    {7680, 4320}, {3840, 2160}, {2560, 1440}, {1920, 1080},
    {1280, 720},  {854, 480},   {640, 360},   {426, 240},
};
constexpr int kProbeFrameRates[] = {120, 60, 50, 30, 25, 24};

std::string StripSizeParameters(const std::string& mime) {
  std::string out;
  for (std::string_view part : base::SplitStringPiece(
           mime, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    const std::string lower = base::ToLowerASCII(part);
    if (!out.empty() && (base::StartsWith(lower, "width=") ||
                         base::StartsWith(lower, "height=") ||
                         base::StartsWith(lower, "framerate="))) {
      continue;
    }
    if (!out.empty()) {
      out += "; ";
    }
    out.append(part);
  }
  return out;
}

bool CanPlay(const std::string& mime) {
  return SbMediaCanPlayMimeAndKeySystem(mime.c_str(), "") !=
         kSbMediaSupportTypeNotSupported;
}

SbMediaColorMetadata MakeColorMetadata(const me_video_config& cfg) {
  SbMediaColorMetadata m{};
  m.bits_per_channel = cfg.bit_depth > 0 ? cfg.bit_depth : 8;

  if (cfg.mastering_luminance_max > 0) {
    SbMediaMasteringMetadata& mm = m.mastering_metadata;
    auto chroma = [](uint32_t v) { return static_cast<float>(v) / 50000.f; };
    mm.primary_r_chromaticity_x = chroma(cfg.mastering_primaries_x[0]);
    mm.primary_r_chromaticity_y = chroma(cfg.mastering_primaries_y[0]);
    mm.primary_g_chromaticity_x = chroma(cfg.mastering_primaries_x[1]);
    mm.primary_g_chromaticity_y = chroma(cfg.mastering_primaries_y[1]);
    mm.primary_b_chromaticity_x = chroma(cfg.mastering_primaries_x[2]);
    mm.primary_b_chromaticity_y = chroma(cfg.mastering_primaries_y[2]);
    mm.white_point_chromaticity_x = chroma(cfg.mastering_white_x);
    mm.white_point_chromaticity_y = chroma(cfg.mastering_white_y);
    mm.luminance_max =
        static_cast<float>(cfg.mastering_luminance_max) / 10000.f;
    mm.luminance_min =
        static_cast<float>(cfg.mastering_luminance_min) / 10000.f;
  }
  m.max_cll = cfg.max_cll;
  m.max_fall = cfg.max_fall;

  const bool range_known = cfg.color_range == 1u || cfg.color_range == 2u;
  if (cfg.color_primaries == 0 && cfg.transfer_characteristics == 0 &&
      cfg.matrix_coefficients == 0) {
    m.primaries = kSbMediaPrimaryIdBt709;
    m.transfer = kSbMediaTransferIdBt709;
    m.matrix = kSbMediaMatrixIdBt709;
    m.range = range_known ? static_cast<SbMediaRangeId>(cfg.color_range)
                          : kSbMediaRangeIdLimited;
  } else {
    m.primaries = cfg.color_primaries != 0
                      ? static_cast<SbMediaPrimaryId>(cfg.color_primaries)
                      : kSbMediaPrimaryIdUnspecified;
    m.transfer =
        cfg.transfer_characteristics != 0
            ? static_cast<SbMediaTransferId>(cfg.transfer_characteristics)
            : kSbMediaTransferIdUnspecified;
    m.matrix = cfg.matrix_coefficients != 0
                   ? static_cast<SbMediaMatrixId>(cfg.matrix_coefficients)
                   : kSbMediaMatrixIdUnspecified;
    m.range = range_known ? static_cast<SbMediaRangeId>(cfg.color_range)
                          : kSbMediaRangeIdUnspecified;
  }
  return m;
}

// Like SbPlayerBridge::CreatePlayer(), so the platform sees the same features
// for players created here.
void ApplyExperimentalFeatures(const std::map<std::string, int64_t>& features) {
  const auto* ext = static_cast<
      const StarboardExtensionExperimentalFeaturesConfigurationApi*>(
      SbSystemGetExtension(
          kStarboardExtensionExperimentalFeaturesConfigurationName));
  if (!ext ||
      strcmp(ext->name,
             kStarboardExtensionExperimentalFeaturesConfigurationName) != 0 ||
      ext->version < 1 || !ext->SetExperimentalFeaturesForCurrentThread) {
    return;
  }
  std::vector<StarboardExperimentalFeatureEntry> entries;
  entries.reserve(features.size());
  for (const auto& [key, value] : features) {
    entries.push_back({key.c_str(), value});
  }
  const StarboardExtensionExperimentalFeatures extension_features{
      entries.data(), entries.size()};
  ext->SetExperimentalFeaturesForCurrentThread(&extension_features);
}

// Like StarboardRenderer::UpdateAudioWriteDuration(): writes further ahead
// when audio leaves the device.
int64_t AudioWriteDurationUs(SbPlayer player) {
  // Bounds the loop like SbPlayerBridge::GetAudioConfigurations().
  constexpr int kMaxAudioConfigurations = 32;
  for (int i = 0; i < kMaxAudioConfigurations; ++i) {
    SbMediaAudioConfiguration configuration;
    if (!SbPlayerGetAudioConfiguration(player, i, &configuration)) {
      break;
    }
    switch (configuration.connector) {
      case kSbMediaAudioConnectorBluetooth:
      case kSbMediaAudioConnectorRemoteWired:
      case kSbMediaAudioConnectorRemoteWireless:
      case kSbMediaAudioConnectorRemoteOther:
        return kSbPlayerWriteDurationRemote;
      default:
        break;
    }
  }
  return kSbPlayerWriteDurationLocal;
}

int64_t AdjustWriteDurationForRate(int64_t duration_us, int32_t rate_milli) {
  if (rate_milli <= 1000) {
    return duration_us;
  }
  return duration_us * static_cast<int64_t>(rate_milli) / 1000;
}

// Mirrors StarboardRenderer::OnNeedData()'s write throttle.
bool TooFarAhead(int64_t last_pts_us,
                 int64_t seek_time_us,
                 int64_t media_time_us,
                 int64_t media_time_age_us,
                 int64_t write_duration_us) {
  if (last_pts_us < 0 || last_pts_us - seek_time_us <= write_duration_us) {
    return false;
  }
  const int64_t ahead =
      last_pts_us - (media_time_us + std::max<int64_t>(0, media_time_age_us));
  return ahead > write_duration_us + kMediaTimeCheckIntervalUs;
}

}  // namespace

PlayerHost::PlayerHost(const ModuleContext& context)
    : owner_(context.owner),
      events_(context.events),
      player_thread_("StarboardWasmPlayer") {
  weak_this_ = weak_factory_.GetWeakPtr();
  base::Thread::Options options;
  options.thread_type = base::ThreadType::kDisplayCritical;
  CHECK(player_thread_.StartWithOptions(std::move(options)));
  player_runner_ = player_thread_.task_runner();
  SurfaceRegistry::Get().AddObserver(this);
}

PlayerHost::~PlayerHost() {
  SurfaceRegistry::Get().RemoveObserver(this);
  {
    base::AutoLock lock(mu_);
    for (const auto& kv : surfaces_) {
      SurfaceRegistry::Get().Unregister(kv.second.token);
    }
    surfaces_.clear();
  }
  player_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&PlayerHost::DestroyAllOnPlayerThread,
                                          base::Unretained(this)));
  player_thread_.Stop();
}

uint32_t PlayerHost::SurfaceCreate(uint32_t flags) {
  if (flags != SB_SURFACE_PUNCH_OUT) {
    return 0;
  }
  const std::string token = base::UnguessableToken::Create().ToString();
  SurfaceRegistry::Get().Register(token);
  base::AutoLock lock(mu_);
  uint32_t id = next_surface_id_++;
  if (id == 0) {
    id = next_surface_id_++;
  }
  surfaces_[id].token = token;
  return id;
}

void PlayerHost::SurfaceDestroy(uint32_t surface) {
  base::AutoLock lock(mu_);
  auto it = surfaces_.find(surface);
  if (it == surfaces_.end()) {
    return;
  }
  SurfaceRegistry::Get().Unregister(it->second.token);
  surfaces_.erase(it);
}

void PlayerHost::SurfaceSetBounds(uint32_t surface,
                                  int32_t x,
                                  int32_t y,
                                  int32_t width,
                                  int32_t height,
                                  int32_t z_index) {
  {
    base::AutoLock lock(mu_);
    auto it = surfaces_.find(surface);
    if (it == surfaces_.end()) {
      return;
    }
    it->second.has_guest_bounds = true;
    it->second.guest_bounds = PlaneRect{x, y, width, height};
    it->second.z_index = z_index;
  }
  player_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PlayerHost::ApplyBoundsOnPlayerThread,
                                base::Unretained(this)));
}

std::string PlayerHost::SurfaceToken(uint32_t surface) const {
  base::AutoLock lock(mu_);
  auto it = surfaces_.find(surface);
  return it == surfaces_.end() ? std::string() : it->second.token;
}

uint32_t PlayerHost::PlayerCreate(const uint8_t* audio,
                                  uint32_t audio_len,
                                  const uint8_t* video,
                                  uint32_t video_len,
                                  uint32_t surface,
                                  uint32_t /*flags*/) {
  std::string surface_token;
  {
    base::AutoLock lock(mu_);
    auto it = surfaces_.find(surface);
    if (it == surfaces_.end()) {
      return 0;
    }
    surface_token = it->second.token;
  }
#if BUILDFLAG(IS_ANDROID)
  if (!starboard::VideoSurfaceHolder::IsVideoSurfaceAvailable() &&
      pending_destroys_.load(std::memory_order_acquire) == 0) {
    surface_waiter_.store(true, std::memory_order_release);
    ScheduleSurfaceProbe();
    return 0;
  }
#endif  // BUILDFLAG(IS_ANDROID)

  me_audio_config a_cfg{};
  const bool has_audio = audio != nullptr && audio_len >= 8;
  if (has_audio) {
    memcpy(&a_cfg, audio, std::min<size_t>(audio_len, sizeof(a_cfg)));
  }
  me_video_config v_cfg{};
  const bool has_video = video != nullptr && video_len >= 8;
  if (has_video) {
    memcpy(&v_cfg, video, std::min<size_t>(video_len, sizeof(v_cfg)));
  }

  uint32_t id = 0;
  {
    base::AutoLock lock(mu_);
    id = next_id_++;
    if (id == 0) {
      id = next_id_++;
    }
    PlayerEntry& entry = players_[id];
    entry.id = id;
    entry.surface = surface;
    entry.callback_context =
        std::make_unique<CallbackContext>(CallbackContext{this, id});
    entry.last_media_time_retrieved_us = MonotonicNowUs();
    if (has_audio && a_cfg.codec != ME_AUDIO_NONE) {
      entry.audio_configs.emplace_back();
      PopulateAudioConfigInPlace(a_cfg, &entry.audio_configs.back());
    }
    if (has_video && v_cfg.codec != ME_VIDEO_NONE) {
      entry.video_configs.emplace_back();
      PopulateVideoConfigInPlace(v_cfg, &entry.video_configs.back());
      SurfaceRegistry::Get().SetVideoSize(
          surface_token, entry.video_configs.back().stream_info.frame_width,
          entry.video_configs.back().stream_info.frame_height);
    }
  }
  player_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&PlayerHost::CreateOnPlayerThread,
                                          base::Unretained(this), id));
  return id;
}

void PlayerHost::PlayerDestroy(uint32_t player) {
  {
    base::AutoLock lock(mu_);
    auto it = players_.find(player);
    if (it == players_.end() || it->second.destroying) {
      return;
    }
    it->second.destroying = true;
  }
  pending_destroys_.fetch_add(1, std::memory_order_acq_rel);
  player_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&PlayerHost::DestroyOnPlayerThread,
                                          base::Unretained(this), player));
}

void PlayerHost::PlayerSeek(uint32_t player,
                            int64_t position_us,
                            uint32_t ticket) {
  int sb_ticket = SB_PLAYER_INITIAL_TICKET;
  {
    base::AutoLock lock(mu_);
    auto it = players_.find(player);
    if (it == players_.end() || it->second.destroying) {
      return;
    }
    PlayerEntry& entry = it->second;
    // Starboard requires every seek to use a new ticket; the guest's ticket
    // is only echoed back in events.
    sb_ticket = ++entry.sb_ticket;
    entry.guest_ticket = ticket;
    entry.seek_time_us = position_us;
    entry.last_media_time_us = position_us;
    entry.last_media_time_retrieved_us = MonotonicNowUs();
    entry.last_audio_pts_us = -1;
    entry.last_video_pts_us = -1;
    entry.audio_eos_written = false;
    entry.video_eos_written = false;
    entry.audio_needs_data_deferred = false;
    entry.video_needs_data_deferred = false;
  }
  player_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PlayerHost::SeekOnPlayerThread, base::Unretained(this),
                     player, position_us, sb_ticket));
}

uint32_t PlayerHost::PlayerWriteSamples(uint32_t player,
                                        uint32_t track,
                                        const uint8_t* bytes,
                                        uint32_t bytes_len,
                                        const uint8_t* infos_raw,
                                        uint32_t count,
                                        const uint8_t* config,
                                        uint32_t config_len) {
  if (bytes == nullptr || infos_raw == nullptr || count == 0) {
    return 0;
  }
  if (track != ME_TRACK_AUDIO && track != ME_TRACK_VIDEO) {
    return 0;
  }

  const SbMediaType media_type =
      (track == ME_TRACK_AUDIO) ? kSbMediaTypeAudio : kSbMediaTypeVideo;

  std::vector<SbPlayerSampleInfo> sb_infos;
  uint32_t accepted = 0;

  base::AutoLock lock(mu_);
  auto it = players_.find(player);
  if (it == players_.end() || it->second.destroying ||
      it->second.create_failed) {
    return 0;
  }
  PlayerEntry& entry = it->second;

  if (track == ME_TRACK_AUDIO && config != nullptr && config_len > 0) {
    me_audio_config a_cfg{};
    memcpy(&a_cfg, config, std::min<size_t>(config_len, sizeof(a_cfg)));
    if (entry.audio_configs.empty() ||
        memcmp(&entry.audio_configs.back().raw, &a_cfg, sizeof(a_cfg)) != 0) {
      entry.audio_configs.emplace_back();
      PopulateAudioConfigInPlace(a_cfg, &entry.audio_configs.back());
    }
  } else if (track == ME_TRACK_VIDEO && config != nullptr && config_len > 0) {
    me_video_config v_cfg{};
    memcpy(&v_cfg, config, std::min<size_t>(config_len, sizeof(v_cfg)));
    if (entry.video_configs.empty() ||
        memcmp(&entry.video_configs.back().raw, &v_cfg, sizeof(v_cfg)) != 0) {
      entry.video_configs.emplace_back();
      PopulateVideoConfigInPlace(v_cfg, &entry.video_configs.back());
      auto surface = surfaces_.find(entry.surface);
      SurfaceRegistry::Get().SetVideoSize(
          surface == surfaces_.end() ? std::string() : surface->second.token,
          entry.video_configs.back().stream_info.frame_width,
          entry.video_configs.back().stream_info.frame_height);
    }
  }
  if ((media_type == kSbMediaTypeAudio && entry.audio_configs.empty()) ||
      (media_type == kSbMediaTypeVideo && entry.video_configs.empty())) {
    return 0;
  }

  const int max_per_write = entry.max_samples_per_write[track];
  const uint32_t limit =
      std::min<uint32_t>(count, static_cast<uint32_t>(max_per_write));

  std::vector<me_sample_info> infos(limit);
  memcpy(infos.data(), infos_raw, sizeof(me_sample_info) * limit);

  const int64_t ref_time_us =
      std::max(entry.seek_time_us, entry.last_media_time_us);
  const int64_t max_audio_pts_us =
      ref_time_us + AdjustWriteDurationForRate(entry.audio_write_duration_us,
                                               entry.rate_milli);
  const int64_t max_video_pts_us =
      ref_time_us +
      AdjustWriteDurationForRate(kVideoWriteDurationUs, entry.rate_milli);

  sb_infos.reserve(limit);
  for (uint32_t i = 0; i < limit; ++i) {
    if (media_type == kSbMediaTypeAudio && i > 0 && !entry.video_eos_written &&
        infos[i - 1].pts_us >= max_audio_pts_us) {
      break;
    }
    if (media_type == kSbMediaTypeVideo && i > 0 &&
        infos[i - 1].pts_us >= max_video_pts_us) {
      break;
    }
    const me_sample_info& info = infos[i];
    if (static_cast<uint64_t>(info.offset) + info.size > bytes_len) {
      break;
    }
    const size_t alloc_size = info.size > 0 ? info.size : 1;
    uint8_t* copy = static_cast<uint8_t*>(malloc(alloc_size));
    if (copy == nullptr) {
      break;
    }
    if (info.size > 0) {
      memcpy(copy, bytes + info.offset, info.size);
    }
    entry.allocated_buffers.insert(copy);

    SbPlayerSampleInfo sb_info{};
    sb_info.type = media_type;
    sb_info.buffer = copy;
    sb_info.buffer_size = static_cast<int>(info.size);
    sb_info.timestamp = info.pts_us;
    if (media_type == kSbMediaTypeAudio) {
      sb_info.audio_sample_info.stream_info =
          entry.audio_configs.back().stream_info;
    } else {
      sb_info.video_sample_info.stream_info =
          entry.video_configs.back().stream_info;
      sb_info.video_sample_info.is_key_frame =
          (info.flags & ME_SAMPLE_KEY_FRAME) != 0;
    }
    sb_infos.push_back(sb_info);
    ++accepted;
  }

  if (media_type == kSbMediaTypeAudio && accepted > 0) {
    entry.last_audio_pts_us = infos[accepted - 1].pts_us;
  } else if (media_type == kSbMediaTypeVideo && accepted > 0) {
    entry.last_video_pts_us = infos[accepted - 1].pts_us;
  }

  if (accepted > 0) {
    player_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PlayerHost::WriteOnPlayerThread, base::Unretained(this),
                       player, media_type, std::move(sb_infos)));
  }
  return accepted;
}

void PlayerHost::PlayerWriteEos(uint32_t player, uint32_t track) {
  {
    base::AutoLock lock(mu_);
    auto it = players_.find(player);
    if (it == players_.end() || it->second.destroying) {
      return;
    }
    PlayerEntry& entry = it->second;
    if (entry.create_failed) {
      return;
    }
    if (track == ME_TRACK_VIDEO && !entry.video_configs.empty()) {
      entry.video_eos_written = true;
      entry.video_needs_data_deferred = false;
      if (entry.audio_needs_data_deferred) {
        entry.audio_needs_data_deferred = false;
        QueuePlayerEventLocked(entry, ME_EV_NEEDS_DATA, ME_TRACK_AUDIO,
                               entry.guest_ticket,
                               /*coalesce=*/true);
      }
    } else if (track == ME_TRACK_AUDIO && !entry.audio_configs.empty()) {
      entry.audio_eos_written = true;
      entry.audio_needs_data_deferred = false;
    } else {
      return;
    }
  }
  player_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PlayerHost::WriteEosOnPlayerThread, base::Unretained(this), player,
          track == ME_TRACK_AUDIO ? kSbMediaTypeAudio : kSbMediaTypeVideo));
}

void PlayerHost::PlayerSetRate(uint32_t player, int32_t rate_milli) {
  {
    base::AutoLock lock(mu_);
    auto it = players_.find(player);
    if (it == players_.end() || it->second.destroying) {
      return;
    }
    it->second.rate_milli = rate_milli;
    if (it->second.last_sent_rate_milli == rate_milli) {
      return;
    }
    it->second.last_sent_rate_milli = rate_milli;
  }
  player_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PlayerHost::SetRateOnPlayerThread,
                                base::Unretained(this), player, rate_milli));
}

void PlayerHost::PlayerSetVolume(uint32_t player, int32_t volume_milli) {
  {
    base::AutoLock lock(mu_);
    auto it = players_.find(player);
    if (it == players_.end() || it->second.destroying) {
      return;
    }
    if (it->second.last_sent_volume_milli == volume_milli) {
      return;
    }
    it->second.last_sent_volume_milli = volume_milli;
  }
  player_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PlayerHost::SetVolumeOnPlayerThread,
                                base::Unretained(this), player, volume_milli));
}

uint32_t PlayerHost::PlayerGetInfo(uint32_t player,
                                   uint8_t* out,
                                   uint32_t out_len) {
  if (out == nullptr || out_len == 0) {
    return 0;
  }
  me_player_info info{};
  bool stale = false;
  {
    base::AutoLock lock(mu_);
    auto it = players_.find(player);
    if (it == players_.end() || it->second.destroying) {
      return 0;
    }
    const PlayerEntry& entry = it->second;
    uint32_t cfg_w = 0;
    uint32_t cfg_h = 0;
    if (!entry.video_configs.empty()) {
      cfg_w = static_cast<uint32_t>(
          std::max(0, entry.video_configs.back().stream_info.frame_width));
      cfg_h = static_cast<uint32_t>(
          std::max(0, entry.video_configs.back().stream_info.frame_height));
    }
    const SbPlayerInfo& sb_info = entry.info;
    info.struct_size = sizeof(info);
    info.player_state = entry.state;
    if (entry.info_valid) {
      info.media_time_us = sb_info.current_media_timestamp;
      info.total_video_frames =
          static_cast<uint32_t>(std::max(0, sb_info.total_video_frames));
      info.dropped_video_frames =
          static_cast<uint32_t>(std::max(0, sb_info.dropped_video_frames));
      info.corrupted_video_frames =
          static_cast<uint32_t>(std::max(0, sb_info.corrupted_video_frames));
      info.playback_rate_milli =
          static_cast<int32_t>(sb_info.playback_rate * 1000.0);
      info.volume_milli = static_cast<int32_t>(sb_info.volume * 1000.0);
      info.is_paused = sb_info.is_paused ? 1u : 0u;
    } else {
      info.media_time_us = entry.seek_time_us;
      info.volume_milli = entry.last_sent_volume_milli;
      info.is_paused = 1;
    }
    info.frame_width = static_cast<uint32_t>(
        entry.info_valid && sb_info.frame_width > 0 ? sb_info.frame_width
                                                    : cfg_w);
    info.frame_height = static_cast<uint32_t>(
        entry.info_valid && sb_info.frame_height > 0 ? sb_info.frame_height
                                                     : cfg_h);
    stale = !entry.info_valid ||
            MonotonicNowUs() - entry.info_us > kInfoRefreshIntervalUs;
  }
  if (stale && !refresh_timer_armed_.load()) {
    RequestRefresh();
  }
  const uint32_t to_copy = std::min<uint32_t>(out_len, sizeof(info));
  memcpy(out, &info, to_copy);
  return to_copy;
}

uint32_t PlayerHost::MediaCaps(const char* mime,
                               uint32_t mime_len,
                               uint8_t* out,
                               uint32_t out_len) {
  if (out == nullptr || out_len == 0) {
    return 0;
  }
  const std::string mime_str(mime != nullptr ? mime : "",
                             mime != nullptr ? mime_len : 0);
  const std::string lower_mime = base::ToLowerASCII(mime_str);
  const bool is_video = base::StartsWith(lower_mime, "video/");
  const bool decode_to_texture =
      lower_mime.find("decode-to-texture=true") != std::string::npos;

  ProbedCaps probed;
  if (!mime_str.empty() && mime_str.size() < ME_MIME_MAX &&
      !decode_to_texture) {
    auto cached = caps_cache_.find(mime_str);
    if (cached == caps_cache_.end()) {
      if (caps_cache_.size() >= kMaxCachedCaps) {
        caps_cache_.clear();
      }
      cached =
          caps_cache_.emplace(mime_str, ProbeCaps(mime_str, is_video)).first;
    }
    probed = cached->second;
  }

  std::string current_mime;
  int max_per_write = 0;
  {
    base::AutoLock lock(mu_);
    max_per_write = settings_max_samples_per_write_;
    for (const auto& kv : players_) {
      const PlayerEntry& entry = kv.second;
      if (entry.destroying || !SbPlayerIsValid(entry.sb_player)) {
        continue;
      }
      const int track = is_video ? ME_TRACK_VIDEO : ME_TRACK_AUDIO;
      max_per_write = entry.max_samples_per_write[track];
      if (is_video && !entry.video_configs.empty()) {
        current_mime = entry.video_configs.back().mime;
      } else if (!is_video && !entry.audio_configs.empty()) {
        current_mime = entry.audio_configs.back().mime;
      }
    }
  }

  me_media_caps caps{};
  caps.struct_size = sizeof(caps);
  caps.can_play = probed.can_play;
  caps.can_change_type =
      current_mime.empty()
          ? 1u
          : (SbMediaCanChangeType(current_mime.c_str(), mime_str.c_str()) ? 1u
                                                                          : 0u);
  // Starboard can't report how many players can run at once, and platforms
  // commonly have a single punch-out video surface.
  caps.max_players = 1;
  caps.punch_out = 1;
  caps.max_width = probed.max_width;
  caps.max_height = probed.max_height;
  caps.max_frame_rate_milli = probed.max_frame_rate_milli;
  caps.max_samples_per_write = static_cast<uint32_t>(max_per_write);

  const uint32_t to_copy = std::min<uint32_t>(out_len, sizeof(caps));
  memcpy(out, &caps, to_copy);
  return to_copy;
}

// static
PlayerHost::ProbedCaps PlayerHost::ProbeCaps(const std::string& mime,
                                             bool is_video) {
  ProbedCaps caps;
  const SbMediaSupportType support =
      SbMediaCanPlayMimeAndKeySystem(mime.c_str(), "");
  caps.can_play = support == kSbMediaSupportTypeProbably ? 2
                  : support == kSbMediaSupportTypeMaybe  ? 1
                                                         : 0;
  // Starboard answers "maybe" for a MIME without codecs whatever the size,
  // so only a definite answer is worth probing.
  if (!is_video || support != kSbMediaSupportTypeProbably) {
    return caps;
  }
  const std::string base_mime = StripSizeParameters(mime);
  for (const auto& resolution : kProbeResolutions) {
    const std::string sized =
        base::StringPrintf("%s; width=%d; height=%d", base_mime.c_str(),
                           resolution.width, resolution.height);
    if (!CanPlay(sized)) {
      continue;
    }
    caps.max_width = resolution.width;
    caps.max_height = resolution.height;
    for (int frame_rate : kProbeFrameRates) {
      if (CanPlay(base::StringPrintf("%s; framerate=%d", sized.c_str(),
                                     frame_rate))) {
        caps.max_frame_rate_milli = static_cast<uint32_t>(frame_rate) * 1000;
        break;
      }
    }
    break;
  }
  return caps;
}

void PlayerHost::OnSurfaceBoundsChanged(const std::string& /*token*/) {
  player_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PlayerHost::ApplyBoundsOnPlayerThread,
                                base::Unretained(this)));
}

// static
void PlayerHost::PopulateAudioConfigInPlace(const me_audio_config& cfg,
                                            StoredAudioConfig* out) {
  out->raw = cfg;
  const size_t mime_len = BoundedStrLen(cfg.mime, sizeof(cfg.mime));
  out->mime = mime_len > 0 ? std::string(cfg.mime, mime_len)
                           : "audio/mp4; codecs=\"mp4a.40.2\"";
  const uint32_t csd_len =
      std::min<uint32_t>(cfg.csd_len, static_cast<uint32_t>(sizeof(cfg.csd)));
  out->csd.assign(cfg.csd, cfg.csd + csd_len);

  uint32_t samples_per_second =
      cfg.sample_rate_hz > 0 ? cfg.sample_rate_hz : 44100;
  uint32_t bits_per_sample = 16;
  if (cfg.codec == ME_AUDIO_OPUS &&
      base::ToLowerASCII(out->mime).find("webm") != std::string::npos) {
    samples_per_second = 48000;
    bits_per_sample = 32;
  } else if (cfg.codec == ME_AUDIO_PCM && cfg.bits_per_sample > 0) {
    bits_per_sample = cfg.bits_per_sample;
  }

  memset(&out->stream_info, 0, sizeof(out->stream_info));
  out->stream_info.codec = MapAudioCodec(cfg.codec);
  out->stream_info.mime = out->mime.c_str();
  out->stream_info.number_of_channels =
      static_cast<uint16_t>(cfg.channels > 0 ? cfg.channels : 2);
  out->stream_info.samples_per_second = static_cast<int>(samples_per_second);
  out->stream_info.bits_per_sample = static_cast<uint16_t>(bits_per_sample);
  out->stream_info.audio_specific_config_size =
      static_cast<uint16_t>(out->csd.size());
  out->stream_info.audio_specific_config =
      out->csd.empty() ? nullptr : out->csd.data();
}

// static
void PlayerHost::PopulateVideoConfigInPlace(const me_video_config& cfg,
                                            StoredVideoConfig* out) {
  out->raw = cfg;
  const size_t mime_len = BoundedStrLen(cfg.mime, sizeof(cfg.mime));
  out->mime = mime_len > 0 ? std::string(cfg.mime, mime_len)
                           : "video/mp4; codecs=\"avc1.64001f\"";
  const size_t caps_len = BoundedStrLen(cfg.max_video_capabilities,
                                        sizeof(cfg.max_video_capabilities));
  out->max_video_capabilities.assign(cfg.max_video_capabilities, caps_len);

  memset(&out->stream_info, 0, sizeof(out->stream_info));
  out->stream_info.codec = MapVideoCodec(cfg.codec);
  out->stream_info.mime = out->mime.c_str();
  out->stream_info.max_video_capabilities = out->max_video_capabilities.c_str();
  out->stream_info.frame_width =
      static_cast<int>(cfg.coded_width > 0 ? cfg.coded_width : 1280);
  out->stream_info.frame_height =
      static_cast<int>(cfg.coded_height > 0 ? cfg.coded_height : 720);
  out->stream_info.color_metadata = MakeColorMetadata(cfg);
}

// static
int64_t PlayerHost::MediaTimeAge(const PlayerEntry& entry) {
  if (entry.rate_milli == 0 || entry.state != ME_PLAYER_PRESENTING) {
    return 0;
  }
  return MonotonicNowUs() - entry.last_media_time_retrieved_us;
}

void PlayerHost::DestroyAllOnPlayerThread() {
  std::vector<SbPlayer> to_destroy;
  {
    base::AutoLock lock(mu_);
    for (auto& kv : players_) {
      kv.second.destroying = true;
      if (SbPlayerIsValid(kv.second.sb_player)) {
        to_destroy.push_back(kv.second.sb_player);
      }
    }
  }
  for (SbPlayer sb : to_destroy) {
    SbPlayerDestroy(sb);
  }
  base::AutoLock lock(mu_);
  for (auto& kv : players_) {
    for (const void* buf : kv.second.allocated_buffers) {
      free(const_cast<void*>(buf));
    }
  }
  players_.clear();
}

void PlayerHost::CreateOnPlayerThread(uint32_t id) {
  SbPlayerCreationParam creation_param{};
  creation_param.drm_system = kSbDrmSystemInvalid;
  creation_param.output_mode = kSbPlayerOutputModePunchOut;
  CallbackContext* callback_context = nullptr;
  {
    base::AutoLock lock(mu_);
    auto it = players_.find(id);
    if (it == players_.end() || it->second.destroying) {
      return;
    }
    PlayerEntry& entry = it->second;
    callback_context = entry.callback_context.get();
    if (!entry.audio_configs.empty()) {
      creation_param.audio_stream_info = entry.audio_configs.back().stream_info;
    } else {
      creation_param.audio_stream_info.codec = kSbMediaAudioCodecNone;
      creation_param.audio_stream_info.mime = "";
    }
    if (!entry.video_configs.empty()) {
      creation_param.video_stream_info = entry.video_configs.back().stream_info;
    } else {
      creation_param.video_stream_info.codec = kSbMediaVideoCodecNone;
      creation_param.video_stream_info.mime = "";
      creation_param.video_stream_info.max_video_capabilities = "";
    }
  }

  // Fetched for every player, like the embedder's pipeline does, since the
  // settings can change at runtime.
  const PlayerSettings settings = GetPlayerSettings();
  ApplyExperimentalFeatures(settings.experimental_features);
  const int settings_max_per_write = settings.max_samples_per_write > 0
                                         ? settings.max_samples_per_write
                                         : kDefaultMaxSamplesPerWrite;
  {
    base::AutoLock lock(mu_);
    viewport_ = PlaneRect{0, 0, std::max(0, settings.viewport_width),
                          std::max(0, settings.viewport_height)};
    settings_max_samples_per_write_ = settings_max_per_write;
  }

  SbDecodeTargetGraphicsContextProvider* context_provider = nullptr;
#if BUILDFLAG(IS_ANDROID)
  context_provider = DummyDecodeTargetProvider();
  if (creation_param.video_stream_info.codec != kSbMediaVideoCodecNone &&
      !starboard::VideoSurfaceHolder::IsVideoSurfaceAvailable()) {
    const int64_t wait_start_us = MonotonicNowUs();
    while (!starboard::VideoSurfaceHolder::IsVideoSurfaceAvailable() &&
           MonotonicNowUs() - wait_start_us < kSurfaceWaitTimeoutUs) {
      base::PlatformThread::Sleep(base::Milliseconds(2));
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)

  SbPlayer sb_player = SbPlayerCreate(
      kSbWindowInvalid, &creation_param, &PlayerHost::DeallocateSampleCB,
      &PlayerHost::DecoderStatusCB, &PlayerHost::PlayerStatusCB,
      &PlayerHost::PlayerErrorCB, callback_context, context_provider);

  if (!SbPlayerIsValid(sb_player)) {
    LOG(ERROR) << "SbPlayerCreate() failed for player " << id;
    base::AutoLock lock(mu_);
    auto it = players_.find(id);
    if (it != players_.end() && !it->second.destroying) {
      it->second.published = true;
      ReportCreateFailedLocked(it->second);
    }
    return;
  }

  const int audio_max =
      SbPlayerGetMaximumNumberOfSamplesPerWrite(sb_player, kSbMediaTypeAudio);
  const int video_max =
      SbPlayerGetMaximumNumberOfSamplesPerWrite(sb_player, kSbMediaTypeVideo);
  const int64_t audio_write_duration_us = AudioWriteDurationUs(sb_player);
  {
    base::AutoLock lock(mu_);
    auto it = players_.find(id);
    if (it != players_.end()) {
      PlayerEntry& entry = it->second;
      entry.sb_player = sb_player;
      entry.max_samples_per_write[ME_TRACK_AUDIO] =
          std::clamp(audio_max, 1, settings_max_per_write);
      entry.max_samples_per_write[ME_TRACK_VIDEO] =
          std::clamp(video_max, 1, settings_max_per_write);
      entry.audio_write_duration_us = audio_write_duration_us;
      entry.published = true;
      if (!entry.destroying) {
        for (const DeferredEvent& ev : entry.deferred_events) {
          QueuePlayerEventLocked(entry, ev.type, ev.a, ev.b, ev.coalesce);
        }
      }
      entry.deferred_events.clear();
    }
  }
  ApplyBoundsOnPlayerThread();
  // Refreshes that ran before `sb_player` was stored did nothing.
  RequestRefresh();
}

void PlayerHost::QueuePlayerEventLocked(PlayerEntry& entry,
                                        uint32_t type,
                                        int64_t a,
                                        int64_t b,
                                        bool coalesce) {
  if (!entry.published) {
    entry.deferred_events.push_back({type, a, b, coalesce});
    return;
  }
  if (coalesce) {
    events_->PushCoalesced(type, entry.id, a, b);
  } else {
    events_->Push(type, entry.id, a, b);
  }
}

void PlayerHost::UpdateAudioWriteDurationOnPlayerThread(uint32_t id) {
  SbPlayer sb_player = kSbPlayerInvalid;
  {
    base::AutoLock lock(mu_);
    auto it = players_.find(id);
    if (it == players_.end() || it->second.destroying) {
      return;
    }
    sb_player = it->second.sb_player;
  }
  if (!SbPlayerIsValid(sb_player)) {
    return;
  }
  const int64_t audio_write_duration_us = AudioWriteDurationUs(sb_player);
  base::AutoLock lock(mu_);
  auto it = players_.find(id);
  if (it != players_.end()) {
    it->second.audio_write_duration_us = audio_write_duration_us;
  }
}

void PlayerHost::ReportCreateFailedLocked(PlayerEntry& entry) {
  entry.create_failed = true;
  QueuePlayerEventLocked(entry, ME_EV_PLAYER_ERROR,
                         ME_PLAYER_ERROR_CREATE_FAILED, 0, /*coalesce=*/false);
#if BUILDFLAG(IS_ANDROID)
  if (!starboard::VideoSurfaceHolder::IsVideoSurfaceAvailable()) {
    surface_waiter_.store(true, std::memory_order_release);
    owner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PlayerHost::ScheduleSurfaceProbe, weak_this_));
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

void PlayerHost::DestroyOnPlayerThread(uint32_t id) {
  SbPlayer sb_player = kSbPlayerInvalid;
  {
    base::AutoLock lock(mu_);
    auto it = players_.find(id);
    if (it == players_.end()) {
      pending_destroys_.fetch_sub(1, std::memory_order_acq_rel);
      return;
    }
    sb_player = it->second.sb_player;
  }
  if (SbPlayerIsValid(sb_player)) {
    SbPlayerDestroy(sb_player);
  }
  pending_destroys_.fetch_sub(1, std::memory_order_acq_rel);
  base::AutoLock lock(mu_);
#if BUILDFLAG(IS_ANDROID)
  if (starboard::VideoSurfaceHolder::IsVideoSurfaceAvailable() &&
      surface_waiter_.exchange(false, std::memory_order_acq_rel)) {
    events_->PushCoalesced(ME_EV_SURFACE_READY, 0, 0, 0);
  }
#endif  // BUILDFLAG(IS_ANDROID)
  auto it = players_.find(id);
  if (it == players_.end()) {
    return;
  }
  for (const void* buf : it->second.allocated_buffers) {
    free(const_cast<void*>(buf));
  }
  if (!it->second.destroyed_event_sent) {
    events_->Push(ME_EV_PLAYER_STATE, id, ME_PLAYER_DESTROYED, 0);
  }
  players_.erase(it);
}

void PlayerHost::SeekOnPlayerThread(uint32_t id,
                                    int64_t position_us,
                                    int ticket) {
  SbPlayer sb_player = kSbPlayerInvalid;
  {
    base::AutoLock lock(mu_);
    auto it = players_.find(id);
    if (it == players_.end() || it->second.destroying) {
      return;
    }
    sb_player = it->second.sb_player;
  }
  if (SbPlayerIsValid(sb_player)) {
    SbPlayerSeek(sb_player, position_us, ticket);
  }
}

void PlayerHost::WriteOnPlayerThread(uint32_t id,
                                     SbMediaType type,
                                     std::vector<SbPlayerSampleInfo> samples) {
  SbPlayer sb_player = kSbPlayerInvalid;
  {
    base::AutoLock lock(mu_);
    auto it = players_.find(id);
    if (it == players_.end()) {
      return;
    }
    sb_player = it->second.sb_player;
    if (!SbPlayerIsValid(sb_player) || it->second.destroying) {
      // Starboard will never deallocate these; free them now.
      for (const SbPlayerSampleInfo& sample : samples) {
        if (it->second.allocated_buffers.erase(sample.buffer) > 0) {
          free(const_cast<void*>(sample.buffer));
        }
      }
      return;
    }
  }
  SbPlayerWriteSamples(sb_player, type, samples.data(),
                       static_cast<int>(samples.size()));
}

void PlayerHost::WriteEosOnPlayerThread(uint32_t id, SbMediaType type) {
  SbPlayer sb_player = kSbPlayerInvalid;
  {
    base::AutoLock lock(mu_);
    auto it = players_.find(id);
    if (it == players_.end() || it->second.destroying) {
      return;
    }
    sb_player = it->second.sb_player;
  }
  if (SbPlayerIsValid(sb_player)) {
    SbPlayerWriteEndOfStream(sb_player, type);
  }
}

void PlayerHost::SetRateOnPlayerThread(uint32_t id, int32_t rate_milli) {
  SbPlayer sb_player = kSbPlayerInvalid;
  {
    base::AutoLock lock(mu_);
    auto it = players_.find(id);
    if (it == players_.end() || it->second.destroying) {
      return;
    }
    sb_player = it->second.sb_player;
  }
  if (!SbPlayerIsValid(sb_player)) {
    return;
  }
  SbPlayerSetPlaybackRate(sb_player, static_cast<double>(rate_milli) / 1000.0);
  RequestRefresh();
}

void PlayerHost::SetVolumeOnPlayerThread(uint32_t id, int32_t volume_milli) {
  SbPlayer sb_player = kSbPlayerInvalid;
  {
    base::AutoLock lock(mu_);
    auto it = players_.find(id);
    if (it == players_.end() || it->second.destroying) {
      return;
    }
    sb_player = it->second.sb_player;
  }
  if (!SbPlayerIsValid(sb_player)) {
    return;
  }
  SbPlayerSetVolume(sb_player, static_cast<double>(volume_milli) / 1000.0);
}

void PlayerHost::ApplyBoundsOnPlayerThread() {
  struct Target {
    SbPlayer sb_player;
    PlaneRect rect;
    int32_t z_index;
  };
  std::vector<Target> targets;
  {
    base::AutoLock lock(mu_);
    for (auto& kv : players_) {
      PlayerEntry& entry = kv.second;
      if (entry.destroying || !SbPlayerIsValid(entry.sb_player)) {
        continue;
      }
      auto surface = surfaces_.find(entry.surface);
      if (surface == surfaces_.end()) {
        continue;
      }
      // Unpositioned surfaces fill the viewport, like StarboardRenderer.
      PlaneRect rect;
      const int32_t z_index = surface->second.z_index;
      if (!SurfaceRegistry::Get().Bounds(surface->second.token, &rect)) {
        if (surface->second.has_guest_bounds) {
          rect = surface->second.guest_bounds;
        } else if (viewport_.width > 0 && viewport_.height > 0) {
          rect = viewport_;
        } else {
          // Unknown viewport; a new SbPlayer fills the screen already.
          continue;
        }
      }
      if (entry.has_applied_bounds && entry.applied_z_index == z_index &&
          entry.applied_bounds == rect) {
        continue;
      }
      entry.has_applied_bounds = true;
      entry.applied_bounds = rect;
      entry.applied_z_index = z_index;
      targets.push_back({entry.sb_player, rect, z_index});
    }
  }
  for (const Target& t : targets) {
    SbPlayerSetBounds(t.sb_player, t.z_index, t.rect.x, t.rect.y, t.rect.width,
                      t.rect.height);
  }
}

void PlayerHost::RequestRefresh() {
  if (refresh_requested_.exchange(true)) {
    return;
  }
  player_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&PlayerHost::RefreshOnPlayerThread,
                                          base::Unretained(this)));
}

void PlayerHost::ScheduleRefreshTimer() {
  if (refresh_timer_running_) {
    return;
  }
  refresh_timer_running_ = true;
  refresh_timer_armed_.store(true);
  player_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PlayerHost::OnRefreshTimer, base::Unretained(this)),
      base::Microseconds(kInfoRefreshIntervalUs));
}

void PlayerHost::OnRefreshTimer() {
  refresh_timer_running_ = false;
  refresh_timer_armed_.store(false);
  RefreshOnPlayerThread();
}

void PlayerHost::RefreshOnPlayerThread() {
  refresh_requested_.store(false);
  std::vector<std::pair<uint32_t, SbPlayer>> targets;
  {
    base::AutoLock lock(mu_);
    for (const auto& kv : players_) {
      const PlayerEntry& entry = kv.second;
      if (entry.destroying || !SbPlayerIsValid(entry.sb_player)) {
        continue;
      }
      targets.emplace_back(kv.first, entry.sb_player);
    }
  }
  bool any_presenting = false;
  for (const auto& [id, sb_player] : targets) {
    SbPlayerInfo sb_info{};
    SbPlayerGetInfo(sb_player, &sb_info);
    base::AutoLock lock(mu_);
    auto it = players_.find(id);
    if (it == players_.end() || it->second.destroying) {
      continue;
    }
    PlayerEntry& entry = it->second;
    entry.info = sb_info;
    entry.info_valid = true;
    entry.info_us = MonotonicNowUs();
    entry.last_media_time_us = sb_info.current_media_timestamp;
    entry.last_media_time_retrieved_us = entry.info_us;
    if (entry.state == ME_PLAYER_PRESENTING) {
      any_presenting = true;
    }
    if (entry.audio_needs_data_deferred &&
        (entry.video_eos_written ||
         !TooFarAhead(entry.last_audio_pts_us, entry.seek_time_us,
                      entry.last_media_time_us, /*age=*/0,
                      AdjustWriteDurationForRate(entry.audio_write_duration_us,
                                                 entry.rate_milli)))) {
      entry.audio_needs_data_deferred = false;
      QueuePlayerEventLocked(entry, ME_EV_NEEDS_DATA, ME_TRACK_AUDIO,
                             entry.guest_ticket,
                             /*coalesce=*/true);
    }
    if (entry.video_needs_data_deferred &&
        !TooFarAhead(entry.last_video_pts_us, entry.seek_time_us,
                     entry.last_media_time_us, /*age=*/0,
                     AdjustWriteDurationForRate(kVideoWriteDurationUs,
                                                entry.rate_milli))) {
      entry.video_needs_data_deferred = false;
      QueuePlayerEventLocked(entry, ME_EV_NEEDS_DATA, ME_TRACK_VIDEO,
                             entry.guest_ticket,
                             /*coalesce=*/true);
    }
  }
  if (any_presenting) {
    ScheduleRefreshTimer();
  }
}

void PlayerHost::ScheduleSurfaceProbe() {
  if (surface_probe_pending_) {
    return;
  }
  surface_probe_pending_ = true;
  owner_->PostDelayedTask(FROM_HERE,
                          base::BindOnce(&PlayerHost::ProbeSurface, weak_this_),
                          base::Milliseconds(100));
}

void PlayerHost::ProbeSurface() {
  surface_probe_pending_ = false;
#if BUILDFLAG(IS_ANDROID)
  if (!starboard::VideoSurfaceHolder::IsVideoSurfaceAvailable()) {
    ScheduleSurfaceProbe();
    return;
  }
#endif  // BUILDFLAG(IS_ANDROID)
  if (!surface_waiter_.exchange(false, std::memory_order_acq_rel)) {
    return;
  }
  events_->PushCoalesced(ME_EV_SURFACE_READY, 0, 0, 0);
}

// static
void PlayerHost::DeallocateSampleCB(SbPlayer /*player*/,
                                    void* context,
                                    const void* sample_buffer) {
  if (sample_buffer == nullptr || context == nullptr) {
    return;
  }
  const CallbackContext* ctx = static_cast<const CallbackContext*>(context);
  PlayerHost* host = ctx->host;
  base::AutoLock lock(host->mu_);
  auto it = host->players_.find(ctx->id);
  if (it != host->players_.end() &&
      it->second.allocated_buffers.erase(sample_buffer) > 0) {
    free(const_cast<void*>(sample_buffer));
  }
}

// static
void PlayerHost::DecoderStatusCB(SbPlayer player,
                                 void* context,
                                 SbMediaType type,
                                 SbPlayerDecoderState state,
                                 int ticket) {
  if (!SbPlayerIsValid(player) || state != kSbPlayerDecoderStateNeedsData ||
      context == nullptr) {
    return;
  }
  const CallbackContext* ctx = static_cast<const CallbackContext*>(context);
  PlayerHost* host = ctx->host;
  const int64_t track =
      (type == kSbMediaTypeAudio) ? ME_TRACK_AUDIO : ME_TRACK_VIDEO;
  base::AutoLock lock(host->mu_);
  auto it = host->players_.find(ctx->id);
  if (it == host->players_.end() || it->second.destroying) {
    return;
  }
  PlayerEntry& entry = it->second;
  if (ticket != entry.sb_ticket) {
    return;
  }
  // Throttle only while presenting: during preroll the player needs data to
  // reach PRESENTING, and the refresh timer that flushes deferred requests
  // runs only while presenting.
  const bool presenting = entry.state == ME_PLAYER_PRESENTING;
  if (type == kSbMediaTypeAudio) {
    const bool video_done =
        entry.video_eos_written || entry.video_configs.empty();
    entry.audio_needs_data_deferred =
        presenting && !video_done &&
        TooFarAhead(entry.last_audio_pts_us, entry.seek_time_us,
                    entry.last_media_time_us, MediaTimeAge(entry),
                    AdjustWriteDurationForRate(entry.audio_write_duration_us,
                                               entry.rate_milli));
    if (entry.audio_needs_data_deferred) {
      return;
    }
  } else {
    entry.video_needs_data_deferred =
        presenting && !entry.video_eos_written &&
        TooFarAhead(entry.last_video_pts_us, entry.seek_time_us,
                    entry.last_media_time_us, MediaTimeAge(entry),
                    AdjustWriteDurationForRate(kVideoWriteDurationUs,
                                               entry.rate_milli));
    if (entry.video_needs_data_deferred) {
      return;
    }
  }
  host->QueuePlayerEventLocked(entry, ME_EV_NEEDS_DATA, track,
                               entry.guest_ticket, /*coalesce=*/true);
}

// static
void PlayerHost::PlayerStatusCB(SbPlayer player,
                                void* context,
                                SbPlayerState state,
                                int ticket) {
  if (!SbPlayerIsValid(player) || context == nullptr) {
    return;
  }
  const CallbackContext* ctx = static_cast<const CallbackContext*>(context);
  PlayerHost* host = ctx->host;
  const uint32_t me_state = MapPlayerState(state);
  if (me_state == ME_PLAYER_NONE) {
    return;
  }
  {
    base::AutoLock lock(host->mu_);
    auto it = host->players_.find(ctx->id);
    if (it == host->players_.end()) {
      return;
    }
    it->second.state = me_state;
    if (it->second.destroying && me_state != ME_PLAYER_DESTROYED) {
      return;
    }
    if (me_state == ME_PLAYER_DESTROYED) {
      it->second.destroyed_event_sent = true;
    }
    // Only the latest seek's guest ticket is kept; events from an earlier seek
    // report UINT32_MAX, which matches no seek.
    const uint32_t guest_ticket = ticket == it->second.sb_ticket
                                      ? it->second.guest_ticket
                                      : std::numeric_limits<uint32_t>::max();
    host->QueuePlayerEventLocked(it->second, ME_EV_PLAYER_STATE, me_state,
                                 guest_ticket, /*coalesce=*/false);
  }
  if (me_state != ME_PLAYER_DESTROYED) {
    host->RequestRefresh();
  }
  if (me_state == ME_PLAYER_PRESENTING) {
    // Audio configurations may be known only once presenting.
    host->player_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PlayerHost::UpdateAudioWriteDurationOnPlayerThread,
                       base::Unretained(host), ctx->id));
  }
}

// static
void PlayerHost::PlayerErrorCB(SbPlayer player,
                               void* context,
                               SbPlayerError error,
                               const char* message) {
  if (context == nullptr) {
    return;
  }
  const CallbackContext* ctx = static_cast<const CallbackContext*>(context);
  LOG(ERROR) << "SbPlayer error " << error << " for player " << ctx->id << ": "
             << (message ? message : "");
  // A failing SbPlayerCreate() also reports here, with an invalid player;
  // CreateOnPlayerThread() handles that case.
  if (!SbPlayerIsValid(player)) {
    return;
  }
  PlayerHost* host = ctx->host;
  base::AutoLock lock(host->mu_);
  auto it = host->players_.find(ctx->id);
  if (it == host->players_.end() || it->second.destroying) {
    return;
  }
  if (it->second.state == ME_PLAYER_NONE) {
    // SbPlayerCreate() returns before the platform sets the player up (on
    // Android, before it takes the video surface); failing that is a create
    // failure, not a decode error.
    host->ReportCreateFailedLocked(it->second);
    return;
  }
  const int64_t code = error == kSbPlayerErrorCapabilityChanged
                           ? ME_PLAYER_ERROR_CAPABILITY_CHANGED
                           : ME_PLAYER_ERROR_DECODE;
  host->QueuePlayerEventLocked(it->second, ME_EV_PLAYER_ERROR, code, 0,
                               /*coalesce=*/false);
}

}  // namespace starboard_wasm
}  // namespace cobalt
