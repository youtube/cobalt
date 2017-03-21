// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/base/audio_latency.h"

#include <stdint.h>

#include "base/logging.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// TODO(olka): extend unit tests, use real-world sample rates.

TEST(AudioLatency, HighLatencyBufferSizes) {
#if defined(OS_WIN)
  for (int i = 6400; i <= 204800; i *= 2) {
    EXPECT_EQ(2 * (i / 100),
              AudioLatency::GetHighLatencyBufferSize(i, i / 100));
  }
#else
  for (int i = 6400; i <= 204800; i *= 2)
    EXPECT_EQ(2 * (i / 100), AudioLatency::GetHighLatencyBufferSize(i, 32));
#endif  // defined(OS_WIN)
}

TEST(AudioLatency, InteractiveBufferSizes) {
#if defined(OS_ANDROID)
  int i = 6400;
  for (; i <= 102400; i *= 2)
    EXPECT_EQ(2048, AudioLatency::GetInteractiveBufferSize(i / 100));
  for (; i <= 204800; i *= 2)
    EXPECT_EQ(i / 100, AudioLatency::GetInteractiveBufferSize(i / 100));
#else
  for (int i = 6400; i <= 204800; i *= 2)
    EXPECT_EQ(i / 100, AudioLatency::GetInteractiveBufferSize(i / 100));
#endif  // defined(OS_ANDROID)
}

TEST(AudioLatency, RtcBufferSizes) {
  for (int i = 6400; i < 204800; i *= 2) {
    EXPECT_EQ(i / 100, AudioLatency::GetRtcBufferSize(i, 0));
#if defined(OS_WIN)
    EXPECT_EQ(500, AudioLatency::GetRtcBufferSize(i, 500));
#elif defined(OS_ANDROID)
    EXPECT_EQ(i / 50, AudioLatency::GetRtcBufferSize(i, i / 50 - 1));
    EXPECT_EQ(i / 50 + 1, AudioLatency::GetRtcBufferSize(i, i / 50 + 1));
#else
    EXPECT_EQ(i / 100, AudioLatency::GetRtcBufferSize(i, 500));
#endif  // defined(OS_WIN)
  }
}
}  // namespace media
