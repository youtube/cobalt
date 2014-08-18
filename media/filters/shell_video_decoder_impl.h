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
#include "media/base/video_decoder_config.h"
#include "media/filters/shell_video_decoder.h"

namespace media {

// TODO(***REMOVED***) : Make this decoder handle decoder errors. Now it assumes
// that the input stream is always correct.
class ShellRawVideoDecoder {
 public:
  enum DecodeStatus {
    FRAME_DECODED,  // Successfully decoded a frame.
    NEED_MORE_DATA,  // Need more data to decode the next frame.
    RETRY_WITH_SAME_BUFFER,  // The decoder is busy, retry later.
    FATAL_ERROR  // Decoder encounters fatal error, abort playback.
  };
  typedef media::ShellBuffer ShellBuffer;
  typedef media::VideoDecoderConfig VideoDecoderConfig;
  typedef media::VideoFrame VideoFrame;

  ShellRawVideoDecoder() {}
  virtual ~ShellRawVideoDecoder() {}
  virtual DecodeStatus Decode(const scoped_refptr<ShellBuffer>& buffer,
                              scoped_refptr<VideoFrame>* frame) = 0;
  virtual bool Flush() = 0;
  virtual bool UpdateConfig(const VideoDecoderConfig& config) = 0;

  static ShellRawVideoDecoder* Create(const VideoDecoderConfig& config,
                                      media::Decryptor* decryptor,
                                      bool was_encrypted);

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellRawVideoDecoder);
};

class MEDIA_EXPORT ShellVideoDecoderImpl : public ShellVideoDecoder {
 public:
  ShellVideoDecoderImpl(
      const scoped_refptr<base::MessageLoopProxy>& message_loop);
  virtual ~ShellVideoDecoderImpl();

  // ShellVideoDecoder implementation.
  virtual void Initialize(const scoped_refptr<DemuxerStream>& stream,
                          const PipelineStatusCB& status_cb,
                          const StatisticsCB& statistics_cb) OVERRIDE;
  virtual void Read(const ReadCB& read_cb) OVERRIDE;
  virtual void Reset(const base::Closure& closure) OVERRIDE;
  virtual void Stop(const base::Closure& closure) OVERRIDE;

 private:
  enum DecoderState {
    kUninitialized,
    kNormal,
    kFlushCodec,
    kDecodeFinished,
    kShellDecodeError
  };

  void BufferReady(DemuxerStream::Status status,
                   const scoped_refptr<ShellBuffer>& buffer);

  // actually makes the call to the platform decoder to decode
  void DecodeBuffer(const scoped_refptr<ShellBuffer>& buffer);
  // Posts a task to read from the demuxer stream.
  void ReadFromDemuxerStream();
  // Reset decoder and call |reset_cb_|.
  void DoReset();
  // called on decoder thread, fatal error on decode, we shut down decode
  void DecoderFatalError();

  DecoderState state_;
  scoped_refptr<base::MessageLoopProxy> media_pipeline_message_loop_;
  scoped_refptr<DemuxerStream> demuxer_stream_;
  ReadCB read_cb_;
  base::Closure reset_cb_;

  scoped_ptr<ShellRawVideoDecoder> raw_decoder_;

  // TODO(***REMOVED***) : ensure the demuxer can handle multiple EOS requests
  //                   then remove this hack.
  scoped_refptr<ShellBuffer> eof_buffer_;

  // All decoding tasks will be performed on this thread's message loop
  base::Thread decoder_thread_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ShellVideoDecoderImpl);
};

}  // namespace media

#endif  // MEDIA_FILTERS_SHELL_VIDEO_DECODER_IMPL_H_
