// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/string_number_conversions.h"
#include "media/audio/audio_parameters.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(AudioParameters, GetPacketSize) {
  EXPECT_EQ(100, AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                                 1, 1000,  8, 100).GetPacketSize());
  EXPECT_EQ(200, AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                                 1, 1000,  16, 100).GetPacketSize());
  EXPECT_EQ(200, AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                                 2, 1000,  8, 100).GetPacketSize());
  EXPECT_EQ(200, AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                                 1, 1000,  8, 200).GetPacketSize());
  EXPECT_EQ(800, AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                                 2, 1000,  16, 200).GetPacketSize());
}

TEST(AudioParameters, Compare) {
  AudioParameters values[] = {
    AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, 1, 1000,  8, 100),
    AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, 1, 1000,  8, 200),
    AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, 1, 1000, 16, 100),
    AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, 1, 1000, 16, 200),
    AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, 1, 2000,  8, 100),
    AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, 1, 2000,  8, 200),
    AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, 1, 2000, 16, 100),
    AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, 1, 2000, 16, 200),

    AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, 2, 1000,  8, 100),
    AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, 2, 1000,  8, 200),
    AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, 2, 1000, 16, 100),
    AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, 2, 1000, 16, 200),
    AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, 2, 2000,  8, 100),
    AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, 2, 2000,  8, 200),
    AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, 2, 2000, 16, 100),
    AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, 2, 2000, 16, 200),

    AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY, 1, 1000,  8, 100),
    AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY, 1, 1000,  8, 200),
    AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY, 1, 1000, 16, 100),
    AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY, 1, 1000, 16, 200),
    AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY, 1, 2000,  8, 100),
    AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY, 1, 2000,  8, 200),
    AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY, 1, 2000, 16, 100),
    AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY, 1, 2000, 16, 200),

    AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY, 2, 1000,  8, 100),
    AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY, 2, 1000,  8, 200),
    AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY, 2, 1000, 16, 100),
    AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY, 2, 1000, 16, 200),
    AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY, 2, 2000,  8, 100),
    AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY, 2, 2000,  8, 200),
    AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY, 2, 2000, 16, 100),
    AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY, 2, 2000, 16, 200),
  };

  AudioParameters::Compare target;
  for (size_t i = 0; i < arraysize(values); ++i) {
    for (size_t j = 0; j < arraysize(values); ++j) {
      SCOPED_TRACE("i=" + base::IntToString(i) + " j=" + base::IntToString(j));
      EXPECT_EQ(i < j, target(values[i], values[j]));
    }

    // Verify that a value is never less than itself.
    EXPECT_FALSE(target(values[i], values[i]));
  }
}
