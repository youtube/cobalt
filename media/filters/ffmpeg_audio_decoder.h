// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_
#define MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_

#include <list>

#include "base/message_loop.h"
#include "media/base/filters.h"

struct AVCodecContext;

namespace media {

class DataBuffer;

class MEDIA_EXPORT FFmpegAudioDecoder : public AudioDecoder {
 public:
  explicit FFmpegAudioDecoder(MessageLoop* message_loop);
  virtual ~FFmpegAudioDecoder();

  // Filter implementation.
  virtual void Flush(FilterCallback* callback) OVERRIDE;

  // AudioDecoder implementation.
  virtual void Initialize(DemuxerStream* stream, FilterCallback* callback,
                          StatisticsCallback* stats_callback) OVERRIDE;
  virtual void ProduceAudioSamples(scoped_refptr<Buffer> output) OVERRIDE;
  virtual int bits_per_channel() OVERRIDE;
  virtual ChannelLayout channel_layout() OVERRIDE;
  virtual int sample_rate() OVERRIDE;

 private:
  // Methods running on decoder thread.
  void DoInitialize(const scoped_refptr<DemuxerStream>& stream,
                    FilterCallback* callback,
                    StatisticsCallback* stats_callback);
  void DoFlush(FilterCallback* callback);
  void DoProduceAudioSamples(const scoped_refptr<Buffer>& output);
  void DoDecodeBuffer(const scoped_refptr<Buffer>& input);

  // Reads from the demuxer stream with corresponding callback method.
  void ReadFromDemuxerStream();
  void DecodeBuffer(Buffer* buffer);

  // Updates the output buffer's duration and timestamp based on the input
  // buffer. Will fall back to an estimated timestamp if the input lacks a
  // valid timestamp.
  void UpdateDurationAndTimestamp(const Buffer* input, DataBuffer* output);

  // Calculates duration based on size of decoded audio bytes.
  base::TimeDelta CalculateDuration(int size);

  MessageLoop* message_loop_;

  scoped_refptr<DemuxerStream> demuxer_stream_;
  scoped_ptr<StatisticsCallback> stats_callback_;
  AVCodecContext* codec_context_;

  // Decoded audio format.
  int bits_per_channel_;
  ChannelLayout channel_layout_;
  int sample_rate_;

  base::TimeDelta estimated_next_timestamp_;

  // Holds decoded audio. As required by FFmpeg, input/output buffers should
  // be allocated with suitable padding and alignment. av_malloc() provides
  // us that guarantee.
  const int decoded_audio_size_;
  uint8* decoded_audio_;  // Allocated via av_malloc().

  // Holds downstream-provided buffers.
  std::list<scoped_refptr<Buffer> > output_buffers_;

  // Tracks reads issued for compressed data.
  int pending_reads_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FFmpegAudioDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_
