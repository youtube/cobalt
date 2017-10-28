// Copyright 2015 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
#if defined(COBALT_MEDIA_SOURCE_2016)
#include "cobalt/media/base/shell_video_frame_provider.h"
#else  // defined(COBALT_MEDIA_SOURCE_2016)
#include "media/base/video_frame.h"
#endif  // defined(COBALT_MEDIA_SOURCE_2016)
#include "net/base/net_util.h"

namespace cobalt {
namespace media {
namespace sandbox {
namespace {

// Allows args to be pushed into a stack. This allows passing variables to
// functions that take in the int, char** args format.
struct Args {
  Args(int argc, char** argv) {
    for (int i = 0; i < argc; ++i) {
      PushBack(argv[i]);
    }
  }

  ~Args() {
    while (size()) {
      PopBack();
    }
  }

  char** get_argv() {
    if (buffs_.empty()) {
      return NULL;
    } else {
      return &buffs_[0];
    }
  }

  char* get(int i) {
    return buffs_[i];
  }

  int size() const {
    return static_cast<int>(buffs_.size());
  }

  void PushBack(const char* in) {
    char* string_cp = new char[strlen(in) + 1];
    strcpy(string_cp, in);
    buffs_.push_back(string_cp);
  }

  void PopBack() {
    if (!buffs_.empty()) {
      delete[] buffs_.back();
      buffs_.pop_back();
    }
  }

  std::vector<char*> buffs_;
};

using render_tree::Image;
#if !defined(COBALT_MEDIA_SOURCE_2016)
using ::media::VideoFrame;
#endif  // !defined(COBALT_MEDIA_SOURCE_2016)

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
  Args args(argc, argv);

  // 2nd argument is supposed to be the url. If this doesn't exist then
  // a default url pointing to the resource is used instead.
  if (args.size() < 2) {
    LOG(INFO) << "Warning: " << argv[0]
              << " had no url, defaulting to hard-coded path.";

    const char kHardcodedMp4Url[] =
        "https://storage.googleapis.com/yt-cobalt-media-element-demo/"
        "progressive.mp4";
    args.PushBack(kHardcodedMp4Url);
  }

  MediaSandbox media_sandbox(
      args.size(), args.get_argv(),
      FilePath(FILE_PATH_LITERAL("web_media_player_sandbox_trace.json")));

  const char* mp4_hard_coded_url = args.get(1);

  // Note that we can't access PathService until MediaSandbox is initialized.
  GURL video_url = ResolveUrl(mp4_hard_coded_url);

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
