// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace media;

// Test that input is supported and output is not.
TEST(IOSAudioTest, AudioSupport) {
  AudioManager* audio_manager = AudioManager::Create();
  ASSERT_TRUE(NULL != audio_manager);
  ASSERT_FALSE(audio_manager->HasAudioOutputDevices());
  ASSERT_TRUE(audio_manager->HasAudioInputDevices());
}

// Test that input stream can be opened and closed.
TEST(IOSAudioTest, InputStreamOpenAndClose) {
  AudioManager* audio_manager = AudioManager::Create();
  ASSERT_TRUE(NULL != audio_manager);
  if (!audio_manager->HasAudioInputDevices())
    return;
  AudioInputStream* ias = audio_manager->MakeAudioInputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_STEREO,
                      8000, 16, 1024),
      std::string("test_device"));
  ASSERT_TRUE(NULL != ias);
  EXPECT_TRUE(ias->Open());
  ias->Close();
}
