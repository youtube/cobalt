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

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/decode_target.h"
#include "starboard/extension/experimental/experimental_features.h"
#include "starboard/system.h"

namespace starboard {

SbPlayerBenchmarkHelper::SbPlayerBenchmarkHelper() {
  ConfigureExperimentalFeaturesForCurrentThread();
}

SbPlayerBenchmarkHelper::~SbPlayerBenchmarkHelper() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  DestroyPlayer();
}

void SbPlayerBenchmarkHelper::ConfigureExperimentalFeaturesForCurrentThread()
    const {
  // Mirror what the media pipeline does before creating a player. Go through
  // the Starboard extension rather than the internal C++ function so that the
  // benchmarks also link against a shared Starboard library.
  auto* experimental_features_extension = static_cast<
      const StarboardExtensionExperimentalFeaturesConfigurationApi*>(
      SbSystemGetExtension(
          kStarboardExtensionExperimentalFeaturesConfigurationName));
  if (experimental_features_extension &&
      strcmp(experimental_features_extension->name,
             kStarboardExtensionExperimentalFeaturesConfigurationName) == 0 &&
      experimental_features_extension->version >= 1) {
    StarboardExtensionExperimentalFeatures features = {nullptr, 0};
    experimental_features_extension->SetExperimentalFeaturesForCurrentThread(
        &features);
  }
}

SbPlayerBenchmarkHelper::InitResult SbPlayerBenchmarkHelper::InitializePlayer(
    const char* audio_file,
    const char* video_file,
    SbPlayerOutputMode output_mode) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(player_ == kSbPlayerInvalid);

  ConfigureExperimentalFeaturesForCurrentThread();

  SbPlayerCreationParam creation_param = {};
  creation_param.drm_system = kSbDrmSystemInvalid;
  creation_param.output_mode = output_mode;
  // The mime strings have to outlive the SbPlayerCreate() call below.
  std::string audio_mime;
  std::string video_mime;

  // Parse DMP files upfront (kDisableReadOnDemand) so lazy file reads and
  // sample allocations never happen inside timed benchmark iterations.
  if (audio_file) {
    audio_reader_ = std::make_unique<VideoDmpReader>(
        audio_file, VideoDmpReader::kDisableReadOnDemand);
    if (audio_reader_->number_of_audio_buffers() == 0) {
      SB_LOG(ERROR) << "Failed to load audio DMP: " << audio_file;
      DestroyPlayer();
      return InitResult::kFailed;
    }
    audio_reader_->audio_stream_info().ConvertTo(
        &creation_param.audio_stream_info);
    audio_mime = audio_reader_->audio_mime_type();
    creation_param.audio_stream_info.mime = audio_mime.c_str();
  } else {
    creation_param.audio_stream_info.codec = kSbMediaAudioCodecNone;
  }

  if (video_file) {
    video_reader_ = std::make_unique<VideoDmpReader>(
        video_file, VideoDmpReader::kDisableReadOnDemand);
    if (video_reader_->number_of_video_buffers() == 0) {
      SB_LOG(ERROR) << "Failed to load video DMP: " << video_file;
      DestroyPlayer();
      return InitResult::kFailed;
    }
    video_reader_->video_stream_info().ConvertTo(
        &creation_param.video_stream_info);
    video_mime = video_reader_->video_mime_type();
    creation_param.video_stream_info.mime = video_mime.c_str();
  } else {
    creation_param.video_stream_info.codec = kSbMediaVideoCodecNone;
  }

  if (!IsOutputModeSupported(creation_param)) {
    DestroyPlayer();
    return InitResult::kOutputModeNotSupported;
  }

  output_mode_ = output_mode;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    error_occurred_ = false;
    received_player_states_.clear();
    presenting_time_.reset();
    audio_needs_data_ = false;
    video_needs_data_ = false;
    ticket_ = SB_PLAYER_INITIAL_TICKET;
  }

  SbPlayer player =
      SbPlayerCreate(GetWindow(), &creation_param, DummyDeallocateSampleFunc,
                     DecoderStatusCallback, PlayerStatusCallback,
                     PlayerErrorCallback, this, GetDecoderTargetProvider());

  if (!SbPlayerIsValid(player)) {
    SB_LOG(ERROR) << "SbPlayerCreate failed";
    DestroyPlayer();
    return InitResult::kFailed;
  }

  player_ = player;

  if (!WaitForPlayerState(kSbPlayerStateInitialized,
                          SB_PLAYER_INITIAL_TICKET)) {
    SB_LOG(ERROR) << "Player failed to reach kSbPlayerStateInitialized";
    DestroyPlayer();
    return InitResult::kFailed;
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
  audio_reader_.reset();
  video_reader_.reset();
}

bool SbPlayerBenchmarkHelper::FeedUntilPresenting(
    Clock::time_point* out_presenting_time,
    int64_t timeout_us) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  // In decode-to-texture mode, platforms without an internal video renderer
  // sink (such as Linux) only advance VideoRendererImpl and release cached
  // frames when SbPlayerGetCurrentFrame() is polled. Poll at ~60 Hz while
  // feeding/waiting so decoder buffers can drain.
  constexpr auto kDecodeTargetPollInterval = std::chrono::milliseconds(16);
  const bool poll_decode_target =
      output_mode_ == kSbPlayerOutputModeDecodeToTexture;
  const auto timeout = std::chrono::microseconds(timeout_us);
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
      if (poll_decode_target && now >= next_decode_target_poll) {
        need_decode_target_poll = true;
        next_decode_target_poll = now + kDecodeTargetPollInterval;
      }

      if (!write_audio && !write_video && !need_decode_target_poll) {
        if (now >= deadline) {
          SB_LOG(ERROR) << "Timed out waiting for kSbPlayerStatePresenting"
                        << " (audio_needs_data=" << audio_needs_data_
                        << ", video_needs_data=" << video_needs_data_ << ")";
          return false;
        }
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

void SbPlayerBenchmarkHelper::Seek(int64_t seek_time_us) {
  int ticket = PrepareSeek(seek_time_us);
  SbPlayerSeek(player_, seek_time_us, ticket);
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
                                                 int64_t timeout_us) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  std::unique_lock<std::mutex> lock(mutex_);
  auto timeout = std::chrono::microseconds(timeout_us);
  condition_variable_.wait_for(lock, timeout, [this, state, ticket] {
    return error_occurred_ ||
           (ticket == ticket_ && received_player_states_.count(state) > 0);
  });
  return !error_occurred_ && ticket == ticket_ &&
         received_player_states_.count(state) > 0;
}

void SbPlayerBenchmarkHelper::WriteSamples(SbMediaType type) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  if (!SbPlayerIsValid(player_)) {
    return;
  }

  const bool is_audio = type == kSbMediaTypeAudio;
  VideoDmpReader* reader = is_audio ? audio_reader_.get() : video_reader_.get();
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
  fake_graphics_context_provider_.RunOnGlesContextThread([player]() {
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
  helper->condition_variable_.notify_all();
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

bool PlayerInitializationWaiter::WaitForInitialization(int64_t timeout_us) {
  std::unique_lock<std::mutex> lock(mutex_);
  condition_variable_.wait_for(
      lock, std::chrono::microseconds(timeout_us),
      [this] { return initialized_ || error_occurred_; });
  return initialized_ && !error_occurred_;
}

bool PlayerInitializationWaiter::HasError() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return error_occurred_;
}

}  // namespace starboard
