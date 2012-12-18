// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/opus_audio_decoder.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "base/sys_byteorder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/data_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer.h"
#include "media/base/pipeline.h"
#include "third_party/opus/src/include/opus.h"
#include "third_party/opus/src/include/opus_multistream.h"

namespace media {

static uint16 ReadLE16(const uint8* data, size_t data_size, int read_offset) {
  DCHECK(data);
  uint16 value = 0;
  DCHECK_LE(read_offset + sizeof(value), data_size);
  memcpy(&value, data + read_offset, sizeof(value));
  return base::ByteSwapToLE16(value);
}

// Returns true if the decode result was end of stream.
static inline bool IsEndOfStream(int decoded_size, Buffer* input) {
  // Two conditions to meet to declare end of stream for this decoder:
  // 1. Opus didn't output anything.
  // 2. An end of stream buffer is received.
  return decoded_size == 0 && input->IsEndOfStream();
}

// The Opus specification is part of IETF RFC 6716:
// http://tools.ietf.org/html/rfc6716

// Opus uses Vorbis channel mapping, and Vorbis channel mapping specifies
// mappings for up to 8 channels. See section 4.3.9 of the vorbis
// specification:
// http://www.xiph.org/vorbis/doc/Vorbis_I_spec.html
static const int kMaxVorbisChannels = 8;

// Opus allows for decode of S16 or float samples. OpusAudioDecoder always uses
// S16 samples.
static const int kBitsPerChannel = 16;
static const int kBytesPerChannel = kBitsPerChannel / 8;

// Maximum packet size used in Xiph's opusdec and FFmpeg's libopusdec.
static const int kMaxOpusOutputPacketSizeSamples = 960 * 6;
static const int kMaxOpusOutputPacketSizeBytes =
    kMaxOpusOutputPacketSizeSamples * kBytesPerChannel;

static void RemapOpusChannelLayout(const uint8* opus_mapping,
                                   int num_channels,
                                   uint8* channel_layout) {
  DCHECK_LE(num_channels, kMaxVorbisChannels);

  // Opus uses Vorbis channel layout.
  const int32 num_layouts = kMaxVorbisChannels;
  const int32 num_layout_values = kMaxVorbisChannels;
  const uint8 kVorbisChannelLayouts[num_layouts][num_layout_values] = {
    { 0 },
    { 0, 1 },
    { 0, 2, 1 },
    { 0, 1, 2, 3 },
    { 0, 2, 1, 3, 4 },
    { 0, 2, 1, 5, 3, 4 },
    { 0, 2, 1, 6, 5, 3, 4 },
    { 0, 2, 1, 7, 5, 6, 3, 4 },
  };

  // Reorder the channels to produce the same ordering as FFmpeg, which is
  // what the pipeline expects.
  const uint8* vorbis_layout_offset = kVorbisChannelLayouts[num_channels - 1];
  for (int channel = 0; channel < num_channels; ++channel)
    channel_layout[channel] = opus_mapping[vorbis_layout_offset[channel]];
}

// Opus Header contents:
// - "OpusHead" (64 bits)
// - version number (8 bits)
// - Channels C (8 bits)
// - Pre-skip (16 bits)
// - Sampling rate (32 bits)
// - Gain in dB (16 bits, S7.8)
// - Mapping (8 bits, 0=single stream (mono/stereo) 1=Vorbis mapping,
//            2..254: reserved, 255: multistream with no mapping)
//
// - if (mapping != 0)
//    - N = totel number of streams (8 bits)
//    - M = number of paired streams (8 bits)
//    - C times channel origin
//         - if (C<2*M)
//            - stream = byte/2
//            - if (byte&0x1 == 0)
//                - left
//              else
//                - right
//         - else
//            - stream = byte-M

// Default audio output channel layout. Used to initialize |stream_map| in
// OpusHeader, and passed to opus_multistream_decoder_create() when the header
// does not contain mapping information. The values are valid only for mono and
// stereo output: Opus streams with more than 2 channels require a stream map.
static const int kMaxChannelsWithDefaultLayout = 2;
static const uint8 kDefaultOpusChannelLayout[kMaxChannelsWithDefaultLayout] = {
    0, 1 };

// Size of the Opus header excluding optional mapping information.
static const int kOpusHeaderSize = 19;

// Offset to the channel count byte in the Opus header.
static const int kOpusHeaderChannelsOffset = 9;

// Offset to the pre-skip value in the Opus header.
static const int kOpusHeaderSkipSamplesOffset = 10;

// Offset to the channel mapping byte in the Opus header.
static const int kOpusHeaderChannelMappingOffset = 18;

// Header contains a stream map. The mapping values are in extra data beyond
// the always present |kOpusHeaderSize| bytes of data. The mapping data
// contains stream count, coupling information, and per channel mapping values:
//   - Byte 0: Number of streams.
//   - Byte 1: Number coupled.
//   - Byte 2: Starting at byte 2 are |header->channels| uint8 mapping values.
static const int kOpusHeaderNumStreamsOffset = kOpusHeaderSize;
static const int kOpusHeaderNumCoupledOffset = kOpusHeaderNumStreamsOffset + 1;
static const int kOpusHeaderStreamMapOffset = kOpusHeaderNumStreamsOffset + 2;

struct OpusHeader {
  OpusHeader()
      : channels(0),
        skip_samples(0),
        channel_mapping(0),
        num_streams(0),
        num_coupled(0) {
    memcpy(stream_map,
           kDefaultOpusChannelLayout,
           kMaxChannelsWithDefaultLayout);
  }
  int channels;
  int skip_samples;
  int channel_mapping;
  int num_streams;
  int num_coupled;
  uint8 stream_map[kMaxVorbisChannels];
};

// Returns true when able to successfully parse and store Opus header data in
// data parsed in |header|. Based on opus header parsing code in libopusdec
// from FFmpeg, and opus_header from Xiph's opus-tools project.
static void ParseOpusHeader(const uint8* data, int data_size,
                            const AudioDecoderConfig& config,
                            OpusHeader* header) {
  CHECK_GE(data_size, kOpusHeaderSize);

  header->channels = *(data + kOpusHeaderChannelsOffset);

  CHECK(header->channels > 0 && header->channels <= kMaxVorbisChannels)
      << "invalid channel count in header: " << header->channels;

  header->skip_samples =
      ReadLE16(data, data_size, kOpusHeaderSkipSamplesOffset);

  header->channel_mapping = *(data + kOpusHeaderChannelMappingOffset);

  if (!header->channel_mapping) {
    CHECK_LE(header->channels, kMaxChannelsWithDefaultLayout)
        << "Invalid header, missing stream map.";

    header->num_streams = 1;
    header->num_coupled =
        (ChannelLayoutToChannelCount(config.channel_layout()) > 1) ? 1 : 0;
    return;
  }

  CHECK_GE(data_size, kOpusHeaderStreamMapOffset + header->channels)
      << "Invalid stream map.";

  header->num_streams = *(data + kOpusHeaderNumStreamsOffset);
  header->num_coupled = *(data + kOpusHeaderNumCoupledOffset);

  if (header->num_streams + header->num_coupled != header->channels)
    LOG(WARNING) << "Inconsistent channel mapping.";

  for (int i = 0; i < kMaxVorbisChannels; ++i)
    header->stream_map[i] = *(data + kOpusHeaderStreamMapOffset + i);
}

OpusAudioDecoder::OpusAudioDecoder(
    const scoped_refptr<base::MessageLoopProxy>& message_loop)
    : message_loop_(message_loop),
      opus_decoder_(NULL),
      bits_per_channel_(0),
      channel_layout_(CHANNEL_LAYOUT_NONE),
      samples_per_second_(0),
      last_input_timestamp_(kNoTimestamp()),
      output_bytes_to_drop_(0),
      skip_samples_(0) {
}

void OpusAudioDecoder::Initialize(
    const scoped_refptr<DemuxerStream>& stream,
    const PipelineStatusCB& status_cb,
    const StatisticsCB& statistics_cb) {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &OpusAudioDecoder::DoInitialize, this,
        stream, status_cb, statistics_cb));
    return;
  }
  DoInitialize(stream, status_cb, statistics_cb);
}

void OpusAudioDecoder::Read(const ReadCB& read_cb) {
  // Complete operation asynchronously on different stack of execution as per
  // the API contract of AudioDecoder::Read()
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &OpusAudioDecoder::DoRead, this, read_cb));
}

int OpusAudioDecoder::bits_per_channel() {
  return bits_per_channel_;
}

ChannelLayout OpusAudioDecoder::channel_layout() {
  return channel_layout_;
}

int OpusAudioDecoder::samples_per_second() {
  return samples_per_second_;
}

void OpusAudioDecoder::Reset(const base::Closure& closure) {
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &OpusAudioDecoder::DoReset, this, closure));
}

OpusAudioDecoder::~OpusAudioDecoder() {
  // TODO(scherkus): should we require Stop() to be called? this might end up
  // getting called on a random thread due to refcounting.
  CloseDecoder();
}

void OpusAudioDecoder::DoInitialize(
    const scoped_refptr<DemuxerStream>& stream,
    const PipelineStatusCB& status_cb,
    const StatisticsCB& statistics_cb) {
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

void OpusAudioDecoder::DoReset(const base::Closure& closure) {
  opus_multistream_decoder_ctl(opus_decoder_, OPUS_RESET_STATE);
  ResetTimestampState();
  closure.Run();
}

void OpusAudioDecoder::DoRead(const ReadCB& read_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(!read_cb.is_null());
  CHECK(read_cb_.is_null()) << "Overlapping decodes are not supported.";

  read_cb_ = read_cb;
  ReadFromDemuxerStream();
}

void OpusAudioDecoder::DoDecodeBuffer(
    DemuxerStream::Status status,
    const scoped_refptr<DecoderBuffer>& input) {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &OpusAudioDecoder::DoDecodeBuffer, this, status, input));
    return;
  }

  DCHECK(!read_cb_.is_null());
  DCHECK_EQ(status != DemuxerStream::kOk, !input) << status;

  if (status == DemuxerStream::kAborted) {
    DCHECK(!input);
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
    return;
  }

  if (status == DemuxerStream::kConfigChanged) {
    DCHECK(!input);

    scoped_refptr<DataBuffer> output_buffer;

    // Send a "end of stream" buffer to the decode loop
    // to output any remaining data still in the decoder.
    if (!Decode(DecoderBuffer::CreateEOSBuffer(), true, &output_buffer)) {
      base::ResetAndReturn(&read_cb_).Run(kDecodeError, NULL);
      return;
    }

    DVLOG(1) << "Config changed.";

    if (!ConfigureDecoder()) {
      base::ResetAndReturn(&read_cb_).Run(kDecodeError, NULL);
      return;
    }

    ResetTimestampState();

    if (output_buffer) {
      // Execute callback to return the decoded audio.
      base::ResetAndReturn(&read_cb_).Run(kOk, output_buffer);
    } else {
      // We exhausted the input data, but it wasn't enough for a frame.  Ask for
      // more data in order to fulfill this read.
      ReadFromDemuxerStream();
    }

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

  if (!input->IsEndOfStream()) {
    if (last_input_timestamp_ != kNoTimestamp() &&
        input->GetTimestamp() != kNoTimestamp() &&
        input->GetTimestamp() < last_input_timestamp_) {
      base::TimeDelta diff = input->GetTimestamp() - last_input_timestamp_;
      DVLOG(1) << "Input timestamps are not monotonically increasing! "
               << " ts " << input->GetTimestamp().InMicroseconds() << " us"
               << " diff " << diff.InMicroseconds() << " us";
      base::ResetAndReturn(&read_cb_).Run(kDecodeError, NULL);
    }

    last_input_timestamp_ = input->GetTimestamp();
  }

  scoped_refptr<DataBuffer> output_buffer;

  if (!Decode(input, false, &output_buffer)) {
    base::ResetAndReturn(&read_cb_).Run(kDecodeError, NULL);
    return;
  }

  if (output_buffer) {
    // Execute callback to return the decoded audio.
    base::ResetAndReturn(&read_cb_).Run(kOk, output_buffer);
  } else {
    // We exhausted the input data, but it wasn't enough for a frame.  Ask for
    // more data in order to fulfill this read.
    ReadFromDemuxerStream();
  }
}

