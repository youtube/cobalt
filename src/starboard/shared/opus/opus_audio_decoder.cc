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

#include "starboard/shared/opus/opus_audio_decoder.h"

#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/string.h"

namespace starboard {
namespace shared {
namespace opus {

namespace {
const int kMaxOpusFramesPerAU = 9600;
}  // namespace

OpusAudioDecoder::OpusAudioDecoder(const SbMediaAudioHeader& audio_header)
    : audio_header_(audio_header) {
#if SB_HAS_QUIRK(SUPPORT_INT16_AUDIO_SAMPLES)
  working_buffer_.resize(kMaxOpusFramesPerAU *
                         audio_header_.number_of_channels * sizeof(opus_int16));
#else   // SB_HAS_QUIRK(SUPPORT_INT16_AUDIO_SAMPLES)
  working_buffer_.resize(kMaxOpusFramesPerAU *
                         audio_header_.number_of_channels * sizeof(float));
#endif  // SB_HAS_QUIRK(SUPPORT_INT16_AUDIO_SAMPLES)

  int error;
  decoder_ = opus_decoder_create(audio_header_.samples_per_second,
                                 audio_header_.number_of_channels, &error);
  if (error != OPUS_OK) {
    SB_LOG(ERROR) << "Failed to create decoder with error: "
                  << opus_strerror(error);
    decoder_ = NULL;
    return;
  }
  SB_DCHECK(decoder_ != NULL);
}

OpusAudioDecoder::~OpusAudioDecoder() {
  if (decoder_) {
    opus_decoder_destroy(decoder_);
  }
}

void OpusAudioDecoder::Initialize(const OutputCB& output_cb,
                                  const ErrorCB& error_cb) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb);
  SB_DCHECK(!output_cb_);
  SB_DCHECK(error_cb);
  SB_DCHECK(!error_cb_);

  output_cb_ = output_cb;
  error_cb_ = error_cb;
}

void OpusAudioDecoder::Decode(const scoped_refptr<InputBuffer>& input_buffer,
                              const ConsumedCB& consumed_cb) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(input_buffer);
  SB_DCHECK(output_cb_);

  Schedule(consumed_cb);

  if (stream_ended_) {
    SB_LOG(ERROR) << "Decode() is called after WriteEndOfStream() is called.";
    return;
  }

#if SB_HAS_QUIRK(SUPPORT_INT16_AUDIO_SAMPLES)
  const char kDecodeFunctionName[] = "opus_decode";
  int decoded_frames = opus_decode(
      decoder_, static_cast<const unsigned char*>(input_buffer->data()),
      input_buffer->size(),
      reinterpret_cast<opus_int16*>(working_buffer_.data()),
      kMaxOpusFramesPerAU, 0);
#else   // SB_HAS_QUIRK(SUPPORT_INT16_AUDIO_SAMPLES)
  const char kDecodeFunctionName[] = "opus_decode_float";
  int decoded_frames = opus_decode_float(
      decoder_, static_cast<const unsigned char*>(input_buffer->data()),
      input_buffer->size(), reinterpret_cast<float*>(working_buffer_.data()),
      kMaxOpusFramesPerAU, 0);
#endif  // SB_HAS_QUIRK(SUPPORT_INT16_AUDIO_SAMPLES)
  if (decoded_frames <= 0) {
    // TODO: Consider fill it with silence.
    SB_LOG(ERROR) << kDecodeFunctionName
                  << "() failed with error code: " << decoded_frames;
    error_cb_(kSbPlayerErrorDecode,
              FormatString("%s() failed with error code: %d",
                           kDecodeFunctionName, decoded_frames));
    return;
  }

  scoped_refptr<DecodedAudio> decoded_audio = new DecodedAudio(
      audio_header_.number_of_channels, GetSampleType(), GetStorageType(),
      input_buffer->timestamp(),
      audio_header_.number_of_channels * decoded_frames *
          starboard::media::GetBytesPerSample(GetSampleType()));
  SbMemoryCopy(decoded_audio->buffer(), working_buffer_.data(),
               decoded_audio->size());
  decoded_audios_.push(decoded_audio);
  Schedule(output_cb_);
}

void OpusAudioDecoder::WriteEndOfStream() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_);

  // Opus has no dependent frames so we needn't flush the decoder.  Set the
  // flag to ensure that Decode() is not called when the stream is ended.
  stream_ended_ = true;
  // Put EOS into the queue.
  decoded_audios_.push(new DecodedAudio);

  Schedule(output_cb_);
}

scoped_refptr<OpusAudioDecoder::DecodedAudio> OpusAudioDecoder::Read() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_);
  SB_DCHECK(!decoded_audios_.empty());

  scoped_refptr<DecodedAudio> result;
  if (!decoded_audios_.empty()) {
    result = decoded_audios_.front();
    decoded_audios_.pop();
  }
  return result;
}

void OpusAudioDecoder::Reset() {
  SB_DCHECK(BelongsToCurrentThread());

  stream_ended_ = false;
  while (!decoded_audios_.empty()) {
    decoded_audios_.pop();
  }
}

bool OpusAudioDecoder::is_valid() const {
  return decoder_ != NULL;
}

SbMediaAudioSampleType OpusAudioDecoder::GetSampleType() const {
  SB_DCHECK(BelongsToCurrentThread());
#if SB_HAS_QUIRK(SUPPORT_INT16_AUDIO_SAMPLES)
  return kSbMediaAudioSampleTypeInt16;
#else   // SB_HAS_QUIRK(SUPPORT_INT16_AUDIO_SAMPLES)
  return kSbMediaAudioSampleTypeFloat32;
#endif  // SB_HAS_QUIRK(SUPPORT_INT16_AUDIO_SAMPLES)
}

SbMediaAudioFrameStorageType OpusAudioDecoder::GetStorageType() const {
  SB_DCHECK(BelongsToCurrentThread());
  return kSbMediaAudioFrameStorageTypeInterleaved;
}

int OpusAudioDecoder::GetSamplesPerSecond() const {
  return audio_header_.samples_per_second;
}

}  // namespace opus
}  // namespace shared
}  // namespace starboard
