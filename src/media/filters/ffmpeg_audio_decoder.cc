// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_audio_decoder.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/data_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer.h"
#include "media/base/pipeline.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_glue.h"

namespace media {

// Helper structure for managing multiple decoded audio frames per packet.
struct QueuedAudioBuffer {
  AudioDecoder::Status status;
  scoped_refptr<Buffer> buffer;
};

// Returns true if the decode result was end of stream.
static inline bool IsEndOfStream(int result, int decoded_size, Buffer* input) {
  // Three conditions to meet to declare end of stream for this decoder:
  // 1. FFmpeg didn't read anything.
  // 2. FFmpeg didn't output anything.
  // 3. An end of stream buffer is received.
  return result == 0 && decoded_size == 0 && input->IsEndOfStream();
}

FFmpegAudioDecoder::FFmpegAudioDecoder(
    const scoped_refptr<base::MessageLoopProxy>& message_loop)
    : message_loop_(message_loop),
      codec_context_(NULL),
      bits_per_channel_(0),
      channel_layout_(CHANNEL_LAYOUT_NONE),
      samples_per_second_(0),
      bytes_per_frame_(0),
      last_input_timestamp_(kNoTimestamp()),
      output_bytes_to_drop_(0),
      av_frame_(NULL) {
}

void FFmpegAudioDecoder::Initialize(
    const scoped_refptr<DemuxerStream>& stream,
    const PipelineStatusCB& status_cb,
    const StatisticsCB& statistics_cb) {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &FFmpegAudioDecoder::DoInitialize, this,
        stream, status_cb, statistics_cb));
    return;
  }
  DoInitialize(stream, status_cb, statistics_cb);
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
  ReleaseFFmpegResources();
}

void FFmpegAudioDecoder::DoInitialize(
    const scoped_refptr<DemuxerStream>& stream,
    const PipelineStatusCB& status_cb,
    const StatisticsCB& statistics_cb) {
  FFmpegGlue::InitializeFFmpeg();

  if (demuxer_stream_) {
    // TODO(scherkus): initialization currently happens more than once in
    // PipelineIntegrationTest.BasicPlayback.
    LOG(ERROR) << "Initialize has already been called.";
    CHECK(false);
  }

  demuxer_stream_ = stream;

  if (!ConfigureDecoder()) {
    status_cb.Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  statistics_cb_ = statistics_cb;
  status_cb.Run(PIPELINE_OK);
}

void FFmpegAudioDecoder::DoReset(const base::Closure& closure) {
  avcodec_flush_buffers(codec_context_);
  ResetTimestampState();
  queued_audio_.clear();
  closure.Run();
}

void FFmpegAudioDecoder::DoRead(const ReadCB& read_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(!read_cb.is_null());
  CHECK(read_cb_.is_null()) << "Overlapping decodes are not supported.";

  read_cb_ = read_cb;

  // If we don't have any queued audio from the last packet we decoded, ask for
  // more data from the demuxer to satisfy this read.
  if (queued_audio_.empty()) {
    ReadFromDemuxerStream();
    return;
  }

  base::ResetAndReturn(&read_cb_).Run(
      queued_audio_.front().status, queued_audio_.front().buffer);
  queued_audio_.pop_front();
}

void FFmpegAudioDecoder::DoDecodeBuffer(
    DemuxerStream::Status status,
    const scoped_refptr<DecoderBuffer>& input) {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &FFmpegAudioDecoder::DoDecodeBuffer, this, status, input));
    return;
  }

  DCHECK(!read_cb_.is_null());
  DCHECK(queued_audio_.empty());
  DCHECK_EQ(status != DemuxerStream::kOk, !input) << status;

  if (status == DemuxerStream::kAborted) {
    DCHECK(!input);
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
    return;
  }

  if (status == DemuxerStream::kConfigChanged) {
    DCHECK(!input);

    // Send a "end of stream" buffer to the decode loop
    // to output any remaining data still in the decoder.
    RunDecodeLoop(DecoderBuffer::CreateEOSBuffer(), true);

    DVLOG(1) << "Config changed.";

    if (!ConfigureDecoder()) {
      base::ResetAndReturn(&read_cb_).Run(kDecodeError, NULL);
      return;
    }

    ResetTimestampState();

    if (queued_audio_.empty()) {
      ReadFromDemuxerStream();
      return;
    }

    base::ResetAndReturn(&read_cb_).Run(
        queued_audio_.front().status, queued_audio_.front().buffer);
    queued_audio_.pop_front();
    return;
  }

  DCHECK_EQ(status, DemuxerStream::kOk);
  DCHECK(input);

  // Make sure we are notified if http://crbug.com/49709 returns.  Issue also
  // occurs with some damaged files.
  if (!input->IsEndOfStream() && input->GetTimestamp() == kNoTimestamp() &&
      output_timestamp_helper_->base_timestamp() == kNoTimestamp()) {
    DVLOG(1) << "Received a buffer without timestamps!";
    base::ResetAndReturn(&read_cb_).Run(kDecodeError, NULL);
    return;
  }

  bool is_vorbis = codec_context_->codec_id == CODEC_ID_VORBIS;
  if (!input->IsEndOfStream()) {
    if (last_input_timestamp_ == kNoTimestamp()) {
      if (is_vorbis && (input->GetTimestamp() < base::TimeDelta())) {
        // Dropping frames for negative timestamps as outlined in section A.2
        // in the Vorbis spec. http://xiph.org/vorbis/doc/Vorbis_I_spec.html
        int frames_to_drop = floor(
            0.5 + -input->GetTimestamp().InSecondsF() * samples_per_second_);
        output_bytes_to_drop_ = bytes_per_frame_ * frames_to_drop;
      } else {
        last_input_timestamp_ = input->GetTimestamp();
      }
    } else if (input->GetTimestamp() != kNoTimestamp()) {
      if (input->GetTimestamp() < last_input_timestamp_) {
        base::TimeDelta diff = input->GetTimestamp() - last_input_timestamp_;
        DVLOG(1) << "Input timestamps are not monotonically increasing! "
                 << " ts " << input->GetTimestamp().InMicroseconds() << " us"
                 << " diff " << diff.InMicroseconds() << " us";
        base::ResetAndReturn(&read_cb_).Run(kDecodeError, NULL);
        return;
      }

      last_input_timestamp_ = input->GetTimestamp();
    }
  }

  RunDecodeLoop(input, false);

  // We exhausted the provided packet, but it wasn't enough for a frame.  Ask
  // for more data in order to fulfill this read.
  if (queued_audio_.empty()) {
    ReadFromDemuxerStream();
    return;
  }

  // Execute callback to return the first frame we decoded.
  base::ResetAndReturn(&read_cb_).Run(
      queued_audio_.front().status, queued_audio_.front().buffer);
  queued_audio_.pop_front();
}

