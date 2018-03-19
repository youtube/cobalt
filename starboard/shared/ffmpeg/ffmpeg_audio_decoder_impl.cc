// Copyright 2018 Google Inc. All Rights Reserved.
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

// This file contains the explicit specialization of the AudioDecoderImpl class
// for the value 'FFMPEG'.

#include "starboard/shared/ffmpeg/ffmpeg_audio_decoder_impl.h"

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
  return kSbMediaAudioSampleTypeInt16Deprecated;
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

const bool g_registered =
    FFMPEGDispatch::RegisterSpecialization(FFMPEG,
                                           LIBAVCODEC_VERSION_MAJOR,
                                           LIBAVFORMAT_VERSION_MAJOR,
                                           LIBAVUTIL_VERSION_MAJOR);

}  // namespace

AudioDecoderImpl<FFMPEG>::AudioDecoderImpl(
    SbMediaAudioCodec audio_codec,
    const SbMediaAudioHeader& audio_header)
    : audio_codec_(audio_codec),
      codec_context_(NULL),
      av_frame_(NULL),
      stream_ended_(false),
      audio_header_(audio_header) {
  SB_DCHECK(g_registered) << "Decoder Specialization registration failed.";
  SB_DCHECK(GetFfmpegCodecIdByMediaCodec(audio_codec) != AV_CODEC_ID_NONE)
      << "Unsupported audio codec " << audio_codec;
  ffmpeg_ = FFMPEGDispatch::GetInstance();
  SB_DCHECK(ffmpeg_);
  if ((ffmpeg_->specialization_version()) == FFMPEG) {
    InitializeCodec();
  }
}

AudioDecoderImpl<FFMPEG>::~AudioDecoderImpl() {
  TeardownCodec();
}

// static
AudioDecoder* AudioDecoderImpl<FFMPEG>::Create(
    SbMediaAudioCodec audio_codec,
    const SbMediaAudioHeader& audio_header) {
  return new AudioDecoderImpl<FFMPEG>(audio_codec, audio_header);
}

void AudioDecoderImpl<FFMPEG>::Initialize(const OutputCB& output_cb,
                                          const ErrorCB& error_cb) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb);
  SB_DCHECK(!output_cb_);
  SB_DCHECK(error_cb);
  SB_DCHECK(!error_cb_);

  output_cb_ = output_cb;
  error_cb_ = error_cb;
}

void AudioDecoderImpl<FFMPEG>::Decode(
    const scoped_refptr<InputBuffer>& input_buffer,
    const ConsumedCB& consumed_cb) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(input_buffer);
  SB_DCHECK(output_cb_);
  SB_CHECK(codec_context_ != NULL);

  Schedule(consumed_cb);

  if (stream_ended_) {
    SB_LOG(ERROR) << "Decode() is called after WriteEndOfStream() is called.";
    return;
  }

  AVPacket packet;
  ffmpeg_->av_init_packet(&packet);
  packet.data = const_cast<uint8_t*>(input_buffer->data());
  packet.size = input_buffer->size();

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 8, 0)
  ffmpeg_->av_frame_unref(av_frame_);
#else   // LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 8, 0)
  ffmpeg_->avcodec_get_frame_defaults(av_frame_);
#endif  // LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 8, 0)
  int frame_decoded = 0;
  int result = ffmpeg_->avcodec_decode_audio4(codec_context_, av_frame_,
                                              &frame_decoded, &packet);
  if (result != input_buffer->size()) {
    // TODO: Consider fill it with silence.
    SB_DLOG(WARNING) << "avcodec_decode_audio4() failed with result: " << result
                     << " with input buffer size: " << input_buffer->size()
                     << " and frame decoded: " << frame_decoded;
    error_cb_();
    return;
  }

  if (frame_decoded != 1) {
    // TODO: Adjust timestamp accordingly when decoding result is shifted.
    SB_DCHECK(frame_decoded == 0);
    SB_DLOG(WARNING) << "avcodec_decode_audio4() returns with 0 frames decoded";
    return;
  }

  int decoded_audio_size = ffmpeg_->av_samples_get_buffer_size(
      NULL, codec_context_->channels, av_frame_->nb_samples,
      codec_context_->sample_fmt, 1);
  audio_header_.samples_per_second = codec_context_->sample_rate;

  if (decoded_audio_size > 0) {
    scoped_refptr<DecodedAudio> decoded_audio = new DecodedAudio(
        codec_context_->channels, GetSampleType(), GetStorageType(),
        input_buffer->timestamp(),
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

void AudioDecoderImpl<FFMPEG>::WriteEndOfStream() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_);

  // AAC has no dependent frames so we needn't flush the decoder.  Set the flag
  // to ensure that Decode() is not called when the stream is ended.
  stream_ended_ = true;
  // Put EOS into the queue.
  decoded_audios_.push(new DecodedAudio);

  Schedule(output_cb_);
}

