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
#include <utility>

#include "starboard/android/shared/audio_decoder_passthrough.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/memory.h"

namespace starboard {
namespace android {
namespace shared {
namespace {

// Soft limit to ensure that the user of AudioRendererPassthrough won't keep
// pushing data when there are enough decoded audio buffers.
constexpr int kMaxDecodedAudios = 64;

constexpr SbTime kAudioTrackUpdateInternal = kSbTimeMillisecond * 5;

constexpr int kPreferredBufferSizeInBytes = 16 * 1024;
// TODO: Enable passthrough with tunnel mode.
constexpr int kTunnelModeAudioSessionId = -1;

// C++ rewrite of ExoPlayer function parseAc3SyncframeAudioSampleCount(), it
// works for AC-3, E-AC-3, and E-AC-3-JOC.
// The ExoPlayer implementation is based on
// https://www.etsi.org/deliver/etsi_ts/102300_102399/102366/01.04.01_60/ts_102366v010401p.pdf.
int ParseAc3SyncframeAudioSampleCount(const uint8_t* buffer, int size) {
  SB_DCHECK(buffer);

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

AudioRendererPassthrough::AudioRendererPassthrough(
    const SbMediaAudioSampleInfo& audio_sample_info,
    SbDrmSystem drm_system,
    bool enable_audio_device_callback)
    : audio_sample_info_(audio_sample_info),
      enable_audio_device_callback_(enable_audio_device_callback) {
  SB_DCHECK(audio_sample_info_.codec == kSbMediaAudioCodecAc3 ||
            audio_sample_info_.codec == kSbMediaAudioCodecEac3);
  if (SbDrmSystemIsValid(drm_system)) {
    SB_LOG(INFO) << "Creating AudioDecoder as decryptor.";
    scoped_ptr<AudioDecoder> audio_decoder(new AudioDecoder(
        audio_sample_info_.codec, audio_sample_info, drm_system));
    if (audio_decoder->is_valid()) {
      decoder_.reset(audio_decoder.release());
    }
  } else {
    SB_LOG(INFO) << "Creating AudioDecoderPassthrough.";
    decoder_.reset(
        new AudioDecoderPassthrough(audio_sample_info_.samples_per_second));
  }
}

AudioRendererPassthrough::~AudioRendererPassthrough() {
  SB_DCHECK(BelongsToCurrentThread());

  if (is_valid()) {
    SB_LOG(INFO) << "Force a seek to 0 to reset all states before destructing.";
    Seek(0);
  }
}

void AudioRendererPassthrough::Initialize(const ErrorCB& error_cb,
                                          const PrerolledCB& prerolled_cb,
                                          const EndedCB& ended_cb) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(error_cb);
  SB_DCHECK(prerolled_cb);
  SB_DCHECK(ended_cb);
  SB_DCHECK(!error_cb_);
  SB_DCHECK(!prerolled_cb_);
  SB_DCHECK(!ended_cb_);
  SB_DCHECK(decoder_);

  error_cb_ = error_cb;
  prerolled_cb_ = prerolled_cb;
  ended_cb_ = ended_cb;

  decoder_->Initialize(
      std::bind(&AudioRendererPassthrough::OnDecoderOutput, this), error_cb);
}

void AudioRendererPassthrough::WriteSample(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(input_buffer);
  SB_DCHECK(can_accept_more_data_.load());

  if (!audio_track_thread_) {
    audio_track_thread_.reset(
        new JobThread("AudioPassthrough", 0, kSbThreadPriorityHigh));
    audio_track_thread_->Schedule(std::bind(
        &AudioRendererPassthrough::CreateAudioTrackAndStartProcessing, this));
  }

  if (frames_per_input_buffer_ == 0) {
    frames_per_input_buffer_ = ParseAc3SyncframeAudioSampleCount(
        input_buffer->data(), input_buffer->size());
    SB_LOG(INFO) << "Got frames per input buffer " << frames_per_input_buffer_;
  }

  can_accept_more_data_.store(false);

  decoder_->Decode(
      input_buffer,
      std::bind(&AudioRendererPassthrough::OnDecoderConsumed, this));
}

void AudioRendererPassthrough::WriteEndOfStream() {
  SB_DCHECK(BelongsToCurrentThread());

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
  SB_DCHECK(BelongsToCurrentThread());

  if (volume_ == volume) {
    SB_LOG(INFO) << "Volume already at " << volume;
    return;
  }

  SB_LOG(INFO) << "Set volume to " << volume;

  ScopedLock scoped_lock(mutex_);
  volume_ = volume;
}

bool AudioRendererPassthrough::IsEndOfStreamWritten() const {
  SB_DCHECK(BelongsToCurrentThread());

  return end_of_stream_written_;
}

bool AudioRendererPassthrough::IsEndOfStreamPlayed() const {
  SB_DCHECK(BelongsToCurrentThread());

  return end_of_stream_played_.load();
}

bool AudioRendererPassthrough::CanAcceptMoreData() const {
  SB_DCHECK(BelongsToCurrentThread());

  ScopedLock scoped_lock(mutex_);
  return can_accept_more_data_.load() &&
         decoded_audios_.size() < kMaxDecodedAudios;
}

void AudioRendererPassthrough::Play() {
  SB_DCHECK(BelongsToCurrentThread());

  if (!paused_) {
    SB_LOG(INFO) << "Already playing.";
    return;
  }

  SB_LOG(INFO) << "Play.";

  ScopedLock scoped_lock(mutex_);
  paused_ = false;
}

void AudioRendererPassthrough::Pause() {
  SB_DCHECK(BelongsToCurrentThread());

  if (paused_) {
    SB_LOG(INFO) << "Already paused.";
    return;
  }

  SB_LOG(INFO) << "Pause.";

  ScopedLock scoped_lock(mutex_);
  paused_ = true;
}

void AudioRendererPassthrough::SetPlaybackRate(double playback_rate) {
  SB_DCHECK(BelongsToCurrentThread());

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

  ScopedLock scoped_lock(mutex_);
  playback_rate_ = playback_rate;
}

void AudioRendererPassthrough::Seek(SbTime seek_to_time) {
  SB_DCHECK(BelongsToCurrentThread());

  SB_LOG(INFO) << "Seek to " << seek_to_time;

  decoder_->Reset();

  bool seek_to_time_set = false;
  if (audio_track_thread_) {
    audio_track_thread_->ScheduleAndWait(
        std::bind(&AudioRendererPassthrough::FlushAudioTrackAndStopProcessing,
                  this, seek_to_time));
    // |seek_to_time_| is updated inside FlushAudioTrackAndStopProcessing(),
    // update the flag so we needn't set it again below.
    seek_to_time_set = true;
    // Destroy the audio track thread, it will be re-created during preroll.
    audio_track_thread_.reset();
  }

  CancelPendingJobs();

  ScopedLock scoped_lock(mutex_);

  can_accept_more_data_.store(true);
  prerolled_.store(false);
  end_of_stream_played_.store(false);
  total_frames_written_ = 0;

  end_of_stream_written_ = false;

  stop_called_ = false;
  playback_head_position_when_stopped_ = 0;
  stopped_at_ = 0;
  if (!seek_to_time_set) {
    seek_to_time_ = seek_to_time;
  }
  paused_ = true;
  decoded_audios_ = std::queue<scoped_refptr<DecodedAudio>>();  // clear it
  decoded_audio_writing_in_progress_ = nullptr;
  decoded_audio_writing_offset_ = 0;
  total_frames_written_on_audio_track_thread_ = 0;
}

// This function can be called from *any* threads.
SbTime AudioRendererPassthrough::GetCurrentMediaTime(bool* is_playing,
                                                     bool* is_eos_played,
                                                     bool* is_underflow,
                                                     double* playback_rate) {
  SB_DCHECK(is_playing);
  SB_DCHECK(is_eos_played);
  SB_DCHECK(is_underflow);
  SB_DCHECK(playback_rate);

  ScopedLock scoped_lock(mutex_);
  *is_playing = !paused_;
  *is_eos_played = end_of_stream_played_.load();
  *is_underflow = false;  // TODO: Support underflow
  *playback_rate = playback_rate_;

  if (!audio_track_bridge_) {
    return seek_to_time_;
  }

  if (stop_called_) {
    // When AudioTrackBridge::Stop() is called, the playback will continue until
    // all the frames written are played, as the AudioTrack in created in
    // MODE_STREAM.
    auto now = SbTimeGetMonotonicNow();
    SB_DCHECK(now >= stopped_at_);
    auto time_elapsed = now - stopped_at_;
    int64_t frames_played =
        time_elapsed * audio_sample_info_.samples_per_second / kSbTimeSecond;
    int64_t total_frames_played =
        frames_played + playback_head_position_when_stopped_;
    total_frames_played = std::min(total_frames_played, total_frames_written_);
    return seek_to_time_ + total_frames_played * kSbTimeSecond /
                               audio_sample_info_.samples_per_second;
  }

  SbTime updated_at;
  auto playback_head_position =
      audio_track_bridge_->GetPlaybackHeadPosition(&updated_at);
  if (playback_head_position <= 0) {
    // The playback is warming up, don't adjust the media time by the monotonic
    // system time.
    return seek_to_time_;
  }

  // TODO: This may cause time regression, because the unadjusted time will be
  //       returned on pause, after an adjusted time has been returned.
  SbTime playback_time =
      seek_to_time_ + playback_head_position * kSbTimeSecond /
                          audio_sample_info_.samples_per_second;
  if (paused_ || playback_rate_ == 0.0) {
    return playback_time;
  }

  // TODO: Cap this to the maximum frames written to the AudioTrack.
  auto now = SbTimeGetMonotonicNow();
  SB_LOG_IF(WARNING, now < updated_at)
      << "now (" << now << ") is not greater than updated_at (" << updated_at
      << ").";
  playback_time += std::max<SbTime>(now - updated_at, 0);

  return playback_time;
}

void AudioRendererPassthrough::CreateAudioTrackAndStartProcessing() {
  SB_DCHECK(audio_track_thread_);
  SB_DCHECK(audio_track_thread_->BelongsToCurrentThread());
  SB_DCHECK(error_cb_);

  if (audio_track_bridge_) {
    SB_DCHECK(!update_status_and_write_data_token_.is_valid());
    AudioTrackState initial_state;
    update_status_and_write_data_token_ = audio_track_thread_->Schedule(
        std::bind(&AudioRendererPassthrough::UpdateStatusAndWriteData, this,
                  initial_state));
    SB_LOG(INFO) << "|audio_track_bridge_| already created, start processing.";
    return;
  }

  std::unique_ptr<AudioTrackBridge> audio_track_bridge(new AudioTrackBridge(
      audio_sample_info_.codec == kSbMediaAudioCodecAc3
          ? kSbMediaAudioCodingTypeAc3
          : kSbMediaAudioCodingTypeDolbyDigitalPlus,
      optional<SbMediaAudioSampleType>(),  // Not required in passthrough mode
      audio_sample_info_.number_of_channels,
      audio_sample_info_.samples_per_second, kPreferredBufferSizeInBytes,
      enable_audio_device_callback_, kTunnelModeAudioSessionId));

  if (!audio_track_bridge->is_valid()) {
    error_cb_(kSbPlayerErrorDecode, "Error creating AudioTrackBridge");
    return;
  }

  {
    ScopedLock scoped_lock(mutex_);
    audio_track_bridge_ = std::move(audio_track_bridge);
  }

  AudioTrackState initial_state;
  update_status_and_write_data_token_ = audio_track_thread_->Schedule(
      std::bind(&AudioRendererPassthrough::UpdateStatusAndWriteData, this,
                initial_state));
  SB_LOG(INFO) << "|audio_track_bridge_| created, start processing.";
}

void AudioRendererPassthrough::FlushAudioTrackAndStopProcessing(
    SbTime seek_to_time) {
  SB_DCHECK(audio_track_thread_);
  SB_DCHECK(audio_track_thread_->BelongsToCurrentThread());

  SB_LOG(INFO) << "Pause audio track and stop processing.";

  // Flushing of |audio_track_bridge_| and updating of |seek_to_time_| have to
  // be done together under lock to avoid |seek_to_time_| being added to a stale
  // playback head or vice versa in GetCurrentMediaTime().
  ScopedLock scoped_lock(mutex_);

  // We have to reuse |audio_track_bridge_| instead of creating a new one, to
  // reduce output mode switching between PCM and e/ac3.  Otherwise a noticeable
  // silence can be observed after seeking on some audio receivers.
  // TODO: Consider reusing audio sink for non-passthrough playbacks, to see if
  //       it reduces latency after seeking.
  audio_track_bridge_->PauseAndFlush();
  seek_to_time_ = seek_to_time;
  paused_ = true;
  if (update_status_and_write_data_token_.is_valid()) {
    audio_track_thread_->RemoveJobByToken(update_status_and_write_data_token_);
    update_status_and_write_data_token_.ResetToInvalid();
  }
}

void AudioRendererPassthrough::UpdateStatusAndWriteData(
    const AudioTrackState previous_state) {
  SB_DCHECK(audio_track_thread_);
  SB_DCHECK(audio_track_thread_->BelongsToCurrentThread());
  SB_DCHECK(error_cb_);
  SB_DCHECK(audio_track_bridge_);

  if (enable_audio_device_callback_ &&
      audio_track_bridge_->GetAndResetHasAudioDeviceChanged()) {
    SB_LOG(INFO) << "Audio device changed, raising a capability changed error "
                    "to restart playback.";
    error_cb_(kSbPlayerErrorCapabilityChanged,
              "Audio device capability changed");
    audio_track_bridge_->PauseAndFlush();
    return;
  }

  AudioTrackState current_state;

  {
    ScopedLock scoped_lock(mutex_);
    current_state.volume = volume_;
    current_state.paused = paused_;
    current_state.playback_rate = playback_rate_;

    if (!decoded_audio_writing_in_progress_ && !decoded_audios_.empty()) {
      decoded_audio_writing_in_progress_ = decoded_audios_.front();
      decoded_audios_.pop();
      decoded_audio_writing_offset_ = 0;
    }
  }

  if (previous_state.volume != current_state.volume) {
    audio_track_bridge_->SetVolume(current_state.volume);
  }
  if (previous_state.playing() != current_state.playing()) {
    if (current_state.playing()) {
      audio_track_bridge_->Play();
      SB_LOG(INFO) << "Played on AudioTrack thread.";
      ScopedLock scoped_lock(mutex_);
      stop_called_ = false;
    } else {
      audio_track_bridge_->Pause();
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
      ScopedLock scoped_lock(mutex_);
      if (current_state.playing() && !stop_called_) {
        // TODO: Check if we can apply the same stop logic to non-passthrough.
        audio_track_bridge_->Stop();
        stop_called_ = true;
        playback_head_position_when_stopped_ =
            audio_track_bridge_->GetPlaybackHeadPosition(&stopped_at_);
        total_frames_written_ = total_frames_written_on_audio_track_thread_;
        decoded_audio_writing_in_progress_ = nullptr;
        SB_LOG(INFO) << "Audio track stopped at " << stopped_at_
                     << ", playback head: "
                     << playback_head_position_when_stopped_;
      }
    } else {
      auto sample_buffer = decoded_audio_writing_in_progress_->buffer() +
                           decoded_audio_writing_offset_;
      auto samples_to_write = (decoded_audio_writing_in_progress_->size() -
                               decoded_audio_writing_offset_);
      // TODO: |sync_time| currently doesn't take partial writes into account.
      //       It is not used in non-tunneled mode so it doesn't matter, but we
      //       should revisit this.
      auto sync_time = decoded_audio_writing_in_progress_->timestamp();
      int samples_written = audio_track_bridge_->WriteSample(
          sample_buffer, samples_to_write, sync_time);
      // Error code returned as negative value, like kAudioTrackErrorDeadObject.
      if (samples_written < 0) {
        if (samples_written == AudioTrackBridge::kAudioTrackErrorDeadObject) {
          // Inform the audio end point change.
          SB_LOG(INFO)
              << "Write error for dead audio track, audio device capability "
                 "has likely changed. Restarting playback.";
          error_cb_(kSbPlayerErrorCapabilityChanged,
                    "Audio device capability changed");
          audio_track_bridge_->PauseAndFlush();
          return;
        }
        // `kSbPlayerErrorDecode` is used for general SbPlayer error, there is
        // no error code corresponding to audio sink.
        error_cb_(
            kSbPlayerErrorDecode,
            FormatString("Error while writing frames: %d", samples_written));
      }
      decoded_audio_writing_offset_ += samples_written;

      if (decoded_audio_writing_offset_ ==
          decoded_audio_writing_in_progress_->size()) {
        total_frames_written_on_audio_track_thread_ += frames_per_input_buffer_;
        decoded_audio_writing_in_progress_ = nullptr;
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

  // EOS is handled on this thread instead of in GetCurrentMediaTime(), because
  // GetCurrentMediaTime() is not guaranteed to be called.
  if (stop_called_ && !end_of_stream_played_.load()) {
    auto time_elapsed = SbTimeGetMonotonicNow() - stopped_at_;
    auto frames_played =
        time_elapsed * audio_sample_info_.samples_per_second / kSbTimeSecond;
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

  ScopedLock scoped_lock(mutex_);
  decoded_audios_.push(decoded_audio);
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
