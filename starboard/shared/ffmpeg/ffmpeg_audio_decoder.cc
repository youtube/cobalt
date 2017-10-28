// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "starboard/shared/ffmpeg/ffmpeg_audio_decoder.h"

#include "starboard/audio_sink.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/shared/starboard/media/media_util.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

namespace {

SbMediaAudioSampleType GetSupportedSampleType() {
  if (SbAudioSinkIsAudioSampleTypeSupported(kSbMediaAudioSampleTypeFloat32)) {
    return kSbMediaAudioSampleTypeFloat32;
  }
  return kSbMediaAudioSampleTypeInt16;
}

AVCodecID GetFfmpegCodecIdByMediaCodec(SbMediaAudioCodec audio_codec) {
  switch (audio_codec) {
    case kSbMediaAudioCodecAac:
      return AV_CODEC_ID_AAC;
    case kSbMediaAudioCodecOpus:
      return AV_CODEC_ID_OPUS;
    default:
      return AV_CODEC_ID_NONE;
  }
}

}  // namespace

AudioDecoder::AudioDecoder(SbMediaAudioCodec audio_codec,
                           const SbMediaAudioHeader& audio_header)
    : audio_codec_(audio_codec),
      codec_context_(NULL),
      av_frame_(NULL),
      stream_ended_(false),
      audio_header_(audio_header) {
  SB_DCHECK(GetFfmpegCodecIdByMediaCodec(audio_codec) != AV_CODEC_ID_NONE)
      << "Unsupported audio codec " << audio_codec;

  InitializeCodec();
}

AudioDecoder::~AudioDecoder() {
  TeardownCodec();
}

void AudioDecoder::Initialize(const Closure& output_cb,
                              const Closure& error_cb) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb.is_valid());
  SB_DCHECK(!output_cb_.is_valid());
  SB_UNREFERENCED_PARAMETER(error_cb);

  output_cb_ = output_cb;
}

void AudioDecoder::Decode(const scoped_refptr<InputBuffer>& input_buffer,
                          const Closure& consumed_cb) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(input_buffer);
  SB_DCHECK(output_cb_.is_valid());
  SB_CHECK(codec_context_ != NULL);

  Schedule(consumed_cb);

  if (stream_ended_) {
    SB_LOG(ERROR) << "Decode() is called after WriteEndOfStream() is called.";
    return;
  }

  AVPacket packet;
  av_init_packet(&packet);
  packet.data = const_cast<uint8_t*>(input_buffer->data());
  packet.size = input_buffer->size();

  avcodec_get_frame_defaults(av_frame_);
  int frame_decoded = 0;
  int result =
      avcodec_decode_audio4(codec_context_, av_frame_, &frame_decoded, &packet);
  if (result != input_buffer->size() || frame_decoded != 1) {
    // TODO: Consider fill it with silence.
    SB_DLOG(WARNING) << "avcodec_decode_audio4() failed with result: " << result
                     << " with input buffer size: " << input_buffer->size()
                     << " and frame decoded: " << frame_decoded;
    return;
  }

  int decoded_audio_size = av_samples_get_buffer_size(
      NULL, codec_context_->channels, av_frame_->nb_samples,
      codec_context_->sample_fmt, 1);
  audio_header_.samples_per_second = codec_context_->sample_rate;

  if (decoded_audio_size > 0) {
    scoped_refptr<DecodedAudio> decoded_audio = new DecodedAudio(
        codec_context_->channels, GetSampleType(), GetStorageType(),
        input_buffer->pts(),
        codec_context_->channels * av_frame_->nb_samples *
            starboard::media::GetBytesPerSample(GetSampleType()));
    if (GetStorageType() == kSbMediaAudioFrameStorageTypeInterleaved) {
      SbMemoryCopy(decoded_audio->buffer(), *av_frame_->extended_data,
                   decoded_audio->size());
    } else {
      SB_DCHECK(GetStorageType() == kSbMediaAudioFrameStorageTypePlanar);
      const int per_channel_size_in_bytes =
          decoded_audio->size() / decoded_audio->channels();
      for (int i = 0; i < decoded_audio->channels(); ++i) {
        SbMemoryCopy(decoded_audio->buffer() + per_channel_size_in_bytes * i,
                     av_frame_->extended_data[i], per_channel_size_in_bytes);
      }
    }
    decoded_audios_.push(decoded_audio);
    Schedule(output_cb_);
  } else {
    // TODO: Consider fill it with silence.
    SB_LOG(ERROR) << "Decoded audio frame is empty.";
  }
}