scoped_refptr<AudioDecoderImpl<FFMPEG>::DecodedAudio>
AudioDecoderImpl<FFMPEG>::Read() {
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

void AudioDecoderImpl<FFMPEG>::Reset() {
  SB_DCHECK(BelongsToCurrentThread());

  stream_ended_ = false;
  while (!decoded_audios_.empty()) {
    decoded_audios_.pop();
  }

  CancelPendingJobs();
}

bool AudioDecoderImpl<FFMPEG>::is_valid() const {
  return (ffmpeg_ != NULL) && ffmpeg_->is_valid() && (codec_context_ != NULL);
}

SbMediaAudioSampleType AudioDecoderImpl<FFMPEG>::GetSampleType() const {
  SB_DCHECK(BelongsToCurrentThread());

  if (codec_context_->sample_fmt == AV_SAMPLE_FMT_S16 ||
      codec_context_->sample_fmt == AV_SAMPLE_FMT_S16P) {
    return kSbMediaAudioSampleTypeInt16Deprecated;
  } else if (codec_context_->sample_fmt == AV_SAMPLE_FMT_FLT ||
             codec_context_->sample_fmt == AV_SAMPLE_FMT_FLTP) {
    return kSbMediaAudioSampleTypeFloat32;
  }

  SB_NOTREACHED();

  return kSbMediaAudioSampleTypeFloat32;
}

SbMediaAudioFrameStorageType AudioDecoderImpl<FFMPEG>::GetStorageType() const {
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

int AudioDecoderImpl<FFMPEG>::GetSamplesPerSecond() const {
  return audio_header_.samples_per_second;
}

void AudioDecoderImpl<FFMPEG>::InitializeCodec() {
  codec_context_ = ffmpeg_->avcodec_alloc_context3(NULL);

  if (codec_context_ == NULL) {
    SB_LOG(ERROR) << "Unable to allocate ffmpeg codec context";
    return;
  }

  codec_context_->codec_type = AVMEDIA_TYPE_AUDIO;
  codec_context_->codec_id = GetFfmpegCodecIdByMediaCodec(audio_codec_);
  // Request_sample_fmt is set by us, but sample_fmt is set by the decoder.
  if (GetSupportedSampleType() == kSbMediaAudioSampleTypeInt16Deprecated) {
    codec_context_->request_sample_fmt = AV_SAMPLE_FMT_S16;
  } else {
    codec_context_->request_sample_fmt = AV_SAMPLE_FMT_FLT;
  }

  codec_context_->channels = audio_header_.number_of_channels;
  codec_context_->sample_rate = audio_header_.samples_per_second;
  codec_context_->extradata = NULL;
  codec_context_->extradata_size = 0;

  if (codec_context_->codec_id == AV_CODEC_ID_OPUS &&
      audio_header_.audio_specific_config_size > 0) {
    // AV_INPUT_BUFFER_PADDING_SIZE is not defined in ancient avcodec.h.  Use a
    // large enough padding here explicitly.
    const int kAvInputBufferPaddingSize = 256;
    codec_context_->extradata_size = audio_header_.audio_specific_config_size;
    codec_context_->extradata = static_cast<uint8_t*>(ffmpeg_->av_malloc(
        codec_context_->extradata_size + kAvInputBufferPaddingSize));
    SB_DCHECK(codec_context_->extradata);
    SbMemoryCopy(codec_context_->extradata, audio_header_.audio_specific_config,
                 codec_context_->extradata_size);
    SbMemorySet(codec_context_->extradata + codec_context_->extradata_size, 0,
                kAvInputBufferPaddingSize);
  }

  AVCodec* codec = ffmpeg_->avcodec_find_decoder(codec_context_->codec_id);

  if (codec == NULL) {
    SB_LOG(ERROR) << "Unable to allocate ffmpeg codec context";
    TeardownCodec();
    return;
  }

  int rv = ffmpeg_->OpenCodec(codec_context_, codec);
  if (rv < 0) {
    SB_LOG(ERROR) << "Unable to open codec";
    TeardownCodec();
    return;
  }

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 8, 0)
  av_frame_ = ffmpeg_->av_frame_alloc();
#else   // LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 8, 0)
  av_frame_ = ffmpeg_->avcodec_alloc_frame();
#endif  // LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 8, 0)
  if (av_frame_ == NULL) {
    SB_LOG(ERROR) << "Unable to allocate audio frame";
    TeardownCodec();
  }
}

void AudioDecoderImpl<FFMPEG>::TeardownCodec() {
  if (codec_context_) {
    ffmpeg_->CloseCodec(codec_context_);
    if (codec_context_->extradata_size) {
      ffmpeg_->av_freep(&codec_context_->extradata);
    }
    ffmpeg_->av_freep(&codec_context_);
  }
  ffmpeg_->av_freep(&av_frame_);
}

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard
