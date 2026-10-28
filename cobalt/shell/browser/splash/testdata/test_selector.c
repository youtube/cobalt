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

// A test splash selector, written against the splash selector ABI v1 (see
// ../README.md). In production the selector is written by the web app team
// (e.g. in Rust); this C version only exists so that the tests can be built
// with the toolchain in the Chromium tree. Regenerate the embedded bytes with
// ./regenerate_test_selector.sh after changing this file.
//
// Selection rules:
//   "topic":"music"            -> "music.webm"
//   "screen_height" >= 2160    -> "splash_2160.webm"
//   otherwise                  -> "splash_1080.webm"
// Test hooks:
//   "test_mode":"loop"         -> never returns (exercises the host timeout)
//   "test_mode":"trap"         -> traps (exercises the host error path)
//   "test_mode":"bad_output"   -> returns an out-of-bounds result

typedef unsigned int u32;
typedef unsigned long long u64;

#define EXPORT(name) __attribute__((export_name(name)))

static unsigned char g_heap[16 * 1024];
static u32 g_heap_top;

EXPORT("splash_abi_version") u32 splash_abi_version(void) {
  return 1;
}

EXPORT("splash_alloc") void* splash_alloc(u32 size) {
  if (size > sizeof(g_heap) - g_heap_top) {
    return 0;
  }
  void* ptr = g_heap + g_heap_top;
  g_heap_top += (size + 7u) & ~7u;
  return ptr;
}

static int Find(const char* s, u32 len, const char* pattern) {
  u32 pattern_len = 0;
  while (pattern[pattern_len]) {
    ++pattern_len;
  }
  for (u32 i = 0; i + pattern_len <= len; ++i) {
    u32 j = 0;
    while (j < pattern_len && s[i + j] == pattern[j]) {
      ++j;
    }
    if (j == pattern_len) {
      return (int)(i + pattern_len);
    }
  }
  return -1;
}

static u32 ParseUint(const char* s, u32 len, int pos) {
  u32 value = 0;
  for (u32 i = (u32)pos; i < len && s[i] >= '0' && s[i] <= '9'; ++i) {
    value = value * 10 + (u32)(s[i] - '0');
  }
  return value;
}

static u64 Result(const char* out) {
  u32 len = 0;
  while (out[len]) {
    ++len;
  }
  return ((u64)len << 32) | (u32)(unsigned long)out;
}

EXPORT("splash_select") u64 splash_select(const char* input, u32 len) {
  if (Find(input, len, "\"test_mode\":\"loop\"") >= 0) {
    volatile u32 counter = 0;
    for (;;) {
      ++counter;
    }
  }
  if (Find(input, len, "\"test_mode\":\"trap\"") >= 0) {
    __builtin_trap();
  }
  if (Find(input, len, "\"test_mode\":\"bad_output\"") >= 0) {
    return ((u64)16 << 32) | 0xFFFFFFF0u;
  }
  if (Find(input, len, "\"topic\":\"music\"") >= 0) {
    return Result("music.webm");
  }
  int pos = Find(input, len, "\"screen_height\":");
  if (pos >= 0 && ParseUint(input, len, pos) >= 2160) {
    return Result("splash_2160.webm");
  }
  return Result("splash_1080.webm");
}
