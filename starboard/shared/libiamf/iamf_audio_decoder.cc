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
#include <string>

#include "third_party/libiamf/source/code/include/IAMF_defines.h"

namespace starboard {
namespace shared {
namespace libiamf {

namespace {
using shared::starboard::player::DecodedAudio;

constexpr int kForceBinauralAudio = false;
// Keep disabled as surround audio may require changes further up the SbPlayer.
constexpr int kEnableSurroundAudio = false;

std::string ErrorCodeToString(int code) {
  switch (code) {
    case IAMF_OK:
      return "IAMF_OK";
    case IAMF_ERR_BAD_ARG:
      return "IAMF_ERR_BAD_ARG";
    case IAMF_ERR_BUFFER_TOO_SMALL:
      return "IAMF_ERR_BUFFER_TOO_SMALL";
    case IAMF_ERR_INTERNAL:
      return "IAMF_ERR_INTERNAL";
    case IAMF_ERR_INVALID_PACKET:
      return "IAMF_ERR_INVALID_PACKET";
    case IAMF_ERR_INVALID_STATE:
      return "IAMF_ERR_INVALID_STATE";
    case IAMF_ERR_UNIMPLEMENTED:
      return "IAMF_ERR_UNIMPLEMENTED";
    case IAMF_ERR_ALLOC_FAIL:
      return "IAMF_ERR_ALLOC_FAIL";
    default:
      return "Unknown IAMF error code " + std::to_string(code);
  }
}
}  // namespace

IamfAudioDecoder::IamfAudioDecoder(const AudioStreamInfo& audio_stream_info,
                                   bool prefer_binaural_audio)
    : audio_stream_info_(audio_stream_info),
      prefer_binarual_audio_(prefer_binaural_audio),
      reader_(false, false) {
  decoder_ = IAMF_decoder_open();
  if (!decoder_) {
    SB_DLOG(ERROR) << "Error creating libiamf decoder";
  }
}

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
    SB_DLOG(ERROR) << "Decode() is called after WriteEndOfStream() is called.";
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
  SB_DCHECK(is_valid());

  reader_.ResetAndRead(input_buffer);
  if (!reader_.is_valid()) {
    SB_DLOG(INFO) << "Failed to parse IA Descriptors";
    error_cb_(kSbPlayerErrorDecode, "Failed to parse IA Descriptors");
    return false;
  }
  if (!decoder_is_configured_) {
    if (!InitializeCodec()) {
      SB_DLOG(INFO) << "Failed to initialize IAMF decoder";
      error_cb_(kSbPlayerErrorDecode, "Failed to initialize IAMF decoder");
      return false;
    }
  }

  scoped_refptr<DecodedAudio> decoded_audio = new DecodedAudio(
      audio_stream_info_.number_of_channels, GetSampleType(),
      kSbMediaAudioFrameStorageTypeInterleaved, input_buffer->timestamp(),
      audio_stream_info_.number_of_channels * reader_.samples_per_buffer() *
          starboard::media::GetBytesPerSample(GetSampleType()));
  int samples_decoded = IAMF_decoder_decode(
      decoder_, reader_.data().data(), reader_.data().size(), nullptr,
      reinterpret_cast<void*>(decoded_audio->data()));
  if (samples_decoded < 1) {
    SB_DLOG(INFO) << "IAMF_decoder_decode() error "
                  << ErrorCodeToString(samples_decoded);
    error_cb_(kSbPlayerErrorDecode, "Failed to decode IAMF sample, error " +
                                        ErrorCodeToString(samples_decoded));
    return false;
  }

  SB_DCHECK(samples_decoded <= reader_.samples_per_buffer());

  decoded_audio->ShrinkTo(audio_stream_info_.number_of_channels *
                          reader_.samples_per_buffer() *
                          starboard::media::GetBytesPerSample(GetSampleType()));

  // TODO: Enable partial audio once float32 pcm output is available.
  const auto& sample_info = input_buffer->audio_sample_info();
  decoded_audio->AdjustForDiscardedDurations(
      audio_stream_info_.samples_per_second,
      sample_info.discarded_duration_from_front,
      sample_info.discarded_duration_from_back);

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
  SB_DCHECK(is_valid());
  SB_DCHECK(!decoder_is_configured_);

  // TODO: libiamf has an issue outputting 32 bit float samples, set to 16 bit
  // for now.
  int error = IAMF_decoder_set_bit_depth(decoder_, 16);
  if (error != IAMF_OK) {
    SB_DLOG(ERROR) << "IAMF_decoder_set_bit_depth() fails with error "
                   << ErrorCodeToString(error);
    return false;
  }

  error = IAMF_decoder_set_sampling_rate(decoder_, 48000);
  if (error != IAMF_OK) {
    SB_DLOG(ERROR) << "IAMF_decoder_set_sampling_rate() fails with error "
                   << ErrorCodeToString(error);
    return false;
  }

  if (kForceBinauralAudio || prefer_binarual_audio_) {
    SB_DLOG(INFO) << "Configuring IamfAudioDecoder for binaural output";
    error = IAMF_decoder_output_layout_set_binaural(decoder_);
    if (error != IAMF_OK) {
      SB_DLOG(ERROR)
          << "IAMF_decoder_output_layout_set_binaural() fails with error "
          << ErrorCodeToString(error);
      return false;
    }
  } else {
    // Default to stereo output. If kEnableSurroundAudio is true, set to a sound
    // system matching, the platform's audio configuration, if available.
    IAMF_SoundSystem sound_system = SOUND_SYSTEM_A;
    if (kEnableSurroundAudio) {
      SbMediaAudioConfiguration out_config;
      SbMediaGetAudioConfiguration(0, &out_config);
      int channels = std::max(out_config.number_of_channels, 2);
      if (channels > 8 || channels < 1) {
        SB_DLOG(ERROR) << "Can't create decoder with " << channels
                       << " channels";
        return false;
      }
      switch (channels) {
        case 1:
          sound_system = SOUND_SYSTEM_MONO;
          break;
        case 2:
          // Stereo output.
          sound_system = SOUND_SYSTEM_A;
          SB_DLOG(INFO) << "Configuring IamfAudioDecoder for stereo output";
          break;
        case 6:
          // 5.1 output.
          sound_system = SOUND_SYSTEM_B;
          SB_DLOG(INFO) << "Configuring IamfAudioDecoder for 5.1 output";
          break;
        case 8:
          // 7.1 output.
          sound_system = SOUND_SYSTEM_C;
          SB_DLOG(INFO) << "Configuring IamfAudioDecoder for 7.1 output";
          break;
        default:
          SB_NOTREACHED();
      }
    } else {
      SB_DLOG(INFO) << "Defaulting to stereo output.";
    }

    error = IAMF_decoder_output_layout_set_sound_system(decoder_, sound_system);
    if (error != IAMF_OK) {
      SB_DLOG(ERROR)
          << "IAMF_decoder_output_layout_set_sound_system() fails with error "
          << ErrorCodeToString(error);
      return false;
    }
  }

  error = IAMF_decoder_set_pts(decoder_, 0, 90000);
  if (error != IAMF_OK) {
    SB_DLOG(ERROR) << "IAMF_decoder_set_pts() fails with error "
                   << ErrorCodeToString(error);
    return false;
  }

  error = IAMF_decoder_set_mix_presentation_id(decoder_,
                                               reader_.mix_presentation_id());
  if (error != IAMF_OK) {
    SB_DLOG(ERROR) << "IAMF_decoder_set_mix_presentation_id() fails with error "
                   << ErrorCodeToString(error);
    return false;
  }

  error = IAMF_decoder_peak_limiter_enable(decoder_, 0);
  if (error != IAMF_OK) {
    SB_DLOG(ERROR) << "IAMF_decoder_peak_limiter_enable() fails with error "
                   << ErrorCodeToString(error);
    return false;
  }

  error = IAMF_decoder_set_normalization_loudness(decoder_, .0f);
  if (error != IAMF_OK) {
    SB_DLOG(ERROR)
        << "IAMF_decoder_set_normalization_loudness() fails with error "
        << ErrorCodeToString(error);
    return false;
  }

  error = IAMF_decoder_configure(decoder_, reader_.config_obus().data(),
                                 reader_.config_size(), nullptr);
  if (error != IAMF_OK) {
    SB_DLOG(ERROR) << "IAMF_decoder_configure() fails with error "
                   << ErrorCodeToString(error);
    return false;
  }

  decoder_is_configured_ = true;

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
  *samples_per_second = 48000;
  return result;
}

void IamfAudioDecoder::Reset() {
  SB_DCHECK(BelongsToCurrentThread());

  if (is_valid()) {
    TeardownCodec();
    decoder_ = IAMF_decoder_open();
  }

  decoder_is_configured_ = false;

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
  return kSbMediaAudioSampleTypeInt16Deprecated;
}

}  // namespace libiamf
}  // namespace shared
}  // namespace starboard
