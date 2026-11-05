// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/audio_renderer_passthrough.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

#include "starboard/android/shared/audio_decoder_passthrough.h"
#include "starboard/android/shared/media_capabilities_cache.h"
#include "starboard/android/shared/media_codec_audio_decoder.h"
#include "starboard/common/check_op.h"
#include "starboard/common/media.h"
#include "starboard/common/string.h"
#include "starboard/common/thread_options.h"
#include "starboard/common/time.h"
#include "starboard/shared/starboard/media/mime_supportability_cache.h"
#include "third_party/jni_zero/jni_zero.h"

namespace starboard {
namespace {

// Soft limit to ensure that the user of AudioRendererPassthrough won't keep
// pushing data when there are enough decoded audio buffers.
constexpr int kMaxDecodedAudios = 64;

constexpr int64_t kAudioTrackUpdateInternal = 5'000;  // 5ms

constexpr int kPreferredBufferSizeInBytes = 16 * 1024;

// For IAMF, besides end of stream and AudioTrack being full, preroll is also
// considered finished once this duration of audio has been written to
// AudioTrack.  This allows playback to start when the upstream pushes less data
// than what is needed to fill AudioTrack.
constexpr int64_t kPrerollDurationUs = 300'000;  // 300ms

// AudioTrack doesn't start playing until the written data reaches its start
// threshold, which defaults to the whole buffer size.  For IAMF, the amount of
// data to buffer before playing is already decided by preroll (see
// |kPrerollDurationUs|), so lower the start threshold to let AudioTrack start
// as soon as Play() is called.  For compressed formats, a frame is one byte.
// AudioTrack.setStartThresholdInFrames() requires API level 31, which is always
// available as IAMF passthrough is only enabled on Android 16 (API level 36)
// or later (see player_components_factory.cc).
constexpr int kIamfStartThresholdInFrames = 1;

// Used only when num_samples_per_frame can't be parsed from the IAMF Codec
// Config OBU. 960 is 20ms of Opus at 48kHz.
constexpr int kIamfDefaultAudioFramesPerBuffer = 960;

constexpr int kIamfObuTypeCodecConfig = 0;

// Reads an IAMF leb128() value (at most 8 bytes) from |data| at |*offset|, and
// advances |*offset| past it. Returns false on malformed or truncated input.
bool ReadIamfLeb128(const uint8_t* data,
                    int size,
                    int* offset,
                    uint64_t* value) {
  constexpr int kMaxLeb128Bytes = 8;
  *value = 0;
  for (int i = 0; i < kMaxLeb128Bytes; ++i) {
    if (*offset >= size) {
      return false;
    }
    uint8_t byte = data[(*offset)++];
    *value |= static_cast<uint64_t>(byte & 0x7f) << (i * 7);
    if (!(byte & 0x80)) {
      return true;
    }
  }
  return false;
}

// Returns num_samples_per_frame of the first Codec Config OBU found in |data|,
// or 0 if there is none.  Every Audio Frame OBU in a temporal unit carries
// num_samples_per_frame samples, so this is also the frame count of one
// temporal unit (i.e. one input buffer).
//
// OBU syntax (IAMF spec section 3.2 and 3.6):
//   obu_header: obu_type(5) | obu_redundant_copy(1) |
//               obu_trimming_status_flag(1) | obu_extension_flag(1)
//               obu_size                          leb128()
//               [num_samples_to_trim_at_end       leb128()]  if trimming flag
//               [num_samples_to_trim_at_start     leb128()]  if trimming flag
//               [extension_header_size            leb128()]  if extension flag
//               [extension_header_bytes           ...     ]  if extension flag
//   codec_config_obu: codec_config_id             leb128()
//                     codec_id                    4 bytes
//                     num_samples_per_frame       leb128()
//                     audio_roll_distance         2 bytes
//                     decoder_config              ...
int ParseIamfNumSamplesPerFrame(const uint8_t* data, int size) {
  int offset = 0;
  while (offset < size) {
    const uint8_t header = data[offset++];
    const int obu_type = header >> 3;
    const bool trimming_flag = (header >> 1) & 1;
    const bool extension_flag = header & 1;

    uint64_t obu_size;
    if (!ReadIamfLeb128(data, size, &offset, &obu_size) ||
        obu_size > static_cast<uint64_t>(size - offset)) {
      return 0;
    }
    // |obu_size| counts all bytes after the obu_size field itself.
    const int obu_end = offset + static_cast<int>(obu_size);

    if (obu_type == kIamfObuTypeCodecConfig) {
      uint64_t value;
      if (trimming_flag) {
        if (!ReadIamfLeb128(data, obu_end, &offset, &value) ||
            !ReadIamfLeb128(data, obu_end, &offset, &value)) {
          return 0;
        }
      }
      if (extension_flag) {
        if (!ReadIamfLeb128(data, obu_end, &offset, &value) ||
            value > static_cast<uint64_t>(obu_end - offset)) {
          return 0;
        }
        offset += static_cast<int>(value);
      }
      uint64_t codec_config_id;
      if (!ReadIamfLeb128(data, obu_end, &offset, &codec_config_id)) {
        return 0;
      }
      constexpr int kCodecIdSize = 4;
      if (obu_end - offset < kCodecIdSize) {
        return 0;
      }
      offset += kCodecIdSize;
      uint64_t num_samples_per_frame;
      if (!ReadIamfLeb128(data, obu_end, &offset, &num_samples_per_frame) ||
          num_samples_per_frame == 0 ||
          num_samples_per_frame > static_cast<uint64_t>(INT32_MAX)) {
        return 0;
      }
      return static_cast<int>(num_samples_per_frame);
    }

    offset = obu_end;
  }
  return 0;
}

constexpr int kIamfObuTypeSequenceHeader = 31;

bool StartsWithIamfSequenceHeader(const uint8_t* data, int size) {
  return size > 0 && (data[0] >> 3) == kIamfObuTypeSequenceHeader;
}

// Returns a copy of |audio| with |config| prepended to its data.
DecodedAudio PrependIamfConfig(const DecodedAudio& audio,
                               const std::vector<uint8_t>& config) {
  const int config_size = static_cast<int>(config.size());
  const int audio_size = static_cast<int>(audio.size_in_bytes());
  DecodedAudio result(audio.channels(), audio.sample_type(), audio.timestamp(),
                      config_size + audio_size);
  memcpy(result.data(), config.data(), config_size);
  memcpy(result.data() + config_size, audio.data(), audio_size);
  return result;
}

// C++ rewrite of ExoPlayer function parseAc3SyncframeAudioSampleCount(), it
// works for AC-3, E-AC-3, and E-AC-3-JOC.
// The ExoPlayer implementation is based on
// https://www.etsi.org/deliver/etsi_ts/102300_102399/102366/01.04.01_60/ts_102366v010401p.pdf.
int ParseAc3SyncframeAudioSampleCount(const uint8_t* buffer, int size) {
  SB_CHECK(buffer);

  constexpr int kAudioSamplesPerAudioBlock = 256;
  // Each syncframe has 6 blocks that provide 256 new audio samples. See
  // subsection 4.1.
  constexpr int kAc3SyncFrameAudioSampleCount = 6 * kAudioSamplesPerAudioBlock;
  // Number of audio blocks per E-AC-3 syncframe, indexed by numblkscod.
  constexpr int kBlocksPerSyncFrameByNumblkscod[] = {1, 2, 3, 6};

  if (size < 6) {
    SB_LOG(WARNING) << "Invalid e/ac3 input buffer size " << size;
    return kAc3SyncFrameAudioSampleCount;
  }

  // Parse the bitstream ID for AC-3 and E-AC-3 (see subsections 4.3, E.1.2 and
  // E.1.3.1.6).
  const bool is_eac3 = ((buffer[5] & 0xF8) >> 3) > 10;
  if (is_eac3) {
    int fscod = (buffer[4] & 0xC0) >> 6;
    int numblkscod = fscod == 0x03 ? 3 : (buffer[4] & 0x30) >> 4;
    return kBlocksPerSyncFrameByNumblkscod[numblkscod] *
           kAudioSamplesPerAudioBlock;
  } else {
    return kAc3SyncFrameAudioSampleCount;
  }
}

}  // namespace

// static
NonNullResult<std::unique_ptr<AudioRendererPassthrough>>
AudioRendererPassthrough::Create(
    JobQueue* job_queue,
    const AudioStreamInfo& audio_stream_info,
    SbDrmSystem drm_system,
    bool enable_flush_during_seek,
    std::optional<int> tunnel_mode_audio_session_id) {
  std::unique_ptr<AudioDecoder> decoder;
  if (SbDrmSystemIsValid(drm_system)) {
    SB_LOG(INFO) << "Creating AudioDecoder as decryptor.";
    auto result = MediaCodecAudioDecoder::Create(
        job_queue, audio_stream_info, drm_system, enable_flush_during_seek);
    if (result) {
      decoder = std::move(result.value());
    } else {
      return Failure("Failed to create MediaCodecAudioDecoder: " +
                     result.error());
    }
  } else {
    SB_LOG(INFO) << "Creating AudioDecoderPassthrough.";
    decoder = std::make_unique<AudioDecoderPassthrough>(
        audio_stream_info.samples_per_second);
  }

  return std::make_unique<AudioRendererPassthrough>(
      PassKey<AudioRendererPassthrough>(), job_queue, audio_stream_info,
      std::move(decoder), &AudioTrack::Create, tunnel_mode_audio_session_id);
}

// static
std::unique_ptr<AudioRendererPassthrough>
AudioRendererPassthrough::CreateForTesting(
    JobQueue* job_queue,
    const AudioStreamInfo& audio_stream_info,
    std::unique_ptr<AudioDecoder> decoder,
    AudioTrackFactory audio_track_factory,
    std::optional<int> tunnel_mode_audio_session_id) {
  SB_CHECK(audio_track_factory);
  return std::make_unique<AudioRendererPassthrough>(
      PassKey<AudioRendererPassthrough>(), job_queue, audio_stream_info,
      std::move(decoder), std::move(audio_track_factory),
      tunnel_mode_audio_session_id);
}

// static
int AudioRendererPassthrough::ParseAc3SyncframeAudioSampleCountForTesting(
    const std::vector<uint8_t>& buffer) {
  return ParseAc3SyncframeAudioSampleCount(buffer.data(),
                                           static_cast<int>(buffer.size()));
}

AudioRendererPassthrough::AudioRendererPassthrough(
    PassKey<AudioRendererPassthrough>,
    JobQueue* job_queue,
    const AudioStreamInfo& audio_stream_info,
    std::unique_ptr<AudioDecoder> decoder,
    AudioTrackFactory audio_track_factory,
    std::optional<int> tunnel_mode_audio_session_id)
    : JobOwner(job_queue),
      audio_stream_info_(audio_stream_info),
      tunnel_mode_audio_session_id_(tunnel_mode_audio_session_id),
      decoder_(std::move(decoder)),
      audio_track_factory_(std::move(audio_track_factory)) {
  SB_CHECK(decoder_);
  SB_CHECK(audio_track_factory_);
  SB_CHECK(audio_stream_info_.codec == kSbMediaAudioCodecAc3 ||
           audio_stream_info_.codec == kSbMediaAudioCodecEac3 ||
           audio_stream_info_.codec == kSbMediaAudioCodecIamf);
}

AudioRendererPassthrough::~AudioRendererPassthrough() {
  SB_CHECK(BelongsToCurrentThread());

  SB_LOG(INFO) << "Force a seek to 0 to reset all states before destructing.";
  Seek(0);
}

void AudioRendererPassthrough::Initialize(const ErrorCB& error_cb,
                                          const PrerolledCB& prerolled_cb,
                                          const EndedCB& ended_cb) {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK(error_cb);
  SB_DCHECK(prerolled_cb);
  SB_DCHECK(ended_cb);
  SB_DCHECK(!error_cb_);
  SB_DCHECK(!prerolled_cb_);
  SB_DCHECK(!ended_cb_);

  error_cb_ = error_cb;
  prerolled_cb_ = prerolled_cb;
  ended_cb_ = ended_cb;

  decoder_->Initialize(
      std::bind(&AudioRendererPassthrough::OnDecoderOutput, this), error_cb);
}

void AudioRendererPassthrough::WriteSamples(const InputBuffers& input_buffers) {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK(!input_buffers.empty());
  SB_DCHECK(can_accept_more_data_.load());

  if (!audio_track_thread_) {
    audio_track_thread_ = JobThread::Create(
        "AudioPassthrough", ThreadOptions().SetPriority(ThreadPriority::kHigh));
    audio_track_thread_->Schedule(std::bind(
        &AudioRendererPassthrough::CreateAudioTrackAndStartProcessing, this));
  }

  can_accept_more_data_.store(false);

  decoder_->Decode(
      input_buffers,
      std::bind(&AudioRendererPassthrough::OnDecoderConsumed, this));
}

void AudioRendererPassthrough::WriteEndOfStream() {
  SB_CHECK(BelongsToCurrentThread());

  if (end_of_stream_written_) {
    SB_LOG(INFO) << "WriteEndOfStream() ignored as |end_of_stream_written_| is"
                 << " true.";
    return;
  }

  SB_LOG(INFO) << "WriteEndOfStream() called.";

  end_of_stream_written_ = true;

  if (audio_track_thread_) {
    decoder_->WriteEndOfStream();
    return;
  }

  SB_LOG(INFO) << "Audio eos reached without any samples written.";
  end_of_stream_played_.store(true);
  ended_cb_();
}

void AudioRendererPassthrough::SetVolume(double volume) {
  SB_CHECK(BelongsToCurrentThread());

  if (volume_ == volume) {
    SB_LOG(INFO) << "Volume already at " << volume;
    return;
  }

  SB_LOG(INFO) << "Set volume to " << volume;

  std::lock_guard scoped_lock(mutex_);
  volume_ = volume;
}

bool AudioRendererPassthrough::IsEndOfStreamWritten() const {
  SB_CHECK(BelongsToCurrentThread());

  return end_of_stream_written_;
}

bool AudioRendererPassthrough::IsEndOfStreamPlayed() const {
  SB_CHECK(BelongsToCurrentThread());

  return end_of_stream_played_.load();
}

bool AudioRendererPassthrough::CanAcceptMoreData() const {
  SB_CHECK(BelongsToCurrentThread());

  std::lock_guard scoped_lock(mutex_);
  return can_accept_more_data_.load() &&
         decoded_audios_.size() < kMaxDecodedAudios;
}

void AudioRendererPassthrough::Play() {
  SB_CHECK(BelongsToCurrentThread());

  if (!paused_) {
    SB_LOG(INFO) << "Already playing.";
    return;
  }

  SB_LOG(INFO) << "Play.";

  std::lock_guard scoped_lock(mutex_);
  paused_ = false;
}

void AudioRendererPassthrough::Pause() {
  SB_CHECK(BelongsToCurrentThread());

  if (paused_) {
    SB_LOG(INFO) << "Already paused.";
    return;
  }

  SB_LOG(INFO) << "Pause.";

  std::lock_guard scoped_lock(mutex_);
  paused_ = true;
}

void AudioRendererPassthrough::SetPlaybackRate(double playback_rate) {
  SB_CHECK(BelongsToCurrentThread());

  if (playback_rate > 0.0 && playback_rate != 1.0) {
    // TODO: Report unsupported playback rate as an error.
    SB_LOG(WARNING) << "Playback rate " << playback_rate << " is not supported"
                    << " and is set to 1.0.";
    playback_rate = 1.0;
  }

  if (playback_rate_ == playback_rate) {
    SB_LOG(INFO) << "Playback rate already at " << playback_rate;
    return;
  }

  SB_LOG(INFO) << "Change playback rate from " << playback_rate_ << " to "
               << playback_rate << ".";

  std::lock_guard scoped_lock(mutex_);
  playback_rate_ = playback_rate;
}

void AudioRendererPassthrough::Seek(int64_t seek_to_time) {
  SB_CHECK(BelongsToCurrentThread());

  SB_LOG(INFO) << "Seek to " << seek_to_time;

  decoder_->Reset();

  bool seek_to_time_set = false;
  if (audio_track_thread_) {
    audio_track_thread_->ScheduleAndWait(
        std::bind(&AudioRendererPassthrough::FlushAudioTrackAndStopProcessing,
                  this, seek_to_time));
    audio_track_thread_->Stop();
    // |seek_to_time_| is updated inside FlushAudioTrackAndStopProcessing(),
    // update the flag so we needn't set it again below.
    seek_to_time_set = true;
    // Destroy the audio track thread, it will be re-created during preroll.
    audio_track_thread_.reset();
  }

  CancelPendingJobs();

  std::lock_guard scoped_lock(mutex_);

  can_accept_more_data_.store(true);
  prerolled_.store(false);
  end_of_stream_played_.store(false);
  total_frames_written_ = 0;

  end_of_stream_written_ = false;

  stop_called_ = false;
  playback_head_position_when_stopped_ = 0;
  stopped_at_ = 0;
  first_audio_timestamp_ = -1;
  if (!seek_to_time_set) {
    seek_to_time_ = seek_to_time;
  }
  paused_ = true;
  decoded_audios_ = std::queue<DecodedAudio>();  // clear it
  decoded_audio_writing_in_progress_ = std::nullopt;
  decoded_audio_writing_offset_ = 0;
  total_frames_written_on_audio_track_thread_ = 0;
}

// This function can be called from *any* threads.
int64_t AudioRendererPassthrough::GetCurrentMediaTime(bool* is_playing,
                                                      bool* is_eos_played,
                                                      bool* is_underflow,
                                                      double* playback_rate) {
  SB_DCHECK(is_playing);
  SB_DCHECK(is_eos_played);
  SB_DCHECK(is_underflow);
  SB_DCHECK(playback_rate);

  std::lock_guard scoped_lock(mutex_);
  *is_playing = !paused_;
  *is_eos_played = end_of_stream_played_.load();
  *is_underflow = false;  // TODO: Support underflow
  *playback_rate = playback_rate_;

  if (!audio_track_) {
    return seek_to_time_;
  }

  int64_t audio_start_time;
  if (first_audio_timestamp_ > -1) {
    audio_start_time = first_audio_timestamp_;
  } else {
    audio_start_time = seek_to_time_;
  }

  int64_t playback_time;
  if (stop_called_) {
    // When AudioTrack::Stop() is called, the playback will continue until
    // all the frames written are played, as the AudioTrack is created in
    // MODE_STREAM.
    auto now = CurrentMonotonicTime();
    SB_DCHECK_GE(now, stopped_at_);
    auto time_elapsed = now - stopped_at_;
    int64_t frames_played =
        time_elapsed * audio_stream_info_.samples_per_second / 1'000'000LL;
    int64_t total_frames_played =
        frames_played + playback_head_position_when_stopped_;
    total_frames_played = std::min(total_frames_played, total_frames_written_);
    playback_time =
        audio_start_time + total_frames_played * 1'000'000LL /
                               audio_stream_info_.samples_per_second;
    return std::max(playback_time, seek_to_time_);
  }

  int64_t updated_at;
  auto playback_head_position = audio_track_->GetAudioTimestamp(&updated_at);
  if (playback_head_position <= 0) {
    // The playback is warming up, don't adjust the media time by the monotonic
    // system time.
    return std::max(audio_start_time, seek_to_time_);
  }

  // TODO: This may cause time regression, because the unadjusted time will be
  //       returned on pause, after an adjusted time has been returned.
  playback_time = audio_start_time + playback_head_position * 1'000'000LL /
                                         audio_stream_info_.samples_per_second;

  // When underlying AudioTrack is paused, we use returned playback time
  // directly. Note that we should not use |paused_| or |playback_rate_| here.
  // As we sync audio sink state on |audio_track_thread_|, when |paused_| is set
  // to false, the underlying AudioTrack may still be paused. In that case, the
  // returned playback time and last frame consumed time would be out of date.
  // For example, when resume the playback, if we call GetAudioTimestamp()
  // before calling AudioTrack.Play(), the returned playback time and last frame
  // consumed time would be the same as at when we pause the video.
  if (audio_track_paused_) {
    return std::max(playback_time, seek_to_time_);
  }

  // TODO: Cap this to the maximum frames written to the AudioTrack.
  auto now = CurrentMonotonicTime();
  SB_LOG_IF(WARNING, now < updated_at)
      << "now (" << now << ") is not greater than updated_at (" << updated_at
      << ").";
  SB_LOG_IF(WARNING, now - updated_at > 1'000'000LL)
      << "Elapsed time (" << now - updated_at
      << ") is greater than 1s. (playback_time " << playback_time << ")";

  playback_time += std::max<int64_t>(now - updated_at, 0);

  return std::max(playback_time, seek_to_time_);
}

void AudioRendererPassthrough::CreateAudioTrackAndStartProcessing() {
  SB_DCHECK(audio_track_thread_);
  SB_DCHECK(audio_track_thread_->BelongsToCurrentThread());
  SB_DCHECK(error_cb_);

  if (audio_track_) {
    SB_DCHECK(!update_status_and_write_data_token_);
    AudioTrackState initial_state;
    update_status_and_write_data_token_ = audio_track_thread_->Schedule(
        std::bind(&AudioRendererPassthrough::UpdateStatusAndWriteData, this,
                  initial_state));
    SB_LOG(INFO) << "|audio_track_| already created, start processing.";
    return;
  }

  SbMediaAudioCodingType coding_type;
  switch (audio_stream_info_.codec) {
    case kSbMediaAudioCodecAc3:
      coding_type = kSbMediaAudioCodingTypeAc3;
      break;
    case kSbMediaAudioCodecEac3:
      coding_type = kSbMediaAudioCodingTypeDolbyDigitalPlus;
      break;
    default:
      SB_DCHECK(audio_stream_info_.codec == kSbMediaAudioCodecIamf);
      coding_type = kSbMediaAudioCodingTypeIamfBaseProfileOpus;
      break;
  }
  std::unique_ptr<AudioTrack> audio_track = audio_track_factory_(
      coding_type,
      /*sample_type=*/std::nullopt,  // Not required in passthrough mode
      audio_stream_info_.number_of_channels,
      audio_stream_info_.samples_per_second, kPreferredBufferSizeInBytes,
      tunnel_mode_audio_session_id_,
      /*is_web_audio=*/false);

  if (!audio_track) {
    // In tunnel mode, there is no fallback to non-tunnel mode, as the video
    // decoder has already been created with the same audio session id.
    error_cb_(kSbPlayerErrorDecode,
              FormatString("Error creating AudioTrack for %s passthrough "
                           "(tunnel mode: %s).",
                           GetMediaAudioCodecName(audio_stream_info_.codec),
                           tunnel_mode_audio_session_id_ ? "true" : "false"));
    return;
  }

  if (audio_stream_info_.codec == kSbMediaAudioCodecIamf) {
    const int start_threshold =
        audio_track->SetStartThresholdInFrames(kIamfStartThresholdInFrames);
    SB_LOG_IF(WARNING, start_threshold == 0)
        << "Failed to set AudioTrack start threshold, it stays at "
        << audio_track->GetStartThresholdInFrames() << " frames.";
    SB_LOG_IF(INFO, start_threshold > 0)
        << "Set AudioTrack start threshold to " << start_threshold
        << " frames.";
  }

  {
    std::lock_guard scoped_lock(mutex_);
    audio_track_ = std::move(audio_track);
  }

  AudioTrackState initial_state;
  update_status_and_write_data_token_ = audio_track_thread_->Schedule(
      std::bind(&AudioRendererPassthrough::UpdateStatusAndWriteData, this,
                initial_state));
  SB_LOG(INFO) << "|audio_track_| created, start processing.";
}

void AudioRendererPassthrough::FlushAudioTrackAndStopProcessing(
    int64_t seek_to_time) {
  SB_DCHECK(audio_track_thread_);
  SB_DCHECK(audio_track_thread_->BelongsToCurrentThread());

  SB_LOG(INFO) << "Pause audio track and stop processing.";

  // Flushing of |audio_track_| and updating of |seek_to_time_| have to
  // be done together under lock to avoid |seek_to_time_| being added to a stale
  // playback head or vice versa in GetCurrentMediaTime().
  std::lock_guard scoped_lock(mutex_);

  // We have to reuse |audio_track_| instead of creating a new one, to
  // reduce output mode switching between PCM and e/ac3.  Otherwise a noticeable
  // silence can be observed after seeking on some audio receivers.
  // TODO: Consider reusing audio sink for non-passthrough playbacks, to see if
  //       it reduces latency after seeking.
  if (audio_track_) {
    audio_track_->PauseAndFlush();
  }
  seek_to_time_ = seek_to_time;
  paused_ = true;
  if (update_status_and_write_data_token_) {
    audio_track_thread_->RemoveJobByToken(&update_status_and_write_data_token_);
  }
}

void AudioRendererPassthrough::UpdateStatusAndWriteData(
    const AudioTrackState previous_state) {
  SB_DCHECK(audio_track_thread_);
  SB_DCHECK(audio_track_thread_->BelongsToCurrentThread());
  SB_DCHECK(error_cb_);
  SB_DCHECK(audio_track_);

  // Encoded passthrough bitstreams (e.g. AC3/E-AC3) cannot be seamlessly routed
  // across different sinks (e.g. Bluetooth or built-in speakers), so any device
  // change requires recreating the player to renegotiate audio capabilities.
  if (audio_track_->GetAndResetAudioDeviceChange() !=
      AudioDeviceChange::kNone) {
    SB_LOG(INFO) << "Audio device changed, raising a capability changed error "
                    "to restart playback.";
    ReportCapabilityChanged();
    audio_track_->PauseAndFlush();
    return;
  }

  AudioTrackState current_state;

  {
    std::lock_guard scoped_lock(mutex_);
    current_state.volume = volume_;
    current_state.paused = paused_;
    current_state.playback_rate = playback_rate_;

    if (!decoded_audio_writing_in_progress_ && !decoded_audios_.empty()) {
      decoded_audio_writing_in_progress_ = std::move(decoded_audios_.front());
      decoded_audios_.pop();
      decoded_audio_writing_offset_ = 0;

      // The IAMF descriptor OBUs are delivered out-of-band in
      // |audio_specific_config|, but the HAL decoder expects them in-band.
      // |total_frames_written_on_audio_track_thread_| is reset in Seek(), so
      // this is the first buffer after AudioTrack creation or a seek (the
      // AudioTrack is flushed on seek), where the descriptors are required.
      const auto& config = audio_stream_info_.audio_specific_config;
      if (audio_stream_info_.codec == kSbMediaAudioCodecIamf &&
          total_frames_written_on_audio_track_thread_ == 0 &&
          !decoded_audio_writing_in_progress_->is_end_of_stream() &&
          !config.empty() &&
          !StartsWithIamfSequenceHeader(
              decoded_audio_writing_in_progress_->data(),
              decoded_audio_writing_in_progress_->size_in_bytes())) {
        decoded_audio_writing_in_progress_ =
            PrependIamfConfig(*decoded_audio_writing_in_progress_, config);
        SB_LOG(INFO) << "Prepended " << config.size()
                     << " bytes of IAMF descriptors to the buffer at "
                     << decoded_audio_writing_in_progress_->timestamp();
      }
    }
  }

  if (previous_state.volume != current_state.volume) {
    audio_track_->SetVolume(current_state.volume);
  }
  if (previous_state.playing() != current_state.playing()) {
    if (current_state.playing()) {
      audio_track_->Play();
      audio_track_paused_ = false;
      SB_LOG(INFO) << "Played on AudioTrack thread.";
      std::lock_guard scoped_lock(mutex_);
      stop_called_ = false;
    } else {
      audio_track_->Pause();
      audio_track_paused_ = true;
      SB_LOG(INFO) << "Paused on AudioTrack thread.";
    }
  }

  bool fully_written = false;
  if (decoded_audio_writing_in_progress_) {
    if (decoded_audio_writing_in_progress_->is_end_of_stream()) {
      if (!prerolled_.exchange(true)) {
        SB_LOG(INFO) << "Prerolled due to end of stream.";
        prerolled_cb_();
      }
      std::lock_guard scoped_lock(mutex_);
      if (current_state.playing() && !stop_called_) {
        // TODO: Check if we can apply the same stop logic to non-passthrough.
        audio_track_->Stop();
        stop_called_ = true;
        playback_head_position_when_stopped_ =
            audio_track_->GetAudioTimestamp(&stopped_at_);
        total_frames_written_ = total_frames_written_on_audio_track_thread_;
        decoded_audio_writing_in_progress_ = std::nullopt;
        SB_LOG(INFO) << "Audio track stopped at " << stopped_at_
                     << ", playback head: "
                     << playback_head_position_when_stopped_;
      }
    } else {
      auto sample_buffer = decoded_audio_writing_in_progress_->data() +
                           decoded_audio_writing_offset_;
      auto samples_to_write =
          (decoded_audio_writing_in_progress_->size_in_bytes() -
           decoded_audio_writing_offset_);
      // |sync_time| is only used in tunnel mode, where it's written into the
      // A/V sync header of the buffer.  On a partial write, AudioTrack keeps
      // track of the remaining bytes of the current A/V sync packet, and
      // ignores |sync_time| when the rest of the buffer is written.  So it's
      // fine to always pass the timestamp of the buffer.
      auto sync_time = decoded_audio_writing_in_progress_->timestamp();
      int samples_written = audio_track_->WriteSample(
          make_span(sample_buffer, samples_to_write), sync_time);
      // Error code returned as negative value, like kAudioTrackErrorDeadObject.
      if (samples_written < 0) {
        if (samples_written == AudioTrack::kAudioTrackErrorDeadObject) {
          // Inform the audio end point change.
          SB_LOG(INFO)
              << "Write error for dead audio track, audio device capability "
                 "has likely changed. Restarting playback.";
          ReportCapabilityChanged();
        } else {
          // `kSbPlayerErrorDecode` is used for general SbPlayer error, there is
          // no error code corresponding to audio sink.
          error_cb_(
              kSbPlayerErrorDecode,
              FormatString("Error while writing frames: %d", samples_written));
          SB_LOG(INFO) << "Encountered kSbPlayerErrorDecode while writing "
                          "frames, error: "
                       << samples_written;
        }
        audio_track_->PauseAndFlush();
        return;
      }

      if (first_audio_timestamp_ < 0) {
        first_audio_timestamp_ = sync_time;
      }

      decoded_audio_writing_offset_ += samples_written;

      if (decoded_audio_writing_offset_ ==
          decoded_audio_writing_in_progress_->size_in_bytes()) {
        total_frames_written_on_audio_track_thread_ += frames_per_input_buffer_;
        decoded_audio_writing_in_progress_ = std::nullopt;
        decoded_audio_writing_offset_ = 0;
        fully_written = true;
      } else if (!prerolled_.exchange(true)) {
        // The audio sink no longer takes all the samples written to it.  Assume
        // that it has enough samples and preroll is finished.
        SB_LOG(INFO) << "Prerolled.";
        prerolled_cb_();
      }
    }
  }

  // For IAMF, preroll without waiting for AudioTrack to be full, as the
  // upstream may not push enough data to fill it.
  if (audio_stream_info_.codec == kSbMediaAudioCodecIamf &&
      !prerolled_.load()) {
    const int64_t written_duration =
        total_frames_written_on_audio_track_thread_ * 1'000'000LL /
        audio_stream_info_.samples_per_second;
    if (written_duration >= kPrerollDurationUs &&
        !prerolled_.exchange(true)) {
      SB_LOG(INFO) << "Prerolled with " << written_duration
                   << " us of audio written to AudioTrack.";
      prerolled_cb_();
    }
  }

  // EOS is handled on this thread instead of in GetCurrentMediaTime(), because
  // GetCurrentMediaTime() is not guaranteed to be called.
  if (stop_called_ && !end_of_stream_played_.load()) {
    auto time_elapsed = CurrentMonotonicTime() - stopped_at_;
    auto frames_played =
        time_elapsed * audio_stream_info_.samples_per_second / 1'000'000LL;
    if (frames_played + playback_head_position_when_stopped_ >=
        total_frames_written_on_audio_track_thread_) {
      end_of_stream_played_.store(true);
      ended_cb_();
      SB_LOG(INFO) << "Audio playback ended, UpdateStatusAndWriteData stopped.";
      return;
    }
  }

  update_status_and_write_data_token_ = audio_track_thread_->Schedule(
      std::bind(&AudioRendererPassthrough::UpdateStatusAndWriteData, this,
                current_state),
      fully_written ? 0 : kAudioTrackUpdateInternal);
}

void AudioRendererPassthrough::ReportCapabilityChanged() {
  // Invalidate caches so subsequent isTypeSupported() queries see live state.
  MediaCapabilitiesCache::GetInstance()->ClearCache();
  MimeSupportabilityCache::GetInstance()->ClearCachedMimeSupportabilities();

  error_cb_(kSbPlayerErrorCapabilityChanged, "Audio device capability changed");
}

// This function can be called from *any* threads.
void AudioRendererPassthrough::OnDecoderConsumed() {
  auto old_value = can_accept_more_data_.exchange(true);
  SB_DCHECK(!old_value);
}

// This function can be called from *any* threads.
void AudioRendererPassthrough::OnDecoderOutput() {
  int decoded_audio_sample_rate;
  auto decoded_audio = decoder_->Read(&decoded_audio_sample_rate);
  SB_DCHECK(decoded_audio);

  if (!decoded_audio->is_end_of_stream()) {
    SB_DCHECK_GT(decoded_audio->size_in_bytes(), 0);
    // We set |frames_per_input_buffer_| before adding first |decoded_audio|
    // into |decoded_audios_|. The usage of |frames_per_input_buffer_| in
    // UpdateStatusAndWriteData() from another thread only happens when there is
    // audio decoded, so it's thread-safe even if the code is not synchronized
    // using a lock.
    if (audio_stream_info_.codec == kSbMediaAudioCodecIamf) {
      // An IAMF input buffer is one temporal unit, which carries
      // num_samples_per_frame (from the Codec Config OBU) samples.  This
      // doesn't change during playback, so it's only parsed once.
      if (frames_per_input_buffer_ == 0) {
        const auto& config = audio_stream_info_.audio_specific_config;
        frames_per_input_buffer_ = ParseIamfNumSamplesPerFrame(
            config.data(), static_cast<int>(config.size()));
        if (frames_per_input_buffer_ == 0) {
          // The descriptors may also be delivered in-band.
          frames_per_input_buffer_ = ParseIamfNumSamplesPerFrame(
              decoded_audio->data(), decoded_audio->size_in_bytes());
        }
        if (frames_per_input_buffer_ == 0) {
          SB_LOG(WARNING) << "Failed to parse num_samples_per_frame from IAMF "
                             "Codec Config OBU, fallback to "
                          << kIamfDefaultAudioFramesPerBuffer;
          frames_per_input_buffer_ = kIamfDefaultAudioFramesPerBuffer;
        }
        SB_LOG(INFO) << "Got frames per input buffer "
                     << frames_per_input_buffer_;
      }
    } else if (frames_per_input_buffer_ == 0) {
      frames_per_input_buffer_ = ParseAc3SyncframeAudioSampleCount(
          decoded_audio->data(), decoded_audio->size_in_bytes());
      SB_LOG(INFO) << "Got frames per input buffer "
                   << frames_per_input_buffer_;
    } else {
      SB_DCHECK(frames_per_input_buffer_ ==
                ParseAc3SyncframeAudioSampleCount(
                    decoded_audio->data(), decoded_audio->size_in_bytes()));
    }
  }

  std::lock_guard scoped_lock(mutex_);
  decoded_audios_.push(std::move(*decoded_audio));
}

}  // namespace starboard
