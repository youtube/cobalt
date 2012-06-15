// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_audio_decoder.h"

#include "base/bind.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/data_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer.h"
#include "media/base/pipeline.h"
#include "media/ffmpeg/ffmpeg_common.h"

namespace media {

// Returns true if the decode result was a timestamp packet and not actual audio
// data.
static inline bool IsTimestampMarkerPacket(int result, Buffer* input) {
  // We can get a positive result but no decoded data.  This is ok because this
  // this can be a marker packet that only contains timestamp.
  return result > 0 && !input->IsEndOfStream() &&
      input->GetTimestamp() != kNoTimestamp() &&
      input->GetDuration() != kNoTimestamp();
}

// Returns true if the decode result was end of stream.
static inline bool IsEndOfStream(int result, int decoded_size, Buffer* input) {
  // Three conditions to meet to declare end of stream for this decoder:
  // 1. FFmpeg didn't read anything.
  // 2. FFmpeg didn't output anything.
  // 3. An end of stream buffer is received.
  return result == 0 && decoded_size == 0 && input->IsEndOfStream();
}


FFmpegAudioDecoder::FFmpegAudioDecoder(
    const base::Callback<MessageLoop*()>& message_loop_cb)
    : message_loop_factory_cb_(message_loop_cb),
      message_loop_(NULL),
      codec_context_(NULL),
      bits_per_channel_(0),
      channel_layout_(CHANNEL_LAYOUT_NONE),
      samples_per_second_(0),
      av_frame_(NULL) {
}

void FFmpegAudioDecoder::Initialize(
    const scoped_refptr<DemuxerStream>& stream,
    const PipelineStatusCB& status_cb,
    const StatisticsCB& statistics_cb) {
  if (!message_loop_) {
    message_loop_ = message_loop_factory_cb_.Run();
    message_loop_factory_cb_.Reset();
  } else {
    // TODO(scherkus): initialization currently happens more than once in
    // PipelineIntegrationTest.BasicPlayback.
    LOG(ERROR) << "Initialize has already been called.";
  }
  message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&FFmpegAudioDecoder::DoInitialize, this,
                 stream, status_cb, statistics_cb));
}

void FFmpegAudioDecoder::Read(const ReadCB& read_cb) {
  // Complete operation asynchronously on different stack of execution as per
  // the API contract of AudioDecoder::Read()
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &FFmpegAudioDecoder::DoRead, this, read_cb));
}

int FFmpegAudioDecoder::bits_per_channel() {
  return bits_per_channel_;
}

ChannelLayout FFmpegAudioDecoder::channel_layout() {
  return channel_layout_;
}

int FFmpegAudioDecoder::samples_per_second() {
  return samples_per_second_;
}

void FFmpegAudioDecoder::Reset(const base::Closure& closure) {
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &FFmpegAudioDecoder::DoReset, this, closure));
}

FFmpegAudioDecoder::~FFmpegAudioDecoder() {
  // TODO(scherkus): should we require Stop() to be called? this might end up
  // getting called on a random thread due to refcounting.
  if (codec_context_) {
    av_free(codec_context_->extradata);
    avcodec_close(codec_context_);
    av_free(codec_context_);
  }

  if (av_frame_) {
    av_free(av_frame_);
    av_frame_ = NULL;
  }
}

void FFmpegAudioDecoder::DoInitialize(
    const scoped_refptr<DemuxerStream>& stream,
    const PipelineStatusCB& status_cb,
    const StatisticsCB& statistics_cb) {
  demuxer_stream_ = stream;
  const AudioDecoderConfig& config = stream->audio_decoder_config();
  statistics_cb_ = statistics_cb;

  // TODO(scherkus): this check should go in Pipeline prior to creating
  // decoder objects.
  if (!config.IsValidConfig()) {
    DLOG(ERROR) << "Invalid audio stream -"
                << " codec: " << config.codec()
                << " channel layout: " << config.channel_layout()
                << " bits per channel: " << config.bits_per_channel()
                << " samples per second: " << config.samples_per_second();

    status_cb.Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  // Initialize AVCodecContext structure.
  codec_context_ = avcodec_alloc_context3(NULL);
  AudioDecoderConfigToAVCodecContext(config, codec_context_);

  AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);
  if (!codec || avcodec_open2(codec_context_, codec, NULL) < 0) {
    DLOG(ERROR) << "Could not initialize audio decoder: "
                << codec_context_->codec_id;

    status_cb.Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  // Success!
  av_frame_ = avcodec_alloc_frame();
  bits_per_channel_ = config.bits_per_channel();
  channel_layout_ = config.channel_layout();
  samples_per_second_ = config.samples_per_second();

  status_cb.Run(PIPELINE_OK);
}

void FFmpegAudioDecoder::DoReset(const base::Closure& closure) {
  avcodec_flush_buffers(codec_context_);
  estimated_next_timestamp_ = kNoTimestamp();
  closure.Run();
}

void FFmpegAudioDecoder::DoRead(const ReadCB& read_cb) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK(!read_cb.is_null());
  CHECK(read_cb_.is_null()) << "Overlapping decodes are not supported.";

  read_cb_ = read_cb;
  ReadFromDemuxerStream();
}

