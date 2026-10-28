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
// The splash screen's policy, in WebAssembly: which asset to fetch, and where
// from. It chooses what splash.html chooses without it: splash.webm (or
// splash.png) in the topic's cache, and the topic's built-in asset as the
// fallback, so that only how the choice is made differs.
//
// Without libc. JavaScript writes the topic, e.g., "music", to
// input_buffer(), calls decide(), and reads the request URL from
// output_buffer(), e.g.,
// "h5vcc-embedded://splash.webm?src=cache:music/splash.webm&src=builtin:music_splash_480.webm".
//
// Rebuild loader_embedded_resources/splash_policy.wasm with Chromium's clang:
//   third_party/llvm-build/Release+Asserts/bin/clang --target=wasm32 -O2 \
//     -nostdlib -fno-builtin -Wl,--no-entry -Wl,--strip-all \
//     -Wl,--export=decide -Wl,--export=input_buffer \
//     -Wl,--export=output_buffer \
//     -o cobalt/shell/embedded_resources/loader_embedded_resources/splash_policy.wasm \
//     cobalt/shell/embedded_resources/splash_policy/splash_policy.c

// The flags of decide().
enum {
  kVp9 = 1,         // The platform plays VP9.
  kVp9_4k = 2,      // The platform plays 4K VP9.
  kForceImage = 4,  // The splash screen shows its image.
};

static char in[64];
static char out[256];
static int n;

char* input_buffer(void) {
  return in;
}

char* output_buffer(void) {
  return out;
}

static void put(const char* s) {
  while (*s && n < (int)sizeof(out) - 1) {
    out[n++] = *s++;
  }
}

static int topic_is(const char* s, int len) {
  int i = 0;
  for (; i < len && s[i]; ++i) {
    if (in[i] != s[i]) {
      return 0;
    }
  }
  return i == len && s[i] == 0;
}

// Writes the request URL of the splash screen's asset to output_buffer(), and
// returns its length. `topic_len` is the length of the topic in
// input_buffer().
int decide(int flags, int topic_len) {
  if (topic_len < 0 || topic_len > (int)sizeof(in)) {
    topic_len = 0;
  }
  const int music = topic_is("music", topic_len);
  const char* cache = music ? "music" : "default";
  const char* prefix = music ? "music_" : "";
  n = 0;
  if ((flags & kForceImage) || !(flags & kVp9)) {
    put("h5vcc-embedded://splash.png?src=cache:");
    put(cache);
    put("/splash.png&src=builtin:");
    put(prefix);
    put("splash.png");
  } else {
    put("h5vcc-embedded://splash.webm?src=cache:");
    put(cache);
    put("/splash.webm&src=builtin:");
    put(prefix);
    put("splash");
    if (!(flags & kVp9_4k)) {
      put("_480");
    }
    put(".webm");
  }
  out[n] = 0;
  return n;
}
