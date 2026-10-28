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

#ifndef COBALT_SHELL_BROWSER_SPLASH_NATIVE_SPLASH_H_
#define COBALT_SHELL_BROWSER_SPLASH_NATIVE_SPLASH_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cobalt/shell/browser/splash/splash_video_player.h"
#include "cobalt/shell/browser/splash/splash_wasm_selector.h"
#include "starboard/window.h"
#include "url/gurl.h"

namespace content {

class BrowserContext;

// Proof of concept of a splash screen without a WebContents: picks the video
// with SplashWasmSelector, then plays it with SplashVideoPlayer.
// Lives on the UI thread.
class NativeSplash {
 public:
  struct Config {
    Config();
    Config(Config&&);
    Config& operator=(Config&&);
    ~Config();

    GURL splash_origin;
    std::u16string cache_name;
    SplashSelectorInput selector_input;
    // Played if the cache has no video.
    std::vector<uint8_t> builtin_video;
    base::TimeDelta max_duration = base::Seconds(10);
  };

  // Tells NativeSplash about the Starboard window to play in. Starboard
  // platforms create it asynchronously; the splash waits for it.
  static void SetSbWindow(SbWindow window);

  NativeSplash(BrowserContext* browser_context, Config config);
  NativeSplash(const NativeSplash&) = delete;
  NativeSplash& operator=(const NativeSplash&) = delete;
  ~NativeSplash();

  // |on_presenting| runs when the video shows its first frame, and
  // |on_finished| when the splash is over, whether it showed anything or not.
  void Start(base::OnceClosure on_presenting, base::OnceClosure on_finished);

 private:
  void OnSelected(SplashSelection selection);
  void MaybePlay();
  void OnPlayerFinished(SplashVideoPlayer::Result result);

  Config config_;
  SplashWasmSelector selector_;
  SplashVideoPlayer player_;
  std::optional<std::vector<uint8_t>> video_;
  bool playing_ = false;
  base::TimeTicks start_time_;
  base::OnceClosure on_presenting_;
  base::OnceClosure on_finished_;
  base::WeakPtrFactory<NativeSplash> weak_factory_{this};
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SPLASH_NATIVE_SPLASH_H_
