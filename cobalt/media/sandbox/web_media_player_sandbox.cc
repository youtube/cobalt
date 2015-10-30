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
#include "base/time.h"
#include "cobalt/media/sandbox/media_sandbox.h"
#include "cobalt/media/sandbox/web_media_player_helper.h"
#include "cobalt/render_tree/image.h"
#include "media/base/video_frame.h"

namespace cobalt {
namespace media {
namespace sandbox {
namespace {

using base::Bind;
using base::Unretained;
using render_tree::Image;
using ::media::VideoFrame;
using ::media::WebMediaPlayer;

scoped_refptr<Image> FrameCB(WebMediaPlayerHelper* player_helper,
                             const base::TimeDelta& time) {
  UNREFERENCED_PARAMETER(time);

  scoped_refptr<VideoFrame> frame = player_helper->GetCurrentFrame();
  return frame ? reinterpret_cast<Image*>(frame->texture_id()) : NULL;
}

int SandboxMain(int argc, char** argv) {
  // Provide a default video url in the sandbox for convenience.
  const char kDefaultURL[] =
      "file:///cobalt/browser/testdata/media-transition-demo/progressive.mp4";
  const GURL video_url(argc > 1 ? argv[1] : kDefaultURL);

  MediaSandbox media_sandbox(
      argc, argv,
      FilePath(FILE_PATH_LITERAL("web_media_player_sandbox_trace.json")));
  WebMediaPlayerHelper player_helper(media_sandbox.GetMediaModule(),
                                     media_sandbox.GetFetcherFactory(),
                                     video_url);

  media_sandbox.RegisterFrameCB(
      base::Bind(FrameCB, Unretained(&player_helper)));

  for (;;) {
    if (player_helper.IsPlaybackFinished()) {
      break;
    }
    MessageLoop::current()->RunUntilIdle();
  }

  media_sandbox.RegisterFrameCB(MediaSandbox::FrameCB());

  return 0;
}

}  // namespace
}  // namespace sandbox
}  // namespace media
}  // namespace cobalt

int main(int argc, char** argv) {
  return cobalt::media::sandbox::SandboxMain(argc, argv);
}
