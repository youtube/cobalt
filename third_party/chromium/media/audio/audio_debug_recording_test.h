// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_AUDIO_AUDIO_DEBUG_RECORDING_TEST_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_AUDIO_AUDIO_DEBUG_RECORDING_TEST_H_

#include <memory>

#include "base/test/task_environment.h"
#include "third_party/chromium/media/base/media_export.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_m96 {

class MockAudioDebugRecordingManager;
class MockAudioManager;

// Base test class for media/audio/ and services/audio/ debug recording test
// classes.
class AudioDebugRecordingTest : public testing::Test {
 public:
  AudioDebugRecordingTest();

  AudioDebugRecordingTest(const AudioDebugRecordingTest&) = delete;
  AudioDebugRecordingTest& operator=(const AudioDebugRecordingTest&) = delete;

  ~AudioDebugRecordingTest() override;

 protected:
  void CreateAudioManager();
  void ShutdownAudioManager();
  void InitializeAudioDebugRecordingManager();

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockAudioManager> mock_audio_manager_;
  MockAudioDebugRecordingManager* mock_debug_recording_manager_;
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_AUDIO_AUDIO_DEBUG_RECORDING_TEST_H_
