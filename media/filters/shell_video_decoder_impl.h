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

#include <list>
#include <map>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "media/base/demuxer_stream.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/shell_demuxer.h"
#include "media/filters/shell_video_decoder.h"

namespace LB {
class LBVideoDecoder;
}

namespace media {

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
  struct VideoTexture;

  void BufferReady(DemuxerStream::Status status,
                   const scoped_refptr<ShellBuffer>& buffer);

  // Carries out the reading operation scheduled by Read().
  void DoRead(const ReadCB& read_cb);
  // actually makes the call to the platform decoder to decode
  void DecodeBuffer(const scoped_refptr<ShellBuffer>& buffer);
  // Reads from the demuxer stream on the pipeline thread.
  void ReadFromDemuxerStreamTask(const DemuxerStream::ReadCB& callback);
  // Posts a task to read from the demuxer stream.
  void ReadFromDemuxerStream();
  // Reset decoder and call |reset_cb_|.
  void DoReset();
  // called on decoder thread, fatal error on decode, we shut down decode
  void DecoderFatalError();

  enum DecoderState {
    kUninitialized,
    kNormal,
    kFlushCodec,
    kDecodeFinished,
    kShellDecodeError,
  };
  DecoderState state_;
  scoped_refptr<base::MessageLoopProxy> media_pipeline_message_loop_;
  scoped_refptr<DemuxerStream> demuxer_stream_;
  scoped_refptr<ShellFilterGraphLog> filter_graph_log_;
  ReadCB read_cb_;
  base::Closure reset_cb_;
  PipelineStatusCB status_cb_;

  LB::LBVideoDecoder* raw_decoder_;
  bool read_pending_;
  VideoDecoderConfig decoder_config_;
  // TODO(***REMOVED***) : ensure the demuxer can handle multiple EOS requests
  //                   then remove this hack.
  scoped_refptr<ShellBuffer> eof_buffer_;

  // All decoding tasks will be performed on this thread's message loop
  base::Thread decoder_thread_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ShellVideoDecoderImpl);
};

}  // namespace media

#endif  // MEDIA_FILTERS_SHELL_VIDEO_DECODER_IMPL_H_
