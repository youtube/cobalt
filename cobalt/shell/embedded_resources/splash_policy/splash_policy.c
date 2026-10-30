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

// DO NOT SUBMIT: benchmark only (cobalt::features::kSplashWasmPolicy).
//
// The splash screen's policy, in WebAssembly: chooses which asset to show
// from a list of the available ones, built-in or cached by the WebApp
// (SplashAssetList). It chooses what splash.html and the native video choose
// without it: what the WebApp cached for the launch's topic, if any, and the
// topic's built-in asset otherwise, the 1080p video only if the platform plays
// 4K VP9, so that only how the choice is made differs.
//
// Without libc. JavaScript writes one Candidate per asset to items_buffer(),
// at most max_items(), and choose() returns the index of the asset to show.
//
// Rebuild loader_embedded_resources/splash_policy.wasm with Chromium's clang:
//   third_party/llvm-build/Release+Asserts/bin/clang --target=wasm32 -O2 \
//     -nostdlib -fno-builtin -Wl,--no-entry -Wl,--strip-all \
//     -Wl,--export=items_buffer -Wl,--export=max_items -Wl,--export=choose \
//     -o cobalt/shell/embedded_resources/loader_embedded_resources/splash_policy.wasm \
//     cobalt/shell/embedded_resources/splash_policy/splash_policy.c

// An available asset, as written by JavaScript: 12 bytes, little-endian.
struct Candidate {
  unsigned char source;    // 0: built-in, 1: cached by the WebApp.
  unsigned char kind;      // 0: image, 1: video.
  unsigned char topic;     // 0: default, 1: music.
  unsigned char playable;  // Whether the platform plays its type and size.
  unsigned short width;    // In pixels, 0 if unknown.
  unsigned short height;   // In pixels, 0 if unknown.
  unsigned int bytes;
};

// The flags of choose().
enum {
  kVp9 = 1,         // The platform plays VP9.
  kForceImage = 4,  // The splash screen shows its image.
};

enum { kMaxItems = 32 };

static struct Candidate items[kMaxItems];

struct Candidate* items_buffer(void) {
  return items;
}

int max_items(void) {
  return kMaxItems;
}

// Returns the index of the asset to show among the `count` first ones of
// items_buffer(), or -1 if none fits. `topic` is the launch's topic.
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