void AudioDecoder::WriteEndOfStream() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_.is_valid());

  // AAC has no dependent frames so we needn't flush the decoder.  Set the flag
  // to ensure that Decode() is not called when the stream is ended.
  stream_ended_ = true;
  // Put EOS into the queue.
  decoded_audios_.push(new DecodedAudio);

  Schedule(output_cb_);
}

scoped_refptr<AudioDecoder::DecodedAudio> AudioDecoder::Read() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_.is_valid());
  SB_DCHECK(!decoded_audios_.empty());

  scoped_refptr<DecodedAudio> result;
  if (!decoded_audios_.empty()) {
    result = decoded_audios_.front();
    decoded_audios_.pop();
  }
  return result;
}

void AudioDecoder::Reset() {
  SB_DCHECK(BelongsToCurrentThread());

  stream_ended_ = false;
  while (!decoded_audios_.empty()) {
    decoded_audios_.pop();
  }

  CancelPendingJobs();
}

SbMediaAudioSampleType AudioDecoder::GetSampleType() const {
  SB_DCHECK(BelongsToCurrentThread());

  if (codec_context_->sample_fmt == AV_SAMPLE_FMT_S16 ||
      codec_context_->sample_fmt == AV_SAMPLE_FMT_S16P) {
    return kSbMediaAudioSampleTypeInt16;
  } else if (codec_context_->sample_fmt == AV_SAMPLE_FMT_FLT ||
             codec_context_->sample_fmt == AV_SAMPLE_FMT_FLTP) {
    return kSbMediaAudioSampleTypeFloat32;
  }

  SB_NOTREACHED();

  return kSbMediaAudioSampleTypeFloat32;
}

SbMediaAudioFrameStorageType AudioDecoder::GetStorageType() const {
  SB_DCHECK(BelongsToCurrentThread());

  if (codec_context_->sample_fmt == AV_SAMPLE_FMT_S16 ||
      codec_context_->sample_fmt == AV_SAMPLE_FMT_FLT) {
    return kSbMediaAudioFrameStorageTypeInterleaved;
  }
  if (codec_context_->sample_fmt == AV_SAMPLE_FMT_S16P ||
      codec_context_->sample_fmt == AV_SAMPLE_FMT_FLTP) {
    return kSbMediaAudioFrameStorageTypePlanar;
  }

  SB_NOTREACHED();
  return kSbMediaAudioFrameStorageTypeInterleaved;
}

int AudioDecoder::GetSamplesPerSecond() const {
  return audio_header_.samples_per_second;
}

void AudioDecoder::InitializeCodec() {
  InitializeFfmpeg();

  codec_context_ = avcodec_alloc_context3(NULL);

  if (codec_context_ == NULL) {
    SB_LOG(ERROR) << "Unable to allocate ffmpeg codec context";
    return;
  }

  codec_context_->codec_type = AVMEDIA_TYPE_AUDIO;
  codec_context_->codec_id = GetFfmpegCodecIdByMediaCodec(audio_codec_);
  // Request_sample_fmt is set by us, but sample_fmt is set by the decoder.
  if (GetSupportedSampleType() == kSbMediaAudioSampleTypeInt16) {
    codec_context_->request_sample_fmt = AV_SAMPLE_FMT_S16;
  } else {
    codec_context_->request_sample_fmt = AV_SAMPLE_FMT_FLT;
  }

  codec_context_->channels = audio_header_.number_of_channels;
  codec_context_->sample_rate = audio_header_.samples_per_second;

  codec_context_->extradata = NULL;
  codec_context_->extradata_size = 0;

  AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);

  if (codec == NULL) {
    SB_LOG(ERROR) << "Unable to allocate ffmpeg codec context";
    TeardownCodec();
    return;
  }

  int rv = OpenCodec(codec_context_, codec);
  if (rv < 0) {
    SB_LOG(ERROR) << "Unable to open codec";
    TeardownCodec();
    return;
  }

  av_frame_ = avcodec_alloc_frame();
  if (av_frame_ == NULL) {
    SB_LOG(ERROR) << "Unable to allocate audio frame";
    TeardownCodec();
  }
}

void AudioDecoder::TeardownCodec() {
  if (codec_context_) {
    CloseCodec(codec_context_);
    av_free(codec_context_);
    codec_context_ = NULL;
  }
  if (av_frame_) {
    av_free(av_frame_);
    av_frame_ = NULL;
  }
}

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard
