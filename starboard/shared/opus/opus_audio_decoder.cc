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

#include <algorithm>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"

namespace starboard {

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

OpusAudioDecoder::OpusAudioDecoder(const AudioStreamInfo& audio_stream_info)
    : audio_stream_info_(audio_stream_info) {
  InitializeCodec();
}

OpusAudioDecoder::~OpusAudioDecoder() {
  if (is_valid()) {
    opus_multistream_decoder_ctl(decoder_, OPUS_RESET_STATE);
  }
  TeardownCodec();
}

void OpusAudioDecoder::Initialize(OutputCB output_cb, ErrorCB error_cb) {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb);
  SB_DCHECK(!output_cb_);
  SB_DCHECK(error_cb);
  SB_DCHECK(!error_cb_);

  output_cb_ = std::move(output_cb);
  error_cb_ = std::move(error_cb);
}

void OpusAudioDecoder::Decode(const InputBuffers& input_buffers,
                              const ConsumedCB& consumed_cb) {
  SB_CHECK(BelongsToCurrentThread());
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

void OpusAudioDecoder::DecodePendingBuffers() {
  SB_CHECK(BelongsToCurrentThread());
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
        Schedule(std::bind(&OpusAudioDecoder::WriteEndOfStream, this));
        stream_ended_ = false;
      }
      return;
    }
  }

  SB_DCHECK(!pending_audio_buffers_.empty());
  Schedule(std::bind(&OpusAudioDecoder::DecodePendingBuffers, this));
}

bool OpusAudioDecoder::DecodeInternal(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK(input_buffer);
  SB_DCHECK(output_cb_);
  SB_DCHECK(!stream_ended_ || !pending_audio_buffers_.empty());

  scoped_refptr<DecodedAudio> decoded_audio = new DecodedAudio(
      audio_stream_info_.number_of_channels, GetSampleType(),
      kSbMediaAudioFrameStorageTypeInterleaved, input_buffer->timestamp(),
      audio_stream_info_.number_of_channels * frames_per_au_ *
          GetBytesPerSample(GetSampleType()));

  const char kDecodeFunctionName[] = "opus_multistream_decode_float";
  int decoded_frames = opus_multistream_decode_float(
      decoder_, static_cast<const unsigned char*>(input_buffer->data()),
      input_buffer->size(), reinterpret_cast<float*>(decoded_audio->data()),
      frames_per_au_, 0);
  if (decoded_frames == OPUS_BUFFER_TOO_SMALL &&
      frames_per_au_ < kMaxOpusFramesPerAU) {
    frames_per_au_ = kMaxOpusFramesPerAU;
    // Send to decode again with the new |frames_per_au_|.
    return DecodeInternal(input_buffer);
  }
  if (decoded_frames <= 0) {
    // When the following check fails, it indicates that |frames_per_au_| is
    // greater than or equal to |kMaxOpusFramesPerAU|, which should never happen
    // for Opus.
    SB_DCHECK_NE(decoded_frames, OPUS_BUFFER_TOO_SMALL);

    // TODO: Consider fill it with silence.
    SB_LOG(ERROR) << kDecodeFunctionName
                  << "() failed with error code: " << decoded_frames;
    error_cb_(kSbPlayerErrorDecode,
              FormatString("%s() failed with error code: %d",
                           kDecodeFunctionName, decoded_frames));
    return false;
  }

  frames_per_au_ = decoded_frames;
  decoded_audio->ShrinkTo(audio_stream_info_.number_of_channels *
                          frames_per_au_ * GetBytesPerSample(GetSampleType()));
  const auto& sample_info = input_buffer->audio_sample_info();
  decoded_audio->AdjustForDiscardedDurations(
      audio_stream_info_.samples_per_second,
      sample_info.discarded_duration_from_front,
      sample_info.discarded_duration_from_back);
  decoded_audios_.push(decoded_audio);
  output_cb_();
  return true;
}

void OpusAudioDecoder::WriteEndOfStream() {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_);

  // Opus has no dependent frames so we needn't flush the decoder.  Set the
  // flag to ensure that Decode() is not called when the stream is ended.
  stream_ended_ = true;
  if (!pending_audio_buffers_.empty()) {
    return;
  }

  // Put EOS into the queue.
  decoded_audios_.push(new DecodedAudio);

  Schedule(output_cb_);
}

void OpusAudioDecoder::InitializeCodec() {
  int error;
  int channels = audio_stream_info_.number_of_channels;
  if (channels > 8 || channels < 1) {
    SB_LOG(ERROR) << "Can't create decoder with " << channels << " channels";
    return;
  }

  decoder_ = opus_multistream_decoder_create(
      audio_stream_info_.samples_per_second, channels,
      vorbis_mappings[channels - 1].nb_streams,
      vorbis_mappings[channels - 1].nb_coupled_streams,
      vorbis_mappings[channels - 1].mapping, &error);
  if (error != OPUS_OK) {
    SB_LOG(ERROR) << "Failed to create decoder with error: "
                  << opus_strerror(error);
    decoder_ = NULL;
    return;
  }
  SB_DCHECK(decoder_);
}

void OpusAudioDecoder::TeardownCodec() {
  if (is_valid()) {
    opus_multistream_decoder_destroy(decoder_);
    decoder_ = NULL;
  }
}

scoped_refptr<DecodedAudio> OpusAudioDecoder::Read(int* samples_per_second) {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_);
  SB_DCHECK(!decoded_audios_.empty());

  scoped_refptr<DecodedAudio> result;
  if (!decoded_audios_.empty()) {
    result = decoded_audios_.front();
    decoded_audios_.pop();
  }
  *samples_per_second = audio_stream_info_.samples_per_second;
  return result;
}

void OpusAudioDecoder::Reset() {
  SB_CHECK(BelongsToCurrentThread());

  if (is_valid()) {
    int error = opus_multistream_decoder_ctl(decoder_, OPUS_RESET_STATE);
    if (error != OPUS_OK) {
      SB_LOG(ERROR) << "Failed to reset OpusAudioDecoder with error: "
                    << opus_strerror(error);

      // If fail to reset opus decoder, re-create it.
      TeardownCodec();
      InitializeCodec();
    }
  }

  frames_per_au_ = kMaxOpusFramesPerAU;
  stream_ended_ = false;
  while (!decoded_audios_.empty()) {
    decoded_audios_.pop();
  }
  pending_audio_buffers_.clear();
  consumed_cb_ = nullptr;

  CancelPendingJobs();
}

bool OpusAudioDecoder::is_valid() const {
  return decoder_ != NULL;
}

SbMediaAudioSampleType OpusAudioDecoder::GetSampleType() const {
  SB_CHECK(BelongsToCurrentThread());
  return kSbMediaAudioSampleTypeFloat32;
}

}  // namespace starboard
