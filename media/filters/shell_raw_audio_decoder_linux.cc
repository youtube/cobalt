/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "media/filters/shell_raw_audio_decoder_linux.h"

#include <memory.h>

#include <list>

#include "base/bind.h"
#include "base/logging.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/decoder_buffer_pool.h"
#include "media/filters/shell_ffmpeg.h"
#include "media/mp4/aac.h"

namespace media {

namespace {

const size_t kSampleSizeInBytes = sizeof(float);

struct QueuedAudioBuffer {
  //  AudioDecoder::Status status;  // status is used to represent decode errors
  scoped_refptr<DecoderBuffer> buffer;
};

bool IsEndOfStream(int result,
                   int decoded_size,
                   scoped_refptr<DecoderBuffer> buffer) {
  return result == 0 && decoded_size == 0 && buffer->IsEndOfStream();
}

void ResampleToInterleavedFloat(int source_sample_format,
                                int channel_layout,
                                int sample_rate,
                                int samples_per_channel,
                                uint8** input_buffer,
                                uint8* output_buffer) {
  AVAudioResampleContext* context = avresample_alloc_context();
  DCHECK(context);

  av_opt_set_int(context, "in_channel_layout", channel_layout, 0);
  av_opt_set_int(context, "out_channel_layout", channel_layout, 0);
  av_opt_set_int(context, "in_sample_rate", sample_rate, 0);
  av_opt_set_int(context, "out_sample_rate", sample_rate, 0);
  av_opt_set_int(context, "in_sample_fmt", source_sample_format, 0);
  av_opt_set_int(context, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
  av_opt_set_int(context, "internal_sample_fmt", source_sample_format, 0);

  int result = avresample_open(context);
  DCHECK(!result);

  int samples_resampled =
      avresample_convert(context, &output_buffer, 1024, samples_per_channel,
                         input_buffer, 0, samples_per_channel);
  DCHECK_EQ(samples_resampled, samples_per_channel);

  avresample_close(context);
  av_free(context);
}

class ShellRawAudioDecoderLinux : public ShellRawAudioDecoder {
 public:
  ShellRawAudioDecoderLinux();
  ~ShellRawAudioDecoderLinux() override;

  int GetBytesPerSample() const override { return kSampleSizeInBytes; }
  // When the input buffer is not NULL, it can be a normal buffer or an EOS
  // buffer. In this case the function will return the decoded buffer if there
  // is any.
  // The input buffer can be NULL, in this case the function will return a
  // queued buffer if there is any or return NULL if there is no queued buffer.
  void Decode(const scoped_refptr<DecoderBuffer>& buffer,
              const DecodeCB& decoder_cb) override;
  bool Flush() override;
  bool UpdateConfig(const AudioDecoderConfig& config) override;

 private:
  void ReleaseResource();
  void ResetTimestampState();
  void RunDecodeLoop(const scoped_refptr<DecoderBuffer>& input,
                     bool skip_eos_append);

  DecoderBufferPool decoder_buffer_pool_;

  AVCodecContext* codec_context_;
  AVFrame* av_frame_;

  // Decoded audio format.
  int bits_per_channel_;
  ChannelLayout channel_layout_;
  int samples_per_second_;

  // Used for computing output timestamps.
  scoped_ptr<AudioTimestampHelper> output_timestamp_helper_;
  int bytes_per_frame_;
  base::TimeDelta last_input_timestamp_;

  // Since multiple frames may be decoded from the same packet we need to
  // queue them up and hand them out as we receive Read() calls.
  std::list<QueuedAudioBuffer> queued_audio_;

  DISALLOW_COPY_AND_ASSIGN(ShellRawAudioDecoderLinux);
};

ShellRawAudioDecoderLinux::ShellRawAudioDecoderLinux()
    : decoder_buffer_pool_(GetBytesPerSample()),
      codec_context_(NULL),
      av_frame_(NULL),
      bits_per_channel_(0),
      channel_layout_(CHANNEL_LAYOUT_NONE),
      samples_per_second_(0),
      bytes_per_frame_(0),
      last_input_timestamp_(kNoTimestamp()) {
  EnsureFfmpegInitialized();
}

ShellRawAudioDecoderLinux::~ShellRawAudioDecoderLinux() {
  ReleaseResource();
}

void ShellRawAudioDecoderLinux::Decode(
    const scoped_refptr<DecoderBuffer>& buffer,
    const DecodeCB& decoder_cb) {
  if (buffer && !buffer->IsEndOfStream()) {
    if (last_input_timestamp_ == kNoTimestamp()) {
      last_input_timestamp_ = buffer->GetTimestamp();
    } else if (buffer->GetTimestamp() != kNoTimestamp()) {
      DCHECK_GE(buffer->GetTimestamp().ToInternalValue(),
                last_input_timestamp_.ToInternalValue());
      last_input_timestamp_ = buffer->GetTimestamp();
    }
  }

  if (buffer && queued_audio_.empty())
    RunDecodeLoop(buffer, false);

  if (queued_audio_.empty()) {
    decoder_cb.Run(NEED_MORE_DATA, NULL);
    return;
  }
  scoped_refptr<DecoderBuffer> result = queued_audio_.front().buffer;
  queued_audio_.pop_front();
  decoder_cb.Run(BUFFER_DECODED, result);
}

bool ShellRawAudioDecoderLinux::Flush() {
  avcodec_flush_buffers(codec_context_);
  ResetTimestampState();
  queued_audio_.clear();

  return true;
}

bool ShellRawAudioDecoderLinux::UpdateConfig(const AudioDecoderConfig& config) {
  if (!config.IsValidConfig()) {
    DLOG(ERROR) << "Invalid audio stream -"
                << " codec: " << config.codec()
                << " channel layout: " << config.channel_layout()
                << " bits per channel: " << config.bits_per_channel()
                << " samples per second: " << config.samples_per_second();
    return false;
  }

  if (codec_context_ && (bits_per_channel_ != config.bits_per_channel() ||
                         channel_layout_ != config.channel_layout() ||
                         samples_per_second_ != config.samples_per_second())) {
    DVLOG(1) << "Unsupported config change :";
    DVLOG(1) << "\tbits_per_channel : " << bits_per_channel_ << " -> "
             << config.bits_per_channel();
    DVLOG(1) << "\tchannel_layout : " << channel_layout_ << " -> "
             << config.channel_layout();
    DVLOG(1) << "\tsample_rate : " << samples_per_second_ << " -> "
             << config.samples_per_second();
    return false;
  }

  ReleaseResource();

  codec_context_ = avcodec_alloc_context3(NULL);
  DCHECK(codec_context_);
  codec_context_->codec_type = AVMEDIA_TYPE_AUDIO;
  codec_context_->codec_id = CODEC_ID_AAC;
  // request_sample_fmt is set by us, but sample_fmt is set by the decoder.
  codec_context_->request_sample_fmt = AV_SAMPLE_FMT_FLT;  // interleaved float

  codec_context_->channels =
      ChannelLayoutToChannelCount(config.channel_layout());
  codec_context_->sample_rate = config.samples_per_second();

  if (config.extra_data()) {
    codec_context_->extradata_size = config.extra_data_size();
    codec_context_->extradata = reinterpret_cast<uint8_t*>(
        av_malloc(config.extra_data_size() + FF_INPUT_BUFFER_PADDING_SIZE));
    memcpy(codec_context_->extradata, config.extra_data(),
           config.extra_data_size());
    memset(codec_context_->extradata + config.extra_data_size(), '\0',
           FF_INPUT_BUFFER_PADDING_SIZE);
  } else {
    codec_context_->extradata = NULL;
    codec_context_->extradata_size = 0;
  }

  AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);
  DCHECK(codec);

  int rv = avcodec_open2(codec_context_, codec, NULL);
  DCHECK_GE(rv, 0);
  if (rv < 0) {
    DLOG(ERROR) << "Unable to open codec, result = " << rv;
    return false;
  }

  // Ensure avcodec_open2() respected our format request.
  if (codec_context_->sample_fmt != codec_context_->request_sample_fmt) {
    DLOG(INFO) << "Unable to configure a supported sample format,"
               << " sample_fmt = " << codec_context_->sample_fmt
               << " instead of " << codec_context_->request_sample_fmt
               << ". Use libavresample to resample the decoded result to FLT";
    DLOG(INFO) << "Supported formats:";
    const AVSampleFormat* fmt;
    for (fmt = codec_context_->codec->sample_fmts; *fmt != -1; ++fmt) {
      DLOG(INFO) << "  " << *fmt << " (" << av_get_sample_fmt_name(*fmt) << ")";
    }
  }

  av_frame_ = avcodec_alloc_frame();
  DCHECK(av_frame_);

  bits_per_channel_ = config.bits_per_channel();
  channel_layout_ = config.channel_layout();
  samples_per_second_ = config.samples_per_second();
  output_timestamp_helper_.reset(new AudioTimestampHelper(
      kSampleSizeInBytes, config.samples_per_second()));

  ResetTimestampState();

  return true;
}

void ShellRawAudioDecoderLinux::ReleaseResource() {
  if (codec_context_) {
    av_free(codec_context_->extradata);
    avcodec_close(codec_context_);
    av_free(codec_context_);
    codec_context_ = NULL;
  }
  if (av_frame_) {
    av_free(av_frame_);
    av_frame_ = NULL;
  }
}

void ShellRawAudioDecoderLinux::ResetTimestampState() {
  output_timestamp_helper_->SetBaseTimestamp(kNoTimestamp());
  last_input_timestamp_ = kNoTimestamp();
}

void ShellRawAudioDecoderLinux::RunDecodeLoop(
    const scoped_refptr<DecoderBuffer>& input,
    bool skip_eos_append) {
  AVPacket packet;
  av_init_packet(&packet);
  packet.data = input->GetWritableData();
  packet.size = input->GetDataSize();

  do {
    avcodec_get_frame_defaults(av_frame_);
    int frame_decoded = 0;
    int result = avcodec_decode_audio4(codec_context_, av_frame_,
                                       &frame_decoded, &packet);
    DCHECK_GE(result, 0);

    // Update packet size and data pointer in case we need to call the decoder
    // with the remaining bytes from this packet.
    packet.size -= result;
    packet.data += result;

    if (output_timestamp_helper_->base_timestamp() == kNoTimestamp() &&
        !input->IsEndOfStream()) {
      DCHECK_NE(input->GetTimestamp().ToInternalValue(),
                kNoTimestamp().ToInternalValue());
      output_timestamp_helper_->SetBaseTimestamp(input->GetTimestamp());
    }

    const uint8* decoded_audio_data = NULL;
    int decoded_audio_size = 0;
    if (frame_decoded) {
      decoded_audio_data = av_frame_->data[0];
      decoded_audio_size = av_samples_get_buffer_size(
          NULL, codec_context_->channels, av_frame_->nb_samples,
          codec_context_->sample_fmt, 1);
    }

    scoped_refptr<DecoderBuffer> output;

    if (decoded_audio_size > 0) {
      // Copy the audio samples into an output buffer.
      int buffer_size = kSampleSizeInBytes * mp4::AAC::kFramesPerAccessUnit *
                        codec_context_->channels;
      output = decoder_buffer_pool_.Allocate(buffer_size);
      DCHECK(output);
      // Interleave the planar samples to conform to the general decoder
      // requirement. This should eventually be lifted.
      ResampleToInterleavedFloat(
          codec_context_->sample_fmt, codec_context_->channel_layout,
          samples_per_second_, mp4::AAC::kFramesPerAccessUnit,
          av_frame_->extended_data,
          reinterpret_cast<uint8*>(output->GetWritableData()));
      output->SetTimestamp(output_timestamp_helper_->GetTimestamp());
      output->SetDuration(
          output_timestamp_helper_->GetDuration(decoded_audio_size));
      output_timestamp_helper_->AddBytes(decoded_audio_size /
                                         codec_context_->channels);
    } else if (IsEndOfStream(result, decoded_audio_size, input) &&
               !skip_eos_append) {
      DCHECK_EQ(packet.size, 0);
      // Create an end of stream output buffer.
      output = DecoderBuffer::CreateEOSBuffer(kNoTimestamp());
    }

    if (output) {
      QueuedAudioBuffer queue_entry = {output};
      queued_audio_.push_back(queue_entry);
    }

    // TODO: update statistics.
  } while (packet.size > 0);
}

}  // namespace

scoped_ptr<ShellRawAudioDecoder> CreateShellRawAudioDecoderLinux(
    const AudioDecoderConfig& config) {
  scoped_ptr<ShellRawAudioDecoder> decoder(new ShellRawAudioDecoderLinux);
  if (!decoder->UpdateConfig(config)) {
    decoder.reset();
  }
  return decoder.Pass();
}

}  // namespace media
