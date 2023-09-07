// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/uwp/audio_renderer_passthrough.h"

#include <algorithm>
#include <memory>
#include <string>

#include "starboard/common/log.h"
#include "starboard/common/string.h"

namespace starboard {
namespace shared {
namespace uwp {

namespace {

int CodecToIecSampleRate(SbMediaAudioCodec codec) {
  switch (codec) {
    case kSbMediaAudioCodecAc3:
      return 48000;
    case kSbMediaAudioCodecEac3:
      return 192000;
    default:
      SB_NOTREACHED();
      return 0;
  }
}

}  // namespace

AudioRendererPassthrough::AudioRendererPassthrough(
    std::unique_ptr<AudioDecoder> audio_decoder,
    const AudioStreamInfo& audio_stream_info)
    : channels_(audio_stream_info.number_of_channels),
      codec_(audio_stream_info.codec),
      iec_sample_rate_(CodecToIecSampleRate(audio_stream_info.codec)),
      decoder_(audio_decoder.Pass()),
      sink_(new WASAPIAudioSink),
      process_audio_buffers_job_(
          std::bind(&AudioRendererPassthrough::ProcessAudioBuffers, this)) {
  SB_DCHECK(codec_ == kSbMediaAudioCodecAc3 ||
            codec_ == kSbMediaAudioCodecEac3);
  SB_DCHECK(decoder_);
  QueryPerformanceFrequency(&performance_frequency_);
  SB_DCHECK(performance_frequency_.QuadPart > 0);
  SB_LOG(INFO) << "Creating AudioRendererPassthrough with " << channels_
               << " channels.";
}

AudioRendererPassthrough::~AudioRendererPassthrough() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_LOG(INFO) << "Destroying AudioRendererPassthrough with " << channels_
               << " channels.";
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

  error_cb_ = error_cb;
  prerolled_cb_ = prerolled_cb;
  ended_cb_ = ended_cb;

  decoder_->Initialize(
      std::bind(&AudioRendererPassthrough::OnDecoderOutput, this), error_cb);

  if (!sink_->Initialize(channels_, iec_sample_rate_, codec_)) {
    error_cb_(kSbPlayerErrorDecode, "failed to start audio sink");
  }
}

void AudioRendererPassthrough::WriteSamples(const InputBuffers& input_buffers) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(input_buffers.size() == 1);
  SB_DCHECK(input_buffers[0]);
  SB_DCHECK(can_accept_more_data_.load());

  if (end_of_stream_written_.load()) {
    SB_LOG(ERROR) << "Appending audio sample at "
                  << input_buffers[0]->timestamp() << " after EOS reached.";
    return;
  }

  can_accept_more_data_.store(false);
  decoder_->Decode(
      input_buffers,
      std::bind(&AudioRendererPassthrough::OnDecoderConsumed, this));
}

void AudioRendererPassthrough::WriteEndOfStream() {
  SB_DCHECK(BelongsToCurrentThread());

  if (end_of_stream_written_.load()) {
    SB_LOG(ERROR) << "Try to write EOS after EOS is reached";
    return;
  }

  end_of_stream_written_.store(true);
  decoder_->WriteEndOfStream();
}

void AudioRendererPassthrough::SetVolume(double volume) {
  SB_DCHECK(BelongsToCurrentThread());
  sink_->SetVolume(volume);
}

bool AudioRendererPassthrough::IsEndOfStreamWritten() const {
  SB_DCHECK(BelongsToCurrentThread());

  return end_of_stream_written_.load();
}

bool AudioRendererPassthrough::IsEndOfStreamPlayed() const {
  SB_DCHECK(BelongsToCurrentThread());

  return end_of_stream_played_.load();
}

bool AudioRendererPassthrough::CanAcceptMoreData() const {
  SB_DCHECK(BelongsToCurrentThread());

  return !end_of_stream_written_.load() &&
         pending_inputs_.size() < kMaxDecodedAudios &&
         can_accept_more_data_.load();
}

void AudioRendererPassthrough::Play() {
  SB_DCHECK(BelongsToCurrentThread());
  ScopedLock lock(mutex_);
  paused_ = false;
  sink_->Play();
}

void AudioRendererPassthrough::Pause() {
  SB_DCHECK(BelongsToCurrentThread());
  ScopedLock lock(mutex_);
  paused_ = true;
  sink_->Pause();
}

void AudioRendererPassthrough::SetPlaybackRate(double playback_rate) {
  SB_DCHECK(BelongsToCurrentThread());

  if (playback_rate > 0.0 && playback_rate != 1.0) {
    std::string error_message = ::starboard::FormatString(
        "Playback rate %f is not supported", playback_rate);
    error_cb_(kSbPlayerErrorDecode, error_message);
    return;
  }

  ScopedLock lock(mutex_);
  playback_rate_ = playback_rate;
  sink_->SetPlaybackRate(playback_rate);
}

void AudioRendererPassthrough::Seek(SbTime seek_to_time) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(seek_to_time >= 0);
  {
    ScopedLock lock(mutex_);
    seeking_to_time_ = std::max<SbTime>(seek_to_time, 0);
    seeking_ = true;
  }

  total_frames_sent_to_sink_ = 0;
  can_accept_more_data_.store(true);
  process_audio_buffers_job_token_.ResetToInvalid();
  total_buffers_sent_to_sink_ = 0;
  end_of_stream_written_.store(false);
  end_of_stream_played_.store(false);
  pending_inputs_ = std::queue<scoped_refptr<DecodedAudio>>();
  sink_->Reset();
  decoder_->Reset();

  CancelPendingJobs();
}

SbTime AudioRendererPassthrough::GetCurrentMediaTime(bool* is_playing,
                                                     bool* is_eos_played,
                                                     bool* is_underflow,
                                                     double* playback_rate) {
  SB_DCHECK(is_playing);
  SB_DCHECK(is_eos_played);
  SB_DCHECK(is_underflow);
  SB_DCHECK(playback_rate);

  ScopedLock lock(mutex_);
  *is_playing = !paused_ && !seeking_;
  *is_eos_played = end_of_stream_played_.load();
  *is_underflow = false;  // TODO: Support underflow
  *playback_rate = playback_rate_;

  if (seeking_) {
    return seeking_to_time_;
  }

  uint64_t sink_playback_time_updated_at;
  SbTime sink_playback_time = static_cast<SbTime>(
      sink_->GetCurrentPlaybackTime(&sink_playback_time_updated_at));
  if (sink_playback_time <= 0) {
    if (sink_playback_time < 0) {
      error_cb_(kSbPlayerErrorDecode,
                "Error obtaining playback time from WASAPI sink");
    }
    return seeking_to_time_;
  }

  SbTime media_time = seeking_to_time_ + sink_playback_time;
  if (!sink_->playing()) {
    return media_time;
  }

  return media_time +
         CalculateElapsedPlaybackTime(sink_playback_time_updated_at);
}

void AudioRendererPassthrough::OnDecoderConsumed() {
  SB_DCHECK(BelongsToCurrentThread());
  can_accept_more_data_.store(true);
}

void AudioRendererPassthrough::OnDecoderOutput() {
  SB_DCHECK(BelongsToCurrentThread());
  int samples_per_second = 0;
  scoped_refptr<DecodedAudio> decoded_audio =
      decoder_->Read(&samples_per_second);
  SB_DCHECK(decoded_audio);
  pending_inputs_.push(decoded_audio);

  if (process_audio_buffers_job_token_.is_valid()) {
    RemoveJobByToken(process_audio_buffers_job_token_);
    process_audio_buffers_job_token_.ResetToInvalid();
  }
  ProcessAudioBuffers();
}

