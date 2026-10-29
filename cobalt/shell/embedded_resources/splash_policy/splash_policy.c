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

// The splash screen's WebAssembly policy, splash_policy.wasm, with which
// splash.html chooses the asset that it shows when Cobalt's SplashWasmPolicy
// feature is enabled. It chooses from the candidate assets that splash.html
// passes it, the built-in ones and those that the WebApp cached (see
// cobalt/shell/browser/splash_asset_list.h), among those of the kind that the
// splash screen shows and that the platform plays:
// - first, an asset of the launch's topic, e.g., music;
// - then, one that the WebApp cached;
// - then, the one with the highest resolution.
// For the candidates that splash.html passes, that's what splash.html chooses
// without the policy, so that only how the asset is chosen differs.
//
// The module needs no imports, nor libc. splash.html writes one Candidate per
// asset to items_buffer(), at most max_items() of them, then calls choose(),
// which returns the index of the asset to show.
//
// splash_policy.wasm is checked in next to splash.html. After changing this
// file, rebuild it from cobalt/shell/embedded_resources/ with Chromium's clang:
//   ../../../third_party/llvm-build/Release+Asserts/bin/clang \
//     --target=wasm32 -O2 -nostdlib -fno-builtin \
//     -Wl,--no-entry -Wl,--strip-all -Wl,--export=items_buffer \
//     -Wl,--export=max_items -Wl,--export=choose \
//     -o loader_embedded_resources/splash_policy.wasm \
//     splash_policy/splash_policy.c

// Provided by the compiler, without libc.
#include <stdint.h>

// A candidate asset, as splash.html writes it: 12 bytes, little-endian.
struct Candidate {
  uint8_t source;    // 0: built-in, 1: cached by the WebApp.
  uint8_t kind;      // 0: image, 1: video.
  uint8_t topic;     // 0: default, 1: music.
  uint8_t playable;  // Whether the platform plays it: 0 or 1.
  uint16_t width;    // In pixels, 0 if unknown.
  uint16_t height;   // In pixels, 0 if unknown.
  uint32_t bytes;    // The size of the asset.
};

// The flags of choose().
enum {
  kVp9 = 1,         // The platform plays VP9 videos.
  kForceImage = 4,  // The splash screen shows its image.
};

enum { kMaxItems = 32 };

static struct Candidate items[kMaxItems];

// Where splash.html writes the candidates.
struct Candidate* items_buffer(void) {
  return items;
}

// How many candidates items_buffer() holds.
int max_items(void) {
  return kMaxItems;
}

// Returns the index of the asset to show among the `count` first candidates of
// items_buffer(), or -1 if none fits: an image if `flags` has kForceImage or
// lacks kVp9, and a video otherwise. `topic` is the launch's topic, as
// Candidate::topic.
int choose(int count, int flags, int topic) {
  const int want_image = (flags & kForceImage) || !(flags & kVp9);
  int best = -1;
  int best_score = -1;
  for (int i = 0; i < count && i < kMaxItems; ++i) {
    const struct Candidate* c = &items[i];
    if (!c->playable || want_image != (c->kind == 0)) {
      continue;
    }
    // The launch's topic first, then what the WebApp cached, then the highest
    // resolution that the platform plays.
    int score = c->height;
    if (c->source == 1) {
      score += 10000;
    }
    if (c->topic == topic) {
      score += 100000;
    }
    if (score > best_score) {
      best_score = score;
      best = i;
    }
  }
  return best;
}
