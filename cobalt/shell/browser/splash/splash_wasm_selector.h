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

#ifndef COBALT_SHELL_BROWSER_SPLASH_SPLASH_WASM_SELECTOR_H_
#define COBALT_SHELL_BROWSER_SPLASH_SPLASH_WASM_SELECTOR_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cobalt/shell/browser/splash/splash_cache_reader.h"
#include "cobalt/shell/browser/splash/splash_wasm_runner.h"
#include "url/gurl.h"

namespace content {

class BrowserContext;

// What the host tells the selector about this launch.
struct SplashSelectorInput {
  std::string topic;
  std::string locale;
  int screen_width = 0;
  int screen_height = 0;
};

// The outcome of a selection.
struct SplashSelection {
  SplashSelection();
  SplashSelection(SplashSelection&&);
  SplashSelection& operator=(SplashSelection&&);
  ~SplashSelection();

  // The cache key of |video|. Empty if no video could be read, in which case
  // the caller should show its built-in splash.
  std::string key;
  std::vector<uint8_t> video;

  // Whether |key| was chosen by the selector, rather than being the fallback.
  bool selected_by_wasm = false;
  // Why the selector wasn't used, if it wasn't.
  std::string selector_error;

  base::TimeDelta read_selector_time;
  base::TimeDelta run_selector_time;
  base::TimeDelta read_video_time;
};

// Proof of concept: picks the splash video with a Wasm module provided by the
// web app, without a renderer or WebContents.
//
//   1. Reads the selector module (kSelectorKey) from the splash Cache Storage.
//   2. Runs it with WAMR, see RunSplashSelectorWasm(), passing it a
//      SplashSelectorInput as JSON. It returns a cache key.
//   3. Reads the video with that key from the same cache.
//
// Falls back to kFallbackKey if there is no selector, or it fails, or the
// video it picks is missing. Playing the video is up to the caller.
// Lives on the UI thread.
class SplashWasmSelector {
 public:
  static constexpr char kSelectorKey[] = "splash_selector.wasm";
  static constexpr char kFallbackKey[] = "splash.webm";
  static constexpr uint64_t kMaxSelectorSize = 1024 * 1024;

  using Callback = base::OnceCallback<void(SplashSelection)>;

  SplashWasmSelector(BrowserContext* browser_context,
                     const GURL& splash_origin,
                     std::u16string cache_name);
  SplashWasmSelector(const SplashWasmSelector&) = delete;
  SplashWasmSelector& operator=(const SplashWasmSelector&) = delete;
  ~SplashWasmSelector();

  // Runs |callback| asynchronously, unless this is destroyed first.
  void Select(const SplashSelectorInput& input, Callback callback);

  // The JSON object passed to the selector.
  static std::string BuildInputJson(const SplashSelectorInput& input,
                                    base::Time now);

  void set_instruction_limit_for_testing(int limit) {
    instruction_limit_ = limit;
  }

 private:
  void OnSelectorRead(
      base::TimeTicks start,
      base::expected<std::vector<uint8_t>, SplashCacheReader::Error> result);
  void ReadVideo(const std::string& key);
  void OnVideoRead(
      const std::string& key,
      base::TimeTicks start,
      base::expected<std::vector<uint8_t>, SplashCacheReader::Error> result);
  void Finish();

  SplashCacheReader reader_;
  int instruction_limit_ = kDefaultSplashSelectorInstructionLimit;
  std::string input_json_;
  SplashSelection selection_;
  Callback callback_;
  base::WeakPtrFactory<SplashWasmSelector> weak_factory_{this};
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SPLASH_SPLASH_WASM_SELECTOR_H_
