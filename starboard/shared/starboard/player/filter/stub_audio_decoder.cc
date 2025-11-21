// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/player/filter/stub_audio_decoder.h"

#include <algorithm>

#include "starboard/audio_sink.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"

namespace starboard {

namespace {

SbMediaAudioSampleType GetSupportedSampleType() {
  if (SbAudioSinkIsAudioSampleTypeSupported(kSbMediaAudioSampleTypeFloat32)) {
    return kSbMediaAudioSampleTypeFloat32;
  }
  SB_DCHECK(SbAudioSinkIsAudioSampleTypeSupported(
      kSbMediaAudioSampleTypeInt16Deprecated));
  return kSbMediaAudioSampleTypeInt16Deprecated;
}

// Calculate the frames per InputBuffer based on two consecutive audio samples.
int CalculateFramesPerInputBuffer(int sample_rate,
                                  const scoped_refptr<InputBuffer>& first,
                                  const scoped_refptr<InputBuffer>& second) {
  SB_DCHECK(first);
  SB_DCHECK(second);

  int64_t duration = second->timestamp() - first->timestamp();
  if (duration <= 0) {
    SB_LOG(ERROR) << "Duration (" << duration << ") for InputBuffer@ "
                  << first->timestamp() << " is invalid.";
    return -1;
  }

  return AudioDurationToFrames(duration, sample_rate);
}

scoped_refptr<DecodedAudio> CreateDecodedAudio(
    int64_t timestamp,
    SbMediaAudioSampleType sample_type,
    int number_of_channels,
    int frames) {
  int sample_size = GetBytesPerSample(sample_type);
  scoped_refptr<DecodedAudio> decoded_audio(new DecodedAudio(
      number_of_channels, sample_type, kSbMediaAudioFrameStorageTypeInterleaved,
      timestamp, sample_size * number_of_channels * frames));

  for (int j = 0; j < decoded_audio->size_in_bytes() / sample_size; ++j) {
    if (sample_size == 2) {
      *(reinterpret_cast<int16_t*>(decoded_audio->data()) + j) = j;
    } else {
      SB_DCHECK_EQ(sample_size, 4);
      *(reinterpret_cast<float*>(decoded_audio->data()) + j) =
          ((j % 1024) - 512) / 512.0f;
    }
  }

  return decoded_audio;
}

}  // namespace

StubAudioDecoder::StubAudioDecoder(const AudioStreamInfo& audio_stream_info)
    : codec_(audio_stream_info.codec),
      number_of_channels_(audio_stream_info.number_of_channels),
      samples_per_second_(audio_stream_info.samples_per_second),
      sample_type_(GetSupportedSampleType()) {
  if (codec_ == kSbMediaAudioCodecAac) {
    // Assume the frames per input buffer is always 1024 for aac.
    frames_per_input_ = 1024;
  }
}

void StubAudioDecoder::Initialize(const OutputCB& output_cb,
                                  const ErrorCB& error_cb) {
  SB_CHECK(BelongsToCurrentThread());

  output_cb_ = output_cb;
  error_cb_ = error_cb;
}

void StubAudioDecoder::Decode(const InputBuffers& input_buffers,
                              const ConsumedCB& consumed_cb) {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK(!input_buffers.empty());
  for (const auto& input_buffer : input_buffers) {
    SB_DCHECK(input_buffer);
  }

  if (!decoder_thread_) {
    decoder_thread_.reset(new JobThread("stub_audio_decoder"));
  }
  decoder_thread_->job_queue()->Schedule(std::bind(
      &StubAudioDecoder::DecodeBuffers, this, input_buffers, consumed_cb));
}

void StubAudioDecoder::WriteEndOfStream() {
  SB_CHECK(BelongsToCurrentThread());
  if (decoder_thread_) {
    decoder_thread_->job_queue()->Schedule(
        std::bind(&StubAudioDecoder::DecodeEndOfStream, this));
    return;
  }
  decoded_audios_.push(new DecodedAudio());
  output_cb_();
}

scoped_refptr<DecodedAudio> StubAudioDecoder::Read(int* samples_per_second) {
  SB_CHECK(BelongsToCurrentThread());

  *samples_per_second = samples_per_second_;
  std::lock_guard lock(decoded_audios_mutex_);
  if (decoded_audios_.empty()) {
    return scoped_refptr<DecodedAudio>();
  }
  auto result = decoded_audios_.front();
  decoded_audios_.pop();
  return result;
}

void StubAudioDecoder::Reset() {
  SB_CHECK(BelongsToCurrentThread());

  decoder_thread_.reset();
  last_input_buffer_ = NULL;
  total_input_count_ = 0;
  while (!decoded_audios_.empty()) {
    decoded_audios_.pop();
  }
  CancelPendingJobs();
}

void StubAudioDecoder::DecodeBuffers(const InputBuffers& input_buffers,
                                     const ConsumedCB& consumed_cb) {
  for (const auto& input_buffer : input_buffers) {
    DecodeOneBuffer(input_buffer);
  }
  Schedule(consumed_cb);
}

void StubAudioDecoder::DecodeOneBuffer(
    const scoped_refptr<InputBuffer>& input_buffer) {
  const int kMaxInputBeforeMultipleDecodedAudios = 4;

  if (last_input_buffer_) {
    if (!frames_per_input_) {
      auto frames_per_input = CalculateFramesPerInputBuffer(
          samples_per_second_, last_input_buffer_, input_buffer);
      if (frames_per_input <= 0) {
        error_cb_(kSbPlayerErrorDecode,
                  "Failed to calculate frames per input.");
        return;
      }
      frames_per_input_ = frames_per_input;
    }

    scoped_refptr<DecodedAudio> decoded_audio =
        CreateDecodedAudio(last_input_buffer_->timestamp(), sample_type_,
                           number_of_channels_, *frames_per_input_);

    decoded_audio->AdjustForDiscardedDurations(
        samples_per_second_,
        last_input_buffer_->audio_sample_info().discarded_duration_from_front,
        last_input_buffer_->audio_sample_info().discarded_duration_from_back);

    if (total_input_count_ % kMaxInputBeforeMultipleDecodedAudios != 0) {
      std::lock_guard lock(decoded_audios_mutex_);
      decoded_audios_.push(decoded_audio);
      Schedule(output_cb_);
    } else {
      // Divide the content of `decoded_audio` as multiple DecodedAudio objects
      // to ensure that the user of AudioDecoders works with output in
      // arbitrary frames.
      const int kNumDecodedAudioObjects = 3;
      const int sample_size = GetBytesPerSample(sample_type_);
      int offset_in_bytes = 0;

      for (int i = 0; i < kNumDecodedAudioObjects; ++i) {
        size_t frames_of_output =
            decoded_audio->frames() / kNumDecodedAudioObjects;
        size_t size_in_bytes_of_output =
            frames_of_output * sample_size * number_of_channels_;

        if (i == kNumDecodedAudioObjects - 1) {
          // It's the last one, send out all remaining data.
          size_in_bytes_of_output =
              decoded_audio->size_in_bytes() - offset_in_bytes;
          SB_DCHECK(size_in_bytes_of_output %
                        (sample_size * number_of_channels_) ==
                    0);
        }

        auto offset_in_frames =
            offset_in_bytes / (sample_size * number_of_channels_);
        int64_t timestamp =
            decoded_audio->timestamp() +
            AudioDurationToFrames(offset_in_frames, samples_per_second_);

        scoped_refptr<DecodedAudio> current_decoded_audio(
            new DecodedAudio(number_of_channels_, sample_type_,
                             kSbMediaAudioFrameStorageTypeInterleaved,
                             timestamp, size_in_bytes_of_output));
        memcpy(current_decoded_audio->data(),
               decoded_audio->data() + offset_in_bytes,
               size_in_bytes_of_output);
        offset_in_bytes += size_in_bytes_of_output;

        std::lock_guard lock(decoded_audios_mutex_);
        decoded_audios_.push(current_decoded_audio);
        Schedule(output_cb_);
      }
    }
  }
  last_input_buffer_ = input_buffer;
  total_input_count_++;
}

void StubAudioDecoder::DecodeEndOfStream() {
  if (last_input_buffer_) {
    if (!frames_per_input_) {
      if (codec_ == kSbMediaAudioCodecOpus) {
        frames_per_input_ = 960;
      } else if (codec_ == kSbMediaAudioCodecAc3 ||
                 codec_ == kSbMediaAudioCodecEac3) {
        frames_per_input_ = 1536;
      } else if (codec_ == kSbMediaAudioCodecIamf) {
        // The max iamf frames per input varies depending on the stream.
        // Assume 2048 max.
        frames_per_input_ = 2048;
      } else {
        SB_NOTREACHED() << "Unsupported audio codec " << codec_;
      }
    }

    SB_DCHECK(frames_per_input_);

    scoped_refptr<DecodedAudio> decoded_audio =
        CreateDecodedAudio(last_input_buffer_->timestamp(), sample_type_,
                           number_of_channels_, *frames_per_input_);

    auto discarded_duration_from_front =
        last_input_buffer_->audio_sample_info().discarded_duration_from_front;
    auto discarded_duration_from_back =
        last_input_buffer_->audio_sample_info().discarded_duration_from_back;
    SB_DCHECK(AudioDurationToFrames(
                  discarded_duration_from_front + discarded_duration_from_back,
                  last_input_buffer_->audio_stream_info().samples_per_second) <=
              decoded_audio->frames());

    decoded_audio->AdjustForDiscardedDurations(samples_per_second_,
                                               discarded_duration_from_front,
                                               discarded_duration_from_back);

    std::lock_guard lock(decoded_audios_mutex_);
    decoded_audios_.push(decoded_audio);
    Schedule(output_cb_);
  }
  std::lock_guard lock(decoded_audios_mutex_);
  decoded_audios_.push(new DecodedAudio());
  Schedule(output_cb_);
}

}  // namespace starboard