void FFmpegAudioDecoder::ReadFromDemuxerStream() {
  DCHECK(!read_cb_.is_null());

  demuxer_stream_->Read(base::Bind(&FFmpegAudioDecoder::DoDecodeBuffer, this));
}

bool FFmpegAudioDecoder::ConfigureDecoder() {
  const AudioDecoderConfig& config = demuxer_stream_->audio_decoder_config();

  if (!config.IsValidConfig()) {
    DLOG(ERROR) << "Invalid audio stream -"
                << " codec: " << config.codec()
                << " channel layout: " << config.channel_layout()
                << " bits per channel: " << config.bits_per_channel()
                << " samples per second: " << config.samples_per_second();
    return false;
  }

  if (config.is_encrypted()) {
    DLOG(ERROR) << "Encrypted audio stream not supported";
    return false;
  }

  if (codec_context_ &&
      (bits_per_channel_ != config.bits_per_channel() ||
       channel_layout_ != config.channel_layout() ||
       samples_per_second_ != config.samples_per_second())) {
    DVLOG(1) << "Unsupported config change :";
    DVLOG(1) << "\tbits_per_channel : " << bits_per_channel_
             << " -> " << config.bits_per_channel();
    DVLOG(1) << "\tchannel_layout : " << channel_layout_
             << " -> " << config.channel_layout();
    DVLOG(1) << "\tsample_rate : " << samples_per_second_
             << " -> " << config.samples_per_second();
    return false;
  }

  // Release existing decoder resources if necessary.
  ReleaseFFmpegResources();

  // Initialize AVCodecContext structure.
  codec_context_ = avcodec_alloc_context3(NULL);
  AudioDecoderConfigToAVCodecContext(config, codec_context_);

  AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);
  if (!codec || avcodec_open2(codec_context_, codec, NULL) < 0) {
    DLOG(ERROR) << "Could not initialize audio decoder: "
                << codec_context_->codec_id;
    return false;
  }

  // Success!
  av_frame_ = avcodec_alloc_frame();
  bits_per_channel_ = config.bits_per_channel();
  channel_layout_ = config.channel_layout();
  samples_per_second_ = config.samples_per_second();
  output_timestamp_helper_.reset(new AudioTimestampHelper(
      config.bytes_per_frame(), config.samples_per_second()));
  return true;
}

