// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/libiamf/iamf_audio_decoder.h"

#include <algorithm>

#include "third_party/libiamf/source/code/include/IAMF_defines.h"

namespace starboard {
namespace shared {
namespace libiamf {

namespace {
using shared::starboard::player::DecodedAudio;
}  // namespace

IamfAudioDecoder::IamfAudioDecoder(const AudioStreamInfo& audio_stream_info)
    : audio_stream_info_(audio_stream_info) {}

IamfAudioDecoder::~IamfAudioDecoder() {
  TeardownCodec();
}

bool IamfAudioDecoder::is_valid() const {
  return decoder_ != NULL;
}

void IamfAudioDecoder::Initialize(const OutputCB& output_cb,
                                  const ErrorCB& error_cb) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb);
  SB_DCHECK(!output_cb_);
  SB_DCHECK(error_cb);
  SB_DCHECK(!error_cb_);

  output_cb_ = output_cb;
  error_cb_ = error_cb;
}

void IamfAudioDecoder::Decode(const InputBuffers& input_buffers,
                              const ConsumedCB& consumed_cb) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(!input_buffers.empty());
  SB_DCHECK(pending_audio_buffers_.empty());
  SB_DCHECK(output_cb_);

  if (stream_ended_) {
    SB_LOG(ERROR) << "Decode() is called after WriteEndOfStream() is called.";
    return;
  }
  if (input_buffers.size() > kMinimumBuffersToDecode) {
    std::copy(std::begin(input_buffers), std::end(input_buffers),
              std::back_inserter(pending_audio_buffers_));
    consumed_cb_ = consumed_cb;
    DecodePendingBuffers();
  } else {
    for (const auto& input_buffer : input_buffers) {
      if (!DecodeInternal(input_buffer)) {
        return;
      }
    }
    Schedule(consumed_cb);
  }
}

void IamfAudioDecoder::DecodePendingBuffers() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(!pending_audio_buffers_.empty());
  SB_DCHECK(consumed_cb_);

  for (int i = 0; i < kMinimumBuffersToDecode; ++i) {
    if (!DecodeInternal(pending_audio_buffers_.front())) {
      return;
    }
    pending_audio_buffers_.pop_front();
    if (pending_audio_buffers_.empty()) {
      Schedule(consumed_cb_);
      consumed_cb_ = nullptr;
      if (stream_ended_) {
        Schedule(std::bind(&IamfAudioDecoder::WriteEndOfStream, this));
        stream_ended_ = false;
      }
      return;
    }
  }

  SB_DCHECK(!pending_audio_buffers_.empty());
  Schedule(std::bind(&IamfAudioDecoder::DecodePendingBuffers, this));
}

bool IamfAudioDecoder::DecodeInternal(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(input_buffer);
  SB_DCHECK(input_buffer->size() > 0);
  SB_DCHECK(output_cb_);
  SB_DCHECK(!stream_ended_ || !pending_audio_buffers_.empty());

  reader_.Read(input_buffer);
  if (!decoder_) {
    bool decoder_initialized = InitializeCodec();
    if (!decoder_initialized) {
      error_cb_(kSbPlayerErrorDecode, "Failed to initialize IAMF decoder");
      return false;
    }
  }

  scoped_refptr<DecodedAudio> decoded_audio = new DecodedAudio(
      audio_stream_info_.number_of_channels, GetSampleType(),
      kSbMediaAudioFrameStorageTypeInterleaved, input_buffer->timestamp(),
      audio_stream_info_.number_of_channels * kMaxIamfFramesPerAU *
          starboard::media::GetBytesPerSample(GetSampleType()));
  int samples_decoded = IAMF_decoder_decode(
      decoder_, reader_.data().data(), reader_.data().size(), nullptr,
      reinterpret_cast<void*>(decoded_audio->data()));
  if (samples_decoded < 1) {
    SB_LOG(INFO) << "IAMF_decoder_decode() error " << std::hex
                 << samples_decoded;
    error_cb_(kSbPlayerErrorDecode, "Failed to decode IAMF sample");
    return false;
  }

  SB_DCHECK(samples_decoded <= kMaxIamfFramesPerAU);
  SB_LOG(INFO) << "Decoded " << samples_decoded << " samples";

  frames_per_au_ = samples_decoded;
  decoded_audio->ShrinkTo(audio_stream_info_.number_of_channels *
                          frames_per_au_ *
                          starboard::media::GetBytesPerSample(GetSampleType()));

  // TODO: Enable once float32 pcm output is fixed.
  // const auto& sample_info = input_buffer->audio_sample_info();
  // decoded_audio->AdjustForDiscardedDurations(
  //     audio_stream_info_.samples_per_second,
  //     sample_info.discarded_duration_from_front,
  //     sample_info.discarded_duration_from_back);

  decoded_audios_.push(decoded_audio);

  output_cb_();

  return true;
}

void IamfAudioDecoder::WriteEndOfStream() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_);

  stream_ended_ = true;
  if (!pending_audio_buffers_.empty()) {
    return;
  }

  // Put EOS into the queue.
  decoded_audios_.push(new DecodedAudio);

  Schedule(output_cb_);
}

bool IamfAudioDecoder::InitializeCodec() {
  int channels = audio_stream_info_.number_of_channels;
  if (channels > 8 || channels < 1) {
    SB_DLOG(ERROR) << "Can't create decoder with " << channels << " channels";
    return false;
  }

  decoder_ = IAMF_decoder_open();
  if (!decoder_) {
    SB_DLOG(ERROR) << "Error creating libiamf decoder";
    return false;
  }

  // TODO: libiamf has an issue outputting 32 bit float samples, set to 16 bit
  // for now.
  int error = IAMF_decoder_set_bit_depth(decoder_, 16);
  if (error != IAMF_OK) {
    SB_DLOG(ERROR) << "IAMF_decoder_set_bit_depth() fails with error " << error;
    return false;
  }

  error = IAMF_decoder_set_sampling_rate(decoder_, reader_.sample_rate());
  if (error != IAMF_OK) {
    SB_LOG(ERROR) << "IAMF_decoder_set_sampling_rate() fails with error "
                  << error;
    return false;
  }

  IAMF_SoundSystem sound_system = SOUND_SYSTEM_INVALID;
  switch (channels) {
    case 1:
      sound_system = SOUND_SYSTEM_MONO;
      break;
    case 2:
      // Stereo output.
      sound_system = SOUND_SYSTEM_A;
      SB_LOG(INFO) << "Configuring IamfAudioDecoder for stereo output";
      break;
    case 6:
      // 5.1 output.
      sound_system = SOUND_SYSTEM_B;
      SB_LOG(INFO) << "Configuring IamfAudioDecoder for 5.1 output";
      break;
    case 8:
      // 7.1 output.
      sound_system = SOUND_SYSTEM_C;
      SB_LOG(INFO) << "Configuring IamfAudioDecoder for 7.1 output";
      break;
    default:
      SB_NOTREACHED();
  }

  error = IAMF_decoder_output_layout_set_sound_system(decoder_, sound_system);
  if (error != IAMF_OK) {
    SB_LOG(ERROR)
        << "IAMF_decoder_output_layout_set_sound_system() fails with error "
        << error;
    return false;
  }
  SB_LOG(INFO) << "num channels: "
               << IAMF_layout_sound_system_channels_count(sound_system);

  // TODO: Accurately set pts upon resume, if needed.
  error = IAMF_decoder_set_pts(decoder_, 0, 90000);
  if (error != IAMF_OK) {
    SB_LOG(ERROR) << "IAMF_decoder_set_pts() fails with error " << error;
    return false;
  }

  error = IAMF_decoder_set_mix_presentation_id(decoder_,
                                               reader_.mix_presentation_id());
  if (error != IAMF_OK) {
    SB_LOG(ERROR) << "IAMF_decoder_set_mix_presentation_id() fails with error "
                  << error;
    return false;
  }

  error = IAMF_decoder_peak_limiter_enable(decoder_, 0);
  if (error != IAMF_OK) {
    SB_LOG(ERROR) << "IAMF_decoder_peak_limiter_enable() fails with error "
                  << error;
    return false;
  }

  error = IAMF_decoder_set_normalization_loudness(decoder_, .0f);
  if (error != IAMF_OK) {
    SB_LOG(ERROR)
        << "IAMF_decoder_set_normalization_loudness() fails with error "
        << error;
    return false;
  }

  error = IAMF_decoder_configure(decoder_, reader_.config_obus().data(),
                                 reader_.config_size(), nullptr);
  if (error != IAMF_OK) {
    SB_LOG(ERROR) << "IAMF_decoder_configure() fails with error " << error;
    return false;
  }

  return true;
}

void IamfAudioDecoder::TeardownCodec() {
  if (is_valid()) {
    IAMF_decoder_close(decoder_);
    decoder_ = NULL;
  }
}

scoped_refptr<IamfAudioDecoder::DecodedAudio> IamfAudioDecoder::Read(
    int* samples_per_second) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_);
  SB_DCHECK(!decoded_audios_.empty());

  scoped_refptr<DecodedAudio> result;
  if (!decoded_audios_.empty()) {
    result = decoded_audios_.front();
    decoded_audios_.pop();
  }
  *samples_per_second = reader_.sample_rate();
  return result;
}

void IamfAudioDecoder::Reset() {
  SB_DCHECK(BelongsToCurrentThread());

  if (is_valid()) {
    TeardownCodec();
    InitializeCodec();
  }

  frames_per_au_ = kMaxIamfFramesPerAU;
  stream_ended_ = false;
  while (!decoded_audios_.empty()) {
    decoded_audios_.pop();
  }
  pending_audio_buffers_.clear();
  consumed_cb_ = nullptr;

  CancelPendingJobs();
}

SbMediaAudioSampleType IamfAudioDecoder::GetSampleType() const {
  SB_DCHECK(BelongsToCurrentThread());
  // if (audio_stream_info_.bits_per_sample == 32) {
  //   return kSbMediaAudioSampleTypeFloat32;
  // }
  return kSbMediaAudioSampleTypeInt16Deprecated;
}

}  // namespace libiamf
}  // namespace shared
}  // namespace starboard
