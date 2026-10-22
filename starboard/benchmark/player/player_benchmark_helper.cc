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
#include <chrono>
#include <limits>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/extension/experimental/experimental_features.h"
#include "starboard/system.h"

namespace starboard {
namespace benchmark {

SbPlayerBenchmarkHelper::SbPlayerBenchmarkHelper() {
  // Mirror what the media pipeline does before creating a player. Go through
  // the Starboard extension rather than the internal C++ function so that the
  // benchmarks also link against a shared Starboard library.
  auto* experimental_features_extension = static_cast<
      const StarboardExtensionExperimentalFeaturesConfigurationApi*>(
      SbSystemGetExtension(
          kStarboardExtensionExperimentalFeaturesConfigurationName));
  if (experimental_features_extension &&
      experimental_features_extension->version >= 1) {
    StarboardExtensionExperimentalFeatures experimental_features = {nullptr, 0};
    experimental_features_extension->SetExperimentalFeaturesForCurrentThread(
        &experimental_features);
  }
}

SbPlayerBenchmarkHelper::~SbPlayerBenchmarkHelper() {
  DestroyPlayer();
}

bool SbPlayerBenchmarkHelper::InitializePlayer(const char* audio_file,
                                               const char* video_file,
                                               SbPlayerOutputMode output_mode) {
  SbPlayerCreationParam creation_param = {};
  creation_param.drm_system = kSbDrmSystemInvalid;
  creation_param.output_mode = output_mode;

  {
    std::unique_lock<std::mutex> lock(mutex_);
    SB_DCHECK(player_ == kSbPlayerInvalid);

    error_occurred_ = false;
    // Set to kSbPlayerStateDestroyed as a placeholder to ensure we wait for the
    // Initialized callback
    player_state_ = kSbPlayerStateDestroyed;
    player_state_ticket_ = SB_PLAYER_INITIAL_TICKET;
    ticket_ = SB_PLAYER_INITIAL_TICKET;

    if (audio_file) {
      audio_reader_.reset(
          new VideoDmpReader(audio_file, VideoDmpReader::kEnableReadOnDemand));
      if (audio_reader_->number_of_audio_buffers() == 0) {
        SB_LOG(ERROR) << "Failed to load audio DMP: " << audio_file;
        return false;
      }
      audio_reader_->audio_stream_info().ConvertTo(
          &creation_param.audio_stream_info);
      audio_mime_ = audio_reader_->audio_mime_type();
      creation_param.audio_stream_info.mime = audio_mime_.c_str();
      next_audio_sample_index_ = 0;
      audio_eos_written_ = false;
    } else {
      creation_param.audio_stream_info.codec = kSbMediaAudioCodecNone;
    }

    if (video_file) {
      video_reader_.reset(
          new VideoDmpReader(video_file, VideoDmpReader::kEnableReadOnDemand));
      if (video_reader_->number_of_video_buffers() == 0) {
        SB_LOG(ERROR) << "Failed to load video DMP: " << video_file;
        return false;
      }
      video_reader_->video_stream_info().ConvertTo(
          &creation_param.video_stream_info);
      video_mime_ = video_reader_->video_mime_type();
      creation_param.video_stream_info.mime = video_mime_.c_str();
      next_video_sample_index_ = 0;
      video_eos_written_ = false;
    } else {
      creation_param.video_stream_info.codec = kSbMediaVideoCodecNone;
    }
  }

  SbPlayer player =
      SbPlayerCreate(GetWindow(), &creation_param, DummyDeallocateSampleFunc,
                     DecoderStatusCallback, PlayerStatusCallback,
                     DummyPlayerErrorFunc, this, GetDecoderTargetProvider());

  if (!SbPlayerIsValid(player)) {
    SB_LOG(ERROR) << "SbPlayerCreate failed";
    return false;
  }

  {
    std::unique_lock<std::mutex> lock(mutex_);
    player_ = player;
  }

  // Wait for the kSbPlayerStateInitialized callback
  if (!WaitForPlayerState(kSbPlayerStateInitialized,
                          SB_PLAYER_INITIAL_TICKET)) {
    SB_LOG(ERROR) << "Timeout waiting for kSbPlayerStateInitialized";
    return false;
  }

  // Call Seek(0) to start the playback pipelines and decoders!
  Seek(0);
  SbPlayerSetPlaybackRate(player_, 1.0);
  SbPlayerSetVolume(player_, 1.0);

  return true;
}

void SbPlayerBenchmarkHelper::DestroyPlayer() {
  SbPlayer player_to_destroy = kSbPlayerInvalid;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    if (player_ == kSbPlayerInvalid) {
      return;
    }
    player_to_destroy = player_;
    player_ = kSbPlayerInvalid;
  }
  SbPlayerDestroy(player_to_destroy);
  audio_reader_.reset();
  video_reader_.reset();
}

void SbPlayerBenchmarkHelper::FeedData(int64_t duration_us) {
  while (true) {
    bool write_audio = false;
    bool write_video = false;
    bool audio_done = false;
    bool video_done = false;

    {
      std::unique_lock<std::mutex> lock(mutex_);
      if (error_occurred_) {
        SB_LOG(ERROR) << "Error occurred during FeedData";
        break;
      }

      if (audio_reader_) {
        int64_t last_audio_ts = 0;
        if (next_audio_sample_index_ > 0) {
          last_audio_ts =
              audio_reader_
                  ->GetPlayerSampleInfo(kSbMediaTypeAudio,
                                        next_audio_sample_index_ - 1)
                  .timestamp;
        }
        if (audio_eos_written_ || last_audio_ts >= duration_us) {
          audio_done = true;
        } else if (audio_needs_data_) {
          write_audio = true;
        }
      } else {
        audio_done = true;
      }

      if (video_reader_) {
        int64_t last_video_ts = 0;
        if (next_video_sample_index_ > 0) {
          last_video_ts =
              video_reader_
                  ->GetPlayerSampleInfo(kSbMediaTypeVideo,
                                        next_video_sample_index_ - 1)
                  .timestamp;
        }
        if (video_eos_written_ || last_video_ts >= duration_us) {
          video_done = true;
        } else if (video_needs_data_) {
          write_video = true;
        }
      } else {
        video_done = true;
      }

      if (audio_done && video_done) {
        break;
      }

      if (!write_audio && !write_video) {
        cv_.wait(lock);
        continue;
      }
    }

    if (write_audio) {
      WriteSamples(kSbMediaTypeAudio);
    }
    if (write_video) {
      WriteSamples(kSbMediaTypeVideo);
    }
  }
}

void SbPlayerBenchmarkHelper::Seek(int64_t seek_time_us) {
  int ticket = PrepareSeek(seek_time_us);
  SbPlayerSeek(player_, seek_time_us, ticket);
}

int SbPlayerBenchmarkHelper::PrepareSeek(int64_t seek_time_us) {
  std::unique_lock<std::mutex> lock(mutex_);
  audio_eos_written_ = false;
  video_eos_written_ = false;
  audio_needs_data_ = false;
  video_needs_data_ = false;

  next_audio_sample_index_ = FindAudioStartIndex(seek_time_us);
  next_video_sample_index_ = FindVideoStartIndex(seek_time_us);

  ticket_ = GetNextTicket();
  return ticket_;
}

bool SbPlayerBenchmarkHelper::WaitForPlayerState(SbPlayerState state,
                                                 int ticket,
                                                 int64_t timeout_us) {
  std::unique_lock<std::mutex> lock(mutex_);
  auto timeout = std::chrono::microseconds(timeout_us);
  return cv_.wait_for(lock, timeout, [this, state, ticket] {
    return player_state_ == state && player_state_ticket_ == ticket;
  });
}

void SbPlayerBenchmarkHelper::WriteSamples(SbMediaType type) {
  SbPlayer player = kSbPlayerInvalid;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    player = player_;
  }
  if (!SbPlayerIsValid(player)) {
    return;
  }

