// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_audio_decoder.h"

#include "media/base/callback.h"
#include "media/base/data_buffer.h"
#include "media/base/limits.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_demuxer.h"

#if !defined(USE_SSE)
#if defined(__SSE__) || defined(ARCH_CPU_X86_64) || _M_IX86_FP==1
#define USE_SSE 1
#else
#define USE_SSE 0
#endif
#endif
#if USE_SSE
#include <xmmintrin.h>
#endif

namespace media {

// Size of the decoded audio buffer.
const size_t FFmpegAudioDecoder::kOutputBufferSize =
    AVCODEC_MAX_AUDIO_FRAME_SIZE;

FFmpegAudioDecoder::FFmpegAudioDecoder(MessageLoop* message_loop)
    : DecoderBase<AudioDecoder, Buffer>(message_loop),
      codec_context_(NULL),
      estimated_next_timestamp_(kNoTimestamp) {
}

FFmpegAudioDecoder::~FFmpegAudioDecoder() {
}

void FFmpegAudioDecoder::DoInitialize(DemuxerStream* demuxer_stream,
                                      bool* success,
                                      Task* done_cb) {
  AutoTaskRunner done_runner(done_cb);
  *success = false;

  // Get the AVStream by querying for the provider interface.
  AVStreamProvider* av_stream_provider;
  if (!demuxer_stream->QueryInterface(&av_stream_provider)) {
    return;
  }
  AVStream* av_stream = av_stream_provider->GetAVStream();

  // Grab the AVStream's codec context and make sure we have sensible values.
  codec_context_ = av_stream->codec;
  int bps = av_get_bits_per_sample_fmt(codec_context_->sample_fmt);
  if (codec_context_->channels <= 0 ||
      codec_context_->channels > Limits::kMaxChannels ||
      bps <= 0 || bps > Limits::kMaxBitsPerSample ||
      codec_context_->sample_rate <= 0 ||
      codec_context_->sample_rate > Limits::kMaxSampleRate) {
    DLOG(WARNING) << "Invalid audio stream -"
        << " channels: " << codec_context_->channels
        << " bps: " << bps
        << " sample rate: " << codec_context_->sample_rate;
    return;
  }

  // Serialize calls to avcodec_open().
  AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);
  if (!codec || avcodec_open(codec_context_, codec) < 0) {
    return;
  }

  // When calling avcodec_find_decoder(), |codec_context_| might be altered by
  // the decoder to give more accurate values of the output format of the
  // decoder. So set the media format after a decoder is allocated.
  // TODO(hclam): Reuse the information provided by the demuxer for now, we may
  // need to wait until the first buffer is decoded to know the correct
  // information.
  media_format_.SetAsInteger(MediaFormat::kChannels, codec_context_->channels);
  media_format_.SetAsInteger(MediaFormat::kSampleBits,
      av_get_bits_per_sample_fmt(codec_context_->sample_fmt));
  media_format_.SetAsInteger(MediaFormat::kSampleRate,
      codec_context_->sample_rate);

  // Prepare the output buffer.
  output_buffer_.reset(static_cast<uint8*>(av_malloc(kOutputBufferSize)));
  if (!output_buffer_.get()) {
    host()->SetError(PIPELINE_ERROR_OUT_OF_MEMORY);
    return;
  }
  *success = true;
}

void FFmpegAudioDecoder::DoSeek(base::TimeDelta time, Task* done_cb) {
  avcodec_flush_buffers(codec_context_);
  estimated_next_timestamp_ = kNoTimestamp;
  done_cb->Run();
  delete done_cb;
}

// ConvertAudioF32ToS32() converts float audio (F32) to int (S32) in place.
// This is a temporary solution.
// The purpose of this short term fix is to enable WMApro, which decodes to
// float.
// The audio driver has been tested by passing the float audio thru.
// FFmpeg for ChromeOS only exposes U8, S16 and F32.
// To properly expose new audio sample types at the audio driver layer, a enum
// should be created to represent all suppported types, including types
// for Pepper.  FFmpeg should be queried for type and passed along.

// TODO(fbarchard): Remove this function.  Expose all FFmpeg types to driver.
// TODO(fbarchard): If this function is kept, move it to audio_util.cc

#if USE_SSE
const __m128 kFloatScaler = _mm_set1_ps( 2147483648.0f );
static void FloatToIntSaturate(float* p) {
  __m128 a = _mm_set1_ps(*p);
  a = _mm_mul_ss(a, kFloatScaler);
  *reinterpret_cast<int32*>(p) = _mm_cvtss_si32(a);
}
#else
const float kFloatScaler = 2147483648.0f;
const int kMinSample = std::numeric_limits<int32>::min();
const int kMaxSample = std::numeric_limits<int32>::max();
const float kMinSampleFloat =
    static_cast<float>(std::numeric_limits<int32>::min());
const float kMaxSampleFloat =
    static_cast<float>(std::numeric_limits<int32>::max());
static void FloatToIntSaturate(float* p) {
  float f = *p * kFloatScaler + 0.5f;
  int sample;
  if (f <= kMinSampleFloat) {
    sample = kMinSample;
  } else if (f >= kMaxSampleFloat) {
    sample = kMaxSample;
  } else {
    sample = static_cast<int32>(f);
  }
  *reinterpret_cast<int32*>(p) = sample;
}
#endif
static void ConvertAudioF32ToS32(void* buffer, int buffer_size) {
  for (int i = 0; i < buffer_size / 4; ++i) {
    FloatToIntSaturate(reinterpret_cast<float*>(buffer) + i);
  }
}

