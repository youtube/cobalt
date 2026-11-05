// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef CONTENT_RENDERER_MEDIA_COBALT_VIDEO_HOLE_WEB_MEDIA_PLAYER_H_
#define CONTENT_RENDERER_MEDIA_COBALT_VIDEO_HOLE_WEB_MEDIA_PLAYER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/web_media_player.h"

namespace blink {
class WebLocalFrame;
class WebMediaPlayerClient;
class WebURL;
}  // namespace blink

namespace content {

// Creates WebMediaPlayers that only punch a hole through the page, for video
// that Cobalt plays natively behind it, such as the splash screen video of the
// NativeVideoSplashScreen feature. They show a video hole frame, which the
// compositor turns into a transparent area over the video plane, as for the
// punch-out video of StarboardRenderer, but without any media pipeline.
class VideoHoleWebMediaPlayer {
 public:
  // The fragment of the blob: URL that requests a video hole, e.g.,
  // "blob:h5vcc-embedded://splash.html/<uuid>#cobalt-video-hole".
  static constexpr char kUrlFragment[] = "cobalt-video-hole";

  // Returns a video hole player if `url` requests one and `frame` shows one of
  // Cobalt's built-in (h5vcc-embedded) pages, or nullptr otherwise.
  static std::unique_ptr<blink::WebMediaPlayer> MaybeCreate(
      const blink::WebURL& url,
      blink::WebLocalFrame* frame,
      blink::WebMediaPlayerClient* client,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_COBALT_VIDEO_HOLE_WEB_MEDIA_PLAYER_H_