  int max_samples = SbPlayerGetMaximumNumberOfSamplesPerWrite(player, type);
  if (max_samples <= 0) {
    max_samples = 1;
  }

  // Collect the samples under the lock, but call into the player without
  // holding it: the player callbacks acquire the same mutex, so holding it
  // across SbPlayerWriteSamples() would deadlock with an implementation that
  // invokes the callbacks synchronously.
  bool write_end_of_stream = false;
  std::vector<SbPlayerSampleInfo> sample_infos;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    const bool is_audio = type == kSbMediaTypeAudio;
    VideoDmpReader* reader =
        is_audio ? audio_reader_.get() : video_reader_.get();
    if (!reader) {
      return;
    }
    size_t& next_sample_index =
        is_audio ? next_audio_sample_index_ : next_video_sample_index_;
    bool& eos_written = is_audio ? audio_eos_written_ : video_eos_written_;
    bool& needs_data = is_audio ? audio_needs_data_ : video_needs_data_;
    const size_t total_samples = is_audio ? reader->number_of_audio_buffers()
                                          : reader->number_of_video_buffers();

    if (next_sample_index >= total_samples) {
      if (eos_written) {
        return;
      }
      eos_written = true;
      write_end_of_stream = true;
    } else {
      const size_t samples_to_write = std::min(
          static_cast<size_t>(max_samples), total_samples - next_sample_index);
      sample_infos.reserve(samples_to_write);
      for (size_t i = 0; i < samples_to_write; ++i) {
        sample_infos.push_back(
            reader->GetPlayerSampleInfo(type, next_sample_index++));
      }
    }
    needs_data = false;
  }

  if (write_end_of_stream) {
    SbPlayerWriteEndOfStream(player, type);
  } else {
    SbPlayerWriteSamples(player, type, sample_infos.data(),
                         static_cast<int>(sample_infos.size()));
  }
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
  size_t num_samples = video_reader_->number_of_video_buffers();
  size_t best_index = 0;
  for (size_t i = 0; i < num_samples; ++i) {
    auto sample_info = video_reader_->GetPlayerSampleInfo(kSbMediaTypeVideo, i);
    if (sample_info.timestamp <= target_time_us) {
      if (sample_info.video_sample_info.is_key_frame) {
        best_index = i;
      }
    } else {
      break;
    }
  }
  return best_index;
}

void SbPlayerBenchmarkHelper::GetCreationParam(
    SbPlayerCreationParam* out_creation_param,
    SbPlayerOutputMode output_mode) {
  SB_DCHECK(out_creation_param);

  *out_creation_param = {};
  out_creation_param->drm_system = kSbDrmSystemInvalid;
  out_creation_param->output_mode = output_mode;

  static const uint8_t kAacAudioSpecificConfig[16] = {18, 16};
  out_creation_param->audio_stream_info.codec = kSbMediaAudioCodecAac;
  out_creation_param->audio_stream_info.mime = "audio/mp4";
  out_creation_param->audio_stream_info.number_of_channels = 2;
  out_creation_param->audio_stream_info.samples_per_second = 44100;
  out_creation_param->audio_stream_info.bits_per_sample = 16;
  out_creation_param->audio_stream_info.audio_specific_config =
      kAacAudioSpecificConfig;
  out_creation_param->audio_stream_info.audio_specific_config_size =
      sizeof(kAacAudioSpecificConfig);

  out_creation_param->video_stream_info.codec = kSbMediaVideoCodecH264;
  out_creation_param->video_stream_info.mime = "video/mp4";
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

void SbPlayerBenchmarkHelper::DummyDeallocateSampleFunc(
    SbPlayer player,
    void* context,
    const void* sample_buffer) {}

void SbPlayerBenchmarkHelper::DecoderStatusCallback(SbPlayer player,
                                                    void* context,
                                                    SbMediaType type,
                                                    SbPlayerDecoderState state,
                                                    int ticket) {
  SbPlayerBenchmarkHelper* helper =
      static_cast<SbPlayerBenchmarkHelper*>(context);
  std::unique_lock<std::mutex> lock(helper->mutex_);
  if (ticket != helper->ticket_) {
    return;
  }
  if (type == kSbMediaTypeAudio) {
    helper->audio_needs_data_ = (state == kSbPlayerDecoderStateNeedsData);
    helper->audio_ticket_ = ticket;
  } else if (type == kSbMediaTypeVideo) {
    helper->video_needs_data_ = (state == kSbPlayerDecoderStateNeedsData);
    helper->video_ticket_ = ticket;
  }
  helper->cv_.notify_all();
}

void SbPlayerBenchmarkHelper::PlayerStatusCallback(SbPlayer player,
                                                   void* context,
                                                   SbPlayerState state,
                                                   int ticket) {
  SbPlayerBenchmarkHelper* helper =
      static_cast<SbPlayerBenchmarkHelper*>(context);
  std::unique_lock<std::mutex> lock(helper->mutex_);
  if (ticket != helper->ticket_) {
    return;
  }
  helper->player_state_ = state;
  helper->player_state_ticket_ = ticket;
  helper->cv_.notify_all();
}

void SbPlayerBenchmarkHelper::DummyPlayerErrorFunc(SbPlayer player,
                                                   void* context,
                                                   SbPlayerError error,
                                                   const char* message) {
  SB_LOG(ERROR) << "Player error " << error << ": " << (message ? message : "");
  SbPlayerBenchmarkHelper* helper =
      static_cast<SbPlayerBenchmarkHelper*>(context);
  // The benchmarks that only exercise creation and destruction pass a null
  // context.
  if (!helper) {
    return;
  }
  helper->error_occurred_ = true;
  helper->cv_.notify_all();
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
  waiter->cv_.notify_all();
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
  waiter->cv_.notify_all();
}

bool PlayerInitializationWaiter::WaitForInitialization(int64_t timeout_us) {
  std::unique_lock<std::mutex> lock(mutex_);
  cv_.wait_for(lock, std::chrono::microseconds(timeout_us),
               [this] { return initialized_ || error_occurred_; });
  return initialized_ && !error_occurred_;
}

}  // namespace benchmark
}  // namespace starboard
