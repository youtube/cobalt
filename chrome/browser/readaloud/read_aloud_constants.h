// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READALOUD_READ_ALOUD_CONSTANTS_H_
#define CHROME_BROWSER_READALOUD_READ_ALOUD_CONSTANTS_H_

#include <cstddef>

#include "base/time/time.h"
#include "media/base/channel_layout.h"

namespace readaloud {

inline constexpr int kAudioSampleRate = 48000;
inline constexpr media::ChannelLayout kAudioChannelLayout =
    media::CHANNEL_LAYOUT_MONO;
// 500 KiB cap.
inline constexpr size_t kMaxMojoPayloadSizeBytes = 512000;
inline constexpr base::TimeDelta kAudioBufferMinDuration = base::Seconds(5);
inline constexpr base::TimeDelta kAudioBufferPrefetchWatermark =
    base::Seconds(15);
inline constexpr base::TimeDelta kMaxDecodedAudioDuration = base::Seconds(50);
inline constexpr base::TimeDelta kNetworkTimeout = base::Seconds(5);
inline constexpr base::TimeDelta kBufferStallTimeout = base::Seconds(15);
inline constexpr base::TimeDelta kBrowserBufferingWatchdog = base::Seconds(30);
inline constexpr base::TimeDelta kVolumeRampDuration = base::Milliseconds(10);
inline constexpr int kMaxRetryAttempts = 3;

}  // namespace readaloud

#endif  // CHROME_BROWSER_READALOUD_READ_ALOUD_CONSTANTS_H_
