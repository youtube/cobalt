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
#ifndef MEDIA_FILTERS_SHELL_VIDEO_DECODER_IMPL_H_
#define MEDIA_FILTERS_SHELL_VIDEO_DECODER_IMPL_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "media/base/demuxer_stream.h"
#include "media/base/shell_buffer_factory.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"

namespace media {

class ShellRawVideoDecoder {
 public:
  enum DecodeStatus {
    FRAME_DECODED,           // Successfully decoded a frame.
    NEED_MORE_DATA,          // Need more data to decode the next frame.
    RETRY_WITH_SAME_BUFFER,  // Retry later with the same input. Note that in
                             // this case the decoder may still return a valid
                             // buffered frame.
    FATAL_ERROR              // Decoder encounters fatal error, abort playback.
  };
  typedef media::DecoderBuffer DecoderBuffer;
  typedef media::VideoDecoderConfig VideoDecoderConfig;
  typedef media::VideoFrame VideoFrame;
  typedef base::Callback<void(DecodeStatus, const scoped_refptr<VideoFrame>&)>
      DecodeCB;

  ShellRawVideoDecoder() {}
  virtual ~ShellRawVideoDecoder() {}
  virtual void Decode(const scoped_refptr<DecoderBuffer>& buffer,
                      const DecodeCB& decode_cb) = 0;
  virtual bool Flush() = 0;
  virtual bool UpdateConfig(const VideoDecoderConfig& config) = 0;
  // See ShellVideoDecoder for more details about the following two functions.
  virtual void NearlyUnderflow() {}
  virtual void HaveEnoughFrames() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellRawVideoDecoder);
};

class ShellRawVideoDecoderFactory {
 public:
  virtual ~ShellRawVideoDecoderFactory() {}

  virtual scoped_ptr<ShellRawVideoDecoder> Create(
      const VideoDecoderConfig& config,
      Decryptor* decryptor,
      bool was_encrypted) = 0;
};

class MEDIA_EXPORT ShellVideoDecoderImpl : public VideoDecoder {
 public:
  ShellVideoDecoderImpl(
      const scoped_refptr<base::MessageLoopProxy>& message_loop,
      ShellRawVideoDecoderFactory* raw_video_decoder_factory);
  ~ShellVideoDecoderImpl() override;

  // ShellVideoDecoder implementation.
  void Initialize(const scoped_refptr<DemuxerStream>& stream,
                  const PipelineStatusCB& status_cb,
                  const StatisticsCB& statistics_cb) override;
  void Read(const ReadCB& read_cb) override;
  void Reset(const base::Closure& closure) override;
  void Stop(const base::Closure& closure) override;
  void NearlyUnderflow() override;
  void HaveEnoughFrames() override;

 private:
  enum DecoderState {
    kUninitialized,
    kNormal,
    kFlushCodec,
    kDecodeFinished,
    kShellDecodeError
  };

  void BufferReady(DemuxerStream::Status status,
                   const scoped_refptr<DecoderBuffer>& buffer);

  // actually makes the call to the platform decoder to decode
  void DecodeBuffer(const scoped_refptr<DecoderBuffer>& buffer);
  // the callback from the raw decoder indicates an operation has been finished.
  void DecodeCallback(ShellRawVideoDecoder::DecodeStatus status,
                      const scoped_refptr<VideoFrame>& frame);
  // Posts a task to read from the demuxer stream.
  void ReadFromDemuxerStream();
  // Reset decoder and call |reset_cb_|.
  void DoReset();
  // called on decoder thread, fatal error on decode, we shut down decode
  void DecoderFatalError();

  DecoderState state_;
  scoped_refptr<base::MessageLoopProxy> media_pipeline_message_loop_;
  ShellRawVideoDecoderFactory* raw_video_decoder_factory_;
  scoped_refptr<DemuxerStream> demuxer_stream_;
  ReadCB read_cb_;
  base::Closure reset_cb_;

  scoped_ptr<ShellRawVideoDecoder> raw_decoder_;

  // TODO: ensure the demuxer can handle multiple EOS requests then remove this
  // hack.
  scoped_refptr<DecoderBuffer> eof_buffer_;

  scoped_refptr<DecoderBuffer> buffer_to_decode_;

  // All decoding tasks will be performed on this thread's message loop
  base::Thread decoder_thread_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ShellVideoDecoderImpl);
};

}  // namespace media

#endif  // MEDIA_FILTERS_SHELL_VIDEO_DECODER_IMPL_H_
