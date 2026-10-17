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

#ifndef COBALT_SHELL_BROWSER_SPLASH_VIDEO_WEBM_INDEXER_H_
#define COBALT_SHELL_BROWSER_SPLASH_VIDEO_WEBM_INDEXER_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <vector>

#include "base/containers/span.h"

namespace content {

// A frame of a splash screen video: the `size` bytes at `offset` in the WebM
// file it was read from.
struct SplashVideoFrame {
  size_t offset = 0;
  size_t size = 0;
  int64_t timestamp_us = 0;
  bool is_key_frame = false;
};

// The frames of a splash screen video, in decode order.
struct SplashVideoIndex {
  SplashVideoIndex();
  SplashVideoIndex(SplashVideoIndex&&);
  SplashVideoIndex& operator=(SplashVideoIndex&&);
  ~SplashVideoIndex();

  int width = 0;
  int height = 0;
  std::vector<SplashVideoFrame> frames;
};

// Limits on the splash screen videos accepted by IndexSplashVideoWebM(). The
// file size limit is the default one, for the built-in videos.
inline constexpr size_t kMaxSplashVideoFileSize = 1024 * 1024;
inline constexpr size_t kMaxSplashVideoFrames = 300;
inline constexpr int64_t kMaxSplashVideoDurationUs = 10'000'000;
inline constexpr int kMaxSplashVideoDimension = 4096;

// Indexes the frames of a WebM splash screen video without decoding or copying
// them, with the //media WebM element parser rather than the Media Source
// Extensions pipeline.
//
// Accepts a single VP9 video track whose frames aren't laced, whose first frame
// is a key frame and whose timestamps increase, within the limits above, in a
// file of at most `max_file_size` bytes. Every frame must start with a VP9
// frame marker. Returns std::nullopt for anything else, so that `webm` can come
// from an untrusted source.
std::optional<SplashVideoIndex> IndexSplashVideoWebM(
    base::span<const uint8_t> webm,
    size_t max_file_size = kMaxSplashVideoFileSize);

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SPLASH_VIDEO_WEBM_INDEXER_H_
