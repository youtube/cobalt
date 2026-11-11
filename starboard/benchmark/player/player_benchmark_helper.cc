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

#include "starboard/benchmark/player/player_benchmark_helper.h"

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"
#include "starboard/decode_target.h"
#include "starboard/extension/experimental/experimental_features.h"
#include "starboard/system.h"

namespace starboard {
namespace {

// Returns true if |filename| can be opened from the directory that
// ResolveTestFileName() in
// starboard/shared/starboard/player/file_cache_reader.cc picks for it. Keep the
// two in sync.
// VideoDmpReader SB_CHECKs that the file exists, so checking first turns a
// missing file into an error instead of a crash.
bool TestFileExists(const char* filename) {
  std::vector<char> content_path(kSbFileMaxPath + 1);
  if (!SbSystemGetPath(kSbSystemPathContentDirectory, content_path.data(),
                       content_path.size())) {
    return false;
  }
  const std::string content(content_path.data());
  const std::string sep(1, kSbFileSepChar);
  const std::string path_inside_content = sep + "test" + sep + "starboard" +
                                          sep + "shared" + sep + "starboard" +
                                          sep + "player" + sep + "testdata";
  const size_t last_sep = content.find_last_of(kSbFileSepChar);
  const std::string candidates[] = {
      content + path_inside_content,
      content.substr(0, last_sep) + sep + "starboard" + sep +
          content.substr(last_sep + 1) + path_inside_content,
      "/sdcard/chromium_tests_root" + path_inside_content,
  };
  for (const std::string& directory : candidates) {
    struct stat info;
    if (stat(directory.c_str(), &info) == 0 && S_ISDIR(info.st_mode)) {
      const std::string path = directory + sep + filename;
      return stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode) &&
             access(path.c_str(), R_OK) == 0;
    }
  }
  return false;
}

}  // namespace

SbPlayerBenchmarkHelper::SbPlayerBenchmarkHelper(
    FakeGraphicsContextProvider* graphics_context_provider)
    : owned_graphics_context_provider_(
          graphics_context_provider
              ? nullptr
              : std::make_unique<FakeGraphicsContextProvider>()),
      graphics_context_provider_(graphics_context_provider
                                     ? graphics_context_provider
                                     : owned_graphics_context_provider_.get()) {
  ConfigureExperimentalFeaturesForCurrentThread({});
}

SbPlayerBenchmarkHelper::~SbPlayerBenchmarkHelper() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  DestroyPlayer();
}

bool SbPlayerBenchmarkHelper::LoadDmpFiles(const char* audio_file,
                                           const char* video_file,
                                           std::string* out_error) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(out_error);
  SB_DCHECK(!SbPlayerIsValid(player_));

  // Parse DMP files upfront (kDisableReadOnDemand) and cache them by name so
  // lazy file reads, sample allocations, and multi-clip switching never happen
  // inside timed iterations.
  if (!audio_file) {
    audio_reader_ = nullptr;
    audio_mime_ = nullptr;
  } else {
    auto it = loaded_audio_dmps_.find(audio_file);
    if (it == loaded_audio_dmps_.end()) {
      if (!TestFileExists(audio_file)) {
        *out_error = std::string("Cannot find audio DMP file ") + audio_file;
        return false;
      }
      auto reader = std::make_unique<VideoDmpReader>(
          audio_file, VideoDmpReader::kDisableReadOnDemand);
      if (reader->number_of_audio_buffers() == 0) {
        *out_error = std::string("No audio samples in ") + audio_file;
        return false;
      }
      std::string mime = reader->audio_mime_type();
      it = loaded_audio_dmps_
               .emplace(audio_file,
                        LoadedDmp{std::move(reader), std::move(mime)})
               .first;
    }
    audio_reader_ = it->second.reader.get();
    audio_mime_ = &it->second.mime;
  }

  if (!video_file) {
    video_reader_ = nullptr;
    video_mime_ = nullptr;
  } else {
    auto it = loaded_video_dmps_.find(video_file);
    if (it == loaded_video_dmps_.end()) {
      if (!TestFileExists(video_file)) {
        *out_error = std::string("Cannot find video DMP file ") + video_file;
        return false;
      }
      auto reader = std::make_unique<VideoDmpReader>(
          video_file, VideoDmpReader::kDisableReadOnDemand);
      if (reader->number_of_video_buffers() == 0) {
        *out_error = std::string("No video samples in ") + video_file;
        return false;
      }
      std::string mime = reader->video_mime_type();
      it = loaded_video_dmps_
               .emplace(video_file,
                        LoadedDmp{std::move(reader), std::move(mime)})
               .first;
    }
    video_reader_ = it->second.reader.get();
    video_mime_ = &it->second.mime;
  }

  return true;
}

