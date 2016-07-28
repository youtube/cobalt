/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/time.h"
#include "cobalt/base/wrap_main.h"
#include "cobalt/media/sandbox/demuxer_helper.h"
#include "cobalt/media/sandbox/media_sandbox.h"
#include "cobalt/render_tree/image.h"
#include "media/base/bind_to_loop.h"
#include "media/base/video_frame.h"
#if defined(__LB_LINUX__)
#include "media/filters/shell_raw_video_decoder_linux.h"
#elif defined(__LB_PS3__)
#include "media/filters/shell_raw_video_decoder_ps3.h"
#else
#include "media/filters/shell_raw_video_decoder_stub.h"
#endif

using base::Bind;
using base::Unretained;
using cobalt::render_tree::Image;
using ::media::BindToCurrentLoop;
using ::media::DecoderBuffer;
using ::media::Demuxer;
using ::media::DemuxerStream;
using ::media::PipelineStatus;
using ::media::ShellRawVideoDecoder;
using ::media::VideoDecoder;
using ::media::VideoFrame;

namespace cobalt {
namespace media {
namespace sandbox {
namespace {

class RawVideoDecoderSandbox {
 public:
  void SetDemuxer(const scoped_refptr<Demuxer>& demuxer) {
    DCHECK(demuxer);

    scoped_refptr<DemuxerStream> stream =
        demuxer->GetStream(::media::DemuxerStream::VIDEO);
    DCHECK(stream);

    demuxer_ = demuxer;
#if defined(__LB_LINUX__)
    decoder_ = ::media::CreateShellRawVideoDecoderLinux(
        stream->video_decoder_config(), NULL, false);
#elif defined(__LB_PS3__)
    decoder_ = ::media::CreateShellRawVideoDecoderPS3(
        stream->video_decoder_config(), NULL, false);
#else
    decoder_ = ::media::CreateShellRawVideoDecoderStub(
        stream->video_decoder_config(), NULL, false);
#endif
    stream->Read(Bind(&RawVideoDecoderSandbox::ReadCB, Unretained(this)));
  }

  scoped_refptr<Image> GetCurrentFrame(const base::TimeDelta& time) {
    UNREFERENCED_PARAMETER(time);
    return current_frame_
               ? reinterpret_cast<Image*>(current_frame_->texture_id())
               : NULL;
  }

 private:
  void DecodeCB(const scoped_refptr<DecoderBuffer>& buffer,
                ShellRawVideoDecoder::DecodeStatus status,
                const scoped_refptr<VideoFrame>& frame) {
    scoped_refptr<DemuxerStream> stream =
        demuxer_->GetStream(::media::DemuxerStream::VIDEO);

    if (status == ShellRawVideoDecoder::FRAME_DECODED) {
      DCHECK(frame);

      if (frame->IsEndOfStream()) {
        current_frame_ = NULL;
        decoder_.reset();
        demuxer_->Stop(MessageLoop::QuitWhenIdleClosure());
        return;
      } else {
        current_frame_ = frame;
      }
      stream->Read(Bind(&RawVideoDecoderSandbox::ReadCB, Unretained(this)));
    } else if (status == ShellRawVideoDecoder::NEED_MORE_DATA) {
      stream->Read(Bind(&RawVideoDecoderSandbox::ReadCB, Unretained(this)));
    } else if (status == ShellRawVideoDecoder::RETRY_WITH_SAME_BUFFER) {
      if (frame) {
        current_frame_ = frame;
      }
      decoder_->Decode(buffer,
                       BindToCurrentLoop(Bind(&RawVideoDecoderSandbox::DecodeCB,
                                              Unretained(this), buffer)));
    } else {
      NOTREACHED();
    }
  }

  void ReadCB(DemuxerStream::Status status,
              const scoped_refptr<DecoderBuffer>& buffer) {
    DCHECK_EQ(status, DemuxerStream::kOk);
    decoder_->Decode(buffer,
                     BindToCurrentLoop(Bind(&RawVideoDecoderSandbox::DecodeCB,
                                            Unretained(this), buffer)));
  }

  scoped_refptr<Demuxer> demuxer_;
  scoped_ptr<ShellRawVideoDecoder> decoder_;
  scoped_refptr<VideoFrame> current_frame_;
};

int SandboxMain(int argc, char** argv) {
  DCHECK_GT(argc, 1) << " Usage: " << argv[0] << " <url>";
  GURL video_url(argv[1]);
  DCHECK(video_url.is_valid()) << " \"" << argv[1] << "\" is not a valid URL.";

  MediaSandbox media_sandbox(
      argc, argv,
      FilePath(FILE_PATH_LITERAL("raw_video_decoder_sandbox_trace.json")));
  RawVideoDecoderSandbox decoder_sandbox;
  DemuxerHelper demuxer_helper(
      base::MessageLoopProxy::current(), media_sandbox.GetFetcherFactory(),
      video_url, BindToCurrentLoop(Bind(&RawVideoDecoderSandbox::SetDemuxer,
                                        Unretained(&decoder_sandbox))));
  media_sandbox.RegisterFrameCB(Bind(&RawVideoDecoderSandbox::GetCurrentFrame,
                                     Unretained(&decoder_sandbox)));
  MessageLoop::current()->Run();

  LOG(INFO) << "Video playback finished successfully.";

  media_sandbox.RegisterFrameCB(MediaSandbox::FrameCB());

  return 0;
}

}  // namespace
}  // namespace sandbox
}  // namespace media
}  // namespace cobalt

COBALT_WRAP_SIMPLE_MAIN(cobalt::media::sandbox::SandboxMain);
