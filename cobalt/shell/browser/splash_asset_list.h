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

#ifndef COBALT_SHELL_BROWSER_SPLASH_ASSET_LIST_H_
#define COBALT_SHELL_BROWSER_SPLASH_ASSET_LIST_H_

#include <string>

#include "base/functional/callback.h"

namespace content {

class BrowserContext;

// DO NOT SUBMIT: benchmark only (cobalt::features::kSplashWasmPolicy).
//
// Lists the splash screen assets that splash.html's WebAssembly policy chooses
// from: the built-in videos and images, and the ones that the WebApp cached
// with the Cache API of the splash domain, as JSON:
//   [{"source": "builtin" or "cache", "cache": <cache name, or "">,
//     "name": <e.g., "splash.webm">, "kind": "video" or "image",
//     "topic": <"default", "music", ...>, "width": <px, 0 if unknown>,
//     "height": <px, 0 if unknown>, "bytes": <size>}, ...]
// Shell starts listing when the splash screen starts, so that the list is
// usually ready by the time splash.html asks for it
// (h5vcc-embedded://splash_assets.json). The cache is only listed with
// kEnableCachedSplashFile, as it's only read with it.
//
// Lives for the process, on the UI thread.
class SplashAssetList {
 public:
  using Callback = base::OnceCallback<void(const std::string& json)>;

  SplashAssetList() = delete;

  // Starts listing, once per process: later calls do nothing.
  static void Start(BrowserContext* browser_context);

  // Runs `callback` with the list once it's ready, right away if it is,
  // starting the listing if it hasn't started.
  static void Get(BrowserContext* browser_context, Callback callback);
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SPLASH_ASSET_LIST_H_