bool SbPlayerBenchmarkHelper::IsOutputModeSupportedForDmpFiles(
    SbPlayerOutputMode output_mode) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SbPlayerCreationParam creation_param;
  GetCreationParamForDmpFiles(output_mode, &creation_param);
  return IsOutputModeSupported(creation_param);
}

int64_t SbPlayerBenchmarkHelper::GetDurationUs() const {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  if (audio_reader_ && video_reader_) {
    return std::min(audio_reader_->audio_duration(),
                    video_reader_->video_duration());
  }
  if (video_reader_) {
    return video_reader_->video_duration();
  }
  return audio_reader_ ? audio_reader_->audio_duration() : 0;
}

SbPlayerBenchmarkHelper::InitResult SbPlayerBenchmarkHelper::InitializePlayer(
    const char* audio_file,
    const char* video_file,
    SbPlayerOutputMode output_mode,
    InitializeTimes* out_times) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK_EQ(player_, kSbPlayerInvalid);

  InitializeTimes local_times;
  InitializeTimes& times = out_times ? *out_times : local_times;
  times = {};
  {
    std::lock_guard<std::mutex> lock(mutex_);
    error_occurred_ = false;
    player_error_.reset();
    received_player_states_.clear();
    initialized_time_.reset();
    presenting_time_.reset();
    audio_needs_data_ = false;
    video_needs_data_ = false;
    ticket_ = SB_PLAYER_INITIAL_TICKET;
  }

  std::string error;
  if (!LoadDmpFiles(audio_file, video_file, &error)) {
    SB_LOG(ERROR) << error;
    return InitResult::kFailed;
  }

  SbPlayerCreationParam creation_param;
  GetCreationParamForDmpFiles(output_mode, &creation_param);
  if (!IsOutputModeSupported(creation_param)) {
    return InitResult::kOutputModeNotSupported;
  }

  output_mode_ = output_mode;

  times.create_start = Clock::now();
  SbPlayer player =
      SbPlayerCreate(GetWindow(), &creation_param, DummyDeallocateSampleFunc,
                     DecoderStatusCallback, PlayerStatusCallback,
                     PlayerErrorCallback, this, GetDecoderTargetProvider());

  if (!SbPlayerIsValid(player)) {
    SB_LOG(ERROR) << "SbPlayerCreate failed";
    output_mode_ = kSbPlayerOutputModeInvalid;
    return InitResult::kFailed;
  }

  times.created = Clock::now();
  player_ = player;

  if (!WaitForPlayerState(kSbPlayerStateInitialized,
                          SB_PLAYER_INITIAL_TICKET)) {
    SB_LOG(ERROR) << "Player failed to reach kSbPlayerStateInitialized";
    return InitResult::kFailed;
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    times.initialized = initialized_time_;
  }

  // Start the playback pipeline at 1.0x so seek benchmarks measure seeking an
  // actively playing player.
  Seek(0);
  SbPlayerSetPlaybackRate(player_, 1.0);

  return InitResult::kOk;
}

void SbPlayerBenchmarkHelper::DestroyPlayer() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  SbPlayer player_to_destroy = player_;
  player_ = kSbPlayerInvalid;
  output_mode_ = kSbPlayerOutputModeInvalid;

  if (SbPlayerIsValid(player_to_destroy)) {
    SbPlayerDestroy(player_to_destroy);
  }
}

std::optional<SbPlayerBenchmarkHelper::PlayerError>
SbPlayerBenchmarkHelper::GetPlayerError() {
  std::lock_guard<std::mutex> lock(mutex_);
  return player_error_;
}