void FFmpegAudioDecoder::DoDecode(Buffer* input) {
  PipelineStatistics statistics;

  // FFmpeg tends to seek Ogg audio streams in the middle of nowhere, giving us
  // a whole bunch of AV_NOPTS_VALUE packets.  Discard them until we find
  // something valid.  Refer to http://crbug.com/49709
  // TODO(hclam): remove this once fixing the issue in FFmpeg.
  if (input->GetTimestamp() == kNoTimestamp &&
      estimated_next_timestamp_ == kNoTimestamp &&
      !input->IsEndOfStream()) {
    DecoderBase<AudioDecoder, Buffer>::OnDecodeComplete(statistics);
    return;
  }

  // Due to FFmpeg API changes we no longer have const read-only pointers.
  AVPacket packet;
  av_init_packet(&packet);
  packet.data = const_cast<uint8*>(input->GetData());
  packet.size = input->GetDataSize();

  statistics.audio_bytes_decoded = input->GetDataSize();

  int16_t* output_buffer = reinterpret_cast<int16_t*>(output_buffer_.get());
  int output_buffer_size = kOutputBufferSize;
  int result = avcodec_decode_audio3(codec_context_,
                                     output_buffer,
                                     &output_buffer_size,
                                     &packet);

  if (codec_context_->sample_fmt == SAMPLE_FMT_FLT) {
    ConvertAudioF32ToS32(output_buffer, output_buffer_size);
  }

  // TODO(ajwong): Consider if kOutputBufferSize should just be an int instead
  // of a size_t.
  if (result < 0 ||
      output_buffer_size < 0 ||
      static_cast<size_t>(output_buffer_size) > kOutputBufferSize) {
    VLOG(1) << "Error decoding an audio frame with timestamp: "
            << input->GetTimestamp().InMicroseconds() << " us, duration: "
            << input->GetDuration().InMicroseconds() << " us, packet size: "
            << input->GetDataSize() << " bytes";
    DecoderBase<AudioDecoder, Buffer>::OnDecodeComplete(statistics);
    return;
  }

  // If we have decoded something, enqueue the result.
  if (output_buffer_size) {
    DataBuffer* result_buffer = new DataBuffer(output_buffer_size);
    result_buffer->SetDataSize(output_buffer_size);
    uint8* data = result_buffer->GetWritableData();
    memcpy(data, output_buffer, output_buffer_size);

    // We don't trust the demuxer, so always calculate the duration based on
    // the actual number of samples decoded.
    base::TimeDelta duration = CalculateDuration(output_buffer_size);
    result_buffer->SetDuration(duration);

    // Use an estimated timestamp unless the incoming buffer has a valid one.
    if (input->GetTimestamp() == kNoTimestamp) {
      result_buffer->SetTimestamp(estimated_next_timestamp_);

      // Keep the estimated timestamp invalid until we get an incoming buffer
      // with a valid timestamp.  This can happen during seeks, etc...
      if (estimated_next_timestamp_ != kNoTimestamp) {
        estimated_next_timestamp_ += duration;
      }
    } else {
      result_buffer->SetTimestamp(input->GetTimestamp());
      estimated_next_timestamp_ = input->GetTimestamp() + duration;
    }

    EnqueueResult(result_buffer);
    DecoderBase<AudioDecoder, Buffer>::OnDecodeComplete(statistics);
    return;
  }

  // We can get a positive result but no decoded data.  This is ok because this
  // this can be a marker packet that only contains timestamp.  In this case we
  // save the timestamp for later use.
  if (result && !input->IsEndOfStream() &&
      input->GetTimestamp() != kNoTimestamp &&
      input->GetDuration() != kNoTimestamp) {
    estimated_next_timestamp_ = input->GetTimestamp() + input->GetDuration();
    DecoderBase<AudioDecoder, Buffer>::OnDecodeComplete(statistics);
    return;
  }

  // Three conditions to meet to declare end of stream for this decoder:
  // 1. FFmpeg didn't read anything.
  // 2. FFmpeg didn't output anything.
  // 3. An end of stream buffer is received.
  if (result == 0 && output_buffer_size == 0 && input->IsEndOfStream()) {
    DataBuffer* result_buffer = new DataBuffer(0);
    result_buffer->SetTimestamp(input->GetTimestamp());
    result_buffer->SetDuration(input->GetDuration());
    EnqueueResult(result_buffer);
  }
  DecoderBase<AudioDecoder, Buffer>::OnDecodeComplete(statistics);
}

base::TimeDelta FFmpegAudioDecoder::CalculateDuration(size_t size) {
  int64 denominator = codec_context_->channels *
      av_get_bits_per_sample_fmt(codec_context_->sample_fmt) / 8 *
      codec_context_->sample_rate;
  double microseconds = size /
      (denominator / static_cast<double>(base::Time::kMicrosecondsPerSecond));
  return base::TimeDelta::FromMicroseconds(static_cast<int64>(microseconds));
}

}  // namespace
