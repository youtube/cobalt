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

#include "starboard/log.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

namespace {

// The required output format for the decoder is interleaved float.  However
// the output of the ffmpeg decoder can be in other formats.  So libavresample
// is used to convert the output into the required format.
void ConvertToInterleavedFloat(int source_sample_format,
                               int channel_layout,
                               int sample_rate,
                               int samples_per_channel,
                               uint8_t** input_buffer,
                               uint8_t* output_buffer) {
  AVAudioResampleContext* context = avresample_alloc_context();
  SB_DCHECK(context != NULL);

  av_opt_set_int(context, "in_channel_layout", channel_layout, 0);
  av_opt_set_int(context, "out_channel_layout", channel_layout, 0);
  av_opt_set_int(context, "in_sample_rate", sample_rate, 0);
  av_opt_set_int(context, "out_sample_rate", sample_rate, 0);
  av_opt_set_int(context, "in_sample_fmt", source_sample_format, 0);
  av_opt_set_int(context, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
  av_opt_set_int(context, "internal_sample_fmt", source_sample_format, 0);

  int result = avresample_open(context);
  SB_DCHECK(!result);

  int samples_resampled =
      avresample_convert(context, &output_buffer, 1024, samples_per_channel,
                         input_buffer, 0, samples_per_channel);
  SB_DCHECK(samples_resampled == samples_per_channel);

  avresample_close(context);
  av_free(context);
}

}  // namespace

AudioDecoder::AudioDecoder(SbMediaAudioCodec audio_codec,
                           const SbMediaAudioHeader& audio_header)
    : codec_context_(NULL),
      av_frame_(NULL),
      stream_ended_(false),
      audio_header_(audio_header) {
  SB_DCHECK(audio_codec == kSbMediaAudioCodecAac);

  InitializeCodec();
}

AudioDecoder::~AudioDecoder() {
  TeardownCodec();
}

void AudioDecoder::Decode(const InputBuffer& input_buffer,
                          std::vector<float>* output) {
  SB_CHECK(output != NULL);
  SB_CHECK(codec_context_ != NULL);

  if (stream_ended_) {
    SB_LOG(ERROR) << "Decode() is called after WriteEndOfStream() is called.";
    return;
  }

  AVPacket packet;
  av_init_packet(&packet);
  packet.data = const_cast<uint8_t*>(input_buffer.data());
  packet.size = input_buffer.size();

  avcodec_get_frame_defaults(av_frame_);
  int frame_decoded = 0;
  int result =
      avcodec_decode_audio4(codec_context_, av_frame_, &frame_decoded, &packet);
  SB_DCHECK(result == input_buffer.size())
      << result << " " << input_buffer.size() << " " << frame_decoded;
  SB_DCHECK(frame_decoded == 1);

  int decoded_audio_size = av_samples_get_buffer_size(
      NULL, codec_context_->channels, av_frame_->nb_samples,
      codec_context_->sample_fmt, 1);
  audio_header_.samples_per_second = codec_context_->sample_rate;

  if (decoded_audio_size > 0) {
    output->resize(decoded_audio_size / sizeof(float));
    ConvertToInterleavedFloat(
        codec_context_->sample_fmt, codec_context_->channel_layout,
        audio_header_.samples_per_second, av_frame_->nb_samples,
        av_frame_->extended_data, reinterpret_cast<uint8_t*>(&(*output)[0]));
  } else {
    SB_LOG(ERROR) << "Decoded audio frame is empty.";
    output->clear();
  }
}

void AudioDecoder::WriteEndOfStream() {
  // AAC has no dependent frames so we needn't flush the decoder.  Set the flag
  // to ensure that Decode() is not called when the stream is ended.
  stream_ended_ = true;
}

void AudioDecoder::Reset() {
  stream_ended_ = false;
}

int AudioDecoder::GetSamplesPerSecond() {
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
  codec_context_->codec_id = AV_CODEC_ID_AAC;
  // Request_sample_fmt is set by us, but sample_fmt is set by the decoder.
  codec_context_->request_sample_fmt = AV_SAMPLE_FMT_FLT;  // interleaved float

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

  int rv = avcodec_open2(codec_context_, codec, NULL);
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
    avcodec_close(codec_context_);
    av_free(codec_context_);
    codec_context_ = NULL;
  }
  if (av_frame_) {
    av_free(av_frame_);
    av_frame_ = NULL;
  }
}

}  // namespace ffmpeg

namespace starboard {
namespace player {

// static
AudioDecoder* AudioDecoder::Create(SbMediaAudioCodec audio_codec,
                                   const SbMediaAudioHeader& audio_header) {
  return new ffmpeg::AudioDecoder(audio_codec, audio_header);
}

}  // namespace player
}  // namespace starboard

}  // namespace shared
}  // namespace starboard