// static
void SbPlayerBenchmarkHelper::GetCreationParam(
    SbPlayerCreationParam* out_creation_param,
    SbPlayerOutputMode output_mode) {
  SB_DCHECK(out_creation_param);

  *out_creation_param = {};
  out_creation_param->drm_system = kSbDrmSystemInvalid;
  out_creation_param->output_mode = output_mode;

  // 2-byte ISO/IEC 14496-3 AudioSpecificConfig for AAC-LC, 44.1 kHz, stereo.
  static constexpr uint8_t kAacAudioSpecificConfig[] = {0x12, 0x10};
  out_creation_param->audio_stream_info.codec = kSbMediaAudioCodecAac;
  out_creation_param->audio_stream_info.mime =
      "audio/mp4; codecs=\"mp4a.40.2\"; channels=2";
  out_creation_param->audio_stream_info.number_of_channels = 2;
  out_creation_param->audio_stream_info.samples_per_second = 44100;
  out_creation_param->audio_stream_info.bits_per_sample = 16;
  out_creation_param->audio_stream_info.audio_specific_config =
      kAacAudioSpecificConfig;
  out_creation_param->audio_stream_info.audio_specific_config_size =
      sizeof(kAacAudioSpecificConfig);

  out_creation_param->video_stream_info.codec = kSbMediaVideoCodecH264;
  out_creation_param->video_stream_info.mime =
      "video/mp4; codecs=\"avc1.4d402a\"; width=1920; height=1080; "
      "framerate=30";
  out_creation_param->video_stream_info.max_video_capabilities = "";
  out_creation_param->video_stream_info.color_metadata.bits_per_channel = 8;
  out_creation_param->video_stream_info.color_metadata.primaries =
      kSbMediaPrimaryIdBt709;
  out_creation_param->video_stream_info.color_metadata.transfer =
      kSbMediaTransferIdBt709;
  out_creation_param->video_stream_info.color_metadata.matrix =
      kSbMediaMatrixIdBt709;
  out_creation_param->video_stream_info.color_metadata.range =
      kSbMediaRangeIdLimited;
  out_creation_param->video_stream_info.frame_width = 1920;
  out_creation_param->video_stream_info.frame_height = 1080;
}

// static
bool SbPlayerBenchmarkHelper::IsOutputModeSupported(
    const SbPlayerCreationParam& creation_param) {
  return creation_param.output_mode != kSbPlayerOutputModeInvalid &&
         SbPlayerGetPreferredOutputMode(&creation_param) ==
             creation_param.output_mode;
}

void SbPlayerBenchmarkHelper::GetCreationParamForDmpFiles(
    SbPlayerOutputMode output_mode,
    SbPlayerCreationParam* out_creation_param) {
  *out_creation_param = {};
  out_creation_param->drm_system = kSbDrmSystemInvalid;
  out_creation_param->output_mode = output_mode;

  if (audio_reader_) {
    audio_reader_->audio_stream_info().ConvertTo(
        &out_creation_param->audio_stream_info);
    out_creation_param->audio_stream_info.mime = audio_mime_->c_str();
  } else {
    out_creation_param->audio_stream_info.codec = kSbMediaAudioCodecNone;
  }

  if (video_reader_) {
    video_reader_->video_stream_info().ConvertTo(
        &out_creation_param->video_stream_info);
    out_creation_param->video_stream_info.mime = video_mime_->c_str();
  } else {
    out_creation_param->video_stream_info.codec = kSbMediaVideoCodecNone;
  }
}