void OpusAudioDecoder::ReadFromDemuxerStream() {
  DCHECK(!read_cb_.is_null());

  demuxer_stream_->Read(base::Bind(&OpusAudioDecoder::DoDecodeBuffer, this));
}

bool OpusAudioDecoder::ConfigureDecoder() {
  const AudioDecoderConfig& config = demuxer_stream_->audio_decoder_config();

  if (config.codec() != kCodecOpus) {
    DLOG(ERROR) << "ConfigureDecoder(): codec must be kCodecOpus.";
    return false;
  }

  const int channel_count =
      ChannelLayoutToChannelCount(config.channel_layout());
  if (!config.IsValidConfig() || channel_count > kMaxVorbisChannels) {
    DLOG(ERROR) << "ConfigureDecoder(): Invalid or unsupported audio stream -"
                << " codec: " << config.codec()
                << " channel count: " << channel_count
                << " channel layout: " << config.channel_layout()
                << " bits per channel: " << config.bits_per_channel()
                << " samples per second: " << config.samples_per_second();
    return false;
  }

  if (config.bits_per_channel() != kBitsPerChannel) {
    DLOG(ERROR) << "ConfigureDecoder(): 16 bit samples required.";
    return false;
  }

  if (config.is_encrypted()) {
    DLOG(ERROR) << "ConfigureDecoder(): Encrypted audio stream not supported.";
    return false;
  }

  if (opus_decoder_ &&
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

  // Clean up existing decoder if necessary.
  CloseDecoder();

  // Allocate the output buffer if necessary.
  if (!output_buffer_)
    output_buffer_.reset(new int16[kMaxOpusOutputPacketSizeSamples]);

  // Parse the Opus header.
  OpusHeader opus_header;
  ParseOpusHeader(config.extra_data(), config.extra_data_size(),
                  config,
                  &opus_header);

  skip_samples_ = opus_header.skip_samples;

  if (skip_samples_ > 0)
    output_bytes_to_drop_ = skip_samples_ * config.bytes_per_frame();

  uint8 channel_mapping[kMaxVorbisChannels];
  memcpy(&channel_mapping,
         kDefaultOpusChannelLayout,
         kMaxChannelsWithDefaultLayout);

  if (channel_count > kMaxChannelsWithDefaultLayout) {
    RemapOpusChannelLayout(opus_header.stream_map,
                           channel_count,
                           channel_mapping);
  }

  // Init Opus.
  int status = OPUS_INVALID_STATE;
  opus_decoder_ = opus_multistream_decoder_create(config.samples_per_second(),
                                                  channel_count,
                                                  opus_header.num_streams,
                                                  opus_header.num_coupled,
                                                  channel_mapping,
                                                  &status);
  if (!opus_decoder_ || status != OPUS_OK) {
    LOG(ERROR) << "ConfigureDecoder(): opus_multistream_decoder_create failed"
               << " status=" << opus_strerror(status);
    return false;
  }

  // TODO(tomfinegan): Handle audio delay once the matroska spec is updated
  // to represent the value.

  bits_per_channel_ = config.bits_per_channel();
  channel_layout_ = config.channel_layout();
  samples_per_second_ = config.samples_per_second();
  output_timestamp_helper_.reset(new AudioTimestampHelper(
      config.bytes_per_frame(), config.samples_per_second()));
  return true;
}

void OpusAudioDecoder::CloseDecoder() {
  if (opus_decoder_) {
    opus_multistream_decoder_destroy(opus_decoder_);
    opus_decoder_ = NULL;
  }
}

void OpusAudioDecoder::ResetTimestampState() {
  output_timestamp_helper_->SetBaseTimestamp(kNoTimestamp());
  last_input_timestamp_ = kNoTimestamp();
  output_bytes_to_drop_ = 0;
}

bool OpusAudioDecoder::Decode(const scoped_refptr<DecoderBuffer>& input,
                              bool skip_eos_append,
                              scoped_refptr<DataBuffer>* output_buffer) {
  int samples_decoded =
      opus_multistream_decode(opus_decoder_,
                              input->GetData(), input->GetDataSize(),
                              &output_buffer_[0],
                              kMaxOpusOutputPacketSizeSamples,
                              0);
  if (samples_decoded < 0) {
    DCHECK(!input->IsEndOfStream())
        << "Decode(): End of stream buffer produced an error!";

    LOG(ERROR) << "ConfigureDecoder(): opus_multistream_decode failed for"
               << " timestamp: " << input->GetTimestamp().InMicroseconds()
               << " us, duration: " << input->GetDuration().InMicroseconds()
               << " us, packet size: " << input->GetDataSize() << " bytes with"
               << " status: " << opus_strerror(samples_decoded);
    return false;
  }

  uint8* decoded_audio_data = reinterpret_cast<uint8*>(&output_buffer_[0]);
  int decoded_audio_size = samples_decoded *
      demuxer_stream_->audio_decoder_config().bytes_per_frame();
  DCHECK_LE(decoded_audio_size, kMaxOpusOutputPacketSizeBytes);

  if (output_timestamp_helper_->base_timestamp() == kNoTimestamp() &&
      !input->IsEndOfStream()) {
    DCHECK(input->GetTimestamp() != kNoTimestamp());
    output_timestamp_helper_->SetBaseTimestamp(input->GetTimestamp());
  }

  if (decoded_audio_size > 0 && output_bytes_to_drop_ > 0) {
    int dropped_size = std::min(decoded_audio_size, output_bytes_to_drop_);
    DCHECK_EQ(dropped_size % kBytesPerChannel, 0);
    decoded_audio_data += dropped_size;
    decoded_audio_size -= dropped_size;
    output_bytes_to_drop_ -= dropped_size;
  }

  if (decoded_audio_size > 0) {
    // Copy the audio samples into an output buffer.
    *output_buffer = new DataBuffer(decoded_audio_data, decoded_audio_size);
    (*output_buffer)->SetTimestamp(output_timestamp_helper_->GetTimestamp());
    (*output_buffer)->SetDuration(
        output_timestamp_helper_->GetDuration(decoded_audio_size));
    output_timestamp_helper_->AddBytes(decoded_audio_size);
  } else if (IsEndOfStream(decoded_audio_size, input) && !skip_eos_append) {
    DCHECK_EQ(input->GetDataSize(), 0);
    // Create an end of stream output buffer.
    *output_buffer = new DataBuffer(0);
  }

  // Decoding finished successfully, update statistics.
  PipelineStatistics statistics;
  statistics.audio_bytes_decoded = decoded_audio_size;
  statistics_cb_.Run(statistics);

  return true;
}

}  // namespace media
