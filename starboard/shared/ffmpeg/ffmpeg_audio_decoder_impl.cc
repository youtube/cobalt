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

#include <string>

#include "starboard/audio_sink.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/media/media_util.h"

namespace starboard {

namespace {

SbMediaAudioSampleType GetSupportedSampleType() {
  if (SbAudioSinkIsAudioSampleTypeSupported(kSbMediaAudioSampleTypeFloat32)) {
    return kSbMediaAudioSampleTypeFloat32;
  }
  return kSbMediaAudioSampleTypeInt16Deprecated;
}

AVCodecID GetFfmpegCodecIdByMediaCodec(const AudioStreamInfo& stream_info) {
  SbMediaAudioCodec audio_codec = stream_info.codec;

  switch (audio_codec) {
    case kSbMediaAudioCodecAac:
      return AV_CODEC_ID_AAC;
    case kSbMediaAudioCodecAc3:
      return AV_CODEC_ID_AC3;
    case kSbMediaAudioCodecEac3:
      return AV_CODEC_ID_EAC3;
    case kSbMediaAudioCodecOpus:
      return AV_CODEC_ID_OPUS;
    case kSbMediaAudioCodecVorbis:
      return AV_CODEC_ID_VORBIS;
    case kSbMediaAudioCodecMp3:
      return AV_CODEC_ID_MP3;
    case kSbMediaAudioCodecPcm:
      if (stream_info.bits_per_sample == 16) {
        return AV_CODEC_ID_PCM_S16LE;
      } else {
        SB_LOG(ERROR) << "PCM is only supported for 16-bit audio ("
                      << stream_info.bits_per_sample
                      << " bits per sample was requested)";
        return AV_CODEC_ID_NONE;
      }
    case kSbMediaAudioCodecFlac:
      if (stream_info.bits_per_sample == 16) {
        return AV_CODEC_ID_FLAC;
      } else {
        SB_LOG(ERROR) << "FLAC is only supported for 16-bit audio ("
                      << stream_info.bits_per_sample
                      << " bits per sample was requested)";
        return AV_CODEC_ID_NONE;
      }
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

FfmpegAudioDecoderImpl<FFMPEG>::FfmpegAudioDecoderImpl(
    const AudioStreamInfo& audio_stream_info)
    : codec_context_(NULL),
      av_frame_(NULL),
      stream_ended_(false),
      audio_stream_info_(audio_stream_info) {
  SB_DCHECK(g_registered) << "Decoder Specialization registration failed.";
  SB_DCHECK_NE(GetFfmpegCodecIdByMediaCodec(audio_stream_info_),
               AV_CODEC_ID_NONE)
      << "Unsupported audio codec " << audio_stream_info_.codec;
  ffmpeg_ = FFMPEGDispatch::GetInstance();
  SB_DCHECK(ffmpeg_);
  if ((ffmpeg_->specialization_version()) == FFMPEG) {
    InitializeCodec();
  }
}

FfmpegAudioDecoderImpl<FFMPEG>::~FfmpegAudioDecoderImpl() {
  TeardownCodec();
}

// static
FfmpegAudioDecoder* FfmpegAudioDecoderImpl<FFMPEG>::Create(
    const AudioStreamInfo& audio_stream_info) {
  return new FfmpegAudioDecoderImpl<FFMPEG>(audio_stream_info);
}

void FfmpegAudioDecoderImpl<FFMPEG>::Initialize(const OutputCB& output_cb,
                                                const ErrorCB& error_cb) {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb);
  SB_DCHECK(!output_cb_);
  SB_DCHECK(error_cb);
  SB_DCHECK(!error_cb_);

  output_cb_ = output_cb;
  error_cb_ = error_cb;
}

void FfmpegAudioDecoderImpl<FFMPEG>::Decode(const InputBuffers& input_buffers,
                                            const ConsumedCB& consumed_cb) {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK_EQ(input_buffers.size(), 1);
  SB_DCHECK(input_buffers[0]);
  SB_DCHECK(output_cb_);
  SB_CHECK(codec_context_);

  Schedule(consumed_cb);

  if (input_buffers.empty() || !input_buffers[0]) {
    SB_LOG(ERROR) << "No input buffer to decode.";
    return;
  }
  if (stream_ended_) {
    SB_LOG(ERROR) << "Decode() is called after WriteEndOfStream() is called.";
    return;
  }
  const auto& input_buffer = input_buffers[0];

  AVPacket packet;
  ffmpeg_->av_init_packet(&packet);
  packet.data = const_cast<uint8_t*>(input_buffer->data());
  packet.size = input_buffer->size();

#if LIBAVUTIL_VERSION_INT < LIBAVUTIL_VERSION_52_8
  ffmpeg_->avcodec_get_frame_defaults(av_frame_);
#endif  // LIBAVUTIL_VERSION_INT < LIBAVUTIL_VERSION_52_8

  int result = 0;
  if (ffmpeg_->avcodec_version() < kAVCodecHasUniformDecodeAPI) {
    int frame_decoded = 0;
    result = ffmpeg_->avcodec_decode_audio4(codec_context_, av_frame_,
                                            &frame_decoded, &packet);
    if (result != input_buffer->size()) {
      // TODO: Consider fill it with silence.
      SB_DLOG(WARNING) << "avcodec_decode_audio4() failed with result: "
                       << result
                       << " with input buffer size: " << input_buffer->size()
                       << " and frame decoded: " << frame_decoded;
      error_cb_(kSbPlayerErrorDecode,
                FormatString("avcodec_decode_audio4() failed with result %d.",
                             result));
      return;
    }

    if (frame_decoded != 1) {
      // TODO: Adjust timestamp accordingly when decoding result is shifted.
      SB_DCHECK_EQ(frame_decoded, 0);
      SB_DLOG(WARNING) << "avcodec_decode_audio4()/avcodec_receive_frame() "
                          "returns with 0 frames decoded";
      return;
    }

    ProcessDecodedFrame(*input_buffer, *av_frame_);
    return;
  }

  // Newer decode API.
  const int send_packet_result =
      ffmpeg_->avcodec_send_packet(codec_context_, &packet);
  if (send_packet_result != 0) {
    const std::string error_message = FormatString(
        "avcodec_send_packet() failed with result %d.", send_packet_result);
    SB_DLOG(WARNING) << error_message;
    error_cb_(kSbPlayerErrorDecode, error_message);
    return;
  }

  // Keep receiving frames until the decoder has processed the entire packet.
  for (;;) {
    result = ffmpeg_->avcodec_receive_frame(codec_context_, av_frame_);
    if (result != 0) {
      // We either hit an error or are done processing packet.
      break;
    }
    ProcessDecodedFrame(*input_buffer, *av_frame_);
  }

  // A return value of AVERROR(EAGAIN) signifies that the decoder needs
  // another packet, so we are done processing the existing packet at that
  // point.
  if (result != AVERROR(EAGAIN)) {
    SB_DLOG(WARNING) << "avcodec_receive_frame() failed with result: "
                     << result;
    error_cb_(
        kSbPlayerErrorDecode,
        FormatString("avcodec_receive_frame() failed with result %d.", result));
  }
}

void FfmpegAudioDecoderImpl<FFMPEG>::ProcessDecodedFrame(
    const InputBuffer& input_buffer,
    const AVFrame& av_frame) {
#if LIBAVCODEC_VERSION_MAJOR >= 61
  const int channel_count = codec_context_->ch_layout.nb_channels;
#else
  const int channel_count = codec_context_->channels;
#endif
  int decoded_audio_size = ffmpeg_->av_samples_get_buffer_size(
      NULL, channel_count, av_frame.nb_samples, codec_context_->sample_fmt, 1);
  audio_stream_info_.samples_per_second = codec_context_->sample_rate;

  if (decoded_audio_size <= 0) {
    // TODO: Consider fill it with silence.
    SB_LOG(ERROR) << "Decoded audio frame is empty.";
    return;
  }

  scoped_refptr<DecodedAudio> decoded_audio = new DecodedAudio(
      channel_count, GetSampleType(), GetStorageType(),
      input_buffer.timestamp(),
      channel_count * av_frame.nb_samples * GetBytesPerSample(GetSampleType()));
  if (GetStorageType() == kSbMediaAudioFrameStorageTypeInterleaved) {
    memcpy(decoded_audio->data(), *av_frame.extended_data,
           decoded_audio->size_in_bytes());
  } else {
    SB_DCHECK_EQ(GetStorageType(), kSbMediaAudioFrameStorageTypePlanar);
    const int per_channel_size_in_bytes =
        decoded_audio->size_in_bytes() / decoded_audio->channels();
    for (int i = 0; i < decoded_audio->channels(); ++i) {
      memcpy(decoded_audio->data() + per_channel_size_in_bytes * i,
             av_frame.extended_data[i], per_channel_size_in_bytes);
    }
    decoded_audio = decoded_audio->SwitchFormatTo(
        GetSampleType(), kSbMediaAudioFrameStorageTypeInterleaved);
  }
  decoded_audio->AdjustForDiscardedDurations(
      audio_stream_info_.samples_per_second,
      input_buffer.audio_sample_info().discarded_duration_from_front,
      input_buffer.audio_sample_info().discarded_duration_from_back);
  decoded_audios_.push(decoded_audio);
  Schedule(output_cb_);
}

void FfmpegAudioDecoderImpl<FFMPEG>::WriteEndOfStream() {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_);

  // AAC has no dependent frames so we needn't flush the decoder.  Set the flag
  // to ensure that Decode() is not called when the stream is ended.
  stream_ended_ = true;
  // Put EOS into the queue.
  decoded_audios_.push(new DecodedAudio);

  Schedule(output_cb_);
}

scoped_refptr<DecodedAudio> FfmpegAudioDecoderImpl<FFMPEG>::Read(
    int* samples_per_second) {
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

void FfmpegAudioDecoderImpl<FFMPEG>::Reset() {
  SB_CHECK(BelongsToCurrentThread());

  TeardownCodec();
  if ((ffmpeg_->specialization_version()) == FFMPEG) {
    InitializeCodec();
  }

  stream_ended_ = false;
  while (!decoded_audios_.empty()) {
    decoded_audios_.pop();
  }

  CancelPendingJobs();
}

bool FfmpegAudioDecoderImpl<FFMPEG>::is_valid() const {
  return (ffmpeg_ != NULL) && ffmpeg_->is_valid() && (codec_context_ != NULL);
}

SbMediaAudioSampleType FfmpegAudioDecoderImpl<FFMPEG>::GetSampleType() const {
  SB_CHECK(BelongsToCurrentThread());

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

SbMediaAudioFrameStorageType FfmpegAudioDecoderImpl<FFMPEG>::GetStorageType()
    const {
  SB_CHECK(BelongsToCurrentThread());

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

void FfmpegAudioDecoderImpl<FFMPEG>::InitializeCodec() {
  codec_context_ = ffmpeg_->avcodec_alloc_context3(NULL);

  if (codec_context_ == NULL) {
    SB_LOG(ERROR) << "Unable to allocate ffmpeg codec context";
    return;
  }

  codec_context_->codec_type = AVMEDIA_TYPE_AUDIO;
  codec_context_->codec_id = GetFfmpegCodecIdByMediaCodec(audio_stream_info_);
  // Request_sample_fmt is set by us, but sample_fmt is set by the decoder.
  if (GetSupportedSampleType() == kSbMediaAudioSampleTypeInt16Deprecated
      // If we request FLT for 16-bit FLAC, FFmpeg will pick S32 as the closest
      // option. Since the rest of this pipeline doesn't support S32, we should
      // use S16 as the desired format.
      || audio_stream_info_.codec == kSbMediaAudioCodecFlac) {
    codec_context_->request_sample_fmt = AV_SAMPLE_FMT_S16;
  } else {
    codec_context_->request_sample_fmt = AV_SAMPLE_FMT_FLT;
  }

#if LIBAVCODEC_VERSION_MAJOR >= 61
  codec_context_->ch_layout.nb_channels = audio_stream_info_.number_of_channels;
#else
  codec_context_->channels = audio_stream_info_.number_of_channels;
#endif
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

void FfmpegAudioDecoderImpl<FFMPEG>::TeardownCodec() {
  if (codec_context_) {
    ffmpeg_->CloseCodec(codec_context_);
    ffmpeg_->FreeContext(&codec_context_);
  }
  ffmpeg_->FreeFrame(&av_frame_);
}

}  // namespace starboard
