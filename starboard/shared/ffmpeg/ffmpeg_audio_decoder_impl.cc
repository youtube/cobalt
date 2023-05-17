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

// This file contains the explicit specialization of the AudioDecoderImpl class
// for the value 'FFMPEG'.

#include "starboard/shared/ffmpeg/ffmpeg_audio_decoder_impl.h"

#include "starboard/audio_sink.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
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
    case kSbMediaAudioCodecAc3:
#if SB_API_VERSION < 15
      return kSbHasAc3Audio ? AV_CODEC_ID_AC3 : AV_CODEC_ID_NONE;
#endif  // SB_API_VERSION < 15
      return AV_CODEC_ID_AC3;
    case kSbMediaAudioCodecEac3:
#if SB_API_VERSION < 15
      return kSbHasAc3Audio ? AV_CODEC_ID_EAC3 : AV_CODEC_ID_NONE;
#endif  // SB_API_VERSION < 15
      return AV_CODEC_ID_EAC3;
    case kSbMediaAudioCodecOpus:
      return AV_CODEC_ID_OPUS;
    case kSbMediaAudioCodecVorbis:
      return AV_CODEC_ID_VORBIS;
#if SB_API_VERSION >= 14
    case kSbMediaAudioCodecMp3:
      return AV_CODEC_ID_MP3;
