// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_
#define MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_

#include "media/filters/decoder_base.h"

struct AVCodecContext;

namespace media {

// Forward declaration for scoped_ptr_malloc.
class ScopedPtrAVFree;

class MEDIA_EXPORT FFmpegAudioDecoder
    : public DecoderBase<AudioDecoder, Buffer> {
 public:
  explicit FFmpegAudioDecoder(MessageLoop* message_loop);
  virtual ~FFmpegAudioDecoder();

  // AudioDecoder implementation.
  virtual void ProduceAudioSamples(scoped_refptr<Buffer> output);
  virtual int bits_per_channel();
  virtual ChannelLayout channel_layout();
  virtual int sample_rate();

 protected:
  virtual void DoInitialize(DemuxerStream* demuxer_stream, bool* success,
                            Task* done_cb);

  virtual void DoSeek(base::TimeDelta time, Task* done_cb);

  virtual void DoDecode(Buffer* input);

 private:
  // Calculates the duration of an audio buffer based on the sample rate,
  // channels and bits per sample given the size in bytes.
  base::TimeDelta CalculateDuration(size_t size);

  // A FFmpeg defined structure that holds decoder information, this variable
  // is initialized in OnInitialize().
  AVCodecContext* codec_context_;

  // Decoded audio format.
  int bits_per_channel_;
  ChannelLayout channel_layout_;
  int sample_rate_;

  // Estimated timestamp for next packet. Useful for packets without timestamps.
  base::TimeDelta estimated_next_timestamp_;

  // Data buffer to carry decoded raw PCM samples. This buffer is created by
  // av_malloc() and is used throughout the lifetime of this class.
  scoped_ptr_malloc<uint8, ScopedPtrAVFree> output_buffer_;

  static const size_t kOutputBufferSize;

  DISALLOW_COPY_AND_ASSIGN(FFmpegAudioDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_
