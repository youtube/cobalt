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

#include <queue>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"

// set me to 1 to save decoded audio output to disk for debugging
#define __SAVE_DECODER_OUTPUT__ 0

#if __SAVE_DECODER_OUTPUT__
#include "media/audio/shell_wav_test_probe.h"
#endif

namespace media {

class ShellRawAudioDecoder {
 public:
  enum DecodeStatus {
    BUFFER_DECODED,  // Successfully decoded audio data.
    NEED_MORE_DATA   // Need more data to produce decoded audio data.
  };
  typedef media::DecoderBuffer DecoderBuffer;
  typedef media::AudioDecoderConfig AudioDecoderConfig;
  typedef base::Callback<void(DecodeStatus,
                              const scoped_refptr<DecoderBuffer>&)> DecodeCB;

  virtual ~ShellRawAudioDecoder() {}
  virtual int GetBytesPerSample() const = 0;
  virtual void Decode(const scoped_refptr<DecoderBuffer>& buffer,
                      const DecodeCB& decode_cb) = 0;
  virtual bool Flush() = 0;
  virtual bool UpdateConfig(const AudioDecoderConfig& config) = 0;

 protected:
  ShellRawAudioDecoder() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellRawAudioDecoder);
};

class ShellRawAudioDecoderFactory {
 public:
  virtual ~ShellRawAudioDecoderFactory() {}

  virtual scoped_ptr<ShellRawAudioDecoder> Create(
      const AudioDecoderConfig& config) = 0;
};

// The audio decoder mainly has two features:
// 1. It tries to read buffers from the demuxer, then decode the buffers and put
//    decoded buffers into a queue.  In this way the decoder can cache some
//    buffers to speed up audio caching.
// 2. When its Read() function is called, it tries to fulfill the Read() with
//    decoded data.
// A Reset() call will break both of them and halt the decoder until another
// Read() is called.
// The ShellAudioDecoderImpl is only used by the AudioRenderer.  The renderer
// will call Initialize() before any other function is called.  The renderer may
// then call Read() repeatly to request decoded audio data.  Reset() is called
// when a seek is initiated to clear any internal data of the decoder.
// Note that the renderer will not explicitly destroy the decoder when playback
// is finished.  It simply release its reference to the decoder and the decoder
// may be alive for a short time.  When playback is finished, the demuxer will
// keep return EOS buffer.  So the decoder will stop reading quickly.  Once no
// callback is in progress, it will be destroyed automatically because there is
// no reference to it.
class MEDIA_EXPORT ShellAudioDecoderImpl : public AudioDecoder {
 public:
  ShellAudioDecoderImpl(
      const scoped_refptr<base::MessageLoopProxy>& message_loop,
      ShellRawAudioDecoderFactory* raw_audio_decoder_factory);
  ~ShellAudioDecoderImpl() override;

 private:
  static const int kMaxQueuedBuffers = 32;

  // AudioDecoder implementation.
  void Initialize(const scoped_refptr<DemuxerStream>& stream,
                  const PipelineStatusCB& status_cb,
                  const StatisticsCB& statistics_cb) override;
  void Read(const ReadCB& read_cb) override;
  int bits_per_channel() override;
  ChannelLayout channel_layout() override;
  int samples_per_second() override;
  void Reset(const base::Closure& reset_cb) override;

  void TryToReadFromDemuxerStream();
  void OnDemuxerRead(DemuxerStream::Status status,
                     const scoped_refptr<DecoderBuffer>& buffer);
  // the callback from the raw decoder indicates an operation has been finished.
  void OnBufferDecoded(ShellRawAudioDecoder::DecodeStatus status,
                       const scoped_refptr<DecoderBuffer>& buffer);

  // The message loop that all functions run on to avoid explicit sync.
  scoped_refptr<base::MessageLoopProxy> message_loop_;
  ShellRawAudioDecoderFactory* raw_audio_decoder_factory_;
  scoped_refptr<DemuxerStream> demuxer_stream_;
  StatisticsCB statistics_cb_;

  scoped_ptr<ShellRawAudioDecoder> raw_decoder_;

  // Are we currently in a read/decode cycle?  One thing worth noting is, Read()
  // and Reset() will only be deferred when demuxer_read_and_decode_in_progress_
  // is true,  so if demuxer_read_and_decode_in_progress_ is false, read_cb_ and
  // reset_cb_ will be NULL.
  bool demuxer_read_and_decode_in_progress_;
  // Save the EOS buffer received so we can send it again to the raw decoder
  // with correct timestamp.  When this is non NULL, we shouldn't read from the
  // demuxer again until it is reset in Reset().
  scoped_refptr<DecoderBuffer> eos_buffer_;

#if __SAVE_DECODER_OUTPUT__
  ShellWavTestProbe test_probe_;
#endif

  int samples_per_second_;
  int num_channels_;

  ReadCB read_cb_;
  base::Closure reset_cb_;

  std::queue<scoped_refptr<DecoderBuffer> > queued_buffers_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ShellAudioDecoderImpl);
};

}  // namespace media

#endif  // MEDIA_FILTERS_SHELL_AUDIO_DECODER_IMPL_H_
