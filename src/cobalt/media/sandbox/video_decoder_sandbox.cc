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
#include "media/filters/shell_video_decoder_impl.h"

using base::Bind;
using base::Unretained;
using cobalt::render_tree::Image;
using ::media::BindToCurrentLoop;
using ::media::Demuxer;
using ::media::DemuxerStream;
using ::media::PipelineStatus;
using ::media::ShellVideoDecoderImpl;
using ::media::VideoDecoder;
using ::media::VideoFrame;

namespace cobalt {
namespace media {
namespace sandbox {
namespace {

class VideoDecoderSandbox {
 public:
  void SetDemuxer(const scoped_refptr<Demuxer>& demuxer) {
    DCHECK(demuxer);

    scoped_refptr<DemuxerStream> stream =
        demuxer->GetStream(::media::DemuxerStream::VIDEO);
    DCHECK(stream);

    demuxer_ = demuxer;
#if defined(__LB_LINUX__)
    decoder_ = new ShellVideoDecoderImpl(
        base::MessageLoopProxy::current(),
        base::Bind(::media::CreateShellRawVideoDecoderLinux));
#elif defined(__LB_PS3__)
    decoder_ = new ShellVideoDecoderImpl(
        base::MessageLoopProxy::current(),
        base::Bind(::media::CreateShellRawVideoDecoderPS3));
#else
    decoder_ = new ShellVideoDecoderImpl(
        base::MessageLoopProxy::current(),
        base::Bind(::media::CreateShellRawVideoDecoderStub));
#endif
    decoder_->Initialize(
        stream,
        Bind(&VideoDecoderSandbox::VideoDecoderReadyCB, Unretained(this)),
        Bind(&VideoDecoderSandbox::PipelineStatisticsCB, Unretained(this)));
  }

  scoped_refptr<Image> GetCurrentFrame(const base::TimeDelta& time) {
    UNREFERENCED_PARAMETER(time);

    return current_frame_
               ? reinterpret_cast<Image*>(current_frame_->texture_id())
               : NULL;
  }

 private:
  void DecodeFrame(VideoDecoder::Status status,
                   const scoped_refptr<VideoFrame>& frame) {
    DCHECK_EQ(status, VideoDecoder::kOk);

    if (frame) {
      if (frame->IsEndOfStream()) {
        current_frame_ = NULL;
        decoder_->Stop(
            Bind(&Demuxer::Stop, demuxer_, MessageLoop::QuitWhenIdleClosure()));
        return;
      } else {
        current_frame_ = frame;
      }
    }

    decoder_->Read(Bind(&VideoDecoderSandbox::DecodeFrame, Unretained(this)));
  }

  void VideoDecoderReadyCB(PipelineStatus status) {
    // Kick off the first Read().
    decoder_->Read(Bind(&VideoDecoderSandbox::DecodeFrame, Unretained(this)));
  }

  void PipelineStatisticsCB(const ::media::PipelineStatistics&) {}

  scoped_refptr<Demuxer> demuxer_;
  scoped_refptr<VideoDecoder> decoder_;
  scoped_refptr<VideoFrame> current_frame_;
};

int SandboxMain(int argc, char** argv) {
  DCHECK_GT(argc, 1) << " Usage: " << argv[0] << " <url>";
  GURL video_url(argv[1]);
  DCHECK(video_url.is_valid()) << " \"" << argv[1] << "\" is not a valid URL.";

  MediaSandbox media_sandbox(
      argc, argv,
      FilePath(FILE_PATH_LITERAL("video_decoder_sandbox_trace.json")));
  VideoDecoderSandbox decoder_sandbox;
  DemuxerHelper demuxer_helper(
      base::MessageLoopProxy::current(), media_sandbox.GetFetcherFactory(),
      video_url, BindToCurrentLoop(Bind(&VideoDecoderSandbox::SetDemuxer,
                                        Unretained(&decoder_sandbox))));

  media_sandbox.RegisterFrameCB(Bind(&VideoDecoderSandbox::GetCurrentFrame,
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