void FFmpegAudioDecoder::ReleaseFFmpegResources() {
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

void FFmpegAudioDecoder::ResetTimestampState() {
  output_timestamp_helper_->SetBaseTimestamp(kNoTimestamp());
  last_input_timestamp_ = kNoTimestamp();
  output_bytes_to_drop_ = 0;
}

void FFmpegAudioDecoder::RunDecodeLoop(
    const scoped_refptr<DecoderBuffer>& input,
    bool skip_eos_append) {
  AVPacket packet;
  av_init_packet(&packet);
  packet.data = const_cast<uint8*>(input->GetData());
  packet.size = input->GetDataSize();

  // Each audio packet may contain several frames, so we must call the decoder
  // until we've exhausted the packet.  Regardless of the packet size we always
  // want to hand it to the decoder at least once, otherwise we would end up
  // skipping end of stream packets since they have a size of zero.
  do {
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

      DLOG(ERROR)
          << "Error decoding an audio frame with timestamp: "
          << input->GetTimestamp().InMicroseconds() << " us, duration: "
          << input->GetDuration().InMicroseconds() << " us, packet size: "
          << input->GetDataSize() << " bytes";

      // TODO(dalecurtis): We should return a kDecodeError here instead:
      // http://crbug.com/145276
      break;
    }

    // Update packet size and data pointer in case we need to call the decoder
    // with the remaining bytes from this packet.
    packet.size -= result;
    packet.data += result;

    if (output_timestamp_helper_->base_timestamp() == kNoTimestamp() &&
        !input->IsEndOfStream()) {
      DCHECK(input->GetTimestamp() != kNoTimestamp());
      if (output_bytes_to_drop_ > 0) {
        // Currently Vorbis is the only codec that causes us to drop samples.
        // If we have to drop samples it always means the timeline starts at 0.
        DCHECK_EQ(codec_context_->codec_id, CODEC_ID_VORBIS);
        output_timestamp_helper_->SetBaseTimestamp(base::TimeDelta());
      } else {
        output_timestamp_helper_->SetBaseTimestamp(input->GetTimestamp());
      }
    }

    const uint8* decoded_audio_data = NULL;
    int decoded_audio_size = 0;
    if (frame_decoded) {
      int output_sample_rate = av_frame_->sample_rate;
      if (output_sample_rate != samples_per_second_) {
        DLOG(ERROR) << "Output sample rate (" << output_sample_rate
                    << ") doesn't match expected rate " << samples_per_second_;

        // This is an unrecoverable error, so bail out.
        QueuedAudioBuffer queue_entry = { kDecodeError, NULL };
        queued_audio_.push_back(queue_entry);
        break;
      }

      decoded_audio_data = av_frame_->data[0];
      decoded_audio_size = av_samples_get_buffer_size(
          NULL, codec_context_->channels, av_frame_->nb_samples,
          codec_context_->sample_fmt, 1);
    }

    scoped_refptr<DataBuffer> output;

    if (decoded_audio_size > 0 && output_bytes_to_drop_ > 0) {
      int dropped_size = std::min(decoded_audio_size, output_bytes_to_drop_);
      decoded_audio_data += dropped_size;
      decoded_audio_size -= dropped_size;
      output_bytes_to_drop_ -= dropped_size;
    }

    if (decoded_audio_size > 0) {
      // Copy the audio samples into an output buffer.
      output = new DataBuffer(decoded_audio_data, decoded_audio_size);
      output->SetTimestamp(output_timestamp_helper_->GetTimestamp());
      output->SetDuration(
          output_timestamp_helper_->GetDuration(decoded_audio_size));
      output_timestamp_helper_->AddBytes(decoded_audio_size);
    } else if (IsEndOfStream(result, decoded_audio_size, input) &&
               !skip_eos_append) {
      DCHECK_EQ(packet.size, 0);
      // Create an end of stream output buffer.
      output = new DataBuffer(0);
    }

    if (output) {
      QueuedAudioBuffer queue_entry = { kOk, output };
      queued_audio_.push_back(queue_entry);
    }

    // Decoding finished successfully, update statistics.
    if (result > 0) {
      PipelineStatistics statistics;
      statistics.audio_bytes_decoded = result;
      statistics_cb_.Run(statistics);
    }
  } while (packet.size > 0);
}

}  // namespace media
