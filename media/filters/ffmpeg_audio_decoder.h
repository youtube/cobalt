// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_
#define MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_

#include <list>

#include "base/callback.h"
#include "media/base/audio_decoder.h"
#include "media/base/demuxer_stream.h"

struct AVCodecContext;
struct AVFrame;

namespace base {
class MessageLoopProxy;
}

namespace media {

class AudioTimestampHelper;
class DataBuffer;
class DecoderBuffer;
struct QueuedAudioBuffer;

class MEDIA_EXPORT FFmpegAudioDecoder : public AudioDecoder {
 public:
  explicit FFmpegAudioDecoder(
      const scoped_refptr<base::MessageLoopProxy>& message_loop);

  // AudioDecoder implementation.
  virtual void Initialize(const scoped_refptr<DemuxerStream>& stream,
                          const PipelineStatusCB& status_cb,
                          const StatisticsCB& statistics_cb) override;
  virtual void Read(const ReadCB& read_cb) override;
  virtual int bits_per_channel() override;
  virtual ChannelLayout channel_layout() override;
  virtual int samples_per_second() override;
  virtual void Reset(const base::Closure& closure) override;

 protected:
  virtual ~FFmpegAudioDecoder();

 private:
  // Methods running on decoder thread.
  void DoInitialize(const scoped_refptr<DemuxerStream>& stream,
                    const PipelineStatusCB& status_cb,
                    const StatisticsCB& statistics_cb);
  void DoReset(const base::Closure& closure);
  void DoRead(const ReadCB& read_cb);
  void DoDecodeBuffer(DemuxerStream::Status status,
                      const scoped_refptr<DecoderBuffer>& input);

  // Reads from the demuxer stream with corresponding callback method.
  void ReadFromDemuxerStream();

  bool ConfigureDecoder();
  void ReleaseFFmpegResources();
  void ResetTimestampState();
  void RunDecodeLoop(const scoped_refptr<DecoderBuffer>& input,
                     bool skip_eos_append);

  scoped_refptr<base::MessageLoopProxy> message_loop_;

  scoped_refptr<DemuxerStream> demuxer_stream_;
  StatisticsCB statistics_cb_;
  AVCodecContext* codec_context_;

  // Decoded audio format.
  int bits_per_channel_;
  ChannelLayout channel_layout_;
  int samples_per_second_;

  // Used for computing output timestamps.
  scoped_ptr<AudioTimestampHelper> output_timestamp_helper_;
  int bytes_per_frame_;
  base::TimeDelta last_input_timestamp_;

  // Number of output sample bytes to drop before generating
  // output buffers.
  int output_bytes_to_drop_;

  // Holds decoded audio.
  AVFrame* av_frame_;

  ReadCB read_cb_;

  // Since multiple frames may be decoded from the same packet we need to queue
  // them up and hand them out as we receive Read() calls.
  std::list<QueuedAudioBuffer> queued_audio_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FFmpegAudioDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_
