// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_audio_decoder.h"

#include "base/bind.h"
#include "media/base/data_buffer.h"
#include "media/base/demuxer.h"
#include "media/base/filter_host.h"
#include "media/base/limits.h"
#include "media/ffmpeg/ffmpeg_common.h"

namespace media {

// Returns true if the decode result was an error.
static bool IsErrorResult(int result, int decoded_size) {
  return result < 0 ||
      decoded_size < 0 ||
      decoded_size > AVCODEC_MAX_AUDIO_FRAME_SIZE;
}

// Returns true if the decode result produced audio samples.
static bool ProducedAudioSamples(int decoded_size) {
  return decoded_size > 0;
}

// Returns true if the decode result was a timestamp packet and not actual audio
// data.
static bool IsTimestampMarkerPacket(int result, Buffer* input) {
  // We can get a positive result but no decoded data.  This is ok because this
  // this can be a marker packet that only contains timestamp.
  return result > 0 && !input->IsEndOfStream() &&
      input->GetTimestamp() != kNoTimestamp &&
      input->GetDuration() != kNoTimestamp;
}

// Returns true if the decode result was end of stream.
static bool IsEndOfStream(int result, int decoded_size, Buffer* input) {
  // Three conditions to meet to declare end of stream for this decoder:
  // 1. FFmpeg didn't read anything.
  // 2. FFmpeg didn't output anything.
  // 3. An end of stream buffer is received.
  return result == 0 && decoded_size == 0 && input->IsEndOfStream();
}


FFmpegAudioDecoder::FFmpegAudioDecoder(MessageLoop* message_loop)
    : message_loop_(message_loop),
      codec_context_(NULL),
      bits_per_channel_(0),
      channel_layout_(CHANNEL_LAYOUT_NONE),
      sample_rate_(0),
      decoded_audio_size_(AVCODEC_MAX_AUDIO_FRAME_SIZE),
      decoded_audio_(static_cast<uint8*>(av_malloc(decoded_audio_size_))),
      pending_reads_(0) {
}

FFmpegAudioDecoder::~FFmpegAudioDecoder() {
  av_free(decoded_audio_);
}

void FFmpegAudioDecoder::Flush(FilterCallback* callback) {
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &FFmpegAudioDecoder::DoFlush, callback));
}

void FFmpegAudioDecoder::Initialize(
    DemuxerStream* stream,
    FilterCallback* callback,
    StatisticsCallback* stats_callback) {
  // TODO(scherkus): change Initialize() signature to pass |stream| as a
  // scoped_refptr<>.
  scoped_refptr<DemuxerStream> ref_stream(stream);
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &FFmpegAudioDecoder::DoInitialize,
                        ref_stream, callback, stats_callback));
}

void FFmpegAudioDecoder::ProduceAudioSamples(scoped_refptr<Buffer> buffer) {
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &FFmpegAudioDecoder::DoProduceAudioSamples,
                        buffer));
}

int FFmpegAudioDecoder::bits_per_channel() {
  return bits_per_channel_;
}

ChannelLayout FFmpegAudioDecoder::channel_layout() {
  return channel_layout_;
}

int FFmpegAudioDecoder::sample_rate() {
  return sample_rate_;
}

void FFmpegAudioDecoder::DoInitialize(
    const scoped_refptr<DemuxerStream>& stream,
    FilterCallback* callback,
    StatisticsCallback* stats_callback) {
  scoped_ptr<FilterCallback> c(callback);

  demuxer_stream_ = stream;
  AVStream* av_stream = demuxer_stream_->GetAVStream();
  CHECK(av_stream);

  stats_callback_.reset(stats_callback);

  // Grab the AVStream's codec context and make sure we have sensible values.
  codec_context_ = av_stream->codec;
  int bps = av_get_bits_per_sample_fmt(codec_context_->sample_fmt);
  if (codec_context_->channels <= 0 ||
      codec_context_->channels > Limits::kMaxChannels ||
      (codec_context_->channel_layout == 0 && codec_context_->channels > 2) ||
      bps <= 0 || bps > Limits::kMaxBitsPerSample ||
      codec_context_->sample_rate <= 0 ||
      codec_context_->sample_rate > Limits::kMaxSampleRate) {
    DLOG(ERROR) << "Invalid audio stream -"
                << " channels: " << codec_context_->channels
                << " channel layout:" << codec_context_->channel_layout
                << " bps: " << bps
                << " sample rate: " << codec_context_->sample_rate;

    host()->SetError(PIPELINE_ERROR_DECODE);
    callback->Run();
    return;
  }

  // Serialize calls to avcodec_open().
  AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);
  if (!codec || avcodec_open(codec_context_, codec) < 0) {
    DLOG(ERROR) << "Could not initialize audio decoder: "
                << codec_context_->codec_id;

    host()->SetError(PIPELINE_ERROR_DECODE);
    callback->Run();
    return;
  }

  // Success!
  bits_per_channel_ = av_get_bits_per_sample_fmt(codec_context_->sample_fmt);
  channel_layout_ =
      ChannelLayoutToChromeChannelLayout(codec_context_->channel_layout,
                                         codec_context_->channels);
  sample_rate_ = codec_context_->sample_rate;

  callback->Run();
}

void FFmpegAudioDecoder::DoFlush(FilterCallback* callback) {
  avcodec_flush_buffers(codec_context_);
  estimated_next_timestamp_ = kNoTimestamp;

  callback->Run();
  delete callback;
}

void FFmpegAudioDecoder::DoProduceAudioSamples(
    const scoped_refptr<Buffer>& output) {
  output_buffers_.push_back(output);
  ReadFromDemuxerStream();
}

void FFmpegAudioDecoder::DoDecodeBuffer(const scoped_refptr<Buffer>& input) {
  DCHECK(!output_buffers_.empty());
  DCHECK_GT(pending_reads_, 0);
  pending_reads_--;

  // FFmpeg tends to seek Ogg audio streams in the middle of nowhere, giving us
  // a whole bunch of AV_NOPTS_VALUE packets.  Discard them until we find
  // something valid.  Refer to http://crbug.com/49709
  if (input->GetTimestamp() == kNoTimestamp &&
      estimated_next_timestamp_ == kNoTimestamp &&
      !input->IsEndOfStream()) {
    ReadFromDemuxerStream();
    return;
  }

  AVPacket packet;
  av_init_packet(&packet);
  if (input->IsEndOfStream()) {
    packet.data = NULL;
    packet.size = 0;
  } else {
    packet.data = const_cast<uint8*>(input->GetData());
    packet.size = input->GetDataSize();
  }

  PipelineStatistics statistics;
  statistics.audio_bytes_decoded = input->GetDataSize();

  int decoded_audio_size = decoded_audio_size_;
  int result = avcodec_decode_audio3(
      codec_context_, reinterpret_cast<int16_t*>(decoded_audio_),
      &decoded_audio_size, &packet);

  if (IsErrorResult(result, decoded_audio_size)) {
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

  scoped_refptr<DataBuffer> output;

  if (ProducedAudioSamples(decoded_audio_size)) {
    // Copy the audio samples into an output buffer.
    output = new DataBuffer(decoded_audio_size);
    output->SetDataSize(decoded_audio_size);
    uint8* data = output->GetWritableData();
    memcpy(data, decoded_audio_, decoded_audio_size);

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
  stats_callback_->Run(statistics);
  if (output) {
    DCHECK_GT(output_buffers_.size(), 0u);
    output_buffers_.pop_front();

    ConsumeAudioSamples(output);
  } else {
    ReadFromDemuxerStream();
  }
}

void FFmpegAudioDecoder::ReadFromDemuxerStream() {
  DCHECK(!output_buffers_.empty())
      << "Reads should only occur if there are output buffers.";

  pending_reads_++;
  demuxer_stream_->Read(base::Bind(&FFmpegAudioDecoder::DecodeBuffer, this));
}

void FFmpegAudioDecoder::DecodeBuffer(Buffer* buffer) {
  // TODO(scherkus): change DemuxerStream::Read() to use scoped_refptr<> for
  // callback.
  scoped_refptr<Buffer> ref_buffer(buffer);
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &FFmpegAudioDecoder::DoDecodeBuffer, ref_buffer));
}

void FFmpegAudioDecoder::UpdateDurationAndTimestamp(
    const Buffer* input,
    DataBuffer* output) {
  // Always calculate duration based on the actual number of samples decoded.
  base::TimeDelta duration = CalculateDuration(output->GetDataSize());
  output->SetDuration(duration);

  // Use the incoming timestamp if it's valid.
  if (input->GetTimestamp() != kNoTimestamp) {
    output->SetTimestamp(input->GetTimestamp());
    estimated_next_timestamp_ = input->GetTimestamp() + duration;
    return;
  }

  // Otherwise use an estimated timestamp and attempt to update the estimation
  // as long as it's valid.
  output->SetTimestamp(estimated_next_timestamp_);
  if (estimated_next_timestamp_ != kNoTimestamp) {
    estimated_next_timestamp_ += duration;
  }
}

base::TimeDelta FFmpegAudioDecoder::CalculateDuration(int size) {
  int64 denominator = ChannelLayoutToChannelCount(channel_layout_) *
      bits_per_channel_ / 8 * sample_rate_;
  double microseconds = size /
      (denominator / static_cast<double>(base::Time::kMicrosecondsPerSecond));
  return base::TimeDelta::FromMicroseconds(static_cast<int64>(microseconds));
}

}  // namespace media
