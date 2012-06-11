// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_
#define MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_

#include <list>

#include "base/callback.h"
#include "base/message_loop.h"
#include "media/base/audio_decoder.h"

struct AVCodecContext;
struct AVFrame;

namespace media {

class DataBuffer;
class DecoderBuffer;

class MEDIA_EXPORT FFmpegAudioDecoder : public AudioDecoder {
 public:
  FFmpegAudioDecoder(const base::Callback<MessageLoop*()>& message_loop_cb);

  // AudioDecoder implementation.
  virtual void Initialize(const scoped_refptr<DemuxerStream>& stream,
                          const PipelineStatusCB& status_cb,
                          const StatisticsCB& statistics_cb) OVERRIDE;
  virtual void Read(const ReadCB& read_cb) OVERRIDE;
  virtual int bits_per_channel() OVERRIDE;
  virtual ChannelLayout channel_layout() OVERRIDE;
  virtual int samples_per_second() OVERRIDE;
  virtual void Reset(const base::Closure& closure) OVERRIDE;

 protected:
  virtual ~FFmpegAudioDecoder();

 private:
  // Methods running on decoder thread.
  void DoInitialize(const scoped_refptr<DemuxerStream>& stream,
                    const PipelineStatusCB& status_cb,
                    const StatisticsCB& statistics_cb);
  void DoReset(const base::Closure& closure);
  void DoRead(const ReadCB& read_cb);
  void DoDecodeBuffer(const scoped_refptr<DecoderBuffer>& input);

  // Reads from the demuxer stream with corresponding callback method.
  void ReadFromDemuxerStream();
  void DecodeBuffer(const scoped_refptr<DecoderBuffer>& buffer);

  // Updates the output buffer's duration and timestamp based on the input
  // buffer. Will fall back to an estimated timestamp if the input lacks a
  // valid timestamp.
  void UpdateDurationAndTimestamp(const Buffer* input, DataBuffer* output);

  // Calculates duration based on size of decoded audio bytes.
  base::TimeDelta CalculateDuration(int size);

  // Delivers decoded samples to |read_cb_| and resets the callback.
  void DeliverSamples(const scoped_refptr<Buffer>& samples);

  // This is !is_null() iff Initialize() hasn't been called.
  base::Callback<MessageLoop*()> message_loop_factory_cb_;
  MessageLoop* message_loop_;

  scoped_refptr<DemuxerStream> demuxer_stream_;
  StatisticsCB statistics_cb_;
  AVCodecContext* codec_context_;

  // Decoded audio format.
  int bits_per_channel_;
  ChannelLayout channel_layout_;
  int samples_per_second_;

  base::TimeDelta estimated_next_timestamp_;

  // Holds decoded audio.
  AVFrame* av_frame_;

  ReadCB read_cb_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FFmpegAudioDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_
