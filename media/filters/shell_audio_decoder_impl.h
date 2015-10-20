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
#ifndef MEDIA_FILTERS_SHELL_AUDIO_DECODER_IMPL_H_
#define MEDIA_FILTERS_SHELL_AUDIO_DECODER_IMPL_H_

#include <list>

#include "base/callback.h"
#include "base/message_loop.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/buffers.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/filters/shell_audio_decoder.h"

// set me to 1 to save decoded audio output to disk for debugging
#define __SAVE_DECODER_OUTPUT__ 0

#if __SAVE_DECODER_OUTPUT__
#include "media/audio/shell_wav_test_probe.h"
#endif

namespace media {

class ShellRawAudioDecoder {
 public:
  typedef media::DecoderBuffer DecoderBuffer;
  typedef media::AudioDecoderConfig AudioDecoderConfig;
  typedef base::Callback<scoped_ptr<ShellRawAudioDecoder>(
                             const AudioDecoderConfig& config)> Creator;

  virtual ~ShellRawAudioDecoder() {}
  virtual scoped_refptr<DecoderBuffer> Decode(
      const scoped_refptr<DecoderBuffer>& buffer) = 0;
  virtual bool Flush() = 0;
  virtual bool UpdateConfig(const AudioDecoderConfig& config) = 0;

  // TODO(***REMOVED***, b/16796265): Move factory method into ShellMediaPlatform
  // and inject into ShellAudioDecoderImpl.
  static scoped_ptr<ShellRawAudioDecoder> Create(
      const AudioDecoderConfig& config);

 protected:
  ShellRawAudioDecoder() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellRawAudioDecoder);
};

class MEDIA_EXPORT ShellAudioDecoderImpl : public ShellAudioDecoder {
 public:
  ShellAudioDecoderImpl(
      const scoped_refptr<base::MessageLoopProxy>& message_loop,
      const ShellRawAudioDecoder::Creator& raw_audio_decoder_creator);
  ~ShellAudioDecoderImpl() OVERRIDE;

  // AudioDecoder implementation.
  void Initialize(const scoped_refptr<DemuxerStream>& stream,
                  const PipelineStatusCB& status_cb,
                  const StatisticsCB& statistics_cb) OVERRIDE;
  void Read(const ReadCB& read_cb) OVERRIDE;
  int bits_per_channel() OVERRIDE;
  ChannelLayout channel_layout() OVERRIDE;
  int samples_per_second() OVERRIDE;
  void Reset(const base::Closure& closure) OVERRIDE;

 private:
  // There's a DemuxerStream::Status as well as an AudioDecoder::Status enum.
  // We make our own to keep track of ShellAudioDecoderImpl specific state, and
  // give it a lengthy name to disambiguate it from the others.
  enum ShellAudioDecoderStatus {
    kUninitialized,
    kNormal,
    kFlushing,
    kStopped,
    kShellDecodeError
  };

  // initialization
  void DoInitialize(const scoped_refptr<DemuxerStream>& stream,
                    const PipelineStatusCB& status_cb,
                    const StatisticsCB& statistics_cb);
  bool ValidateConfig(const AudioDecoderConfig& config);
  void DoDecodeBuffer();
  void DoRead();

  void QueueBuffer(DemuxerStream::Status status,
                   const scoped_refptr<DecoderBuffer>& buffer);
  void RequestBuffer();

  // general state
  scoped_refptr<base::MessageLoopProxy> message_loop_;
  ShellRawAudioDecoder::Creator raw_audio_decoder_creator_;
  scoped_refptr<DemuxerStream> demuxer_stream_;
  StatisticsCB statistics_cb_;
  ShellAudioDecoderStatus shell_audio_decoder_status_;

#if __SAVE_DECODER_OUTPUT__
  ShellWavTestProbe test_probe_;
#endif

  // cached stream info
  int samples_per_second_;
  int num_channels_;

  scoped_ptr<ShellRawAudioDecoder> raw_decoder_;

  // Callback on completion of a decode
  bool pending_renderer_read_;
  ReadCB read_cb_;

  static const int kMaxQueuedBuffers = 8;
  typedef std::pair<DemuxerStream::Status, const scoped_refptr<DecoderBuffer> >
      QueuedBuffer;
  std::list<QueuedBuffer> queued_buffers_;
  bool pending_demuxer_read_;

  // Callback on flushing all pending reads
  base::Closure reset_cb_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ShellAudioDecoderImpl);
};

}  // namespace media

#endif  // MEDIA_FILTERS_SHELL_AUDIO_DECODER_IMPL_H_
