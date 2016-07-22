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
#include "base/path_service.h"
#include "base/time.h"
#include "cobalt/base/wrap_main.h"
#include "cobalt/media/sandbox/media_sandbox.h"
#include "cobalt/media/sandbox/web_media_player_helper.h"
#include "cobalt/render_tree/image.h"
#include "media/base/video_frame.h"
#include "net/base/net_util.h"

namespace cobalt {
namespace media {
namespace sandbox {
namespace {

using render_tree::Image;
using ::media::VideoFrame;

GURL ResolveUrl(const char* arg) {
  GURL video_url(arg);
  if (!video_url.is_valid()) {
    // Assume the input is a path.
    // Try to figure out the path to this file and convert it to a URL.
    FilePath result(arg);
    if (!result.IsAbsolute()) {
      FilePath content_path;
      PathService::Get(base::DIR_EXE, &content_path);
      DCHECK(content_path.IsAbsolute());
      // TODO: Get the "real" exe path.
      result = content_path.DirName().DirName().Append(result);
    }
    video_url = net::FilePathToFileURL(result);
  }
  return video_url;
}

scoped_refptr<Image> FrameCB(WebMediaPlayerHelper* player_helper,
                             const base::TimeDelta& time) {
  UNREFERENCED_PARAMETER(time);

  scoped_refptr<VideoFrame> frame = player_helper->GetCurrentFrame();
  return frame ? reinterpret_cast<Image*>(frame->texture_id()) : NULL;
}

int SandboxMain(int argc, char** argv) {
  if (argc != 2 && argc != 3) {
    LOG(ERROR) << "Usage: " << argv[0]
               << " [--null_audio_streamer] <url|path>";
    return 1;
  }
  MediaSandbox media_sandbox(
      argc, argv,
      FilePath(FILE_PATH_LITERAL("web_media_player_sandbox_trace.json")));

  // Note that we can't access PathService until MediaSandbox is initialized.
  GURL video_url = ResolveUrl(argv[argc - 1]);

  if (!video_url.is_valid()) {
    LOG(ERROR) << " Invalid URL: " << video_url;
    return 1;
  }

  LOG(INFO) << "Loading " << video_url;

  WebMediaPlayerHelper player_helper(media_sandbox.GetMediaModule(),
                                     media_sandbox.GetFetcherFactory(),
                                     video_url);

  media_sandbox.RegisterFrameCB(
      base::Bind(FrameCB, base::Unretained(&player_helper)));

  for (;;) {
    if (player_helper.IsPlaybackFinished()) {
      break;
    }
    MessageLoop::current()->RunUntilIdle();
  }

  media_sandbox.RegisterFrameCB(MediaSandbox::FrameCB());
  LOG(INFO) << "Playback finished.";

  return 0;
}

}  // namespace
}  // namespace sandbox
}  // namespace media
}  // namespace cobalt

COBALT_WRAP_SIMPLE_MAIN(cobalt::media::sandbox::SandboxMain);