bool SbPlayerBenchmarkHelper::FeedUntilPresenting(
    Clock::time_point* out_presenting_time,
    Clock::duration timeout) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  // In decode-to-texture mode, platforms without an internal video renderer
  // sink (such as Linux) only advance VideoRendererImpl and release cached
  // frames when SbPlayerGetCurrentFrame() is polled. Poll at ~60 Hz while
  // feeding/waiting so decoder buffers can drain.
  constexpr auto kDecodeTargetPollInterval = std::chrono::milliseconds(16);
  const bool poll_decode_target =
      output_mode_ == kSbPlayerOutputModeDecodeToTexture;
  // The deadline is pushed out whenever samples are written, bounding how long
  // the player may go without making progress rather than total feed time.
  const auto initial_now = Clock::now();
  auto deadline = initial_now + timeout;
  auto next_decode_target_poll = initial_now + kDecodeTargetPollInterval;

  while (true) {
    bool write_audio = false;
    bool write_video = false;
    bool need_decode_target_poll = false;

    {
      std::unique_lock<std::mutex> lock(mutex_);
      if (error_occurred_) {
        SB_LOG(ERROR) << "Player reported an error while feeding data";
        return false;
      }
      if (received_player_states_.count(kSbPlayerStatePresenting) > 0) {
        if (out_presenting_time) {
          SB_DCHECK(presenting_time_.has_value());
          *out_presenting_time = *presenting_time_;
        }
        return true;
      }

      if (audio_reader_ && !audio_eos_written_ && audio_needs_data_) {
        write_audio = true;
      }
      if (video_reader_ && !video_eos_written_ && video_needs_data_) {
        write_video = true;
      }

      const auto now = Clock::now();
      if (!write_audio && !write_video && now >= deadline) {
        SB_LOG(ERROR) << "Timed out waiting for kSbPlayerStatePresenting"
                      << " (audio_needs_data=" << audio_needs_data_
                      << ", video_needs_data=" << video_needs_data_ << ")";
        return false;
      }
      if (poll_decode_target && now >= next_decode_target_poll) {
        need_decode_target_poll = true;
        next_decode_target_poll = now + kDecodeTargetPollInterval;
      }

      if (!write_audio && !write_video && !need_decode_target_poll) {
        const Clock::time_point wait_deadline =
            poll_decode_target ? std::min(deadline, next_decode_target_poll)
                               : deadline;
        condition_variable_.wait_until(lock, wait_deadline);
      }
    }

    if (write_audio) {
      WriteSamples(kSbMediaTypeAudio);
      deadline = Clock::now() + timeout;
    }
    if (write_video) {
      WriteSamples(kSbMediaTypeVideo);
      deadline = Clock::now() + timeout;
    }
    if (need_decode_target_poll) {
      PumpDecodeTarget();
    }
  }
}

int SbPlayerBenchmarkHelper::PrepareSeek(int64_t seek_time_us) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  const size_t audio_start_index = FindAudioStartIndex(seek_time_us);
  const size_t video_start_index = FindVideoStartIndex(seek_time_us);

  audio_eos_written_ = false;
  video_eos_written_ = false;
  next_audio_sample_index_ = audio_start_index;
  next_video_sample_index_ = video_start_index;

  std::lock_guard<std::mutex> lock(mutex_);
  audio_needs_data_ = false;
  video_needs_data_ = false;
  received_player_states_.clear();
  presenting_time_.reset();
  return ++ticket_;
}

bool SbPlayerBenchmarkHelper::WaitForPlayerState(SbPlayerState state,
                                                 int ticket,
                                                 Clock::duration timeout) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  std::unique_lock<std::mutex> lock(mutex_);
  condition_variable_.wait_for(lock, timeout, [this, state, ticket] {
    return error_occurred_ ||
           (ticket == ticket_ && received_player_states_.count(state) > 0);
  });
  return !error_occurred_ && ticket == ticket_ &&
         received_player_states_.count(state) > 0;
}

// static
void SbPlayerBenchmarkHelper::DummyDeallocateSampleFunc(
    SbPlayer player,
    void* context,
    const void* sample_buffer) {}

// static
void SbPlayerBenchmarkHelper::DecoderStatusCallback(SbPlayer player,
                                                    void* context,
                                                    SbMediaType type,
                                                    SbPlayerDecoderState state,
                                                    int ticket) {
  SbPlayerBenchmarkHelper* helper =
      static_cast<SbPlayerBenchmarkHelper*>(context);
  std::lock_guard<std::mutex> lock(helper->mutex_);
  if (ticket != helper->ticket_) {
    return;
  }

  if (type == kSbMediaTypeAudio) {
    helper->audio_needs_data_ = (state == kSbPlayerDecoderStateNeedsData);
  } else if (type == kSbMediaTypeVideo) {
    helper->video_needs_data_ = (state == kSbPlayerDecoderStateNeedsData);
  }

  helper->condition_variable_.notify_all();
}

// static
void SbPlayerBenchmarkHelper::PlayerStatusCallback(SbPlayer player,
                                                   void* context,
                                                   SbPlayerState state,
                                                   int ticket) {
  SbPlayerBenchmarkHelper* helper =
      static_cast<SbPlayerBenchmarkHelper*>(context);
  std::lock_guard<std::mutex> lock(helper->mutex_);
  if (ticket != helper->ticket_) {
    return;
  }

  helper->received_player_states_.insert(state);
  if (state == kSbPlayerStateInitialized &&
      !helper->initialized_time_.has_value()) {
    helper->initialized_time_ = Clock::now();
  }
  if (state == kSbPlayerStatePresenting &&
      !helper->presenting_time_.has_value()) {
    helper->presenting_time_ = Clock::now();
  }

  helper->condition_variable_.notify_all();
}

// static
void SbPlayerBenchmarkHelper::PlayerErrorCallback(SbPlayer player,
                                                  void* context,
                                                  SbPlayerError error,
                                                  const char* message) {
  SB_LOG(ERROR) << "Player error " << error << ": " << (message ? message : "");
  SbPlayerBenchmarkHelper* helper =
      static_cast<SbPlayerBenchmarkHelper*>(context);

  // Set the flag under the mutex so that the notification cannot slip in
  // between a waiter's predicate check and its wait.
  std::lock_guard<std::mutex> lock(helper->mutex_);
  helper->error_occurred_ = true;
  if (!helper->player_error_.has_value()) {
    helper->player_error_ = PlayerError{error, message ? message : ""};
  }
  helper->condition_variable_.notify_all();
}

bool SbPlayerBenchmarkHelper::SetExperimentalFeatures(
    const std::map<std::string, int64_t>& features) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  return ConfigureExperimentalFeaturesForCurrentThread(features);
}

bool SbPlayerBenchmarkHelper::ConfigureExperimentalFeaturesForCurrentThread(
    const std::map<std::string, int64_t>& features) const {
  // Android's SbPlayerCreate() unconditionally calls
  // GetExperimentalFeaturesForCurrentThread(), which SB_CHECKs that
  // SetExperimentalFeaturesForCurrentThread() has been called on the current
  // thread (even when all experimental features use their default values). Go
  // through the Starboard extension rather than the internal C++ function so
  // that the benchmarks also link against a shared Starboard library.
  auto* experimental_features_extension = static_cast<
      const StarboardExtensionExperimentalFeaturesConfigurationApi*>(
      SbSystemGetExtension(
          kStarboardExtensionExperimentalFeaturesConfigurationName));
  if (!experimental_features_extension ||
      strcmp(experimental_features_extension->name,
             kStarboardExtensionExperimentalFeaturesConfigurationName) != 0 ||
      experimental_features_extension->version < 1) {
    return false;
  }

  // The entries only need to outlive the call, which copies them.
  std::vector<StarboardExperimentalFeatureEntry> entries;
  entries.reserve(features.size());
  for (const auto& [key, value] : features) {
    entries.push_back({key.c_str(), value});
  }
  StarboardExtensionExperimentalFeatures extension_features = {
      entries.empty() ? nullptr : entries.data(), entries.size()};
  experimental_features_extension->SetExperimentalFeaturesForCurrentThread(
      &extension_features);
  return true;
}

void SbPlayerBenchmarkHelper::Seek(int64_t seek_time_us) {
  int ticket = PrepareSeek(seek_time_us);
  SbPlayerSeek(player_, seek_time_us, ticket);
}

void SbPlayerBenchmarkHelper::WriteSamples(SbMediaType type) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  if (!SbPlayerIsValid(player_)) {
    return;
  }

  const bool is_audio = type == kSbMediaTypeAudio;
  VideoDmpReader* reader = is_audio ? audio_reader_ : video_reader_;
  if (!reader) {
    return;
  }

  int max_samples = SbPlayerGetMaximumNumberOfSamplesPerWrite(player_, type);
  if (max_samples <= 0) {
    max_samples = 1;
  }

  size_t& next_sample_index =
      is_audio ? next_audio_sample_index_ : next_video_sample_index_;
  bool& eos_written = is_audio ? audio_eos_written_ : video_eos_written_;
  const size_t total_samples = is_audio ? reader->number_of_audio_buffers()
                                        : reader->number_of_video_buffers();

  bool write_end_of_stream = false;
  std::vector<SbPlayerSampleInfo> sample_infos;
  if (next_sample_index >= total_samples) {
    if (eos_written) {
      return;
    }
    eos_written = true;
    write_end_of_stream = true;
  } else {
    const size_t samples_to_write = std::min(static_cast<size_t>(max_samples),
                                             total_samples - next_sample_index);
    sample_infos.reserve(samples_to_write);
    for (size_t i = 0; i < samples_to_write; ++i) {
      sample_infos.push_back(
          reader->GetPlayerSampleInfo(type, next_sample_index++));
    }
  }

  // Clear |needs_data| under |mutex_| before calling into the player, and
  // release |mutex_| before SbPlayerWriteSamples()/SbPlayerWriteEndOfStream()
  // so synchronous callback implementations cannot deadlock.
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_audio) {
      audio_needs_data_ = false;
    } else {
      video_needs_data_ = false;
    }
  }

  if (write_end_of_stream) {
    SbPlayerWriteEndOfStream(player_, type);
  } else {
    SbPlayerWriteSamples(player_, type, sample_infos.data(),
                         static_cast<int>(sample_infos.size()));
  }
}

void SbPlayerBenchmarkHelper::PumpDecodeTarget() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  if (output_mode_ != kSbPlayerOutputModeDecodeToTexture ||
      !SbPlayerIsValid(player_)) {
    return;
  }

  SbPlayer player = player_;
  graphics_context_provider_->RunOnGlesContextThread([player]() {
    SbDecodeTarget frame = SbPlayerGetCurrentFrame(player);
    if (SbDecodeTargetIsValid(frame)) {
      SbDecodeTargetRelease(frame);
    }
  });
}

size_t SbPlayerBenchmarkHelper::FindAudioStartIndex(int64_t target_time_us) {
  if (!audio_reader_) {
    return 0;
  }

  size_t num_samples = audio_reader_->number_of_audio_buffers();
  for (size_t i = 0; i < num_samples; ++i) {
    if (audio_reader_->GetPlayerSampleInfo(kSbMediaTypeAudio, i).timestamp >=
        target_time_us) {
      return i;
    }
  }

  return num_samples;
}

size_t SbPlayerBenchmarkHelper::FindVideoStartIndex(int64_t target_time_us) {
  if (!video_reader_) {
    return 0;
  }

  // Video access units are stored in decoding order, so only the timestamps of
  // key frames are guaranteed to be monotonic. Pick the last key frame at or
  // before the target time.
  size_t num_samples = video_reader_->number_of_video_buffers();
  size_t best_index = 0;
  for (size_t i = 0; i < num_samples; ++i) {
    auto sample_info = video_reader_->GetPlayerSampleInfo(kSbMediaTypeVideo, i);
    if (!sample_info.video_sample_info.is_key_frame) {
      continue;
    }
    if (sample_info.timestamp > target_time_us) {
      break;
    }
    best_index = i;
  }

  return best_index;
}

// static
void PlayerInitializationWaiter::PlayerStatusFunc(SbPlayer player,
                                                  void* context,
                                                  SbPlayerState state,
                                                  int ticket) {
  if (state != kSbPlayerStateInitialized) {
    return;
  }

  PlayerInitializationWaiter* waiter =
      static_cast<PlayerInitializationWaiter*>(context);
  std::lock_guard<std::mutex> lock(waiter->mutex_);
  waiter->initialized_ = true;
  waiter->condition_variable_.notify_all();
}

// static
void PlayerInitializationWaiter::PlayerErrorFunc(SbPlayer player,
                                                 void* context,
                                                 SbPlayerError error,
                                                 const char* message) {
  SB_LOG(ERROR) << "Player error " << error
                << " during initialization: " << (message ? message : "");
  PlayerInitializationWaiter* waiter =
      static_cast<PlayerInitializationWaiter*>(context);
  std::lock_guard<std::mutex> lock(waiter->mutex_);
  waiter->error_occurred_ = true;
  waiter->condition_variable_.notify_all();
}

bool PlayerInitializationWaiter::WaitForInitialization(
    SbPlayerBenchmarkHelper::Clock::duration timeout) {
  std::unique_lock<std::mutex> lock(mutex_);
  condition_variable_.wait_for(
      lock, timeout, [this] { return initialized_ || error_occurred_; });
  return initialized_ && !error_occurred_;
}

bool PlayerInitializationWaiter::HasError() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return error_occurred_;
}

}  // namespace starboard
