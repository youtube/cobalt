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

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/memory.h"
#include "starboard/shared/starboard/media/media_util.h"

namespace starboard {
namespace shared {
namespace opus {

namespace {

typedef struct {
  int nb_streams;
  int nb_coupled_streams;
  unsigned char mapping[8];
} VorbisLayout;

/* Index is nb_channel-1 */
static const VorbisLayout vorbis_mappings[8] = {
    {1, 0, {0}},                      /* 1: mono */
    {1, 1, {0, 1}},                   /* 2: stereo */
    {2, 1, {0, 2, 1}},                /* 3: 1-d surround */
    {2, 2, {0, 1, 2, 3}},             /* 4: quadraphonic surround */
    {3, 2, {0, 4, 1, 2, 3}},          /* 5: 5-channel surround */
    {4, 2, {0, 4, 1, 2, 3, 5}},       /* 6: 5.1 surround */
    {4, 3, {0, 4, 1, 2, 3, 5, 6}},    /* 7: 6.1 surround */
    {5, 3, {0, 6, 1, 2, 3, 4, 5, 7}}, /* 8: 7.1 surround */
};

}  // namespace

OpusAudioDecoder::OpusAudioDecoder(
    const SbMediaAudioSampleInfo& audio_sample_info)
    : audio_sample_info_(audio_sample_info) {
  int error;
  int channels = audio_sample_info_.number_of_channels;
  if (channels > 8 || channels < 1) {
    SB_LOG(ERROR) << "Can't create decoder with " << channels << " channels";
    return;
  }

  decoder_ = opus_multistream_decoder_create(
      audio_sample_info_.samples_per_second, channels,
      vorbis_mappings[channels - 1].nb_streams,
      vorbis_mappings[channels - 1].nb_coupled_streams,
      vorbis_mappings[channels - 1].mapping, &error);
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
    opus_multistream_decoder_destroy(decoder_);
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

  if (stream_ended_) {
    SB_LOG(ERROR) << "Decode() is called after WriteEndOfStream() is called.";
    return;
  }

  scoped_refptr<DecodedAudio> decoded_audio = new DecodedAudio(
      audio_sample_info_.number_of_channels, GetSampleType(),
      kSbMediaAudioFrameStorageTypeInterleaved, input_buffer->timestamp(),
      audio_sample_info_.number_of_channels * frames_per_au_ *
          starboard::media::GetBytesPerSample(GetSampleType()));

#if SB_HAS_QUIRK(SUPPORT_INT16_AUDIO_SAMPLES)
  const char kDecodeFunctionName[] = "opus_multistream_decode";
  int decoded_frames = opus_multistream_decode(
      decoder_, static_cast<const unsigned char*>(input_buffer->data()),
      input_buffer->size(),
      reinterpret_cast<opus_int16*>(decoded_audio->buffer()), frames_per_au_,
      0);
#else   // SB_HAS_QUIRK(SUPPORT_INT16_AUDIO_SAMPLES)
  const char kDecodeFunctionName[] = "opus_multistream_decode_float";
  int decoded_frames = opus_multistream_decode_float(
      decoder_, static_cast<const unsigned char*>(input_buffer->data()),
      input_buffer->size(), reinterpret_cast<float*>(decoded_audio->buffer()),
      frames_per_au_, 0);
#endif  // SB_HAS_QUIRK(SUPPORT_INT16_AUDIO_SAMPLES)
  if (decoded_frames == OPUS_BUFFER_TOO_SMALL &&
      frames_per_au_ < kMaxOpusFramesPerAU) {
    frames_per_au_ = kMaxOpusFramesPerAU;
    // Send to decode again with the new |frames_per_au_|.
    Decode(input_buffer, consumed_cb);
    return;
  }
  if (decoded_frames <= 0) {
    // When the following check fails, it indicates that |frames_per_au_| is
    // greater than or equal to |kMaxOpusFramesPerAU|, which should never happen
    // for Opus.
    SB_DCHECK(decoded_frames != OPUS_BUFFER_TOO_SMALL);

    // TODO: Consider fill it with silence.
    SB_LOG(ERROR) << kDecodeFunctionName
                  << "() failed with error code: " << decoded_frames;
    error_cb_(kSbPlayerErrorDecode,
              FormatString("%s() failed with error code: %d",
                           kDecodeFunctionName, decoded_frames));
    return;
  }

  frames_per_au_ = decoded_frames;
  decoded_audio->ShrinkTo(audio_sample_info_.number_of_channels *
                          frames_per_au_ *
                          starboard::media::GetBytesPerSample(GetSampleType()));

  decoded_audios_.push(decoded_audio);
  Schedule(consumed_cb);
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

scoped_refptr<OpusAudioDecoder::DecodedAudio> OpusAudioDecoder::Read(
    int* samples_per_second) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_);
  SB_DCHECK(!decoded_audios_.empty());

  scoped_refptr<DecodedAudio> result;
  if (!decoded_audios_.empty()) {
    result = decoded_audios_.front();
    decoded_audios_.pop();
  }
  *samples_per_second = audio_sample_info_.samples_per_second;
  return result;
}

void OpusAudioDecoder::Reset() {
  SB_DCHECK(BelongsToCurrentThread());

  stream_ended_ = false;
  while (!decoded_audios_.empty()) {
    decoded_audios_.pop();
  }

  CancelPendingJobs();
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

}  // namespace opus
}  // namespace shared
}  // namespace starboard
