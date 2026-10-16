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

#ifndef CONTENT_RENDERER_MEDIA_COBALT_WASM_HOLE_WEB_MEDIA_PLAYER_H_
#define CONTENT_RENDERER_MEDIA_COBALT_WASM_HOLE_WEB_MEDIA_PLAYER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/web_media_player.h"

namespace blink {
class WebMediaPlayerClient;
class WebURL;
}  // namespace blink

namespace content {

class WasmHoleWebMediaPlayer {
 public:
  // Returns a hole-punching WebMediaPlayer when `url` is a `blob:` URL with
  // fragment `sb-surface=<token>`, or nullptr otherwise. The element's geometry
  // positions the Wasm surface registered under that token.
  static std::unique_ptr<blink::WebMediaPlayer> MaybeCreate(
      const blink::WebURL& url,
      blink::WebMediaPlayerClient* client,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_COBALT_WASM_HOLE_WEB_MEDIA_PLAYER_H_