void AudioRendererPassthrough::ProcessAudioBuffers() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(!pending_inputs_.empty());

  process_audio_buffers_job_token_.ResetToInvalid();
  SbTime process_audio_buffers_job_delay = 5 * kSbTimeMillisecond;

  scoped_refptr<DecodedAudio> decoded_audio = pending_inputs_.front();
  SB_DCHECK(decoded_audio);

  if (decoded_audio->is_end_of_stream()) {
    SB_DCHECK(end_of_stream_written_.load());
    ScopedLock lock(mutex_);
    if (seeking_) {
      seeking_ = false;
      Schedule(prerolled_cb_);
    }
    pending_inputs_.pop();
    SB_DCHECK(pending_inputs_.empty());
  } else {
    ScopedLock lock(mutex_);
    while (seeking_ && decoded_audio &&
           CalculateLastOutputTime(decoded_audio) < seeking_to_time_) {
      pending_inputs_.pop();
      decoded_audio = pending_inputs_.empty() ? scoped_refptr<DecodedAudio>()
                                              : pending_inputs_.front();
    }

    if (decoded_audio && TryToWriteAudioBufferToSink(decoded_audio)) {
      pending_inputs_.pop();
      process_audio_buffers_job_delay = 0;
      if (seeking_ && total_buffers_sent_to_sink_ >= kNumPrerollDecodedAudios) {
        seeking_ = false;
        Schedule(prerolled_cb_);
      }
    }
  }

  if (!pending_inputs_.empty()) {
    process_audio_buffers_job_token_ =
        Schedule(process_audio_buffers_job_, process_audio_buffers_job_delay);
    return;
  }

  if (end_of_stream_written_.load() && !end_of_stream_played_.load()) {
    TryToEndStream();
  }
}

bool AudioRendererPassthrough::TryToWriteAudioBufferToSink(
    scoped_refptr<DecodedAudio> decoded_audio) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(decoded_audio);

  bool buffer_written = sink_->WriteBuffer(decoded_audio);
  if (buffer_written && !decoded_audio->is_end_of_stream()) {
    total_frames_sent_to_sink_ += decoded_audio->frames();
    total_buffers_sent_to_sink_++;
  }
  return buffer_written;
}

void AudioRendererPassthrough::TryToEndStream() {
  bool is_playing, is_eos_played, is_underflow;
  double playback_rate;
  int64_t total_frames_played_by_sink =
      GetCurrentMediaTime(&is_playing, &is_eos_played, &is_underflow,
                          &playback_rate) *
      iec_sample_rate_ / kSbTimeSecond;
  // Wait for the audio sink to output the remaining frames before calling
  // Pause().
  if (total_frames_played_by_sink >= total_frames_sent_to_sink_) {
    sink_->Pause();
    end_of_stream_played_.store(true);
    ended_cb_();
    return;
  }
  Schedule(std::bind(&AudioRendererPassthrough::TryToEndStream, this),
           5 * kSbTimeMillisecond);
}

SbTime AudioRendererPassthrough::CalculateElapsedPlaybackTime(
    uint64_t update_time) {
  LARGE_INTEGER current_time;
  QueryPerformanceCounter(&current_time);
  // Convert current performance counter timestamp to units of 100 nanoseconds.
  // https://docs.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclock-getposition#remarks
  uint64_t current_time_converted =
      static_cast<double>(current_time.QuadPart) *
      (10000000.0 / static_cast<double>(performance_frequency_.QuadPart));
  SB_DCHECK(current_time_converted >= update_time);

  // Convert elapsed time to SbTime.
  return ((current_time_converted - update_time) * 100) /
         kSbTimeNanosecondsPerMicrosecond;
}

SbTime AudioRendererPassthrough::CalculateLastOutputTime(
    scoped_refptr<DecodedAudio>& decoded_audio) {
  return decoded_audio->timestamp() +
         (decoded_audio->frames() / iec_sample_rate_ * kSbTimeSecond);
}

}  // namespace uwp
}  // namespace shared
}  // namespace starboard