#endif  // SB_API_VERSION >= 14
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
    const AudioStreamInfo& audio_stream_info)
    : codec_context_(NULL),
      av_frame_(NULL),
      stream_ended_(false),
      audio_stream_info_(audio_stream_info) {
  SB_DCHECK(g_registered) << "Decoder Specialization registration failed.";
  SB_DCHECK(GetFfmpegCodecIdByMediaCodec(audio_stream_info_.codec) !=
            AV_CODEC_ID_NONE)
      << "Unsupported audio codec " << audio_stream_info_.codec;
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
    const AudioStreamInfo& audio_stream_info) {
  return new AudioDecoderImpl<FFMPEG>(audio_stream_info);
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

void AudioDecoderImpl<FFMPEG>::Decode(const InputBuffers& input_buffers,
                                      const ConsumedCB& consumed_cb) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(input_buffers.size() == 1);
  SB_DCHECK(input_buffers[0]);
  SB_DCHECK(output_cb_);
  SB_CHECK(codec_context_ != NULL);

  const auto& input_buffer = input_buffers[0];

  Schedule(consumed_cb);

  if (stream_ended_) {
    SB_LOG(ERROR) << "Decode() is called after WriteEndOfStream() is called.";
    return;
  }

  AVPacket packet;
  ffmpeg_->av_init_packet(&packet);
  packet.data = const_cast<uint8_t*>(input_buffer->data());
  packet.size = input_buffer->size();

#if LIBAVUTIL_VERSION_INT < LIBAVUTIL_VERSION_52_8
  ffmpeg_->avcodec_get_frame_defaults(av_frame_);
#endif  // LIBAVUTIL_VERSION_INT < LIBAVUTIL_VERSION_52_8
  int frame_decoded = 0;
  int result = ffmpeg_->avcodec_decode_audio4(codec_context_, av_frame_,
                                              &frame_decoded, &packet);
  if (result != input_buffer->size()) {
    // TODO: Consider fill it with silence.
    SB_DLOG(WARNING) << "avcodec_decode_audio4() failed with result: " << result
                     << " with input buffer size: " << input_buffer->size()
                     << " and frame decoded: " << frame_decoded;
    error_cb_(
        kSbPlayerErrorDecode,
        FormatString("avcodec_decode_audio4() failed with result %d.", result));
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
  audio_stream_info_.samples_per_second = codec_context_->sample_rate;

  if (decoded_audio_size > 0) {
    scoped_refptr<DecodedAudio> decoded_audio = new DecodedAudio(
        codec_context_->channels, GetSampleType(), GetStorageType(),
        input_buffer->timestamp(),
        codec_context_->channels * av_frame_->nb_samples *
            starboard::media::GetBytesPerSample(GetSampleType()));
    if (GetStorageType() == kSbMediaAudioFrameStorageTypeInterleaved) {
      memcpy(decoded_audio->data(), *av_frame_->extended_data,
             decoded_audio->size_in_bytes());
    } else {
      SB_DCHECK(GetStorageType() == kSbMediaAudioFrameStorageTypePlanar);
      const int per_channel_size_in_bytes =
          decoded_audio->size_in_bytes() / decoded_audio->channels();
      for (int i = 0; i < decoded_audio->channels(); ++i) {
        memcpy(decoded_audio->data() + per_channel_size_in_bytes * i,
               av_frame_->extended_data[i], per_channel_size_in_bytes);
      }
      decoded_audio = decoded_audio->SwitchFormatTo(
          GetSampleType(), kSbMediaAudioFrameStorageTypeInterleaved);
    }
    decoded_audio->AdjustForDiscardedDurations(
        audio_stream_info_.samples_per_second,
        input_buffer->audio_sample_info().discarded_duration_from_front,
        input_buffer->audio_sample_info().discarded_duration_from_back);
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
AudioDecoderImpl<FFMPEG>::Read(int* samples_per_second) {
  SB_DCHECK(BelongsToCurrentThread());
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

void AudioDecoderImpl<FFMPEG>::InitializeCodec() {
  codec_context_ = ffmpeg_->avcodec_alloc_context3(NULL);

  if (codec_context_ == NULL) {
    SB_LOG(ERROR) << "Unable to allocate ffmpeg codec context";
    return;
  }

  codec_context_->codec_type = AVMEDIA_TYPE_AUDIO;
  codec_context_->codec_id =
      GetFfmpegCodecIdByMediaCodec(audio_stream_info_.codec);
  // Request_sample_fmt is set by us, but sample_fmt is set by the decoder.
  if (GetSupportedSampleType() == kSbMediaAudioSampleTypeInt16Deprecated) {
    codec_context_->request_sample_fmt = AV_SAMPLE_FMT_S16;
  } else {
    codec_context_->request_sample_fmt = AV_SAMPLE_FMT_FLT;
  }

  codec_context_->channels = audio_stream_info_.number_of_channels;
  codec_context_->sample_rate = audio_stream_info_.samples_per_second;
  codec_context_->extradata = NULL;
  codec_context_->extradata_size = 0;

  if ((codec_context_->codec_id == AV_CODEC_ID_OPUS ||
       codec_context_->codec_id == AV_CODEC_ID_VORBIS) &&
      !audio_stream_info_.audio_specific_config.empty()) {
    // AV_INPUT_BUFFER_PADDING_SIZE is not defined in ancient avcodec.h.  Use a
    // large enough padding here explicitly.
    const int kAvInputBufferPaddingSize = 256;
    codec_context_->extradata_size =
        audio_stream_info_.audio_specific_config.size();
    codec_context_->extradata = static_cast<uint8_t*>(ffmpeg_->av_malloc(
        codec_context_->extradata_size + kAvInputBufferPaddingSize));
    SB_DCHECK(codec_context_->extradata);
    memcpy(codec_context_->extradata,
           audio_stream_info_.audio_specific_config.data(),
           codec_context_->extradata_size);
    memset(codec_context_->extradata + codec_context_->extradata_size, 0,
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

  if (ffmpeg_->avcodec_version() > kAVCodecSupportsAvFrameAlloc) {
    av_frame_ = ffmpeg_->av_frame_alloc();
  } else {
    av_frame_ = ffmpeg_->avcodec_alloc_frame();
  }
  if (av_frame_ == NULL) {
    SB_LOG(ERROR) << "Unable to allocate audio frame";
    TeardownCodec();
  }
}

void AudioDecoderImpl<FFMPEG>::TeardownCodec() {
  if (codec_context_) {
    ffmpeg_->CloseCodec(codec_context_);
    ffmpeg_->FreeContext(&codec_context_);
  }
  ffmpeg_->FreeFrame(&av_frame_);
}

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard
