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

#include "cobalt/starboard_wasm/wasm_player_host.h"

#include <string.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cobalt/starboard_wasm/event_queue.h"
#include "cobalt/starboard_wasm/guest_memory.h"
#include "cobalt/starboard_wasm/wasm_starboard_abi.h"
#include "media/base/starboard/sbmedia_interface.h"
#include "media/starboard/sbplayer_interface.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "starboard/audio_sink.h"
#include "starboard/extension/experimental/experimental_features.h"
#include "starboard/extension/player_settings.h"
#include "starboard/system.h"
#include "starboard/window.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

namespace cobalt {
namespace starboard_wasm {

namespace {

// Keeps times convertible from double and clear of int64 overflow in
// Starboard's renderers.
constexpr int64_t kMaxTimestamp = int64_t{1} << 53;

// Owns what the pointers in Starboard's stream infos point to.
struct StreamInfoStorage {
  std::string mime;
  std::string max_video_capabilities;
  std::vector<uint8_t> audio_specific_config;
};

struct CreationParam {
  SbPlayerCreationParam param = {};
  StreamInfoStorage audio;
  StreamInfoStorage video;
};

// Keeps the conversion to SbMediaType defined.
bool IsMediaType(int32_t type) {
  return type == kSbMediaTypeAudio || type == kSbMediaTypeVideo;
}

bool ResolveAudioStreamInfo(GuestMemory& memory,
                            const WasmSbMediaAudioStreamInfo& in,
                            StreamInfoStorage* storage,
                            SbMediaAudioStreamInfo* out) {
  // The codec range keeps the enum conversion defined. Prevents division by
  // zero (DecodedAudio::frames(), AudioRendererPcm), negative resampling
  // ratios and InterleavedSincResampler's SB_CHECKs on the channel count.
  if (in.codec < kSbMediaAudioCodecNone || in.codec > kSbMediaAudioCodecIamf ||
      (in.codec != kSbMediaAudioCodecNone &&
       (in.number_of_channels < 1 ||
        in.number_of_channels > SbAudioSinkGetMaxChannels() ||
        in.samples_per_second < 1 ||
        in.samples_per_second > std::numeric_limits<int>::max())) ||
      !memory.ReadString(in.mime, in.mime_size, &storage->mime) ||
      !memory.ReadBytes(in.audio_specific_config, in.audio_specific_config_size,
                        &storage->audio_specific_config)) {
    return false;
  }
  *out = {};
  out->codec = static_cast<SbMediaAudioCodec>(in.codec);
  out->mime = storage->mime.c_str();
  out->number_of_channels = in.number_of_channels;
  out->samples_per_second = in.samples_per_second;
  out->bits_per_sample = in.bits_per_sample;
  out->audio_specific_config_size = in.audio_specific_config_size;
  if (!storage->audio_specific_config.empty()) {
    out->audio_specific_config = storage->audio_specific_config.data();
  }
  return true;
}

bool ResolveVideoStreamInfo(GuestMemory& memory,
                            const WasmSbMediaVideoStreamInfo& in,
                            StreamInfoStorage* storage,
                            SbMediaVideoStreamInfo* out) {
  // The codec range keeps the enum conversion defined. Decoders size frames
  // from the stream info and check it (e.g. SB_CHECK_GT on the frame size
  // when creating a MediaCodec, SB_CHECK on the bit depth in IsSDRVideo()).
  const unsigned int bits = in.color_metadata.bits_per_channel;
  if (in.codec < kSbMediaVideoCodecNone || in.codec > kSbMediaVideoCodecAv2 ||
      (in.codec != kSbMediaVideoCodecNone &&
       (in.frame_width <= 0 || in.frame_height <= 0 ||
        (bits != 8 && bits != 10 && bits != 12))) ||
      !memory.ReadString(in.mime, in.mime_size, &storage->mime) ||
      !memory.ReadString(in.max_video_capabilities,
                         in.max_video_capabilities_size,
                         &storage->max_video_capabilities)) {
    return false;
  }
  *out = {};
  out->codec = static_cast<SbMediaVideoCodec>(in.codec);
  out->mime = storage->mime.c_str();
  out->max_video_capabilities = storage->max_video_capabilities.c_str();
  out->frame_width = in.frame_width;
  out->frame_height = in.frame_height;
  out->color_metadata = in.color_metadata;
  return true;
}

bool ResolveCreationParam(GuestMemory& memory,
                          uint32_t ptr,
                          uint32_t size,
                          CreationParam* out) {
  WasmSbPlayerCreationParam in;
  if (size != sizeof(in) || !memory.Read(ptr, &in)) {
    return false;
  }
  // A nonzero drm_system would be used as an SbDrmSystem pointer. The output
  // mode range keeps the enum conversion defined.
  if (in.drm_system != 0 ||
      in.output_mode < kSbPlayerOutputModeDecodeToTexture ||
      in.output_mode > kSbPlayerOutputModeInvalid ||
      !ResolveAudioStreamInfo(memory, in.audio_stream_info, &out->audio,
                              &out->param.audio_stream_info) ||
      !ResolveVideoStreamInfo(memory, in.video_stream_info, &out->video,
                              &out->param.video_stream_info)) {
    return false;
  }
  out->param.drm_system = kSbDrmSystemInvalid;
  out->param.output_mode = static_cast<SbPlayerOutputMode>(in.output_mode);
  return true;
}

const StarboardExtensionExperimentalFeaturesConfigurationApi*
    g_experimental_features_api_for_testing = nullptr;

const StarboardExtensionExperimentalFeaturesConfigurationApi*
ExperimentalFeaturesApi() {
  if (g_experimental_features_api_for_testing) {
    return g_experimental_features_api_for_testing;
  }
  const auto* api = static_cast<
      const StarboardExtensionExperimentalFeaturesConfigurationApi*>(
      SbSystemGetExtension(
          kStarboardExtensionExperimentalFeaturesConfigurationName));
  if (!api ||
      strcmp(api->name,
             kStarboardExtensionExperimentalFeaturesConfigurationName) != 0 ||
      api->version < 1) {
    return nullptr;
  }
  return api;
}

constinit thread_local bool g_experimental_features_set = false;

const StarboardExtensionPlayerSettingsApi* g_player_settings_api_for_testing =
    nullptr;

const StarboardExtensionPlayerSettingsApi* PlayerSettingsApi() {
  if (g_player_settings_api_for_testing) {
    return g_player_settings_api_for_testing;
  }
  const auto* api = static_cast<const StarboardExtensionPlayerSettingsApi*>(
      SbSystemGetExtension(kStarboardExtensionPlayerSettingsName));
  if (!api || strcmp(api->name, kStarboardExtensionPlayerSettingsName) != 0 ||
      api->version < 1) {
    return nullptr;
  }
  return api;
}

#if BUILDFLAG(IS_ANDROID)
// MediaCodecVideoDecoder's punch-out teardown requires a non-null decode-target
// provider and invokes ClearNativeWindow() through it. ClearNativeWindow()
// manages its own temporary EGL context, so running it inline on the calling
// Starboard thread is safe. Other platforms get no provider, since one without
// a GL context would break their decode-to-texture paths.
void RunInline(SbDecodeTargetGraphicsContextProvider*,
               SbDecodeTargetGlesContextRunnerTarget target,
               void* target_context) {
  target(target_context);
}
#endif  // BUILDFLAG(IS_ANDROID)

SbDecodeTargetGraphicsContextProvider* DecodeTargetProvider() {
#if BUILDFLAG(IS_ANDROID)
  static SbDecodeTargetGraphicsContextProvider provider = {
      /*egl_display=*/nullptr, /*egl_context=*/nullptr, &RunInline,
      /*gles_context_runner_context=*/nullptr};
  return &provider;
#else
  return nullptr;
#endif  // BUILDFLAG(IS_ANDROID)
}

// Set on the GPU thread and run on hosts' threads.
struct GeometrySubscriberBinder {
  base::Lock lock;
  WasmPlayerHost::VideoGeometrySubscriberBinder binder GUARDED_BY(lock);
};

GeometrySubscriberBinder& GetGeometrySubscriberBinder() {
  static base::NoDestructor<GeometrySubscriberBinder> binder;
  return *binder;
}

// A written sample and what its SbPlayerSampleInfo points to.
struct SampleCopy {
  std::vector<uint8_t> buffer;
  std::vector<uint8_t> side_data_bytes;
  SbPlayerSampleSideData side_data = {};
  StreamInfoStorage stream_info;
};

bool ResolveSample(GuestMemory& memory,
                   SbMediaType type,
                   const WasmSbPlayerSampleInfo& in,
                   SampleCopy* copy,
                   SbPlayerSampleInfo* out) {
  // An empty buffer would be null, which decoders dereference (e.g.
  // SB_CHECK(buffer) in AC-3 passthrough). Negative or huge timestamps trip
  // SB_CHECK_GE in VideoRendererImpl::WriteSamples(). A nonzero drm_info would
  // be used as a pointer.
  if (in.buffer_size <= 0 || in.timestamp < 0 || in.timestamp > kMaxTimestamp ||
      in.drm_info != 0 || in.drm_info_size != 0 ||
      !memory.ReadBytes(in.buffer, in.buffer_size, &copy->buffer)) {
    return false;
  }
  *out = {};
  out->type = type;
  out->buffer = copy->buffer.data();
  out->buffer_size = in.buffer_size;
  out->timestamp = in.timestamp;
  if (in.side_data_count != 0) {
    // Starboard reads only the first entry.
    WasmSbPlayerSampleSideData side_data;
    if (!memory.Read(in.side_data, &side_data) ||
        !memory.ReadBytes(side_data.data, side_data.size,
                          &copy->side_data_bytes)) {
      return false;
    }
    copy->side_data = {kMatroskaBlockAdditional, copy->side_data_bytes.data(),
                       copy->side_data_bytes.size()};
    out->side_data = &copy->side_data;
    out->side_data_count = 1;
  }
  if (type == kSbMediaTypeAudio) {
    const WasmSbMediaAudioSampleInfo& audio = in.audio_sample_info;
    // A sample without a codec leaves AdaptiveAudioDecoder without a decoder,
    // which a later end of stream dereferences.
    if (audio.stream_info.codec == kSbMediaAudioCodecNone ||
        !ResolveAudioStreamInfo(memory, audio.stream_info, &copy->stream_info,
                                &out->audio_sample_info.stream_info)) {
      return false;
    }
    out->audio_sample_info.discarded_duration_from_front =
        audio.discarded_duration_from_front;
    out->audio_sample_info.discarded_duration_from_back =
        audio.discarded_duration_from_back;
  } else {
    const WasmSbMediaVideoSampleInfo& video = in.video_sample_info;
    if (video.stream_info.codec == kSbMediaVideoCodecNone ||
        !ResolveVideoStreamInfo(memory, video.stream_info, &copy->stream_info,
                                &out->video_sample_info.stream_info)) {
      return false;
    }
    out->video_sample_info.is_key_frame = video.is_key_frame != 0;
  }
  return true;
}

}  // namespace

// Used by Starboard callbacks on Starboard threads. Lock order: `lock`, then
// the EventQueue's lock.
struct WasmPlayerHost::CallbackContext {
  CallbackContext(scoped_refptr<EventQueue> events,
                  base::WeakPtr<WasmPlayerHost> host,
                  uint32_t player)
      : events(std::move(events)), host(std::move(host)), player(player) {}

  const scoped_refptr<EventQueue> events;
  const base::WeakPtr<WasmPlayerHost> host;
  const uint32_t player;
  base::Lock lock;
  bool destroyed GUARDED_BY(lock) = false;
  std::string error_message GUARDED_BY(lock);
  // Indexed by SbMediaType.
  bool needs_data[kSbMediaTypeVideo + 1] GUARDED_BY(lock) = {};
};

struct WasmPlayerHost::Player {
  struct Bounds {
    int32_t z_index;
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
  };

  SbPlayer sb_player = kSbPlayerInvalid;
  uint32_t surface = 0;
  std::optional<Bounds> bounds;
  std::unique_ptr<CallbackContext> context;
  std::map<const void*, std::unique_ptr<SampleCopy>> samples;
};

WasmPlayerHost::WasmPlayerHost(scoped_refptr<EventQueue> events,
                               ::media::SbPlayerInterface& sbplayer)
    : events_(std::move(events)),
      sbplayer_(sbplayer),
      destroy_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

WasmPlayerHost::~WasmPlayerHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (!players_.empty()) {
    Destroy(players_.begin()->first);
  }
}

// static
void WasmPlayerHost::SetExperimentalFeaturesApiForTesting(
    const StarboardExtensionExperimentalFeaturesConfigurationApi* api) {
  g_experimental_features_api_for_testing = api;
  g_experimental_features_set = false;
}

// static
bool WasmPlayerHost::SetExperimentalFeaturesForCurrentThread(
    GuestMemory& memory,
    uint32_t entries,
    uint32_t count) {
  const auto* api = ExperimentalFeaturesApi();
  const uint8_t* in =
      count ? memory.Array(entries, count,
                           sizeof(WasmStarboardExperimentalFeatureEntry))
            : nullptr;
  if (!api || (count && !in)) {
    return false;
  }
  // The extension copies the keys.
  std::vector<std::string> keys(count);
  std::vector<StarboardExperimentalFeatureEntry> native(count);
  for (uint32_t i = 0; i < count; ++i) {
    WasmStarboardExperimentalFeatureEntry entry;
    memcpy(&entry, in + i * sizeof(entry), sizeof(entry));
    if (!memory.ReadString(entry.key, entry.key_size, &keys[i])) {
      return false;
    }
    native[i] = {keys[i].c_str(), entry.value};
  }
  const StarboardExtensionExperimentalFeatures features{native.data(), count};
  api->SetExperimentalFeaturesForCurrentThread(&features);
  g_experimental_features_set = true;
  return true;
}

// static
void WasmPlayerHost::SetPlayerSettingsApiForTesting(
    const StarboardExtensionPlayerSettingsApi* api) {
  g_player_settings_api_for_testing = api;
}

// static
bool WasmPlayerHost::SetMaxVideoInputSizeForCurrentThread(
    int32_t max_video_input_size) {
  const auto* api = PlayerSettingsApi();
  if (!api || !api->SetMaxVideoInputSizeForCurrentThread) {
    return false;
  }
  api->SetMaxVideoInputSizeForCurrentThread(max_video_input_size);
  return true;
}

// static
bool WasmPlayerHost::SetMaxVideoResolutionForCurrentThread(
    GuestMemory& memory,
    uint32_t max_video_resolution,
    uint32_t max_video_resolution_size) {
  const auto* api = PlayerSettingsApi();
  std::string resolution;
  if (!api || !api->SetMaxVideoResolutionForCurrentThread ||
      !memory.ReadString(max_video_resolution, max_video_resolution_size,
                         &resolution)) {
    return false;
  }
  // The extension copies the string.
  api->SetMaxVideoResolutionForCurrentThread(
      resolution.empty() ? nullptr : resolution.c_str());
  return true;
}

// static
void WasmPlayerHost::ResetForCurrentThread() {
  if (const auto* api = ExperimentalFeaturesApi()) {
    const StarboardExtensionExperimentalFeatures empty{nullptr, 0};
    api->SetExperimentalFeaturesForCurrentThread(&empty);
  }
  g_experimental_features_set = false;
  if (const auto* api = PlayerSettingsApi()) {
    if (api->SetMaxVideoInputSizeForCurrentThread) {
      api->SetMaxVideoInputSizeForCurrentThread(0);
    }
    if (api->SetMaxVideoResolutionForCurrentThread) {
      api->SetMaxVideoResolutionForCurrentThread(nullptr);
    }
  }
}

// static
::media::SbPlayerInterface& WasmPlayerHost::DefaultSbPlayerInterface() {
  static base::NoDestructor<::media::DefaultSbPlayerInterface> sbplayer;
  return *sbplayer;
}

// static
void WasmPlayerHost::SetVideoGeometrySubscriberBinder(
    VideoGeometrySubscriberBinder binder) {
  GeometrySubscriberBinder& global = GetGeometrySubscriberBinder();
  base::AutoLock lock(global.lock);
  global.binder = std::move(binder);
}

uint32_t WasmPlayerHost::CreateSurface() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const uint32_t id = next_surface_++;
  Surface& surface = surfaces_[id];
  surface.token = base::UnguessableToken::Create();
  if (!geometry_subscriber_.is_bound()) {
    GeometrySubscriberBinder& global = GetGeometrySubscriberBinder();
    base::AutoLock lock(global.lock);
    if (global.binder) {
      global.binder.Run(geometry_subscriber_.BindNewPipeAndPassReceiver());
    }
  }
  if (geometry_subscriber_.is_bound()) {
    mojo::PendingRemote<cobalt::media::mojom::VideoGeometryChangeClient> client;
    surface.receiver = geometry_receivers_.Add(
        this, client.InitWithNewPipeAndPassReceiver(), id);
    geometry_subscriber_->SubscribeToVideoGeometryChange(
        surface.token, std::move(client), base::DoNothing());
  }
  return id;
}

void WasmPlayerHost::DestroySurface(uint32_t surface) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = surfaces_.find(surface);
  if (it == surfaces_.end()) {
    return;
  }
  geometry_receivers_.Remove(it->second.receiver);
  surfaces_.erase(it);
  for (auto& [id, player] : players_) {
    if (player->surface == surface) {
      player->surface = 0;
      ApplyBounds(*player);
    }
  }
}

std::string WasmPlayerHost::SurfaceToken(uint32_t surface) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = surfaces_.find(surface);
  return it == surfaces_.end() ? std::string() : it->second.token.ToString();
}

uint32_t WasmPlayerHost::Create(GuestMemory& memory,
                                uint32_t surface,
                                uint32_t creation_param,
                                uint32_t creation_param_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreationParam param;
  // Only punch-out can show video: nothing binds SbPlayerGetCurrentFrame(),
  // and only Android gets a decode-target provider.
  if ((surface != 0 && !surfaces_.contains(surface)) ||
      !ResolveCreationParam(memory, creation_param, creation_param_size,
                            &param) ||
      param.param.output_mode != kSbPlayerOutputModePunchOut) {
    return 0;
  }
  // Starboard may read the features in SbPlayerCreate(), and
  // GetExperimentalFeaturesForCurrentThread() SB_CHECKs that they were set on
  // this thread. An empty set is harmless where they aren't read.
  const auto* api = ExperimentalFeaturesApi();
  if (api && !g_experimental_features_set) {
    const StarboardExtensionExperimentalFeatures empty{nullptr, 0};
    api->SetExperimentalFeaturesForCurrentThread(&empty);
    g_experimental_features_set = true;
  }

  const uint32_t id = next_player_++;
  auto player = std::make_unique<Player>();
  player->context = std::make_unique<CallbackContext>(events_, weak_this_, id);
  player->sb_player =
      sbplayer_->Create(kSbWindowInvalid, &param.param, &DeallocateSampleCB,
                        &DecoderStatusCB, &PlayerStatusCB, &PlayerErrorCB,
                        player->context.get(), DecodeTargetProvider());
  if (!SbPlayerIsValid(player->sb_player)) {
    return 0;
  }
  player->surface = surface;
  ApplyBounds(*player);
  players_[id] = std::move(player);
  return id;
}

void WasmPlayerHost::Destroy(uint32_t player) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = players_.find(player);
  if (it == players_.end()) {
    return;
  }
  std::unique_ptr<Player> destroyed = std::move(it->second);
  players_.erase(it);
  {
    base::AutoLock lock(destroyed->context->lock);
    destroyed->context->destroyed = true;
  }
  events_->DropPlayer(player);
  // SbPlayerDestroy() can block, e.g. until the platform releases the video
  // surface. Starboard may use the samples and the callback context
  // until it returns.
  destroy_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](::media::SbPlayerInterface* sbplayer,
             std::unique_ptr<Player> player) {
            sbplayer->Destroy(player->sb_player);
            player->context->events->Push({kWasmSbEventPlayerDestroyed,
                                           player->context->player, 0, 0, 0});
          },
          base::Unretained(&*sbplayer_), std::move(destroyed)));
}

bool WasmPlayerHost::Seek(uint32_t player,
                          double seek_to_timestamp,
                          int32_t ticket) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Player* p = Find(player);
  // Keeps the conversion to int64 defined; Starboard clamps negative times.
  if (!p ||
      !(std::abs(seek_to_timestamp) <= static_cast<double>(kMaxTimestamp))) {
    return false;
  }
  sbplayer_->Seek(p->sb_player,
                  base::Microseconds(static_cast<int64_t>(seek_to_timestamp)),
                  ticket);
  return true;
}

bool WasmPlayerHost::WriteSamples(GuestMemory& memory,
                                  uint32_t player,
                                  int32_t sample_type,
                                  uint32_t sample_infos,
                                  uint32_t count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Player* p = Find(player);
  const uint8_t* infos =
      memory.Array(sample_infos, count, sizeof(WasmSbPlayerSampleInfo));
  if (!p || !IsMediaType(sample_type) || !infos) {
    return false;
  }
  const SbMediaType type = static_cast<SbMediaType>(sample_type);
  // Bounds the copies below by what Starboard asked for, in samples. Each
  // sample's bytes lie within guest memory, which bounds them too.
  if (!NeedsData(*p, type) ||
      count > static_cast<uint32_t>(
                  std::max(0, sbplayer_->GetMaximumNumberOfSamplesPerWrite(
                                  p->sb_player, type)))) {
    return false;
  }

  std::vector<std::unique_ptr<SampleCopy>> copies;
  std::vector<SbPlayerSampleInfo> samples(count);
  for (uint32_t i = 0; i < count; ++i) {
    WasmSbPlayerSampleInfo info;
    memcpy(&info, infos + i * sizeof(info), sizeof(info));
    copies.push_back(std::make_unique<SampleCopy>());
    if (!ResolveSample(memory, type, info, copies.back().get(), &samples[i])) {
      return false;
    }
  }
  ClearNeedsData(*p, type);
  for (std::unique_ptr<SampleCopy>& copy : copies) {
    const void* buffer = copy->buffer.data();
    p->samples[buffer] = std::move(copy);
  }
  sbplayer_->WriteSamples(p->sb_player, type, samples.data(),
                          static_cast<int>(count));
  return true;
}

bool WasmPlayerHost::WriteEndOfStream(uint32_t player, int32_t stream_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Player* p = Find(player);
  if (!p || !IsMediaType(stream_type) ||
      !NeedsData(*p, static_cast<SbMediaType>(stream_type))) {
    return false;
  }
  ClearNeedsData(*p, static_cast<SbMediaType>(stream_type));
  sbplayer_->WriteEndOfStream(p->sb_player,
                              static_cast<SbMediaType>(stream_type));
  return true;
}

void WasmPlayerHost::SetBounds(uint32_t player,
                               int32_t z_index,
                               int32_t x,
                               int32_t y,
                               int32_t width,
                               int32_t height) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (Player* p = Find(player)) {
    p->bounds = Player::Bounds{z_index, x, y, width, height};
    ApplyBounds(*p);
  }
}

bool WasmPlayerHost::SetPlaybackRate(uint32_t player, double playback_rate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Player* p = Find(player);
  // Prevents SB_CHECK_GE and float-to-int UB in AudioTimeStretcher and
  // MediaTimeProviderImpl; Starboard rejects negative rates itself.
  if (!p || !(playback_rate == 0 || (std::abs(playback_rate) >= 1e-3 &&
                                     std::abs(playback_rate) <= 1e3))) {
    return false;
  }
  return sbplayer_->SetPlaybackRate(p->sb_player, playback_rate);
}

void WasmPlayerHost::SetVolume(uint32_t player, double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Player* p = Find(player);
  // Prevents float-to-int UB in the PulseAudio sink.
  if (p && volume >= 0 && volume <= 1) {
    sbplayer_->SetVolume(p->sb_player, volume);
  }
}

bool WasmPlayerHost::GetInfo(GuestMemory& memory,
                             uint32_t player,
                             uint32_t info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Player* p = Find(player);
  uint8_t* out = memory.Bytes(info, sizeof(WasmSbPlayerInfo));
  if (!p || !out) {
    return false;
  }
  SbPlayerInfo sb_info = {};
  sbplayer_->GetInfo(p->sb_player, &sb_info);
  // Zeroes the padding, which would otherwise leak host memory to the guest.
  WasmSbPlayerInfo wasm_info;
  memset(&wasm_info, 0, sizeof(wasm_info));
  wasm_info.current_media_timestamp = sb_info.current_media_timestamp;
  wasm_info.duration = sb_info.duration;
  wasm_info.start_date = sb_info.start_date;
  wasm_info.frame_width = sb_info.frame_width;
  wasm_info.frame_height = sb_info.frame_height;
  wasm_info.is_paused = sb_info.is_paused;
  wasm_info.volume = sb_info.volume;
  wasm_info.total_video_frames = sb_info.total_video_frames;
  wasm_info.dropped_video_frames = sb_info.dropped_video_frames;
  wasm_info.corrupted_video_frames = sb_info.corrupted_video_frames;
  wasm_info.playback_rate = sb_info.playback_rate;
  memcpy(out, &wasm_info, sizeof(wasm_info));
  return true;
}

bool WasmPlayerHost::GetAudioConfiguration(GuestMemory& memory,
                                           uint32_t player,
                                           int32_t index,
                                           uint32_t out_audio_configuration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Player* p = Find(player);
  uint8_t* out = memory.Bytes(out_audio_configuration,
                              sizeof(WasmSbMediaAudioConfiguration));
  SbMediaAudioConfiguration config = {};
  // A negative index reads before Starboard's configuration vector.
  if (!p || index < 0 || !out ||
      !sbplayer_->GetAudioConfiguration(p->sb_player, index, &config)) {
    return false;
  }
  WasmSbMediaAudioConfiguration wasm_config;
  memset(&wasm_config, 0, sizeof(wasm_config));
  wasm_config.connector = config.connector;
  wasm_config.latency = config.latency;
  wasm_config.coding_type = config.coding_type;
  wasm_config.number_of_channels = config.number_of_channels;
  memcpy(out, &wasm_config, sizeof(wasm_config));
  return true;
}

uint32_t WasmPlayerHost::GetErrorMessage(GuestMemory& memory,
                                         uint32_t player,
                                         uint32_t out,
                                         uint32_t out_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Player* p = Find(player);
  if (!p) {
    return 0;
  }
  std::string message;
  {
    base::AutoLock lock(p->context->lock);
    message = p->context->error_message;
  }
  const uint32_t length = static_cast<uint32_t>(
      std::min<size_t>(message.size(), std::numeric_limits<uint32_t>::max()));
  const uint32_t copied = std::min(length, out_size);
  if (copied == 0) {
    return length;
  }
  uint8_t* dst = memory.Bytes(out, copied);
  if (!dst) {
    return 0;
  }
  memcpy(dst, message.data(), copied);
  return length;
}

int32_t WasmPlayerHost::GetMaximumNumberOfSamplesPerWrite(uint32_t player,
                                                          int32_t sample_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Player* p = Find(player);
  if (!p || !IsMediaType(sample_type)) {
    return 0;
  }
  return sbplayer_->GetMaximumNumberOfSamplesPerWrite(
      p->sb_player, static_cast<SbMediaType>(sample_type));
}

int32_t WasmPlayerHost::GetPreferredOutputMode(GuestMemory& memory,
                                               uint32_t creation_param,
                                               uint32_t creation_param_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreationParam param;
  if (!ResolveCreationParam(memory, creation_param, creation_param_size,
                            &param)) {
    return kSbPlayerOutputModeInvalid;
  }
  return sbplayer_->GetPreferredOutputMode(&param.param);
}

int32_t WasmPlayerHost::CanPlayMimeAndKeySystem(GuestMemory& memory,
                                                uint32_t mime,
                                                uint32_t mime_size,
                                                uint32_t key_system,
                                                uint32_t key_system_size) {
  std::string mime_string;
  std::string key_system_string;
  if (!memory.ReadString(mime, mime_size, &mime_string) ||
      !memory.ReadString(key_system, key_system_size, &key_system_string)) {
    return kSbMediaSupportTypeNotSupported;
  }
  return ::media::GetSbMediaInterface()->CanPlayMimeAndKeySystem(
      mime_string.c_str(), key_system_string.c_str());
}

bool WasmPlayerHost::CanChangeType(GuestMemory& memory,
                                   uint32_t current_mime,
                                   uint32_t current_mime_size,
                                   uint32_t new_mime,
                                   uint32_t new_mime_size) {
  std::string current;
  std::string next;
  return memory.ReadString(current_mime, current_mime_size, &current) &&
         memory.ReadString(new_mime, new_mime_size, &next) &&
         ::media::GetSbMediaInterface()->CanChangeType(current.c_str(),
                                                       next.c_str());
}

size_t WasmPlayerHost::SampleCopiesForTesting(uint32_t player) {
  Player* p = Find(player);
  return p ? p->samples.size() : 0;
}

void WasmPlayerHost::OnVideoGeometryChange(
    const gfx::RectF& rect,
    gfx::OverlayTransform /*transform*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const uint32_t surface = geometry_receivers_.current_context();
  auto it = surfaces_.find(surface);
  const gfx::Rect bounds = gfx::ToEnclosedRect(rect);
  // The compositor reports the rect every frame.
  if (it == surfaces_.end() || it->second.rect == bounds) {
    return;
  }
  it->second.rect = bounds;
  for (auto& [id, player] : players_) {
    if (player->surface == surface) {
      ApplyBounds(*player);
    }
  }
}

// Starboard asks for data only when it can hold more, so writing only then
// bounds the sample copies the host keeps outside V8's heap; an OOM would take
// down the whole single-process app. The request isn't tied to a seek ticket
// on purpose: one from before a seek allows at most one write after it, which
// Starboard treats as part of the new seek, so the bound holds.
// static
bool WasmPlayerHost::NeedsData(Player& player, SbMediaType type) {
  base::AutoLock lock(player.context->lock);
  return player.context->needs_data[type];
}

// static
void WasmPlayerHost::ClearNeedsData(Player& player, SbMediaType type) {
  base::AutoLock lock(player.context->lock);
  player.context->needs_data[type] = false;
}

WasmPlayerHost::Player* WasmPlayerHost::Find(uint32_t player) {
  auto it = players_.find(player);
  return it == players_.end() ? nullptr : it->second.get();
}

void WasmPlayerHost::ApplyBounds(Player& player) {
  auto surface = surfaces_.find(player.surface);
  if (surface != surfaces_.end() && surface->second.rect) {
    const gfx::Rect& rect = *surface->second.rect;
    sbplayer_->SetBounds(player.sb_player,
                         player.bounds ? player.bounds->z_index : 0, rect.x(),
                         rect.y(), rect.width(), rect.height());
  } else if (player.bounds) {
    sbplayer_->SetBounds(player.sb_player, player.bounds->z_index,
                         player.bounds->x, player.bounds->y,
                         player.bounds->width, player.bounds->height);
  }
}

void WasmPlayerHost::OnDeallocateSample(uint32_t player,
                                        const void* sample_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (Player* p = Find(player)) {
    p->samples.erase(sample_buffer);
  }
}

// static
void WasmPlayerHost::PushEvent(void* context, WasmSbEvent event) {
  auto* ctx = static_cast<CallbackContext*>(context);
  base::AutoLock lock(ctx->lock);
  if (!ctx->destroyed) {
    event.player = ctx->player;
    ctx->events->Push(event);
  }
}

// static
void WasmPlayerHost::DeallocateSampleCB(SbPlayer /*player*/,
                                        void* context,
                                        const void* sample_buffer) {
  const auto* ctx = static_cast<const CallbackContext*>(context);
  ctx->events->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&WasmPlayerHost::OnDeallocateSample, ctx->host,
                                ctx->player, sample_buffer));
}

// static
void WasmPlayerHost::DecoderStatusCB(SbPlayer /*player*/,
                                     void* context,
                                     SbMediaType type,
                                     SbPlayerDecoderState state,
                                     int ticket) {
  if (state == kSbPlayerDecoderStateNeedsData && IsMediaType(type)) {
    auto* ctx = static_cast<CallbackContext*>(context);
    base::AutoLock lock(ctx->lock);
    ctx->needs_data[type] = true;
  }
  PushEvent(context, {kWasmSbEventDecoderStatus, 0, type, state, ticket});
}

// static
void WasmPlayerHost::PlayerStatusCB(SbPlayer /*player*/,
                                    void* context,
                                    SbPlayerState state,
                                    int ticket) {
  PushEvent(context, {kWasmSbEventPlayerStatus, 0, state, ticket, 0});
}

// static
void WasmPlayerHost::PlayerErrorCB(SbPlayer player,
                                   void* context,
                                   SbPlayerError error,
                                   const char* message) {
  LOG(ERROR) << "SbPlayer error " << error << ": " << (message ? message : "");
  // Prevents an event for a player that create() never returns:
  // SbPlayerCreate() reports its failures this way too.
  if (!SbPlayerIsValid(player)) {
    return;
  }
  {
    auto* ctx = static_cast<CallbackContext*>(context);
    base::AutoLock lock(ctx->lock);
    ctx->error_message = message ? message : "";
  }
  PushEvent(context, {kWasmSbEventPlayerError, 0, error, 0, 0});
}

}  // namespace starboard_wasm
}  // namespace cobalt