void FFmpegAudioDecoder::DoDecodeBuffer(
    const scoped_refptr<DecoderBuffer>& input) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK(!read_cb_.is_null());

  if (!input) {
    // DemuxeStream::Read() was aborted so we abort the decoder's pending read.
    DeliverSamples(NULL);
    return;
  }

  // FFmpeg tends to seek Ogg audio streams in the middle of nowhere, giving us
  // a whole bunch of AV_NOPTS_VALUE packets.  Discard them until we find
  // something valid.  Refer to http://crbug.com/49709
  if (input->GetTimestamp() == kNoTimestamp() &&
      estimated_next_timestamp_ == kNoTimestamp() &&
      !input->IsEndOfStream()) {
    ReadFromDemuxerStream();
    return;
  }

  AVPacket packet;
  av_init_packet(&packet);
  packet.data = const_cast<uint8*>(input->GetData());
  packet.size = input->GetDataSize();

  PipelineStatistics statistics;
  statistics.audio_bytes_decoded = input->GetDataSize();

  // Reset frame to default values.
  avcodec_get_frame_defaults(av_frame_);

  int frame_decoded = 0;
  int result = avcodec_decode_audio4(
      codec_context_, av_frame_, &frame_decoded, &packet);

  if (result < 0) {
    DCHECK(!input->IsEndOfStream())
        << "End of stream buffer produced an error! "
        << "This is quite possibly a bug in the audio decoder not handling "
        << "end of stream AVPackets correctly.";

    DLOG(ERROR) << "Error decoding an audio frame with timestamp: "
                << input->GetTimestamp().InMicroseconds() << " us, duration: "
                << input->GetDuration().InMicroseconds() << " us, packet size: "
                << input->GetDataSize() << " bytes";

    ReadFromDemuxerStream();
    return;
  }

  int decoded_audio_size = 0;
  if (frame_decoded) {
    decoded_audio_size = av_samples_get_buffer_size(
        NULL, codec_context_->channels, av_frame_->nb_samples,
        codec_context_->sample_fmt, 1);
  }

  scoped_refptr<DataBuffer> output;

  if (decoded_audio_size > 0) {
    // Copy the audio samples into an output buffer.
    output = new DataBuffer(decoded_audio_size);
    output->SetDataSize(decoded_audio_size);
    uint8* data = output->GetWritableData();
    memcpy(data, av_frame_->data[0], decoded_audio_size);

    UpdateDurationAndTimestamp(input, output);
  } else if (IsTimestampMarkerPacket(result, input)) {
    // Nothing else to do here but update our estimation.
    estimated_next_timestamp_ = input->GetTimestamp() + input->GetDuration();
  } else if (IsEndOfStream(result, decoded_audio_size, input)) {
    // Create an end of stream output buffer.
    output = new DataBuffer(0);
    output->SetTimestamp(input->GetTimestamp());
    output->SetDuration(input->GetDuration());
  }

  // Decoding finished successfully, update stats and execute callback.
  statistics_cb_.Run(statistics);
  if (output) {
    DeliverSamples(output);
  } else {
    ReadFromDemuxerStream();
  }
}

void FFmpegAudioDecoder::ReadFromDemuxerStream() {
  DCHECK(!read_cb_.is_null());

  demuxer_stream_->Read(base::Bind(&FFmpegAudioDecoder::DecodeBuffer, this));
}

void FFmpegAudioDecoder::DecodeBuffer(
    const scoped_refptr<DecoderBuffer>& buffer) {
  // TODO(scherkus): fix FFmpegDemuxerStream::Read() to not execute our read
  // callback on the same execution stack so we can get rid of forced task post.
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &FFmpegAudioDecoder::DoDecodeBuffer, this, buffer));
}

void FFmpegAudioDecoder::UpdateDurationAndTimestamp(
    const Buffer* input,
    DataBuffer* output) {
  // Always calculate duration based on the actual number of samples decoded.
  base::TimeDelta duration = CalculateDuration(output->GetDataSize());
  output->SetDuration(duration);

  // Use the incoming timestamp if it's valid.
  if (input->GetTimestamp() != kNoTimestamp()) {
    output->SetTimestamp(input->GetTimestamp());
    estimated_next_timestamp_ = input->GetTimestamp() + duration;
    return;
  }

  // Otherwise use an estimated timestamp and attempt to update the estimation
  // as long as it's valid.
  output->SetTimestamp(estimated_next_timestamp_);
  if (estimated_next_timestamp_ != kNoTimestamp()) {
    estimated_next_timestamp_ += duration;
  }
}

base::TimeDelta FFmpegAudioDecoder::CalculateDuration(int size) {
  int64 denominator = ChannelLayoutToChannelCount(channel_layout_) *
      bits_per_channel_ / 8 * samples_per_second_;
  double microseconds = size /
      (denominator / static_cast<double>(base::Time::kMicrosecondsPerSecond));
  return base::TimeDelta::FromMicroseconds(static_cast<int64>(microseconds));
}

void FFmpegAudioDecoder::DeliverSamples(const scoped_refptr<Buffer>& samples) {
  // Reset the callback before running to protect against reentrancy.
  ReadCB read_cb = read_cb_;
  read_cb_.Reset();
  read_cb.Run(samples);
}

}  // namespace media
